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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAP_HL_H
#define MAP_HL_H

#include "appdata.h"
#include "canvas.h"
#include "map.h"

#include <glib.h>
#include <vector>

struct map_highlight_t {
  std::vector<canvas_item_t *> items;
};

void map_hl_cursor_draw(map_t *map, gint x, gint y, gboolean is_world, gint radius);
void map_hl_cursor_clear(map_t *map);

void map_hl_touchnode_draw(map_t *map, node_t *node);
void map_hl_touchnode_clear(map_t *map);
node_t *map_hl_touchnode_get_node(map_t *map);

void map_hl_remove(appdata_t *appdata);
gboolean map_hl_item_is_highlighted(map_t *map, map_item_t *item);

canvas_item_t *map_hl_circle_new(map_t *map, canvas_group_t group, map_item_t *map_item,
				 gint x, gint y, gint radius, canvas_color_t color);

canvas_item_t *map_hl_polyline_new(map_t *map, canvas_group_t group, map_item_t *map_item,
				   canvas_points_t *points, gint width, canvas_color_t color);

canvas_item_t *map_hl_polygon_new(map_t *map, canvas_group_t group, map_item_t *map_item,
				   canvas_points_t *points, canvas_color_t color);

void map_hl_segment_draw(map_t *map, gint width, gint x0, gint y0, gint x1, gint y1);

#endif // MAP_HL_H
