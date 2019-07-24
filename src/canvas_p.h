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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "canvas.h"
#include "pos.h"

#include <memory>
#include <vector>

#include <osm2go_cpp.h>
#include <osm2go_stl.h>

/* The fuzziness allows to specify how far besides an object a user may */
/* click so it's still considered a click onto that object. This can */
/* be given in meters _and_ in pixels. Both values will be added to */
/* the total fuzziness. */
#define EXTRA_FUZZINESS_METER  0
#define EXTRA_FUZZINESS_PIXEL  8

enum canvas_item_type_t { CANVAS_ITEM_CIRCLE, CANVAS_ITEM_POLY };

class canvas_item_info_t {
protected:
  canvas_item_info_t(canvas_item_type_t t, canvas_t *cv, canvas_item_t *it, canvas_item_destroyer *d);
public:

  const canvas_item_type_t type;
};

class canvas_item_info_circle : public canvas_item_info_t {
public:
  canvas_item_info_circle(canvas_t *cv, canvas_item_t *it, lpos_t c, const unsigned int r);

  const lpos_t center;
  const unsigned int radius;
};

class canvas_item_info_poly : public canvas_item_info_t {
public:
  canvas_item_info_poly(canvas_t *cv, canvas_item_t *it, bool poly,
                        unsigned int wd, const std::vector<lpos_t> &p);

  bool is_polygon;
  unsigned int width;
  // stored as single items to save one size_t of memory per object
  const unsigned int num_points;
  std::unique_ptr<lpos_t[]> points;

  /**
   * @brief get the polygon/polyway segment a certain coordinate is over
   */
  int get_segment(int x, int y, unsigned int fuzziness = 0) const;
};
