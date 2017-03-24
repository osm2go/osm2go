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

#ifndef MAP_EDIT_H
#define MAP_EDIT_H

#include "map.h"
#include "osm.h"

#ifdef __cplusplus
extern "C" {
#endif

void map_edit_way_add_begin(map_t *map, way_t *way_sel);
void map_edit_way_add_segment(map_t *map, gint x, gint y);
void map_edit_way_add_cancel(map_t *map);
void map_edit_way_add_ok(map_t *map);

void map_edit_way_node_add_highlight(map_t *map, map_item_t *item,
				     gint x, gint y);
void map_edit_way_node_add(map_t *map, gint x, gint y);

void map_edit_way_cut_highlight(map_t *map, map_item_t *item, gint x, gint y);
void map_edit_way_cut(map_t *map, gint x, gint y);

void map_edit_node_move(map_t *map, map_item_t *map_item, gint ex, gint ey);

void map_edit_way_reverse(map_t *map);

#ifdef __cplusplus
}
#endif

#endif // MAP_EDIT_H
