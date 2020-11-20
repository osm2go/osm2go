/*
 * SPDX-FileCopyrightText: 2009 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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

std::optional<unsigned int> canvas_t::get_item_segment(const canvas_item_t *item, lpos_t pos) const
{
  const item_mapping_t::const_iterator it = item_mapping.find(item);
  assert(it != item_mapping.end());

  const canvas_item_info_poly *poly = static_cast<const canvas_item_info_poly *>(it->second);

  const float fuzziness = poly->width > 0 ? 0 :
                          EXTRA_FUZZINESS_METER + EXTRA_FUZZINESS_PIXEL / static_cast<float>(get_zoom());

  return poly->get_segment(pos.x, pos.y, fuzziness);
}

namespace {

/* remove item_info from chain as its visual representation */
/* has been destroyed */
template<typename T> class item_info_destroyer : public canvas_item_destroyer {
  T * const info;
  canvas_t * const canvas;
public:
  explicit inline item_info_destroyer(T *i, canvas_t *cv)
    : canvas_item_destroyer(), info(i), canvas(cv) {}

  void run(canvas_item_t *item) override
  {
    const canvas_t::item_mapping_t::iterator it = canvas->item_mapping.find(item);
    assert(it != canvas->item_mapping.end());
    canvas->item_mapping.erase(it);
    delete info;
  }
};

} // namespace

canvas_item_info_t::canvas_item_info_t(canvas_item_type_t t, canvas_t *cv, canvas_item_t *it, canvas_item_destroyer *d)
  : type(t)
{
  cv->item_mapping[it] = this;

  it->destroy_connect(d);
}

canvas_item_info_circle::canvas_item_info_circle(canvas_t *cv, canvas_item_t *it, lpos_t c, const unsigned int r)
  : canvas_item_info_t(CANVAS_ITEM_CIRCLE, cv, it, new item_info_destroyer<canvas_item_info_circle>(this, cv))
  , center(c)
  , radius(r)
{
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

std::optional<unsigned int> canvas_item_info_poly::get_segment(int x, int y, float fuzziness) const
{
  unsigned int retval;
  bool found = false;
  float mindist = static_cast<float>(width) / 2 + fuzziness;
  for(unsigned int i = 0; i < num_points - 1; i++) {
    const lpos_t pos = points[i];
    const lpos_t posnext = points[i + 1];

    const int dxi = posnext.x - pos.x;
    const int dyi = posnext.y - pos.y;
    const float dx = dxi;
    const float dy = dyi;
    float len = dy * dy + dx * dx;
    float m = ((x - pos.x) * dx + (y - pos.y) * dy) / len;

    /* this is a possible candidate */
    if((m >= 0.0f) && (m <= 1.0f)) {

      float n;
      if(abs(dxi) > abs(dyi))
        n = fabsf(sqrtf(len) * (pos.y - y + m * dy) / dx);
      else
        n = fabsf(sqrtf(len) * -(pos.x - x + m * dx) / dy);

      /* check if this is actually on the line and closer than anything */
      /* we found so far */
      if(n < mindist) {
        retval = i;
        found = true;
        mindist = n;
      }
    }
  }

  /* the last and first point are identical for polygons in osm2go. */
  /* goocanvas doesn't need that, but that's how OSM works and it saves */
  /* us from having to check the last->first connection for polygons */
  /* seperately */

  if (found)
    return retval;
  else
    return std::optional<unsigned int>();
}

void map_item_destroyer::run(canvas_item_t *)
{
  delete mi;
}
