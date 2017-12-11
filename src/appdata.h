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

#ifndef APPDATA_H
#define APPDATA_H

#include <array>
#include <gtk/gtk.h>

#include "uicontrol.h"

struct canvas_item_t;
class gps_state_t;
class icon_t;
class iconbar_t;
struct map_state_t;
class map_t;
struct osm_t;
struct presets_items;
struct project_t;
class settings_t;
class statusbar_t;
struct style_t;
struct track_t;

struct appdata_t {
  appdata_t(map_state_t &mstate, icon_t &ic);
  ~appdata_t();

  MainUi * const uicontrol;

  GtkWidget *window;

  statusbar_t * const statusbar;
  project_t *project;
  iconbar_t *iconbar;
  presets_items *presets;

  std::array<GtkWidget *, MainUi::MENU_ITEMS_COUNT> menuitems;

  struct {
    track_t *track;
    canvas_item_t *gps_item; // the purple circle
    int warn_cnt;
  } track;

  map_state_t &map_state;
  map_t *map;
  osm_t *osm;
  settings_t * const settings;
  icon_t &icons;
  style_t *style;
  gps_state_t * const gps_state;

  void track_clear();
  void main_ui_enable();
};

#endif // APPDATA_H
