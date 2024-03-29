/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "wms.h"
#include "wms_p.h"

#include "fdguard.h"
#include "misc.h"
#include "net_io.h"
#include "notifications.h"
#include "project.h"
#include "settings.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <map>
#include <numeric>
#include <strings.h>
#include <unistd.h>
#include <vector>

#include "osm2go_annotations.h"
#include "osm2go_stl.h"
#include <osm2go_i18n.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

namespace {

enum WmsImageFormat {
  WMS_FORMAT_JPG = (1<<0),
  WMS_FORMAT_JPEG = (1<<1),
  WMS_FORMAT_PNG = (1<<2),
  WMS_FORMAT_GIF = (1<<3)
};

struct wms_getmap_t {
  wms_getmap_t()
    : format(0) {}
  unsigned int format;
};

struct wms_cap_t {
  wms_layer_t::list layers;
  wms_getmap_t request;
};

struct wms_t {
  explicit wms_t(const std::string &s)
    : server(s) {}

  std::string server;
  struct size_t {
    size_t() : width(0), height(0) {}
    unsigned int width, height;
  };
  size_t size;

  wms_cap_t cap;
};

struct charcmp {
  inline bool operator()(const char *a, const char *b) const {
    if(unlikely(a == nullptr))
      return b != nullptr;
    if(unlikely(b == nullptr))
      return false;
    return strcmp(a, b) < 0;
  }
};

typedef std::map<const char *, WmsImageFormat, charcmp> FormatMap;
typedef std::map<WmsImageFormat, const char *> ExtensionMap;

inline FormatMap
initImageFormats()
{
  FormatMap ImageFormats;

  ImageFormats["image/png"] = WMS_FORMAT_PNG;
  ImageFormats["image/gif"] = WMS_FORMAT_GIF;
  ImageFormats["image/jpg"] = WMS_FORMAT_JPG;
  ImageFormats["image/jpeg"] = WMS_FORMAT_JPEG;

  return ImageFormats;
}

const FormatMap &
imageFormats()
{
  static const FormatMap ret = initImageFormats();

  return ret;
}

inline ExtensionMap
initFormatExtensions()
{
  ExtensionMap ImageFormatExtensions;

  ImageFormatExtensions[WMS_FORMAT_PNG] = "png";
  ImageFormatExtensions[WMS_FORMAT_GIF] = "gif";
  ImageFormatExtensions[WMS_FORMAT_JPG] = "jpg";
  ImageFormatExtensions[WMS_FORMAT_JPEG] = "jpg";

  return ImageFormatExtensions;
}

const ExtensionMap &
imageFormatExtensions()
{
  static const ExtensionMap ret = initFormatExtensions();

  return ret;
}

bool
wms_bbox_is_valid(const pos_area &bounds)
{
  /* all four coordinates are valid? */
  if(unlikely(!bounds.valid()))
    return false;

  /* min/max span a useful range? */
  if(unlikely(bounds.max.lat - bounds.min.lat < 0.1))
    return false;
  if(unlikely(bounds.max.lon - bounds.min.lon < 0.1))
    return false;

  return true;
}

wms_layer_t
wms_cap_parse_layer(xmlDocPtr doc, xmlNode *a_node)
{
  wms_layer_t wms_layer;
  xmlNode *cur_node = nullptr;

  for (cur_node = a_node->children; cur_node != nullptr; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Layer") == 0) {
        wms_layer.children.push_back(wms_cap_parse_layer(doc, cur_node));
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Name") == 0) {
        xmlString str(xmlNodeListGetString(doc, cur_node->children, 1));
        wms_layer.name = static_cast<const char *>(str);
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Title") == 0) {
        xmlString str(xmlNodeListGetString(doc, cur_node->children, 1));
        wms_layer.title = static_cast<const char *>(str);
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "SRS") == 0) {
        xmlString str(xmlNodeListGetString(doc, cur_node->children, 1));
        if(strcmp(str, wms_layer_t::EPSG4326()) == 0)
          wms_layer.epsg4326 = true;
        else
          wms_layer.srs = static_cast<const char *>(str);
        printf("SRS = %s\n", str.get());
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "LatLonBoundingBox") == 0) {
        wms_layer.llbbox.bounds.min = pos_t::fromXmlProperties(cur_node, "miny", "minx");
        wms_layer.llbbox.bounds.max = pos_t::fromXmlProperties(cur_node, "maxy", "maxx");
      } else
        printf("found unhandled WMT_MS_Capabilities/Capability/Layer/%s\n", cur_node->name);
    }
  }

  wms_layer.llbbox.valid = wms_bbox_is_valid(wms_layer.llbbox.bounds);

  printf("------------------- Layer: %s ---------------------------\n", wms_layer.title.c_str());
  printf("Name: %s\n", wms_layer.name.c_str());
  printf("EPSG-4326: %s\n", wms_layer.epsg4326?"yes":"no");
  if(wms_layer.llbbox.valid)
    printf("LatLonBBox: %f/%f %f/%f\n",
           wms_layer.llbbox.bounds.min.lat, wms_layer.llbbox.bounds.min.lon,
           wms_layer.llbbox.bounds.max.lat, wms_layer.llbbox.bounds.max.lon);
  else
    printf("No/invalid LatLonBBox\n");

  return wms_layer;
}

