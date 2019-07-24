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

#include "map.h"
#include "osm.h"
#include "style.h"

#include <algorithm>
#include <cstdio>

#include <osm2go_cpp.h>

void map_highlight_t::clear()
{
  printf("removing highlight\n");

  std::for_each(items.begin(), items.end(), std::default_delete<canvas_item_t>());
  items.clear();
}

/* create a new item for the cursor */
void map_t::hl_cursor_draw(lpos_t pos, unsigned int radius) {
  delete cursor;

  cursor = canvas->circle_new(CANVAS_GROUP_DRAW, pos, radius,
                              0, style->highlight.node_color);
}

void map_t::hl_cursor_clear()
{
  delete cursor;
  cursor = nullptr;
}

struct find_highlighted {
  const map_item_t &item;
  explicit find_highlighted(const map_item_t &t) : item(t) {}
  bool operator()(canvas_item_t *c);
};

bool find_highlighted::operator()(canvas_item_t* c)
{
  map_item_t *hl_item = c->get_user_data();

  return hl_item != nullptr && hl_item->object == item.object;
}

bool map_highlight_t::isHighlighted(const map_item_t& item) const
{
  if(isEmpty())
    return false;
  const std::vector<canvas_item_t *>::const_iterator itEnd = items.end();
  return std::find_if(items.begin(), itEnd, find_highlighted(item)) != itEnd;
}

void map_highlight_t::circle_new(map_t *map, canvas_group_t group, node_t *node,
                                 unsigned int radius, color_t color)
{
  map_item_t *map_item = new map_item_t(object_t(node));
  map_item->item = map->canvas->circle_new(group, node->lpos, radius, 0, color);
  items.push_back(map_item->item);

  map_item->item->set_user_data(map_item);
}

void map_highlight_t::polygon_new(map_t *map, canvas_group_t group, way_t *way,
                                  const std::vector<lpos_t> &points, color_t color) {
  map_item_t *map_item = new map_item_t(object_t(way));
  map_item->item = map->canvas->polygon_new(group, points, 0, 0, color);
  items.push_back(map_item->item);

  map_item->item->set_user_data(map_item);
}

void map_highlight_t::polyline_new(map_t *map, canvas_group_t group, way_t *way,
                                   const std::vector<lpos_t> &points, color_t color)
{
  map_item_t *map_item = new map_item_t(object_t(way));

  unsigned int width = way->draw.flags & OSM_DRAW_FLAG_BG ? way->draw.bg.width : way->draw.width;
  width = (2 * map->style->highlight.width + width) * map->state.detail;

  map_item->item = map->canvas->polyline_new(group, points, width, color);
  items.push_back(map_item->item);

  map_item->item->set_user_data(map_item);
}
