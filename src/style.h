/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

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
  mutable IconCache node_icons;

  void colorize(node_t *n) const;
  void colorize(way_t *w) const;

  static style_t *load(const std::string &name);
};
