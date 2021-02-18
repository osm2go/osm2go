/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "style.h"
#include "style_p.h"

#include "appdata.h"
#include "josm_elemstyles.h"
#include "map.h"
#include "misc.h"
#include "notifications.h"
#include "settings.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <dirent.h>
#include <fdguard.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string>
#include <strings.h>
#include <sys/stat.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include "osm2go_stl.h"

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

namespace {

float parse_scale_max(xmlNodePtr cur_node)
{
  float scale_max = xml_get_prop_float(cur_node, "scale-max");
  if (!std::isnan(scale_max))
    return scaledn_to_zoom(scale_max);
  else
    return 0.0f;
}

void parse_style_node(xmlNode *a_node, xmlString *fname, style_t &style)
{
  /* -------------- setup defaults -------------------- */
  /* (the defaults are pretty much the potlatch style) */
  style.area.border_width      = 2.0;
  style.area.color             = 0x00000060; // 37.5%
  style.area.zoom_max          = 0.1111;     // zoom factor above which an area is visible & selectable

  style.node.radius            = 4.0;
  style.node.border_radius     = 2.0;
  style.node.color             = 0x000000ff; // black with filling ...
  style.node.fill_color        = 0x008800ff; // ... in dark green
  style.node.show_untagged     = false;
  style.node.zoom_max          = 0.4444;     // zoom factor above which a node is visible & selectable

  style.track.width            = 6.0;
  style.track.color            = 0x0000ff40; // blue
  style.track.gps_color        = 0x000080ff;

  style.way.width              = 3.0;
  style.way.color              = 0x606060ff; // grey
  style.way.zoom_max           = 0.2222;     // zoom above which it's visible & selectable

  style.highlight.width        = 3.0;
  style.highlight.color        = 0xffff0080;  // normal highlights are yellow
  style.highlight.node_color   = 0xff000080;  // node highlights are red
  style.highlight.touch_color  = 0x0000ff80;  // touchnode and
  style.highlight.arrow_color  = 0x0000ff80;  // arrows are blue
  style.highlight.arrow_limit  = 4.0;

  style.frisket.color          = color_t::white();
  style.frisket.border.present = true;
  style.frisket.border.width   = 6.0;
  style.frisket.border.color   = 0x00000099;

  style.icon.enable            = false;
  style.icon.scale             = 1.0;    // icon size (multiplier)

  for (xmlNode *cur_node = a_node->children; cur_node != nullptr; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      const char *nodename = reinterpret_cast<const char *>(cur_node->name);
      if(strcmp(nodename, "elemstyles") == 0) {
        fname->reset(xmlGetProp(cur_node, BAD_CAST "filename"));

      } else if(strcmp(nodename, "node") == 0) {
        parse_color(cur_node, "color", style.node.color);
        parse_color(cur_node, "fill-color", style.node.fill_color);
        style.node.radius = xml_get_prop_float(cur_node, "radius");
        style.node.border_radius = xml_get_prop_float(cur_node, "border-radius");
        style.node.zoom_max = parse_scale_max(cur_node);

        style.node.show_untagged = xml_get_prop_bool(cur_node, "show-untagged");

      } else if(strcmp(nodename, "icon") == 0) {
        style.icon.scale = xml_get_prop_float(cur_node, "scale");
        const xmlString prefix(xmlGetProp(cur_node, BAD_CAST "path-prefix"));
        if(prefix)
          style.icon.path_prefix = static_cast<const char *>(prefix);
        style.icon.enable = xml_get_prop_bool(cur_node, "enable");

      } else if(strcmp(nodename, "way") == 0) {
        parse_color(cur_node, "color", style.way.color);
        style.way.width = xml_get_prop_float(cur_node, "width");
        style.way.zoom_max = parse_scale_max(cur_node);

      } else if(strcmp(nodename, "frisket") == 0) {
        parse_color(cur_node, "color", style.frisket.color);
        style.frisket.border.present = false;

        for(xmlNode *sub_node = cur_node->children; sub_node != nullptr; sub_node=sub_node->next) {
          if(sub_node->type == XML_ELEMENT_NODE &&
             strcmp(reinterpret_cast<const char *>(sub_node->name), "border") == 0) {
            style.frisket.border.present = true;
            style.frisket.border.width = xml_get_prop_float(sub_node, "width");

            parse_color(sub_node, "color", style.frisket.border.color);
          }
        }

      } else if(strcmp(nodename, "highlight") == 0) {
        parse_color(cur_node, "color", style.highlight.color);
        parse_color(cur_node, "node-color", style.highlight.node_color);
        parse_color(cur_node, "touch-color", style.highlight.touch_color);
        parse_color(cur_node, "arrow-color", style.highlight.arrow_color);
        style.highlight.width = xml_get_prop_float(cur_node, "width");
        style.highlight.arrow_limit = xml_get_prop_float(cur_node, "arrow-limit");

      } else if(strcmp(nodename, "track") == 0) {
        parse_color(cur_node, "color", style.track.color);
        parse_color(cur_node, "gps-color", style.track.gps_color);
        style.track.width = xml_get_prop_float(cur_node, "width");

      } else if(strcmp(nodename, "area") == 0) {
        style.area.has_border_color =
            parse_color(cur_node, "border-color", style.area.border_color);
        style.area.border_width = xml_get_prop_float(cur_node,"border-width");
        style.area.zoom_max = parse_scale_max(cur_node);

        parse_color(cur_node, "color", style.area.color);

      } else if(likely(strcmp(nodename, "background") == 0)) {
        parse_color(cur_node, "color", style.background.color);

      } else
        printf("  found unhandled style/%s\n", cur_node->name);
    }
  }

