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

#include "map_hl.h"

#include "canvas.h"
#include "map.h"
#include "style.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include <osm2go_cpp.h>

/* create a new item for the cursor */
void map_hl_cursor_draw(map_t *map, int x, int y, unsigned int radius) {
  map_hl_cursor_draw(map, map->canvas->window2world(x, y), radius);
}

void map_hl_cursor_draw(map_t *map, lpos_t pos, unsigned int radius) {
  delete map->cursor;

  map->cursor = map->canvas->circle_new(CANVAS_GROUP_DRAW, pos.x, pos.y,
		  radius, 0, map->style->highlight.node_color, NO_COLOR);
}

/* special highlight for segments. use when cutting ways */
void map_hl_segment_draw(map_t *map, unsigned int width, const std::vector<lpos_t> &points) {
  map->cursor = map->canvas->polyline_new(CANVAS_GROUP_DRAW, points, width,
                                          map->style->highlight.node_color);
}

void map_hl_cursor_clear(map_t *map) {
  if(map->cursor) {
    delete map->cursor;
    map->cursor = O2G_NULLPTR;
  }
}

/* create a new item used for touched node */
void map_hl_touchnode_draw(map_t *map, node_t *node) {
  delete map->touchnode;

  map->touchnode = map->canvas->circle_new(CANVAS_GROUP_DRAW,
		      node->lpos.x, node->lpos.y,
		      2*map->style->node.radius, 0,
		      map->style->highlight.touch_color, NO_COLOR);

  map->touchnode->set_user_data(node);
}

node_t *map_hl_touchnode_get_node(map_t *map) {
  if(!map->touchnode) return O2G_NULLPTR;
  return static_cast<node_t *>(map->touchnode->get_user_data());
}

void map_hl_touchnode_clear(map_t *map) {
  if(map->touchnode) {
    delete map->touchnode;
    map->touchnode = O2G_NULLPTR;
  }
}

void map_hl_remove(map_t *map) {
  if(!map->highlight) return;

  printf("removing highlight\n");

  map_highlight_t *hl = map->highlight;
  map->highlight = O2G_NULLPTR;
  std::for_each(hl->items.begin(), hl->items.end(), std::default_delete<canvas_item_t>());

  delete hl;
}

struct find_highlighted {
  const map_item_t &item;
  explicit find_highlighted(const map_item_t &t) : item(t) {}
  bool operator()(canvas_item_t *c);
};

bool find_highlighted::operator()(canvas_item_t* c)
{
  map_item_t *hl_item = static_cast<map_item_t *>(c->get_user_data());

  return hl_item && hl_item->object == item.object;
}

bool map_hl_item_is_highlighted(const map_t *map, const map_item_t &item) {
  const map_highlight_t *hl = map->highlight;
  if(!hl)
    return false;
  const std::vector<canvas_item_t *>::const_iterator itEnd = hl->items.end();
  return std::find_if(hl->items.begin(), itEnd, find_highlighted(item)) != itEnd;
}

static void hl_add(map_t *map, canvas_item_t *item)
{
  /* attach highlight object */
  if(!map->highlight)
    map->highlight = new map_highlight_t();
  map->highlight->items.push_back(item);
}

canvas_item_t *map_hl_circle_new(map_t *map, canvas_group_t group,
                                 map_item_t *map_item, int x, int y,
                                 unsigned int radius, canvas_color_t color) {
  map_item->item = map->canvas->circle_new(group, x, y, radius, 0, color, NO_COLOR);
  hl_add(map, map_item->item);

  map_item->item->set_user_data(map_item);

  map_item->item->destroy_connect(map_item_t::free, map_item);

  return map_item->item;
}

canvas_item_t *map_hl_polygon_new(map_t *map, canvas_group_t group, map_item_t *map_item,
                                  const std::vector<lpos_t> &points, canvas_color_t color) {
  map_item->item = map->canvas->polygon_new(group, points, 0, 0, color);
  hl_add(map, map_item->item);

  map_item->item->set_user_data(map_item);

  map_item->item->destroy_connect(map_item_t::free, map_item);

  return map_item->item;
}

canvas_item_t *map_hl_polyline_new(map_t *map, canvas_group_t group, map_item_t *map_item,
                                   const std::vector<lpos_t> &points, unsigned int width,
                                   canvas_color_t color) {
  map_item->item = map->canvas->polyline_new(group, points, width, color);
  hl_add(map, map_item->item);

  map_item->item->set_user_data(map_item);

  map_item->item->destroy_connect(map_item_t::free, map_item);

  return map_item->item;
}
