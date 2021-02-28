/*
 * SPDX-FileCopyrightText: 2017 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "canvas.h"
#include "pos.h"

#include <memory>
#include <optional>
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
  inline ~canvas_item_info_t() {}
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
                        float wd, const std::vector<lpos_t> &p);

  bool is_polygon;
  const float width;
  // stored as single items to save one size_t of memory per object
  const unsigned int num_points;
  const std::unique_ptr<lpos_t[]> points;

  /**
   * @brief get the polygon/polyway segment a certain coordinate is over
   */
  std::optional<unsigned int> get_segment(int x, int y, float fuzziness) const;
};
