/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
 *
 * This file is part of OSM2Go.
 *
 * OSM2Go is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OSM2Go is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "wms.h"
#include "wms_p.h"

#include "appdata.h"
#include "fdguard.h"
#include "map.h"
#include "misc.h"
#include "net_io.h"
#include "project.h"
#include "settings.h"
#include "uicontrol.h"
#include "xml_helpers.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <map>
#include <strings.h>
#include <unistd.h>
#include <vector>

#include "osm2go_annotations.h"
#include "osm2go_stl.h"
#include <osm2go_i18n.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

struct charcmp {
  inline bool operator()(const char *a, const char *b) const {
    return g_strcmp0(a, b) < 0;
  }
};

typedef std::map<const char *, WmsImageFormat, charcmp> FormatMap;
static FormatMap ImageFormats;
static std::map<WmsImageFormat, const char *> ImageFormatExtensions;

static void initImageFormats()
{
  ImageFormats["image/png"] = WMS_FORMAT_PNG;
  ImageFormats["image/gif"] = WMS_FORMAT_GIF;
  ImageFormats["image/jpg"] = WMS_FORMAT_JPG;
  ImageFormats["image/jpeg"] = WMS_FORMAT_JPEG;

  ImageFormatExtensions[WMS_FORMAT_PNG] = "png";
  ImageFormatExtensions[WMS_FORMAT_GIF] = "gif";
  ImageFormatExtensions[WMS_FORMAT_JPG] = "jpg";
  ImageFormatExtensions[WMS_FORMAT_JPEG] = "jpg";
}

static bool wms_bbox_is_valid(const pos_area &bounds) {
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

static wms_layer_t *wms_cap_parse_layer(xmlDocPtr doc, xmlNode *a_node) {
  wms_layer_t *wms_layer = O2G_NULLPTR;
  xmlNode *cur_node = O2G_NULLPTR;

  wms_layer = new wms_layer_t();

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Layer") == 0) {
        wms_layer_t *children = wms_cap_parse_layer(doc, cur_node);
        if(children)
          wms_layer->children.push_back(children);
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Name") == 0) {
        xmlString str(xmlNodeListGetString(doc, cur_node->children, 1));
        wms_layer->name = reinterpret_cast<char *>(str.get());
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Title") == 0) {
        xmlString str(xmlNodeListGetString(doc, cur_node->children, 1));
        wms_layer->title = reinterpret_cast<char *>(str.get());
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "SRS") == 0) {
        xmlString str(xmlNodeListGetString(doc, cur_node->children, 1));
        if(strcmp(reinterpret_cast<char *>(str.get()), wms_layer_t::EPSG4326()) == 0)
          wms_layer->epsg4326 = true;
        else
          wms_layer->srs = reinterpret_cast<char *>(str.get());
        printf("SRS = %s\n", str.get());
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "LatLonBoundingBox") == 0) {
        wms_layer->llbbox.bounds.min = pos_t::fromXmlProperties(cur_node, "miny", "minx");
        wms_layer->llbbox.bounds.max = pos_t::fromXmlProperties(cur_node, "miny", "maxx");
      } else
	printf("found unhandled WMT_MS_Capabilities/Capability/Layer/%s\n",
	       cur_node->name);
    }
  }

  wms_layer->llbbox.valid = wms_bbox_is_valid(wms_layer->llbbox.bounds);

  printf("------------------- Layer: %s ---------------------------\n",
	 wms_layer->title.c_str());
  printf("Name: %s\n", wms_layer->name.c_str());
  printf("EPSG-4326: %s\n", wms_layer->epsg4326?"yes":"no");
  if(wms_layer->llbbox.valid)
    printf("LatLonBBox: %f/%f %f/%f\n",
	   wms_layer->llbbox.bounds.min.lat, wms_layer->llbbox.bounds.min.lon,
	   wms_layer->llbbox.bounds.max.lat, wms_layer->llbbox.bounds.max.lon);
  else
    printf("No/invalid LatLonBBox\n");

  return wms_layer;
}

static wms_getmap_t wms_cap_parse_getmap(xmlDocPtr doc, xmlNode *a_node) {
  wms_getmap_t wms_getmap;
  xmlNode *cur_node;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Format") == 0) {
        xmlString nstr(xmlNodeListGetString(doc, cur_node->children, 1));

        if(nstr) {
          const FormatMap::const_iterator it =
                ImageFormats.find(reinterpret_cast<char *>(nstr.get()));
         if(it != ImageFormats.end())
            wms_getmap.format |= it->second;
        }
      } else
	printf("found unhandled "
	       "WMT_MS_Capabilities/Capability/Request/GetMap/%s\n",
	       cur_node->name);
    }
  }

  printf("Supported formats: %s%s%s\n",
	 (wms_getmap.format & WMS_FORMAT_PNG)?"png ":"",
	 (wms_getmap.format & WMS_FORMAT_GIF)?"gif ":"",
	 (wms_getmap.format & (WMS_FORMAT_JPG | WMS_FORMAT_JPEG))?"jpg ":"");
  return wms_getmap;
}

