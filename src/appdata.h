/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>

#include <osm2go_platform.h>
#include <osm2go_stl.h>

class gps_state_t;
class icon_t;
class iconbar_t;
class MainUi;
class map_t;
class osm_t;
class presets_items;
struct project_t;
class style_t;
struct track_t;

struct appdata_t {
  appdata_t();
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

  map_t *map;
  icon_t &icons;
  std::unique_ptr<style_t> style;
  const std::unique_ptr<gps_state_t> gps_state;

  void track_clear();
  void track_clear_current();
  void main_ui_enable();

  /**
   * @brief update the title of the application window
   *
   * The project name of the currently active project will be used.
   */
  void set_title();
};