wms_getmap_t
wms_cap_parse_getmap(xmlDocPtr doc, xmlNode *a_node)
{
  wms_getmap_t wms_getmap;
  xmlNode *cur_node;
  const FormatMap &ImageFormats = imageFormats();

  for (cur_node = a_node->children; cur_node != nullptr; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Format") == 0) {
        xmlString nstr(xmlNodeListGetString(doc, cur_node->children, 1));

        if(nstr) {
          const FormatMap::const_iterator it = ImageFormats.find(nstr);
          if(it != ImageFormats.end())
            wms_getmap.format |= it->second;
        }
      } else
        printf("found unhandled WMT_MS_Capabilities/Capability/Request/GetMap/%s\n",
               cur_node->name);
    }
  }

  printf("Supported formats: %s%s%s\n",
         (wms_getmap.format & WMS_FORMAT_PNG) ? "png " : "",
         (wms_getmap.format & WMS_FORMAT_GIF) ? "gif " : "",
         (wms_getmap.format & (WMS_FORMAT_JPG | WMS_FORMAT_JPEG)) ? "jpg " : "");
  return wms_getmap;
}

wms_getmap_t
wms_cap_parse_request(xmlDocPtr doc, xmlNode *a_node)
{
  wms_getmap_t wms_request;
  xmlNode *cur_node;

  for (cur_node = a_node->children; cur_node != nullptr; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "GetMap") == 0)
        wms_request = wms_cap_parse_getmap(doc, cur_node);
      else
        printf("found unhandled WMT_MS_Capabilities/Capability/Request/%s\n", cur_node->name);
    }
  }

  return wms_request;
}

bool
wms_cap_parse_cap(xmlDocPtr doc, xmlNode *a_node, wms_cap_t *wms_cap)
{
  xmlNode *cur_node;
  bool has_request = false;

  for (cur_node = a_node->children; cur_node != nullptr; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Request") == 0) {
        wms_cap->request = wms_cap_parse_request(doc, cur_node);
        has_request = true;
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Layer") == 0) {
        wms_cap->layers.push_back(wms_cap_parse_layer(doc, cur_node));
      } else
        printf("found unhandled WMT_MS_Capabilities/Capability/%s\n", cur_node->name);
    }
  }

  return has_request && !wms_cap->layers.empty();
}

bool
wms_cap_parse(wms_t &wms, xmlDocPtr doc, xmlNode *a_node)
{
  xmlNode *cur_node = nullptr;
  bool has_service = false, has_cap = false;

  for (cur_node = a_node->children; cur_node != nullptr; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Service") == 0) {
        has_service = true;
      } else if(likely(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Capability") == 0)) {
        if(!has_cap)
          has_cap = wms_cap_parse_cap(doc, cur_node, &wms.cap);
      } else
        printf("found unhandled WMT_MS_Capabilities/%s\n", cur_node->name);
    }
  }

  return has_service && has_cap;
}

