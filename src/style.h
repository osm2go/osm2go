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

#ifndef STYLE_H
#define STYLE_H

#include "color.h"
#include "icon.h"
#include "osm.h"

#include <string>
#include <vector>
#include <unordered_map>

struct appdata_t;
struct elemstyle_t;

struct style_t {
  explicit style_t();
  ~style_t();

  std::string name;

  struct {
    bool enable;
    float scale;  // how big the icon is drawn
    std::string path_prefix;
  } icon;

  struct {
    color_t color;
    color_t gps_color;
    float width;
  } track;

  struct {
    color_t color;
    float width;
    float zoom_max;
  } way;

  struct {
    bool has_border_color;
    color_t border_color;
    float border_width;
    color_t color;
    float zoom_max;
  } area;

  struct {
    color_t color;

    struct {
      bool present;
      float width;
      color_t color;
    } border;
  } frisket;

  struct {
    float radius, border_radius;
    color_t fill_color, color;
    bool show_untagged;
    float zoom_max;
  } node;

  struct {
    color_t color, node_color, touch_color, arrow_color;
    float width, arrow_limit;
  } highlight;

  struct {
    color_t color;
  } background;

  std::vector<elemstyle_t *> elemstyles;

  typedef std::unordered_map<item_id_t, icon_item *> IconCache;
  IconCache node_icons;

  void colorize_node(node_t *n);
  void colorize_way(way_t *w) const;
  void colorize_world(osm_t::ref osm);
};

style_t *style_load(const std::string &name);

#endif // STYLE_H
