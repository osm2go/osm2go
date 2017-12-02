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
#include "osm.h"

#include <vector>

struct map_item_t;
class map_t;

struct map_highlight_t {
  std::vector<canvas_item_t *> items;
};

/**
 * @brief draw highlight cursor on screen coordinates
 */
void map_hl_cursor_draw(map_t *map, int x, int y, unsigned int radius);
/**
 * @brief draw highlight cursor on map coordinates
 */
void map_hl_cursor_draw(map_t *map, lpos_t pos, unsigned int radius);
void map_hl_cursor_clear(map_t *map);

void map_hl_touchnode_draw(map_t *map, node_t *node);
void map_hl_touchnode_clear(map_t *map);
node_t *map_hl_touchnode_get_node(map_t *map);

void map_hl_remove(map_t *map);
bool map_hl_item_is_highlighted(const map_t *map, const map_item_t &item);

canvas_item_t *map_hl_circle_new(map_t *map, canvas_group_t group, map_item_t *map_item,
                                 int x, int y, unsigned int radius, color_t color);

canvas_item_t *map_hl_polyline_new(map_t *map, canvas_group_t group, map_item_t *map_item,
                                   const std::vector<lpos_t> &points, unsigned int width,
                                   color_t color);

canvas_item_t *map_hl_polygon_new(map_t *map, canvas_group_t group, map_item_t *map_item,
                                  const std::vector<lpos_t> &points, color_t color);

void map_hl_segment_draw(map_t *map, unsigned int width, const std::vector<lpos_t> &points);

#endif // MAP_HL_H
