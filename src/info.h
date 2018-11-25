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

#include "osm.h"

#include <osm2go_platform.h>

#include <string>
#include <vector>

class map_t;
class presets_items;

class tag_context_t {
protected:
  tag_context_t(const object_t &o, const osm_t::TagMap &t, osm2go_platform::Widget *dlg);
public:

  osm2go_platform::DialogGuard dialog;
  object_t object;
  const osm_t::TagMap &tags;

  void info_tags_replace(const osm_t::TagMap &ntags);
};

bool info_dialog(osm2go_platform::Widget *parent, map_t *map, osm_t::ref osm, presets_items *presets, object_t &object);