/* parse root element */
bool
wms_cap_parse_root(wms_t &wms, xmlDocPtr doc)
{
  bool ret = false;

  for (xmlNodePtr cur_node = xmlDocGetRootElement(doc);
       cur_node != nullptr; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(likely(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "WMT_MS_Capabilities") == 0))
        ret = wms_cap_parse(wms, doc, cur_node);
      else
        printf("found unhandled %s\n", cur_node->name);
    }
  }

  return ret;
}

/* get pixel extent of image display */
wms_t::size_t
wms_setup_extent(const pos_area &pbounds)
{
  bounds_t bounds;
  bounds.init(pbounds);

  lpos_t lmin = pbounds.min.toLpos();
  lmin.x -= bounds.center.x;
  lmin.y -= bounds.center.y;
  lmin.x *= bounds.scale;
  lmin.y *= bounds.scale;

  lpos_t lmax = pbounds.max.toLpos();
  lmax.x -= bounds.center.x;
  lmax.y -= bounds.center.y;
  lmax.x *= bounds.scale;
  lmax.y *= bounds.scale;

  wms_t::size_t ret;
  ret.width = lmax.x - lmin.x;
  ret.height = lmax.y - lmin.y;

  size_t m = std::max(ret.width, ret.height);
  if (m > 2048) {
    ret.width *= 2048;
    ret.width /= m;
    ret.height *= 2048;
    ret.height /= m;
  }

  printf("WMS: required image size = %dx%d\n", ret.width, ret.height);

  return ret;
}

} // namespace

/* ---------------------- use ------------------- */

bool wms_llbbox_fits(const pos_area &bounds, const wms_llbbox_t &llbbox) {
  return ((bounds.min.lat >= llbbox.bounds.min.lat) &&
          (bounds.min.lon >= llbbox.bounds.min.lon) &&
          (bounds.max.lat <= llbbox.bounds.max.lat) &&
          (bounds.max.lon <= llbbox.bounds.max.lon));
}

