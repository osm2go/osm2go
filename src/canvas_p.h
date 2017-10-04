/*
 * Copyright (C) 2017 Rolf Eike Beer <eike@sf-mail.de>.
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

#ifndef CANVAS_P_H
#define CANVAS_P_H

#include "canvas.h"

#include "pos.h"

class canvas_item_info_circle : public canvas_item_info_t {
public:
  canvas_item_info_circle(canvas_t *cv, canvas_group_t g, canvas_item_t *it,
                          const int cx, const int cy, const unsigned int radius);

  struct {
    int x, y;
  } center;
  const unsigned int r;
};

class canvas_item_info_poly : public canvas_item_info_t {
public:
  canvas_item_info_poly(canvas_t *cv, canvas_group_t g, canvas_item_t *it, bool poly,
                        unsigned int wd, canvas_points_t *cpoints);
  ~canvas_item_info_poly();

  struct {
    struct {
      int x,y;
    } top_left, bottom_right;
  } bbox;

  bool is_polygon;
  unsigned int width, num_points;
  lpos_t *points;
};

#endif
