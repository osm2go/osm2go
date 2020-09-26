/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
