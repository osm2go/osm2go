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

#ifndef STYLE_H
#define STYLE_H

#include "icon.h"
#include "josm_elemstyles.h"

#include <vector>

struct appdata_t;
class node_t;

struct style_t {
  style_t();
  ~style_t();

  icon_t **iconP;  // pointer to global list of icons
  char *name;

  struct {
    gboolean enable;
    float scale;  // how big the icon is drawn
    char *path_prefix;
  } icon;

  struct {
    elemstyle_color_t color;
    elemstyle_color_t gps_color;
    float width;
  } track;

  struct {
    elemstyle_color_t color;
    float width;
    float zoom_max;
  } way;

  struct {
    gboolean has_border_color;
    elemstyle_color_t border_color;
    float border_width;
    elemstyle_color_t color;
    float zoom_max;
  } area;

  struct {
    float mult;
    elemstyle_color_t color;

    struct {
      gboolean present;
      float width;
      elemstyle_color_t color;
    } border;
  } frisket;

  struct {
    float radius, border_radius;
    elemstyle_color_t fill_color, color;
    gboolean show_untagged;
    float zoom_max;
  } node;

  struct {
    elemstyle_color_t color, node_color, touch_color, arrow_color;
    float width, arrow_limit;
  } highlight;

  struct {
    elemstyle_color_t color;
  } background;

  std::vector<elemstyle_t *> elemstyles;

  std::map<item_id_t, GdkPixbuf *> node_icons;
};

style_t *style_load(const std::string &name, icon_t **iconP);

#ifndef FREMANTLE
void style_select(GtkWidget *parent, appdata_t *appdata);
#else
GtkWidget *style_select_widget(appdata_t *appdata);
void style_change(appdata_t *appdata, const char *name);
#endif

#endif // STYLE_H
