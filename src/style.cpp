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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "style.h"
#include "style_p.h"

#include "appdata.h"
#include "josm_elemstyles.h"
#include "map.h"
#include "misc.h"
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
#include "osm2go_stl.h"

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

static float parse_scale_max(xmlNodePtr cur_node) {
  float scale_max = xml_get_prop_float(cur_node, "scale-max");
  if (!std::isnan(scale_max))
    return scaledn_to_zoom(scale_max);
  else
    return 0.0f;
}

static void parse_style_node(xmlNode *a_node, xmlChar **fname, style_t &style) {
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

  style.frisket.mult           = 3.0;
  style.frisket.color          = 0xffffffff;
  style.frisket.border.present = true;
  style.frisket.border.width   = 6.0;
  style.frisket.border.color   = 0x00000099;

  style.icon.enable            = false;
  style.icon.scale             = 1.0;    // icon size (multiplier)

  for (xmlNode *cur_node = a_node->children; cur_node != O2G_NULLPTR; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      const char *nodename = reinterpret_cast<const char *>(cur_node->name);
      if(strcmp(nodename, "elemstyles") == 0) {
        *fname = xmlGetProp(cur_node, BAD_CAST "filename");

      } else if(strcmp(nodename, "node") == 0) {
        parse_color(cur_node, "color", style.node.color);
        parse_color(cur_node, "fill-color", style.node.fill_color);
        style.node.radius = xml_get_prop_float(cur_node, "radius");
        style.node.border_radius = xml_get_prop_float(cur_node, "border-radius");
        style.node.zoom_max = parse_scale_max(cur_node);

        style.node.show_untagged = xml_get_prop_is(cur_node, "show-untagged", "true");

      } else if(strcmp(nodename, "icon") == 0) {
        style.icon.scale = xml_get_prop_float(cur_node, "scale");
        xmlChar *prefix = xmlGetProp(cur_node, BAD_CAST "path-prefix");
        if(prefix) {
          xmlFree(BAD_CAST style.icon.path_prefix);
          style.icon.path_prefix = reinterpret_cast<char *>(prefix);
        }
        style.icon.enable = xml_get_prop_is(cur_node, "enable", "true");

      } else if(strcmp(nodename, "way") == 0) {
        parse_color(cur_node, "color", style.way.color);
        style.way.width = xml_get_prop_float(cur_node, "width");
        style.way.zoom_max = parse_scale_max(cur_node);

      } else if(strcmp(nodename, "frisket") == 0) {
        style.frisket.mult = xml_get_prop_float(cur_node, "mult");
        parse_color(cur_node, "color", style.frisket.color);
        style.frisket.border.present = false;

        for(xmlNode *sub_node = cur_node->children; sub_node != O2G_NULLPTR; sub_node=sub_node->next) {
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

  assert(style.icon.path_prefix || !style.icon.enable);
}

/**
 * @brief parse a style definition file
 * @param fullname absolute path of the file to read
 * @param fname location to store name of the object style XML file or O2G_NULLPTR
 * @param name_only only parse the style name, leave all other fields empty
 * @param style the object to fill
 * @return if parsing the style succeeded
 *
 * fname may be nullptr when name_only is true
 */
static bool style_parse(const std::string &fullname, xmlChar **fname,
                        bool name_only, style_t &style) {
  xmlDoc *doc = xmlReadFile(fullname.c_str(), O2G_NULLPTR, XML_PARSE_NONET);
  bool ret = false;

  /* parse the file and get the DOM */
  if(unlikely(doc == O2G_NULLPTR)) {
    xmlErrorPtr errP = xmlGetLastError();
    printf("parsing %s failed: %s\n", fullname.c_str(), errP->message);
  } else {
    /* Get the root element node */
    xmlNode *cur_node = O2G_NULLPTR;

    for(cur_node = xmlDocGetRootElement(doc);
        cur_node; cur_node = cur_node->next) {
      if (cur_node->type == XML_ELEMENT_NODE) {
        if(likely(strcmp(reinterpret_cast<const char *>(cur_node->name), "style") == 0)) {
          if(likely(!ret)) {
            style.name = reinterpret_cast<char *>(xmlGetProp(cur_node, BAD_CAST "name"));
            ret = true;
            if(name_only)
              break;
            parse_style_node(cur_node, fname, style);
          }
        } else
          printf("  found unhandled %s\n", cur_node->name);
      }
    }

    xmlFreeDoc(doc);
  }
  return ret;
}

style_t *style_load_fname(icon_t &icons, const std::string &filename) {
  xmlChar *fname = O2G_NULLPTR;
  std::unique_ptr<style_t> style(new style_t(icons));

  if(likely(style_parse(filename, &fname, false, *style.get()))) {
    printf("  elemstyle filename: %s\n", fname);
    style->elemstyles = josm_elemstyles_load(reinterpret_cast<char *>(fname));
    xmlFree(fname);
    return style.release();
  }

  return O2G_NULLPTR;
}

style_t *style_load(const std::string &name, icon_t &icons) {
  printf("Trying to load style %s\n", name.c_str());

  std::string fullname = find_file(name + ".style");

  if (unlikely(fullname.empty())) {
    printf("style %s not found, trying %s instead\n", name.c_str(), DEFAULT_STYLE);
    fullname = find_file(DEFAULT_STYLE ".style");
    if (unlikely(fullname.empty())) {
      printf("  style not found, failed to find fallback style too\n");
      return O2G_NULLPTR;
    }
  }

  printf("  style filename: %s\n", fullname.c_str());

  return style_load_fname(icons, fullname);
}

std::string style_basename(const std::string &name) {
  std::string::size_type pos = name.rfind("/");

  /* and cut off extension */
  std::string::size_type extpos = name.rfind(".");
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

  for(unsigned int i = 0; i < base_paths.size(); i++) {
    /* scan for projects */

    dirguard dir(base_paths[i].fd);

    if(dir.valid()) {
      dirent *d;
      int dfd = dir.dirfd();
      while ((d = dir.next()) != O2G_NULLPTR) {
        if(d->d_type == DT_DIR)
          continue;

        const size_t nlen = strlen(d->d_name);
        if(nlen <= elen || strcmp(d->d_name + nlen - elen, extension) != 0)
          continue;

        struct stat st;
        fstatat(dfd, d->d_name, &st, 0);

        if(unlikely(!S_ISREG(st.st_mode)))
          continue;

        fullname = base_paths[i].pathname + d->d_name;

        icon_t dummyicons;
        style_t style(dummyicons);
        if(style_parse(fullname, O2G_NULLPTR, true, style))
          ret[style.name].swap(fullname);
      }
    }
  }

  return ret;
}

style_t::style_t(icon_t &ic)
  : icons(ic)
  , name(O2G_NULLPTR)
{
  memset(&icon, 0, sizeof(icon));
  memset(&track, 0, sizeof(track));
  memset(&way, 0, sizeof(way));
  memset(&area, 0, sizeof(area));
  memset(&frisket, 0, sizeof(frisket));
  memset(&node, 0, sizeof(node));
  memset(&highlight, 0, sizeof(highlight));

  background.color = 0xffffffff; // white
}

struct unref_icon {
  icon_t &icons;
  explicit unref_icon(icon_t &i) : icons(i) {}
  void operator()(const style_t::IconCache::value_type &pair) {
    icons.icon_free(pair.second);
  }
};

style_t::~style_t()
{
  printf("freeing style\n");

  josm_elemstyles_free(elemstyles);

  std::for_each(node_icons.begin(), node_icons.end(), unref_icon(icons));

  xmlFree(BAD_CAST name);
  xmlFree(BAD_CAST icon.path_prefix);
}

void style_t::colorize_node(node_t *n)
{
  josm_elemstyles_colorize_node(this, n);
}

void style_t::colorize_way(way_t *w) const
{
  josm_elemstyles_colorize_way(this, w);
}

void style_t::colorize_world(osm_t *osm)
{
  josm_elemstyles_colorize_world(this, osm);
}

//vim:et:ts=8:sw=2:sts=2:ai
