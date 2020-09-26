/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "canvas.h"
#include "color.h"
#include "pos.h"

#include <vector>

struct map_item_t;
class map_t;
class node_t;
class way_t;

struct map_highlight_t {
  std::vector<canvas_item_t *> items;

  inline bool isEmpty() const noexcept
  { return items.empty(); }

  /**
   * @brief check if a highligh item exists that covers this object
   */
  bool isHighlighted(const map_item_t &item) const;

  void clear();

  void circle_new(map_t *map, canvas_group_t group, node_t *node, float radius, color_t color);

  void polyline_new(map_t *map, canvas_group_t group, way_t *way,
                    const std::vector<lpos_t> &points, color_t color);

  void polygon_new(map_t *map, canvas_group_t group, way_t *way,
                   const std::vector<lpos_t> &points, color_t color);

};
