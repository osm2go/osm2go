/*
 * Copyright (C) 2009 Till Harbaum <till@harbaum.org>.
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

/**
 * @file canvas.cpp
 */

#include "canvas.h"
#include "canvas_p.h"

#include "map.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

#include "osm2go_annotations.h"
#include "osm2go_stl.h"

canvas_t::canvas_t(osm2go_platform::Widget *w)
  : widget(w)
{
}

/* remove item_info from chain as its visual representation */
/* has been destroyed */
template<typename T> class item_info_destroyer : public canvas_item_destroyer {
public:
  T * const info;
  canvas_t * const canvas;
  explicit inline item_info_destroyer(T *i, canvas_t *cv)
    : canvas_item_destroyer(), info(i), canvas(cv) {}

  virtual void run(canvas_item_t *item) override
  {
    const canvas_t::item_mapping_t::iterator it = canvas->item_mapping.find(item);
    assert(it != canvas->item_mapping.end());
    canvas->item_mapping.erase(it);
    delete info;
  }
};

canvas_item_info_t::canvas_item_info_t(canvas_item_type_t t, canvas_t *cv, canvas_item_t *it, canvas_item_destroyer *d)
  : type(t)
{
  cv->item_mapping[it] = this;

  it->destroy_connect(d);
}

canvas_item_info_circle::canvas_item_info_circle(canvas_t *cv, canvas_item_t *it,
                                                 const int cx, const int cy, const unsigned int radius)
  : canvas_item_info_t(CANVAS_ITEM_CIRCLE, cv, it, new item_info_destroyer<canvas_item_info_circle>(this, cv))
  , r(radius)
{
  center.x = cx;
  center.y = cy;
}

canvas_item_info_poly::canvas_item_info_poly(canvas_t* cv, canvas_item_t* it,
                                             bool poly, unsigned int wd, const std::vector<lpos_t> &p)
  : canvas_item_info_t(CANVAS_ITEM_POLY, cv, it, new item_info_destroyer<canvas_item_info_poly>(this, cv))
  , is_polygon(poly)
  , width(wd)
  , num_points(p.size())
  , points(new lpos_t[p.size()])
{
  // data() is a C++11 extension, but gcc has it since at least 4.2
  memcpy(points.get(), p.data(), p.size() * sizeof(points[0]));
}

void map_item_destroyer::run(canvas_item_t*)
{
  delete mi;
}
