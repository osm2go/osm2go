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

#include <memory>

#include <osm2go_platform.h>
#include <osm2go_stl.h>

class gps_state_t;
class icon_t;
class iconbar_t;
class MainUi;
struct map_state_t;
class map_t;
class osm_t;
class presets_items;
struct project_t;
struct style_t;
struct track_t;

struct appdata_t {
  appdata_t(map_state_t &mstate);
  ~appdata_t();

  static osm2go_platform::Widget *window;

  const std::unique_ptr<MainUi> uicontrol;

  std::unique_ptr<project_t> project;
  std::unique_ptr<iconbar_t> iconbar;
  std::unique_ptr<presets_items> presets;

  struct {
    std::unique_ptr<track_t> track;
    int warn_cnt;
  } track;

  map_state_t &map_state;
  map_t *map;
  icon_t &icons;
  std::unique_ptr<style_t> style;
  const std::unique_ptr<gps_state_t> gps_state;

  void track_clear();
  void main_ui_enable();

  /**
   * @brief update the title of the application window
   *
   * The project name of the currently active project will be used.
   */
  void set_title();
};

#endif // APPDATA_H
