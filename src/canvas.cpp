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
template<typename T>
void item_info_destroy(void *data) {
  T const *info = static_cast<T *>(data);
  const canvas_t::item_mapping_t::iterator it = info->canvas->item_mapping.find(info->item);
  assert(it != info->canvas->item_mapping.end());
  info->canvas->item_mapping.erase(it);
  delete info;
}

canvas_item_info_t::canvas_item_info_t(canvas_item_type_t t, canvas_t *cv, canvas_group_t g, canvas_item_t *it, void (*deleter)(void *))
  : canvas(cv)
  , type(t)
  , group(g)
  , item(it)
{
  canvas->item_info[group].push_back(this);
  canvas->item_mapping[it] = this;

  item->destroy_connect(deleter, this);
}

canvas_item_info_t::~canvas_item_info_t()
{
  std::vector<canvas_item_info_t *> &info_group = canvas->item_info[group];

  /* search for item in chain */
  const std::vector<canvas_item_info_t *>::iterator itEnd = info_group.end();
  std::vector<canvas_item_info_t *>::iterator it = std::find(info_group.begin(),
                                                             itEnd, this);
  assert(it != itEnd);

  info_group.erase(it);
}

canvas_item_info_circle::canvas_item_info_circle(canvas_t *cv, canvas_group_t g, canvas_item_t *it,
                                                 const int cx, const int cy, const unsigned int radius)
  : canvas_item_info_t(CANVAS_ITEM_CIRCLE, cv, g, it, item_info_destroy<canvas_item_info_circle>)
  , r(radius)
{
  center.x = cx;
  center.y = cy;
}

canvas_item_info_poly::canvas_item_info_poly(canvas_t* cv, canvas_group_t g, canvas_item_t* it,
                                             bool poly, unsigned int wd, const std::vector<lpos_t> &p)
  : canvas_item_info_t(CANVAS_ITEM_POLY, cv, g, it, item_info_destroy<canvas_item_info_poly>)
  , is_polygon(poly)
  , width(wd)
  , num_points(p.size())
  , points(new lpos_t[p.size()])
{
  // data() is a C++11 extension, but gcc has it since at least 4.2
  memcpy(points.get(), p.data(), p.size() * sizeof(points[0]));
}

struct item_info_find {
  const canvas_item_t * const citem;
  explicit item_info_find(const canvas_item_t *i) : citem(i) {}
  bool operator()(const canvas_item_info_t *item) {
    return item->item == citem;
  }
};

void canvas_t::item_info_push(canvas_item_t *item) {
  /* search for item in all chains */
  for(unsigned int group = 0; group < CANVAS_GROUPS; group++) {
    std::vector<canvas_item_info_t *> &info_group = item_info[group];
    const std::vector<canvas_item_info_t *>::iterator itEnd = info_group.end();
    std::vector<canvas_item_info_t *>::iterator it = std::find_if(info_group.begin(), itEnd,
                                                                  item_info_find(item));
    if(it != itEnd) {
      std::rotate(it, it + 1, itEnd);
      return;
    }
  }
  assert_unreachable();
}