namespace {

struct child_layer_functor {
  bool epsg4326;
  const wms_llbbox_t * const llbbox;
  const std::string &srs;
  wms_layer_t::list &clayers;
  inline child_layer_functor(bool e, const wms_llbbox_t *x, const std::string &s,
                      wms_layer_t::list &c)
    : epsg4326(e), llbbox(x), srs(s), clayers(c) {}
  void operator()(const wms_layer_t &layer);
};

void child_layer_functor::operator()(const wms_layer_t &layer)
{
  /* get a copy of the parents values for the current one ... */
  const wms_llbbox_t *local_llbbox = llbbox;
  bool local_epsg4326 = epsg4326;

  /* ... and overwrite the inherited stuff with better local stuff */
  if(layer.llbbox.valid)
    local_llbbox = &layer.llbbox;
  local_epsg4326 |= layer.epsg4326;

  /* only named layers with useful bounding box are added to the list */
  if(local_llbbox != nullptr && !layer.name.empty()) {
    wms_layer_t c_layer(layer.title, layer.name, local_epsg4326 ? std::string() : srs,
                        local_epsg4326, *local_llbbox);
    clayers.push_back(c_layer);
  }

  std::for_each(layer.children.begin(), layer.children.end(),
                child_layer_functor(local_epsg4326,
                                    local_llbbox, srs, clayers));
}

struct requestable_layers_functor {
  wms_layer_t::list &c_layer;
  explicit inline requestable_layers_functor(wms_layer_t::list &c) : c_layer(c) {}
  void operator()(const wms_layer_t &layer);
};

void requestable_layers_functor::operator()(const wms_layer_t &layer)
{
  const wms_llbbox_t *llbbox = &layer.llbbox;
  if(!llbbox->valid)
    llbbox = nullptr;

  std::for_each(layer.children.begin(), layer.children.end(),
                child_layer_functor(layer.epsg4326, llbbox, layer.srs,
                                    c_layer));
}

inline bool
layer_is_usable(const wms_layer_t &layer)
{
  return layer.is_usable();
}

struct find_format_reverse_functor {
  const unsigned int mask;
  explicit inline find_format_reverse_functor(unsigned int m) : mask(m) {}
  inline bool operator()(const std::pair<const char *, WmsImageFormat> &p) const
  {
    return p.second & mask;
  }
};

std::string
wmsUrl(const std::string &wms_server, const char *get)
{
  /* nothing has to be done if the last character of path is already a valid URL delimiter */
  const char lastCh = wms_server[wms_server.size() - 1];
  const char *append_char = (lastCh == '?' || lastCh == '&') ? "" :
  /* if there's already a question mark, then add further */
  /* parameters using the &, else use the ? */
                            (wms_server.find('?') != std::string::npos ? "&" : "?");
  std::string url;
  url.reserve(256); // make enough room that most URLs will need no reallocation
  url = wms_server + append_char + "SERVICE=wms&VERSION=1.1.1&REQUEST=Get";
  url += get;

  return url;
}

wms_layer_t::list
wms_get_layers(osm2go_platform::Widget *parent, wms_t &wms)
{
  wms_layer_t::list layers;

  /* ----------- request capabilities -------------- */
  const std::string &url = wmsUrl(wms.server, "Capabilities");
  std::string capmem;

  /* ----------- parse capabilities -------------- */
  if(unlikely(!net_io_download_mem(parent, url, capmem, _("WMS capabilities")))) {
    error_dlg(_("WMS download failed:\n\nGetCapabilities failed"));
    return layers;
  }

  xmlDocGuard doc(xmlReadMemory(capmem.c_str(), capmem.size(),
                                                          nullptr, nullptr, XML_PARSE_NONET));

  /* parse the file and get the DOM */
  bool parse_success = false;
  if(unlikely(!doc)) {
    const xmlError *errP = xmlGetLastError();
    error_dlg(trstring("WMS download failed:\n\n"
              "XML error while parsing capabilities:\n%1").arg(errP->message));
  } else {
    printf("ok, parse doc tree\n");

    parse_success = wms_cap_parse_root(wms, doc.get());
  }

  capmem.clear();

  /* ------------ basic checks ------------- */

  if(!parse_success) {
    error_dlg(_("Incomplete/unexpected reply!"));
    return layers;
  }

  if(!wms.cap.request.format) {
    error_dlg(_("No supported image format found."));
    return layers;
  }

  /* ---------- evaluate layers ----------- */
  printf("\nSearching for usable layers\n");

  requestable_layers_functor fc(layers);

  std::for_each(wms.cap.layers.begin(), wms.cap.layers.end(), fc);

  if(std::none_of(layers.begin(), layers.end(), layer_is_usable)) {
    error_dlg(_("Server provides no data in the required format!\n\n"
                "(epsg4326 and LatLonBoundingBox are mandatory for osm2go)"));
    layers.clear();
  }

  return layers;
}

std::string
wms_get_selected_layer(osm2go_platform::Widget *parent, project_t::ref project, wms_t &wms,
                       const std::string &layers, const std::string &srss)
{
  /* get required image size */
  wms.size = wms_setup_extent(project->bounds);

  /* uses epsg4326 if possible */
  const char *srs = srss.empty() ? wms_layer_t::EPSG4326() : srss.c_str();

  /* build strings of min and max lat and lon to be used in url */
  const std::string coords = project->bounds.print();

  /* find preferred supported video format */
  const FormatMap &ImageFormats = imageFormats();
  const FormatMap::const_iterator itEnd = ImageFormats.end();
  FormatMap::const_iterator it = std::find_if(std::cbegin(ImageFormats), itEnd,
                                              find_format_reverse_functor(wms.cap.request.format));
  assert(it != itEnd);

  char buf[64];
  sprintf(buf, "&WIDTH=%d&HEIGHT=%d&FORMAT=", wms.size.width, wms.size.height);

  /* build complete url */
  const std::array<const char *, 7> parts = { {
  /* append styles entry */
  // it is required, but it may be entirely empty since at least version 1.1.0
  // meaning "default styles for all layers"
                          "&STYLES="
                          "&SRS=", srs, "&BBOX=", coords.c_str(),
                          buf, it->first, "&reaspect=false"
                          } };
  const std::string url = std::accumulate(parts.begin(), parts.end(), wmsUrl(wms.server, "Map&LAYERS=") + layers);

  const ExtensionMap &ImageFormatExtensions = imageFormatExtensions();
  ExtensionMap::const_iterator extIt = ImageFormatExtensions.find(it->second);
  assert(extIt != ImageFormatExtensions.end());
  const std::string filename = project->path + "wms." + extIt->second;

  /* remove any existing image before */
  wms_remove_file(*project);

  trstring::native_type wtitle = _("WMS layer");
  if(!net_io_download_file(parent, url, filename, wtitle))
    return std::string();

  /* there should be a matching file on disk now */
  return filename;
}

} // namespace

