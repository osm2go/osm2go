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

#ifndef MAP_HL_H
#define MAP_HL_H

#include "canvas.h"
#include "color.h"
#include "pos.h"

#include <vector>

struct map_item_t;
class map_t;

struct map_highlight_t {
  std::vector<canvas_item_t *> items;

  inline bool isEmpty() const noexcept
  { return items.empty(); }

  bool isHighlighted(const map_item_t &item) const;

  void clear();

  canvas_item_t *circle_new(map_t *map, canvas_group_t group, map_item_t *map_item,
                            int x, int y, unsigned int radius, color_t color);

  canvas_item_t *polyline_new(map_t *map, canvas_group_t group, map_item_t *map_item,
                              const std::vector<lpos_t> &points, unsigned int width,
                              color_t color);

  canvas_item_t *polygon_new(map_t *map, canvas_group_t group, map_item_t *map_item,
                             const std::vector<lpos_t> &points, color_t color);

};

#endif // MAP_HL_H
