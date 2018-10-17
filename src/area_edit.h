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

#include <vector>

#include <osm2go_platform.h>

struct appdata_t;
class gps_state_t;

struct area_edit_t {
  area_edit_t(gps_state_t *gps, pos_area &b, osm2go_platform::Widget *dlg);
  gps_state_t * const gps_state;
  osm2go_platform::Widget * const parent;   /* parent widget to be placed upon */
  pos_area &bounds;    /* positions to work on */
  std::vector<pos_area> other_bounds;   ///< bounds of all other valid projects

  bool run();
};