/* try to load an existing image into map */
std::string wms_find_file(const std::string &project_path)
{
  const ExtensionMap &ImageFormatExtensions = imageFormatExtensions();
  const ExtensionMap::const_iterator itEnd = ImageFormatExtensions.end();

  std::string filename = project_path + "/wms.";
  const std::string::size_type extpos = filename.size();

  for(ExtensionMap::const_iterator it = ImageFormatExtensions.begin(); it != itEnd; it++) {
    filename.erase(extpos);
    filename += it->second;

    if (std::filesystem::is_regular_file(filename))
      return filename;
  }

  filename.clear();
  return filename;
}

void wms_remove_file(project_t &project) {

  fdguard dirfd(project.path.c_str());
  if(unlikely(!dirfd.valid()))
    return;

  std::string filename = "wms.";
  const std::string::size_type extpos = filename.size();
  const ExtensionMap &ImageFormatExtensions = imageFormatExtensions();

  const ExtensionMap::const_iterator itEnd = ImageFormatExtensions.end();
  for(ExtensionMap::const_iterator it = ImageFormatExtensions.begin(); it != itEnd; it++) {
    filename.erase(extpos);
    filename += it->second;

    unlinkat(project.dirfd, filename.c_str(), 0);
  }
}

namespace {

struct server_preset_s {
  const char *name, *server;
};

const std::array<struct server_preset_s, 1> default_servers = { {
  { "Open Geospatial Consortium Web Services", "https://ows.terrestris.de/osm/service?" }
  /* add more servers here ... */
} };

} // namespace

std::vector<wms_server_t *> wms_server_get_default()
{
  std::vector<wms_server_t *> servers;

  for(unsigned int i = 0; i < default_servers.size(); i++)
    servers.push_back(new wms_server_t(default_servers[i].name, default_servers[i].server));

  return servers;
}

std::string
wms_import(osm2go_platform::Widget *parent, project_t::ref project)
{
  assert(project);

  wms_t wms(project->wms_server);

  /* reset any background adjustments in the project ... */
  project->wms_offset.x = 0;
  project->wms_offset.y = 0;

  /* get server from dialog */
  std::string srv = wms_server_dialog(parent, project->wms_server);
  if (srv.empty())
    return std::string();

  /* ------------- copy values back into project ---------------- */
  project->wms_server.swap(srv);

  const wms_layer_t::list layers = wms_get_layers(parent, wms);
  if(layers.empty())
    return std::string();

  const std::string &l = wms_layer_dialog(parent, project->bounds, layers);
  if(!l.empty())
    return wms_get_selected_layer(parent, project, wms, l, layers.front().srs);

  return std::string();
}
