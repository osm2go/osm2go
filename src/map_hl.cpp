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

#include "map_hl.h"

#include "appdata.h"
#include "style.h"

#include <algorithm>

/* create a new item for the cursor */
void map_hl_cursor_draw(map_t *map, gint x, gint y, gboolean is_world,
			gint radius) {
  if(map->cursor)
    canvas_item_destroy(map->cursor);

  gint wx, wy;
  if(!is_world) canvas_window2world(map->canvas, x, y, &wx, &wy);
  else { wx = x; wy = y; }

  map->cursor = canvas_circle_new(map->canvas, CANVAS_GROUP_DRAW, wx, wy,
		  radius, 0, map->style->highlight.node_color, NO_COLOR);
}

/* special highlight for segments. use when cutting ways */
void map_hl_segment_draw(map_t *map, gint width,
			 gint x0, gint y0, gint x1, gint y1) {
  canvas_points_t *points = canvas_points_new(2);

  points->coords[0] = x0; points->coords[1] = y0;
  points->coords[2] = x1; points->coords[3] = y1;

  map->cursor = canvas_polyline_new(map->canvas, CANVAS_GROUP_DRAW,
		    points, width, map->style->highlight.node_color);
  canvas_points_free(points);
}

void map_hl_cursor_clear(map_t *map) {
  if(map->cursor) {
    canvas_item_destroy(map->cursor);
    map->cursor = NULL;
  }
}

/* create a new item used for touched node */
void map_hl_touchnode_draw(map_t *map, node_t *node) {
  if(map->touchnode)
    canvas_item_destroy(map->touchnode);

  map->touchnode =
    canvas_circle_new(map->canvas, CANVAS_GROUP_DRAW,
		      node->lpos.x, node->lpos.y,
		      2*map->style->node.radius, 0,
		      map->style->highlight.touch_color, NO_COLOR);

  canvas_item_set_user_data(map->touchnode, node);
}

node_t *map_hl_touchnode_get_node(map_t *map) {
  if(!map->touchnode) return NULL;
  return (node_t*)canvas_item_get_user_data(map->touchnode);
}

void map_hl_touchnode_clear(map_t *map) {
  if(map->touchnode) {
    canvas_item_destroy(map->touchnode);
    map->touchnode = NULL;
  }
}

/* called whenever a highlight item is to be destroyed */
gint map_hl_item_destroy_event(G_GNUC_UNUSED GtkWidget *widget, gpointer data) {
  map_item_t *map_item = (map_item_t*)data;

  //  printf("destroying highlight map_item @ %p\n", map_item);
  g_free(map_item);
  return FALSE;
}

void map_hl_remove(appdata_t *appdata) {
  map_t *map = appdata->map;

  if(!map->highlight) return;

  printf("removing highlight\n");

  map_highlight_t *hl = map->highlight;
  map->highlight = NULL;
  std::for_each(hl->items.begin(), hl->items.end(), canvas_item_destroy);

  delete hl;
}

struct find_highlighted {
  const map_item_t * const item;
  find_highlighted(const map_item_t *t) : item(t) {}
  bool operator()(canvas_item_t *c);
};

bool find_highlighted::operator()(canvas_item_t* c)
{
  map_item_t *hl_item = static_cast<map_item_t *>(canvas_item_get_user_data(c));

  return hl_item && hl_item->object == item->object;
}


gboolean map_hl_item_is_highlighted(map_t *map, map_item_t *item) {
  map_highlight_t *hl = map->highlight;
  if(!hl)
    return FALSE;
  return std::find_if(hl->items.begin(), hl->items.end(), find_highlighted(item)) !=
         hl->items.end();
}

static void hl_add(map_t *map, canvas_item_t *item)
{
  /* attach highlight object */
  if(!map->highlight)
    map->highlight = new map_highlight_t();
  map->highlight->items.push_back(item);
}

canvas_item_t *map_hl_circle_new(map_t *map, canvas_group_t group,
		 map_item_t *map_item,
		 gint x, gint y, gint radius, canvas_color_t color) {
  map_item->item =
    canvas_circle_new(map->canvas, group, x, y, radius, 0, color, NO_COLOR);
  hl_add(map, map_item->item);

  canvas_item_set_user_data(map_item->item, map_item);

  canvas_item_destroy_connect(map_item->item,
	     G_CALLBACK(map_hl_item_destroy_event), map_item);

  return map_item->item;
}

canvas_item_t *map_hl_polygon_new(map_t *map, canvas_group_t group, map_item_t *map_item,
				  canvas_points_t *points, canvas_color_t color) {
  map_item->item =
    canvas_polygon_new(map->canvas, group, points, 0, 0, color);
  hl_add(map, map_item->item);

  canvas_item_set_user_data(map_item->item, map_item);

  canvas_item_destroy_connect(map_item->item,
	     G_CALLBACK(map_hl_item_destroy_event), map_item);

  return map_item->item;
}

canvas_item_t *map_hl_polyline_new(map_t *map, canvas_group_t group, map_item_t *map_item,
				   canvas_points_t *points, gint width, canvas_color_t color) {
  map_item->item =
    canvas_polyline_new(map->canvas, group, points, width, color);
  hl_add(map, map_item->item);

  canvas_item_set_user_data(map_item->item, map_item);

  canvas_item_destroy_connect(map_item->item,
	     G_CALLBACK(map_hl_item_destroy_event), map_item);

  return map_item->item;
}
