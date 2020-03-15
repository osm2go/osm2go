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

#pragma once

#include "pos.h"

#include <osm2go_platform.h>

#include <map>
#include <string>
#include <vector>

struct wms_llbbox_t {
  explicit wms_llbbox_t()
    : bounds(pos_t(NAN, NAN), pos_t(NAN, NAN)), valid(false) {}
  pos_area bounds;
  bool valid;
};

struct wms_layer_t {
  explicit wms_layer_t(const std::string &t = std::string(),
                       const std::string &n = std::string(),
                       const std::string &s = std::string(),
                       bool epsg = false,
                       const wms_llbbox_t &x = wms_llbbox_t())
    : title(t), name(n), srs(s), epsg4326(epsg), llbbox(x) {}

  typedef std::vector<wms_layer_t> list;

  std::string title;
  std::string name;
  std::string srs;
  bool epsg4326;
  wms_llbbox_t llbbox;

  list children;

  bool is_usable() const noexcept {
    return !name.empty() && epsg4326 && llbbox.valid;
  }
  static const char *EPSG4326() {
    return "EPSG:4326";
  }
};

bool wms_llbbox_fits(const pos_area &bounds, const wms_llbbox_t &llbbox);
std::string wms_layer_dialog(osm2go_platform::Widget *parent, const pos_area &bounds, const wms_layer_t::list &layers);
std::string wms_server_dialog(osm2go_platform::Widget *parent, const std::string &wms_server);
