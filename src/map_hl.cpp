/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "map_hl.h"

#include "appdata.h"
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
void map_t::hl_cursor_draw(lpos_t pos, unsigned int radius)
{
  cursor.reset(canvas->circle_new(CANVAS_GROUP_DRAW, pos, radius,
                                  0, style->highlight.node_color));
}

void map_t::hl_cursor_clear()
{
  cursor.reset();
}

namespace {

class find_highlighted {
  const map_item_t &item;
public:
  explicit inline find_highlighted(const map_item_t &t) : item(t) {}
  bool operator()(canvas_item_t *c) const;
};

} // namespace

bool find_highlighted::operator()(canvas_item_t* c) const
{
  map_item_t *hl_item = c->get_user_data();

  return hl_item != nullptr && hl_item->object == item.object;
}

bool map_highlight_t::isHighlighted(const map_item_t &item) const
{
  if(isEmpty())
    return false;
  return std::any_of(items.begin(), items.end(), find_highlighted(item));
}

void map_highlight_t::circle_new(map_t *map, canvas_group_t group, node_t *node,
                                 float radius, color_t color)
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

  const unsigned int width = (2 * map->style->highlight.width + way->draw.drawWidth()) *
                             map->appdata.project->map_state.detail;

  map_item->item = map->canvas->polyline_new(group, points, width, color);
  items.push_back(map_item->item);

  map_item->item->set_user_data(map_item);
}
