/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