  assert(!style.icon.path_prefix.empty() || !style.icon.enable);
}

/**
 * @brief parse a style definition file
 * @param fullname absolute path of the file to read
 * @param fname location to store name of the object style XML file or nullptr
 * @param name_only only parse the style name, leave all other fields empty
 * @param style the object to fill
 * @return if parsing the style succeeded
 *
 * fname may be nullptr when name_only is true
 */
bool style_parse(const std::string &fullname, xmlString *fname, style_t &style)
{
  xmlDocGuard doc(xmlReadFile(fullname.c_str(), nullptr, XML_PARSE_NONET));
  bool ret = false;

  /* parse the file and get the DOM */
  if(unlikely(!doc)) {
    xmlErrorPtr errP = xmlGetLastError();
    printf("parsing %s failed: %s\n", fullname.c_str(), errP->message);
  } else {
    for(xmlNode *cur_node = xmlDocGetRootElement(doc.get()); cur_node != nullptr;
        cur_node = cur_node->next) {
      if (cur_node->type == XML_ELEMENT_NODE) {
        if(likely(strcmp(reinterpret_cast<const char *>(cur_node->name), "style") == 0)) {
          if(likely(!ret)) {
            const xmlString name(xmlGetProp(cur_node, BAD_CAST "name"));
            if(likely(name))
              style.name = static_cast<const char *>(name);
            ret = true;
            if(fname == nullptr)
              break;
            parse_style_node(cur_node, fname, style);
          }
        } else
          printf("  found unhandled %s\n", cur_node->name);
      }
    }
  }
  return ret;
}

} // namespace

style_t *style_load_fname(const std::string &filename) {
  xmlString fname;
  std::unique_ptr<josm_elemstyle> style(std::make_unique<josm_elemstyle>());

  if(likely(style_parse(filename, &fname, *style))) {
    printf("  elemstyle filename: %s\n", static_cast<const char *>(fname));
    if (style->load_elemstyles(fname))
      return style.release();
  }

  return nullptr;
}

style_t *style_t::load(const std::string &name)
{
  printf("Trying to load style %s\n", name.c_str());

  std::string fullname = find_file(name + ".style");

  if (unlikely(fullname.empty())) {
    printf("style %s not found, trying %s instead\n", name.c_str(), DEFAULT_STYLE);
    fullname = find_file(DEFAULT_STYLE ".style");
    if (unlikely(fullname.empty())) {
      printf("  style not found, failed to find fallback style too\n");
      return nullptr;
    }
  }

  printf("  style filename: %s\n", fullname.c_str());

  return style_load_fname(fullname);
}

std::string style_basename(const std::string &name) {
  std::string::size_type pos = name.rfind('/');

  /* and cut off extension */
  std::string::size_type extpos = name.rfind('.');
  if(pos == std::string::npos)
    pos = 0;
  else
    pos++; // skip also the '/' itself

  return name.substr(pos, extpos - pos);
}

/* scan all data directories for the given file extension and */
/* return a list of files matching this extension */
std::map<std::string, std::string> style_scan() {
  std::map<std::string, std::string> ret;
  const char *extension = ".style";

  std::string fullname;

  const size_t elen = strlen(extension);
  const std::vector<dirguard> &paths = osm2go_platform::base_paths();
  const std::vector<dirguard>::const_iterator itEnd = paths.end();

  for(std::vector<dirguard>::const_iterator it = paths.begin(); it != itEnd; it++) {
    /* scan for projects */
    // only copy the fd, no need for the pathname as that is not used anyway
    dirguard dir(it->dirfd());

    int dfd = dir.dirfd();
    for(dirent *d = dir.next(); d != nullptr; d = dir.next()) {
      if(d->d_type == DT_DIR)
        continue;

      const size_t nlen = strlen(d->d_name);
      if(nlen <= elen || strcmp(d->d_name + nlen - elen, extension) != 0)
        continue;

      struct stat st;
      if(unlikely(fstatat(dfd, d->d_name, &st, 0) != 0))
        continue;

      if(unlikely(!S_ISREG(st.st_mode)))
        continue;

      fullname = it->path() + d->d_name;

      josm_elemstyle style;
      if(likely(style_parse(fullname, nullptr, style)))
        ret[style.name].swap(fullname);
    }
  }

  return ret;
}

style_t::style_t()
{
  icon.enable = false;
  icon.scale = 0.0;
  memset(&track, 0, sizeof(track));
  memset(&way, 0, sizeof(way));
  memset(&area, 0, sizeof(area));
  memset(&frisket, 0, sizeof(frisket));
  memset(&node, 0, sizeof(node));
  memset(&highlight, 0, sizeof(highlight));

  background.color = color_t::white();
}

static void unref_icon(const style_t::IconCache::value_type &pair)
{
  icon_t::instance().icon_free(pair.second);
}

style_t::~style_t()
{
  printf("freeing style\n");

  std::for_each(node_icons.begin(), node_icons.end(), unref_icon);
}

void style_change(appdata_t &appdata, const std::string &style_path)
{
  const std::string &new_style = style_basename(style_path);
  /* check if style has really been changed */
  if(settings_t::instance()->style == new_style)
    return;

  style_t *nstyle = style_load_fname(style_path);
  if (nstyle == nullptr) {
    error_dlg(trstring("Error loading style %1").arg(style_path));
    return;
  }

  settings_t::instance()->style = new_style;

  appdata.map->clear(map_t::MAP_LAYER_OBJECTS_ONLY);
  osm2go_platform::process_events();

  appdata.style.reset(nstyle);

  /* canvas background may have changed */
  appdata.map->set_bg_color_from_style();

  appdata.map->paint();
}