static wms_request_t wms_cap_parse_request(xmlDocPtr doc, xmlNode *a_node) {
  wms_request_t wms_request;
  xmlNode *cur_node;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "GetMap") == 0) {
	wms_request.getmap = wms_cap_parse_getmap(doc, cur_node);
      } else
	printf("found unhandled WMT_MS_Capabilities/Capability/Request/%s\n",
	       cur_node->name);
    }
  }

  return wms_request;
}

static bool wms_cap_parse_cap(xmlDocPtr doc, xmlNode *a_node, wms_cap_t *wms_cap) {
  xmlNode *cur_node;
  bool has_request = false;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Request") == 0) {
	wms_cap->request = wms_cap_parse_request(doc, cur_node);
        has_request = true;
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Layer") == 0) {
        wms_layer_t *layer = wms_cap_parse_layer(doc, cur_node);
        if(layer)
          wms_cap->layers.push_back(layer);
      } else
	printf("found unhandled WMT_MS_Capabilities/Capability/%s\n",
	       cur_node->name);
    }
  }

  return has_request && !wms_cap->layers.empty();
}

static bool wms_cap_parse(wms_t &wms, xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = O2G_NULLPTR;
  bool has_service = false, has_cap = false;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
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
static bool wms_cap_parse_root(wms_t &wms, xmlDocPtr doc) {
  bool ret = false;

  for (xmlNodePtr cur_node = xmlDocGetRootElement(doc);
       cur_node != O2G_NULLPTR; cur_node = cur_node->next) {
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
static void wms_setup_extent(project_t *project, wms_t *wms) {
  bounds_t bounds;
  bounds.init(project->bounds);

  lpos_t lmin = project->bounds.min.toLpos();
  lmin.x -= bounds.center.x;
  lmin.y -= bounds.center.y;
  lmin.x *= bounds.scale;
  lmin.y *= bounds.scale;

  lpos_t lmax = project->bounds.max.toLpos();
  lmax.x -= bounds.center.x;
  lmax.y -= bounds.center.y;
  lmax.x *= bounds.scale;
  lmax.y *= bounds.scale;

  wms->width = std::min(lmax.x - lmin.x, 2048);
  wms->height = std::min(lmax.y - lmin.y, 2048);

  printf("WMS: required image size = %dx%d\n",
	 wms->width, wms->height);
}

/* --------------- freeing stuff ------------------- */

void wms_layers_free(wms_layer_t::list &layers) {
  std::for_each(layers.begin(), layers.end(), std::default_delete<wms_layer_t>());
}

wms_layer_t::~wms_layer_t() {
  wms_layers_free(children);
}

wms_t::~wms_t() {
  wms_layers_free(cap.layers);
}

/* ---------------------- use ------------------- */

bool wms_llbbox_fits(const project_t *project, const wms_llbbox_t &llbbox) {
  return ((project->bounds.min.lat >= llbbox.bounds.min.lat) &&
          (project->bounds.min.lon >= llbbox.bounds.min.lon) &&
          (project->bounds.max.lat <= llbbox.bounds.max.lat) &&
          (project->bounds.max.lon <= llbbox.bounds.max.lon));
}

struct child_layer_functor {
  bool epsg4326;
  const wms_llbbox_t * const llbbox;
  const std::string &srs;
  wms_layer_t::list &clayers;
  child_layer_functor(bool e, const wms_llbbox_t *x, const std::string &s,
                      wms_layer_t::list &c)
    : epsg4326(e), llbbox(x), srs(s), clayers(c) {}
  void operator()(const wms_layer_t *layer);
};

void child_layer_functor::operator()(const wms_layer_t *layer)
{
  /* get a copy of the parents values for the current one ... */
  const wms_llbbox_t *local_llbbox = llbbox;
  bool local_epsg4326 = epsg4326;

  /* ... and overwrite the inherited stuff with better local stuff */
  if(layer->llbbox.valid)                    local_llbbox = &layer->llbbox;
  local_epsg4326 |= layer->epsg4326;

  /* only named layers with useful bounding box are added to the list */
  if(local_llbbox && !layer->name.empty()) {
    wms_layer_t *c_layer = new wms_layer_t(layer->title, layer->name,
                                           local_epsg4326 ? std::string() : srs,
                                           local_epsg4326, *local_llbbox);
    clayers.push_back(c_layer);
  }

  std::for_each(layer->children.begin(), layer->children.end(),
                child_layer_functor(local_epsg4326,
                                    local_llbbox, srs, clayers));
}

struct requestable_layers_functor {
  wms_layer_t::list &c_layer;
  explicit requestable_layers_functor(wms_layer_t::list &c) : c_layer(c) {}
  void operator()(const wms_layer_t *layer);
};

void requestable_layers_functor::operator()(const wms_layer_t* layer)
{
  const wms_llbbox_t *llbbox = &layer->llbbox;
  if(!llbbox->valid)
    llbbox = O2G_NULLPTR;

  std::for_each(layer->children.begin(), layer->children.end(),
                child_layer_functor(layer->epsg4326, llbbox, layer->srs,
                                    c_layer));
}

static bool setBgImage(appdata_t &appdata, const std::string &filename) {
  bool ret = appdata.map->set_bg_image(filename);
  if(ret) {
    appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_WMS_CLEAR, true);
    appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_WMS_ADJUST, true);
  }
  return ret;
}

static inline bool layer_is_usable(const wms_layer_t *layer) {
  return layer->is_usable();
}

struct find_format_reverse_functor {
  const unsigned int mask;
  explicit find_format_reverse_functor(unsigned int m) : mask(m) {}
  bool operator()(const std::pair<const char *, WmsImageFormat> &p) {
    return p.second & mask;
  }
};

static std::string wmsUrl(const wms_t &wms, const char *get)
{
  /* nothing has to be done if the last character of path is already a valid URL delimiter */
  const char lastCh = wms.path[wms.path.size() - 1];
  const char *append_char = (lastCh == '?' || lastCh == '&') ? "" :
  /* if there's already a question mark, then add further */
  /* parameters using the &, else use the ? */
                            (wms.path.find('?') != std::string::npos ? "&" : "?");
  std::string url;
  url.reserve(256); // make enough room that most URLs will need no reallocation
  url = wms.server + wms.path + append_char +
                                 "SERVICE=wms"
                                 "&VERSION=1.1.1"
                                 "&REQUEST=Get";
  url += get;

  return url;
}

wms_layer_t::list wms_get_layers(appdata_t& appdata, wms_t& wms)
{
  wms_layer_t::list layers;

  /* ------------- copy values back into project ---------------- */
  appdata.project->wms_server = wms.server;
  appdata.project->wms_path = wms.path;

  /* ----------- request capabilities -------------- */

  const std::string &url = wmsUrl(wms, "Capabilities");
  std::string capmem;

  /* ----------- parse capabilities -------------- */
  if(unlikely(!net_io_download_mem(appdata.window, url, capmem, _("WMS capabilities")))) {
    errorf(appdata.window, _("WMS download failed:\n\nGetCapabilities failed"));
    return layers;
  }

  if(unlikely(ImageFormats.empty()))
    initImageFormats();

  std::unique_ptr<xmlDoc, xmlDocDelete> doc(xmlReadMemory(capmem.c_str(), capmem.size(),
                                                          O2G_NULLPTR, O2G_NULLPTR, XML_PARSE_NONET));

  /* parse the file and get the DOM */
  bool parse_success = false;
  if(unlikely(!doc)) {
    xmlErrorPtr errP = xmlGetLastError();
    errorf(appdata.window, _("WMS download failed:\n\n"
            "XML error while parsing capabilities:\n%s"), errP->message);
  } else {
    printf("ok, parse doc tree\n");

    parse_success = wms_cap_parse_root(wms, doc.get());
  }

  capmem.clear();

  /* ------------ basic checks ------------- */

  if(!parse_success) {
    errorf(appdata.window, _("Incomplete/unexpected reply!"));
    return layers;
  }

  if(!wms.cap.request.getmap.format) {
    errorf(appdata.window, _("No supported image format found."));
    return layers;
  }

  /* ---------- evaluate layers ----------- */
  printf("\nSearching for usable layers\n");

  requestable_layers_functor fc(layers);

  std::for_each(wms.cap.layers.begin(), wms.cap.layers.end(), fc);
  bool at_least_one_ok = std::find_if(layers.begin(), layers.end(), layer_is_usable) !=
                         layers.end();

  if(!at_least_one_ok) {
    errorf(appdata.window, _("Server provides no data in the required format!\n\n"
	     "(epsg4326 and LatLonBoundingBox are mandatory for osm2go)"));
    wms_layers_free(layers);
    layers.clear();
  }

  return layers;
}

void wms_get_selected_layer(appdata_t &appdata, wms_t &wms,
                            const wms_layer_t::list &layers,
                            const wms_layer_t::list &selected)
{
  /* get required image size */
  wms_setup_extent(appdata.project, &wms);

  /* start building url */
  std::string url = wmsUrl(wms, "Map&LAYERS=");

  /* append layers */
  const wms_layer_t::list::const_iterator selEnd = selected.end();
  wms_layer_t::list::const_iterator selIt = selected.begin();
  if(selIt != selEnd) {
    url += (*selIt)->name;
    for(++selIt; selIt != selEnd; selIt++) {
      url += ',';
      url += (*selIt)->name;
    }
  }

  /* uses epsg4326 if possible */
#if __cplusplus >= 201103L
  const std::string srss = std::move(layers.front()->srs);
#else
  std::string srss;
  srss.swap(layers.front()->srs);
#endif
  const char *srs = srss.empty() ? wms_layer_t::EPSG4326() : srss.c_str();

  /* append styles entry */
  url += "&STYLES=";

  if(selected.size() > 1)
    url += std::string(selected.size() - 1, ',');

  /* build strings of min and max lat and lon to be used in url */
  const std::string coords = appdata.project->bounds.print(',');

  /* find preferred supported video format */
  const FormatMap::const_iterator itEnd = ImageFormats.end();
  FormatMap::const_iterator it = std::find_if(std::cbegin(ImageFormats), itEnd,
                                              find_format_reverse_functor(wms.cap.request.getmap.format));
  assert(it != itEnd);

  char buf[64];
  sprintf(buf, "&WIDTH=%d&HEIGHT=%d&FORMAT=", wms.width, wms.height);

  /* build complete url */
  const std::array<const char *, 7> parts = { {
                          "&SRS=", srs, "&BBOX=", coords.c_str(),
                          buf, it->first, "&reaspect=false"
                          } };
  for(unsigned int i = 0; i < parts.size(); i++)
    url += parts[i];

  const std::string filename = std::string(appdata.project->path) + "wms." +
                               ImageFormatExtensions[it->second];

  /* remove any existing image before */
  wms_remove(appdata);

  if(!net_io_download_file(appdata.window, url, filename, _("WMS layer")))
    return;

  /* there should be a matching file on disk now */
  setBgImage(appdata, filename);
}

/* try to load an existing image into map */
void wms_load(appdata_t &appdata) {
  if(unlikely(ImageFormatExtensions.empty()))
    initImageFormats();

  const std::map<WmsImageFormat, const char *>::const_iterator itEnd = ImageFormatExtensions.end();
  std::map<WmsImageFormat, const char *>::const_iterator it = ImageFormatExtensions.begin();

  std::string filename = appdata.project->path + "/wms.";
  const std::string::size_type extpos = filename.size();

  for(; it != itEnd; it++) {
    filename.erase(extpos);
    filename += it->second;

    appdata.map->bg.offset.x = appdata.project->wms_offset.x;
    appdata.map->bg.offset.y = appdata.project->wms_offset.y;

    if(setBgImage(appdata, filename))
      break;
  }
}

void wms_remove_file(project_t &project) {
  if(unlikely(ImageFormatExtensions.empty()))
    initImageFormats();

  fdguard dirfd(project.path.c_str());
  if(unlikely(!dirfd.valid()))
    return;

  std::string filename = "wms.";
  const std::string::size_type extpos = filename.size();

  const std::map<WmsImageFormat, const char *>::const_iterator itEnd = ImageFormatExtensions.end();
  for(std::map<WmsImageFormat, const char *>::const_iterator it = ImageFormatExtensions.begin();
      it != itEnd; it++) {
    filename.erase(extpos);
    filename += it->second;

    unlinkat(project.dirfd, filename.c_str(), 0);
  }
}

void wms_remove(appdata_t &appdata) {

  /* this cancels any wms adjustment in progress */
  if(appdata.map->action.type == MAP_ACTION_BG_ADJUST)
    map_action_cancel(appdata.map);

  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_WMS_CLEAR, false);
  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_WMS_ADJUST, false);

  appdata.map->remove_bg_image();

  wms_remove_file(*appdata.project);
}

struct server_preset_s {
  const char *name, *server, *path;
};

static const std::array<struct server_preset_s, 1> default_servers = { {
  { "Open Geospatial Consortium Web Services", "http://ows.terrestris.de", "/osm/service?" }
  /* add more servers here ... */
} };

std::vector<wms_server_t *> wms_server_get_default(void) {
  std::vector<wms_server_t *> servers;

  for(unsigned int i = 0; i < default_servers.size(); i++) {
    wms_server_t *cur = new wms_server_t();
    cur->name = default_servers[i].name;
    cur->server = default_servers[i].server;
    cur->path = default_servers[i].path;
    servers.push_back(cur);
  }

  return servers;
}
