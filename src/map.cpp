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

#include "map.h"

#include "appdata.h"
#include "canvas.h"
#include "color.h"
#include "gps_state.h"
#include "iconbar.h"
#include "info.h"
#include "map_hl.h"
#include "notifications.h"
#include "project.h"
#include "style.h"
#include "track.h"
#include "uicontrol.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_platform.h>
#include <osm2go_stl.h>

struct map_bg_modifier {
  map_bg_modifier() O2G_DELETED_FUNCTION;
  ~map_bg_modifier() O2G_DELETED_FUNCTION;

  static void remove(map_t *map, visible_item_t *way);
  static inline void add(map_t *map, way_t *way, map_item_t *item)
  {
    map->background_items[way] = item;
  }
};

void map_bg_modifier::remove(map_t *map, visible_item_t *way)
{
  std::unordered_map<visible_item_t *, map_item_t *>::iterator it = map->background_items.find(way);
  if(it == map->background_items.end())
    return;

  delete it->second->item;
  map->background_items.erase(it);
}

static void map_statusbar(const std::unique_ptr<MainUi> &uicontrol, const object_t &object,
                          osm_t::ref osm) {
  const std::string &str = object.get_name(*osm);
  MainUi::NotificationFlags flags = object.obj->tags.hasTagCollisions() ?
                                    MainUi::Highlight : MainUi::NoFlags;
  uicontrol->showNotification(str.c_str(), flags);
}

void map_t::outside_error() {
  error_dlg(_("Items must not be placed outside the working area!"));
}

void visible_item_t::item_chain_destroy(map_t *map)
{
  if(map_item == nullptr)
    return;

  if(map != nullptr)
    map_bg_modifier::remove(map, this);

  delete map_item->item;
  map_item = nullptr;
}

static void map_object_select(map_t *map, node_t *node)
{
  map_item_t *map_item = &map->selected;

  assert(map->highlight.isEmpty());

  map_item->object = node;

  /* node may not have any visible representation at all */
  if(node->map_item != nullptr)
    map_item->item = node->map_item->item;
  else
    map_item->item = nullptr;

  map_statusbar(map->appdata.uicontrol, map_item->object, map->appdata.project->osm);
  map->appdata.iconbar->map_item_selected(map_item->object);

  /* create a copy of this map item and mark it as being a highlight */
  float radius;
  style_t::IconCache::iterator it;
  if(map->style->icon.enable &&
     (it = map->style->node_icons.find(node->id)) != map->style->node_icons.end()) {
    /* icons are technically square, so a radius slightly bigger */
    /* than sqrt(2)*MAX(w,h) should fit nicely */
    radius = 0.75f * map->style->icon.scale * it->second->maxDimension();
  } else {
    radius = map->style->highlight.width + map->style->node.radius;
    if(node->ways == 0)
      radius += map->style->node.border_radius;
  }

  map->highlight.circle_new(map, CANVAS_GROUP_NODES_HL, node, radius * map->appdata.project->map_state.detail,
                            map->style->highlight.color);

  if(map_item->item == nullptr)
    /* and draw a fake node */
    map->highlight.circle_new(map, CANVAS_GROUP_NODES_IHL, node,
                              map->style->node.radius, map->style->highlight.node_color);
}

class set_point_pos {
  std::vector<lpos_t> &points;
public:
  explicit inline set_point_pos(std::vector<lpos_t> &p) : points(p) {}
  inline void operator()(const node_t *n) {
    points.push_back(n->lpos);
  }
};

/**
 * @brief create a canvas point array for a way
 * @param way the way to convert
 * @return canvas node array if at least 2 nodes were present
 * @retval nullptr the way has less than 2 nodes
 */
static std::vector<lpos_t>  __attribute__((nonnull(1)))
points_from_node_chain(const way_t *way)
{
  const unsigned int nodes = way->node_chain.size();
  std::vector<lpos_t> points(nodes);

  // the vector has the correct allocated size now, fill as usual
  points.clear();

  /* a way needs at least 2 points to be drawn */
  if (unlikely(nodes < 2))
    return points;

  /* allocate space for nodes */
  std::for_each(way->node_chain.begin(), way->node_chain.end(),
                set_point_pos(points));

  return points;
}

class draw_selected_way_functor {
  node_t *last;
  const float arrow_width;
  map_t * const map;
  way_t * const way;
  const float radius;
public:
  draw_selected_way_functor(float a, map_t *m, way_t *w)
    : last(nullptr), arrow_width(a), map(m), way(w)
    , radius(map->style->node.radius * map->appdata.project->map_state.detail) {}
  void operator()(node_t *node);
};

void draw_selected_way_functor::operator()(node_t *node)
{
  map_item_t item;
  item.object = node;

  /* draw an arrow between every two nodes */
  if(last != nullptr) {
    struct { float x, y; } diff;
    diff.x = node->lpos.x - last->lpos.x;
    diff.y = node->lpos.y - last->lpos.y;

    /* only draw arrow if there's sufficient space */
    /* TODO: what if there's not enough space anywhere? */
    float len = std::sqrt(std::pow(diff.x, 2.0f) + std::pow(diff.y, 2.0f)) / arrow_width;
    if(len > map->style->highlight.arrow_limit) {
      struct { float x, y; } center;
      center.x = (last->lpos.x + node->lpos.x) / 2.0;
      center.y = (last->lpos.y + node->lpos.y) / 2.0;

      /* create a new map item for every arrow */
      diff.x /= len;
      diff.y /= len;

      std::vector<lpos_t> points(4, lpos_t(center.x + diff.x, center.y + diff.y));
      points[1] = lpos_t(center.x + diff.y - diff.x, center.y - diff.x - diff.y);
      points[2] = lpos_t(center.x - diff.y - diff.x, center.y + diff.x - diff.y);

      map->highlight.polygon_new(map, CANVAS_GROUP_WAYS_DIR, way,
                                 points, map->style->highlight.arrow_color);
    }
  }

  if(!map->highlight.isHighlighted(item)) {
    /* create a new map item for every node */
    map->highlight.circle_new(map, CANVAS_GROUP_NODES_IHL, node, radius,
                              map->style->highlight.node_color);
  }

  last = node;
}

void map_t::select_way(way_t *way) {
  assert(highlight.isEmpty());

  selected.object = way;
  selected.item = way->map_item != nullptr ? way->map_item->item : nullptr;

  map_statusbar(appdata.uicontrol, selected.object, appdata.project->osm);
  appdata.iconbar->map_item_selected(selected.object);
  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_MAP_HIDE_SEL, true);

  float arrow_width = way->draw.flags & OSM_DRAW_FLAG_BG ?
                        way->draw.bg.width : way->draw.width;
  arrow_width = (style->highlight.width + arrow_width / 2)
                       * appdata.project->map_state.detail;

  const node_chain_t &node_chain = way->node_chain;
  std::for_each(node_chain.begin(), node_chain.end(),
                draw_selected_way_functor(arrow_width, this, way));

  /* a way needs at least 2 points to be drawn */
  assert(selected.object == way);
  const std::vector<lpos_t> &points = points_from_node_chain(way);
  if(likely(!points.empty()))
    /* create a copy of this map item and mark it as being a highlight */
    highlight.polyline_new(this, CANVAS_GROUP_WAYS_HL, way, points, style->highlight.color);
}

class relation_select_functor {
  map_t * const map;
public:
  explicit inline relation_select_functor(map_t *m) : map(m) {}
  void operator()(member_t &member);
};

void relation_select_functor::operator()(member_t& member)
{
  canvas_item_t *item = nullptr;

  switch(member.object.type) {
  case object_t::NODE: {
    node_t *node = member.object.node;
    printf("  -> node " ITEM_ID_FORMAT "\n", node->id);

    item = map->canvas->circle_new(CANVAS_GROUP_NODES_HL, node->lpos,
                             map->style->highlight.width + map->style->node.radius,
                             0, map->style->highlight.color);
    break;
    }
  case object_t::WAY: {
    way_t *way = member.object.way;
    /* a way needs at least 2 points to be drawn */
    const std::vector<lpos_t> &points = points_from_node_chain(way);
    if(likely(!points.empty())) {
      if(way->draw.flags & OSM_DRAW_FLAG_AREA) {
        item = map->canvas->polygon_new(CANVAS_GROUP_WAYS_HL, points, 0, 0,
                                  map->style->highlight.color);
      } else {
        float hwdth = way->draw.flags & OSM_DRAW_FLAG_BG ?
                      way->draw.bg.width : way->draw.width;
        hwdth += 2 * map->style->highlight.width;
        item = map->canvas->polyline_new(CANVAS_GROUP_WAYS_HL, points, hwdth,
                                   map->style->highlight.color);
      }
    }
    break;
    }

  default:
    break;
  }

  /* attach item to item chain */
  if(item != nullptr)
    map->highlight.items.push_back(item);
}


void map_t::select_relation(relation_t *relation) {
  printf("highlighting relation " ITEM_ID_FORMAT "\n", relation->id);

  assert(highlight.isEmpty());

  selected.object = relation;
  selected.item = nullptr;

  map_statusbar(appdata.uicontrol, selected.object, appdata.project->osm);
  appdata.iconbar->map_item_selected(selected.object);

  /* process all members */
  relation_select_functor fc(this);
  std::for_each(relation->members.begin(), relation->members.end(), fc);
}

static inline void map_object_select(map_t *map, way_t *way)
{
    map->select_way(way);
}

static void map_object_select(map_t *map, const object_t &object)
{
  switch(object.type) {
  case object_t::NODE:
    map_object_select(map, object.node);
    break;
  case object_t::WAY:
    map->select_way(object.way);
    break;
  case object_t::RELATION:
    map->select_relation(object.relation);
    break;
  default:
    assert_unreachable();
  }
}

void map_t::item_deselect() {

  /* save tags for "last" function in info dialog */
  if(selected.object.is_real() && selected.object.obj->tags.hasRealTags()) {
    if(selected.object.type == object_t::NODE)
      last_node_tags = selected.object.obj->tags.asMap();
    else if(selected.object.type == object_t::WAY)
      last_way_tags = selected.object.obj->tags.asMap();
  }

  /* remove statusbar message */
  appdata.uicontrol->showNotification(nullptr);

  /* disable/enable icons in icon bar */
  appdata.iconbar->map_item_selected(object_t());
  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_MAP_HIDE_SEL, false);

  /* remove highlight */
  highlight.clear();

  /* forget about selection */
  selected.object.type = object_t::ILLEGAL;
}

static void
map_node_new(map_t *map, node_t *node, float radius, int width, color_t fill, color_t border)
{
  map_item_t *map_item = new map_item_t(object_t(node));

  style_t::IconCache::const_iterator it;

  const float detail = map->appdata.project->map_state.detail;
  if(!map->style->icon.enable ||
     (it = map->style->node_icons.find(node->id)) == map->style->node_icons.end())
    map_item->item = map->canvas->circle_new(CANVAS_GROUP_NODES, node->lpos,
       radius, width, fill, border);
  else
    map_item->item = map->canvas->image_new(CANVAS_GROUP_NODES, it->second, node->lpos,
                                            detail * map->style->icon.scale);

  map_item->item->set_zoom_max(node->zoom_max / (2 * detail));

  /* attach map_item to nodes map_item_chain */
  if(node->map_item != nullptr)
    delete node->map_item->item;
  node->map_item = map_item;

  map_item->item->set_user_data(map_item);
}

static map_item_t *map_way_new(map_t *map, canvas_group_t group,
                               way_t *way, const std::vector<lpos_t> &points, unsigned int width,
                               color_t color, color_t fill_color)
{
  map_item_t *map_item = new map_item_t(object_t(way));

  if(way->draw.flags & OSM_DRAW_FLAG_AREA) {
    if(!map->style->area.color.is_transparent())
      map_item->item = map->canvas->polygon_new(group, points, width, color, fill_color);
    else
      map_item->item = map->canvas->polyline_new(group, points, width, color);
  } else {
    map_item->item = map->canvas->polyline_new(group, points, width, color);
  }

  map_item->item->set_zoom_max(way->zoom_max / (2 * map->appdata.project->map_state.detail));

  /* a ways outline itself is never dashed */
  if (group != CANVAS_GROUP_WAYS_OL && way->draw.dash_length_on > 0)
    map_item->item->set_dashed(width, way->draw.dash_length_on, way->draw.dash_length_off);

  map_item->item->set_user_data(map_item);

  return map_item;
}

class map_way_draw_functor {
  map_t * const map;
public:
  explicit inline map_way_draw_functor(map_t *m) : map(m) {}
  void operator()(way_t *way);
  inline void operator()(std::pair<item_id_t, way_t *> pair) {
    operator()(pair.second);
  }
};

void map_way_draw_functor::operator()(way_t *way)
{
  /* don't draw a way that's not there anymore */
  if(unlikely(way->isDeleted()))
    return;

  if(unlikely(map->appdata.project->osm->wayIsHidden(way)))
    return;

  /* attach map_item to ways map_item_chain */
  if(way->map_item != nullptr) {
    delete way->map_item->item;
    // no need to reset map_item, it will immediately be overwritten
    map_bg_modifier::remove(map, way);
  }

  /* allocate space for nodes */
  /* a way needs at least 2 points to be drawn */
  const std::vector<lpos_t> &points = points_from_node_chain(way);
  if(unlikely(points.empty())) {
    /* draw a single dot where this single node is */
    way->map_item = new map_item_t(object_t(way));

    assert(!way->node_chain.empty());
    way->map_item->item = map->canvas->circle_new(CANVAS_GROUP_WAYS, way->node_chain.front()->lpos,
                                             map->style->node.radius, 0,
                                             map->style->node.color, 0);

    // TODO: decide: do we need canvas_item_t::set_zoom_max() here too?

    way->map_item->item->set_user_data(way->map_item);
  } else {
    /* draw way */
    const float detail = map->appdata.project->map_state.detail;
    const float width = way->draw.width * detail;
    color_t areacol = color_t::transparent();
    canvas_group_t gr;

    if(way->draw.flags & OSM_DRAW_FLAG_AREA) {
      gr = CANVAS_GROUP_POLYGONS;
      areacol = way->draw.area.color;
    } else if(way->draw.flags & OSM_DRAW_FLAG_BG) {
      gr = CANVAS_GROUP_WAYS_INT;
      map_bg_modifier::add(map, way, map_way_new(map, CANVAS_GROUP_WAYS_OL, way, points,
                                                 way->draw.bg.width * detail,
                                                 way->draw.bg.color, color_t::transparent()));
    } else {
      gr = CANVAS_GROUP_WAYS;
    }

    way->map_item = map_way_new(map, gr, way, points, width, way->draw.color, areacol);
  }
}

void map_t::draw(way_t *way) {
  map_way_draw_functor m(this);
  m(way);
}

class map_node_draw_functor {
  map_t * const map;
  const float border_width;
  const float radius;
public:
  explicit inline map_node_draw_functor(map_t *m)
  : map(m)
  , border_width(map->style->node.border_radius * map->appdata.project->map_state.detail)
  , radius(map->style->node.radius * map->appdata.project->map_state.detail)
  {
  }

  void operator()(node_t *node);
  inline void operator()(std::pair<item_id_t, node_t *> pair) {
    operator()(pair.second);
  }
};

void map_node_draw_functor::operator()(node_t *node)
{
  /* don't draw a node that's not there anymore */
  if(unlikely(node->isDeleted()))
    return;

  int width;
  color_t fill, col;
  if(node->ways == 0) {
    width = border_width;
    fill = map->style->node.fill_color;
    col = map->style->node.color;
  } else if(map->style->node.show_untagged || node->tags.hasRealTags()) {
    width = 0;
    fill = map->style->node.color;
    col = 0;
  } else {
    return;
  }

  map_node_new(map, node, radius, width, fill, col);
}

void map_t::draw(node_t *node) {
  map_node_draw_functor m(this);
  m(node);
}

template<typename T>
void map_t::redraw_item(T *obj)
{
  /* check if the item to be redrawn is the selected one */
  bool is_selected = (selected.object == obj);
  // object must not be passed by reference or by pointer because of this:
  // map_t::item_deselect would modify object.type of the selected object, if
  // exactly that is passed in the switch statements below would see an
  // invalid type
  if(is_selected)
    item_deselect();

  obj->item_chain_destroy(this);

  style->colorize(obj);
  draw(obj);

  /* restore selection if there was one */
  if(is_selected)
    map_object_select(this, obj);
}

static void map_frisket_rectangle(std::vector<lpos_t> &points,
                                  int x0, int x1, int y0, int y1) {
  points[0] = lpos_t(x0, y0);
  points[1] = lpos_t(x1, y0);
  points[2] = lpos_t(x1, y1);
  points[3] = lpos_t(x0, y1);
  points[4] = points[0];
}

/* Draw the frisket area which masks off areas it'd be unsafe to edit,
 * plus its inner edge marker line */
static void map_frisket_draw(map_t *map, const bounds_t &bounds) {
  std::vector<lpos_t> points(5);

  /* don't draw frisket at all if it's completely transparent */
  if(!map->style->frisket.color.is_transparent()) {
    color_t color = map->style->frisket.color;

    /* top rectangle */
    map_frisket_rectangle(points, CANVAS_FRISKET_SCALE * bounds.min.x, CANVAS_FRISKET_SCALE * bounds.max.x,
                                  CANVAS_FRISKET_SCALE * bounds.min.y,                        bounds.min.y);
    map->canvas->polygon_new(CANVAS_GROUP_FRISKET, points, 1, color_t::transparent(), color);

    /* bottom rectangle */
    map_frisket_rectangle(points, CANVAS_FRISKET_SCALE * bounds.min.x, CANVAS_FRISKET_SCALE * bounds.max.x,
                                                         bounds.max.y, CANVAS_FRISKET_SCALE * bounds.max.y);
    map->canvas->polygon_new(CANVAS_GROUP_FRISKET, points, 1, color_t::transparent(), color);

    /* left rectangle */
    map_frisket_rectangle(points, CANVAS_FRISKET_SCALE * bounds.min.x,                        bounds.min.x,
                                  CANVAS_FRISKET_SCALE * bounds.min.y, CANVAS_FRISKET_SCALE * bounds.max.y);
    map->canvas->polygon_new(CANVAS_GROUP_FRISKET, points, 1, color_t::transparent(), color);

    /* right rectangle */
    map_frisket_rectangle(points,                        bounds.max.x, CANVAS_FRISKET_SCALE * bounds.max.x,
                                  CANVAS_FRISKET_SCALE * bounds.min.y, CANVAS_FRISKET_SCALE * bounds.max.y);
    map->canvas->polygon_new(CANVAS_GROUP_FRISKET, points, 1, color_t::transparent(), color);
  }

  if(map->style->frisket.border.present) {
    // Edge marker line
    int ew2 = map->style->frisket.border.width/2;
    map_frisket_rectangle(points, bounds.min.x - ew2, bounds.max.x + ew2,
                                  bounds.min.y - ew2, bounds.max.y + ew2);

    map->canvas->polyline_new(CANVAS_GROUP_FRISKET, points,
			map->style->frisket.border.width,
			map->style->frisket.border.color);

  }
}

static void free_map_item_chain(std::pair<item_id_t, visible_item_t *> pair) {
  // just remove the reference, the canvas will take care for the rest
  pair.second->map_item = nullptr;
}

template<bool b>
static void free_track_item_chain(track_seg_t &seg)
{
  if(b)
    std::for_each(seg.item_chain.begin(), seg.item_chain.end(),
                  std::default_delete<canvas_item_t>());
  seg.item_chain.clear();
}

static void map_free_map_item_chains(appdata_t &appdata) {
  if(unlikely(!appdata.project || !appdata.project->osm))
    return;

  osm_t::ref osm = appdata.project->osm;
  /* free all map_item_chains */
  std::for_each(osm->nodes.begin(), osm->nodes.end(),
                free_map_item_chain);

  std::for_each(osm->ways.begin(), osm->ways.end(),
                free_map_item_chain);

  if (appdata.track.track) {
    /* remove all segments */
    std::for_each(appdata.track.track->segments.begin(),
                  appdata.track.track->segments.end(),
                  free_track_item_chain<false>);
  }
}

/* get the item at position x, y */
map_item_t *map_t::item_at(lpos_t pos) {
  canvas_item_t *item = canvas->get_item_at(pos);

  if(item == nullptr) {
    printf("  there's no item\n");
    return nullptr;
  }

  printf("  there's an item (%p)\n", item);

  map_item_t *map_item = item->get_user_data();

  if(map_item == nullptr) {
    printf("  item has no user data!\n");
    return nullptr;
  }

  printf("  item is %s #" ITEM_ID_FORMAT "\n",
	 map_item->object.type_string(),
	 map_item->object.obj->id);

  return map_item;
}

/* get the real item (no highlight) at x, y */
void map_t::pen_down_item() {
  pen_down.on_item = item_at(canvas->window2world(pen_down.at));

  if(pen_down.on_item == nullptr)
    return;

  /* get the item (parent) this item is the highlight of */
  assert(pen_down.on_item->object.type == object_t::NODE || pen_down.on_item->object.type == object_t::WAY);

  visible_item_t * const vis = static_cast<visible_item_t *>(pen_down.on_item->object.obj);
  if(vis->map_item != nullptr) {
    printf("  using parent item %s #" ITEM_ID_FORMAT "\n", vis->apiString(), vis->id);
    pen_down.on_item = vis->map_item;
  } else {
    printf("  no parent, working on highlight itself\n");
  }
}

/*
 * Scroll the map to a point if that point is currently offscreen.
 * Return true if this was possible, false if position is outside
 * working area
 */
bool map_t::scroll_to_if_offscreen(lpos_t lpos) {
  // Ignore anything outside the working area
  if (!appdata.project->osm->bounds.contains(lpos)) {
    printf("cannot scroll to (%d, %d): outside the working area\n", lpos.x, lpos.y);
    return false;
  }

  if(!canvas->ensureVisible(lpos))
    appdata.project->map_state.scroll_offset = canvas->scroll_get();

  return true;
}

#define GPS_RADIUS_LIMIT  3.0

void map_t::set_zoom(double zoom, bool update_scroll_offsets) {
  map_state_t &state = appdata.project->map_state;
  state.zoom = canvas->set_zoom(zoom);
  bool at_zoom_limit = zoom != state.zoom;

  /* Deselects the current way or node if its zoom_max
   * means that it's not going to render at the current map zoom. */
  if(selected.object.type == object_t::WAY || selected.object.type == object_t::NODE) {
    float zmax = static_cast<visible_item_t *>(selected.object.obj)->zoom_max;

    if(state.zoom < zmax)
      item_deselect();
  }

  if(update_scroll_offsets) {
    state.scroll_offset = canvas->scroll_get();

    if (!at_zoom_limit)
      /* zooming affects the scroll offsets */
      state.scroll_offset = canvas->scroll_to(state.scroll_offset);
  }

  if(gps_item != nullptr) {
    float radius = style->track.width / 2.0f;
    if(zoom < GPS_RADIUS_LIMIT) {
      radius *= GPS_RADIUS_LIMIT;
      radius /= zoom;

      gps_item->set_radius(radius);
    }
  }
}

static bool distance_above(const map_t *map, const osm2go_platform::screenpos &p, int limit) {
  /* add offsets generated by mouse within map and map scrolling */
  osm2go_platform::screenpos s = p - map->pen_down.at;

  return s.x() * s.x() + s.y() * s.y() > limit * limit;
}

/* scroll a certain step */
void map_t::scroll_step(const osm2go_platform::screenpos &p)
{
  appdata.project->map_state.scroll_offset = canvas->scroll_step(p);
}

bool map_t::item_is_selected_node(const map_item_t *map_item) const
{
  if(map_item == nullptr)
    return false;

  /* clicked the highlight directly */
  if(map_item->object.type != object_t::NODE)
    return false;

  if(selected.object.type == object_t::NODE) {
    return selected.object == map_item->object;
  } else if(selected.object.type == object_t::WAY) {
    return selected.object.way->contains_node(map_item->object.node);
  } else {
    printf("%s: selected item is unknown (%s [%i])\n", __PRETTY_FUNCTION__,
           selected.object.type_string(), selected.object.type);
    return false;
  }
}

/* return true if the item given is the currenly selected way */
/* also return false if nothing is selected or the selection is no way */
bool map_t::item_is_selected_way(const map_item_t *map_item) const
{
  if(map_item == nullptr)
    return false;

  if(selected.object.type != object_t::WAY)
    return false;

  return map_item->object == selected.object;
}

void map_t::highlight_refresh() {
  object_t old = selected.object;

  printf("type to refresh is %d\n", old.type);
  if(old.type == object_t::ILLEGAL)
    return;

  item_deselect();
  map_object_select(this, old);
}

static void map_handle_click(map_t *map)
{
  /* problem: on_item may be the highlight itself! So store it! */
  object_t map_obj;
  if(map->pen_down.on_item != nullptr)
    map_obj = map->pen_down.on_item->object;

  /* if we already have something selected, then de-select it */
  map->item_deselect();

  /* select the clicked item (if there was one) */
  if(map_obj.type != object_t::ILLEGAL)
    map_object_select(map, map_obj);
}

class hl_nodes {
  const node_t * const cur_node;
  const lpos_t pos;
  map_t * const map;
  node_t *& res_node;
public:
  hl_nodes(const node_t *c, lpos_t p, map_t *m, node_t *&rnode)
    : cur_node(c), pos(p), map(m), res_node(rnode) {}
  void operator()(const std::pair<item_id_t, node_t *> &p);
  void operator()(node_t *node);
};

void hl_nodes::operator()(const std::pair<item_id_t, node_t *> &p)
{
  node_t * const node = p.second;

  if(node != cur_node && !node->isDeleted())
    operator()(node);
}

void hl_nodes::operator()(node_t* node)
{
  int nx = abs(pos.x - node->lpos.x);
  int ny = abs(pos.y - node->lpos.y);

  if(nx < map->style->node.radius && ny < map->style->node.radius &&
     (nx*nx + ny*ny) < map->style->node.radius * map->style->node.radius)
    res_node = node;
}

void map_t::touchnode_update(lpos_t pos) {
  hl_cursor_draw(pos, style->node.radius);

  touchnode_clear();

  const node_t *cur_node = nullptr;

  /* the "current node" which is the one we are working on and which */
  /* should not be highlighted depends on the action */
  switch(action.type) {

    /* in idle mode the dragged node is not highlighted */
  case MAP_ACTION_IDLE:
    assert(pen_down.on_item != nullptr);
    assert_cmpnum(pen_down.on_item->object.type, object_t::NODE);
    cur_node = pen_down.on_item->object.node;
    break;

  default:
    break;
  }

  /* check if we are close to one of the other nodes */
  node_t *rnode = nullptr;
  hl_nodes fc(cur_node, pos, this, rnode);
  std::map<item_id_t, node_t *> &nodes = appdata.project->osm->nodes;
  std::for_each(nodes.begin(), nodes.end(), fc);

  if(rnode != nullptr) {
    delete touchnode;

    touchnode = canvas->circle_new(CANVAS_GROUP_DRAW, rnode->lpos,
                                   2 * style->node.radius, 0, style->highlight.touch_color);

    touchnode_node = rnode;
  }

  /* during way creation also nodes of the new way */
  /* need to be searched */
  if(touchnode == nullptr && action.way != nullptr && action.way->node_chain.size() > 1) {
    const node_chain_t &chain = action.way->node_chain;
    std::for_each(chain.begin(), std::prev(chain.end()), fc);
  }
}

void map_t::button_press(const osm2go_platform::screenpos &p)
{
  printf("left button pressed\n");
  pen_down.is = true;

  /* save press position */
  pen_down.at = p;
  pen_down.drag = false;     // don't assume drag yet

  /* determine wether this press was on an item */
  pen_down_item();

  /* check if the clicked item is a highlighted node as the user */
  /* might want to drag that */
  pen_down.on_selected_node = item_is_selected_node(pen_down.on_item);

  lpos_t pos = canvas->window2world(p);
  /* button press */
  switch(action.type) {

  case MAP_ACTION_WAY_NODE_ADD:
    way_node_add_highlight(pen_down.on_item, pos);
    break;

  case MAP_ACTION_WAY_CUT:
    way_cut_highlight(pen_down.on_item, pos);
    break;

  case MAP_ACTION_NODE_ADD:
    hl_cursor_draw(pos, style->node.radius);
    break;

  case MAP_ACTION_WAY_ADD:
    touchnode_update(pos);
    break;

  default:
    break;
  }
}

void map_t::button_release(const osm2go_platform::screenpos &p)
{
  pen_down.is = false;

  /* before button release is handled */
  switch(action.type) {
  case MAP_ACTION_BG_ADJUST:
    bg_adjust(p);
    bg_offset += p - pen_down.at;
    break;

  case MAP_ACTION_IDLE:
    /* check if distance to press is above drag limit */
    if(!pen_down.drag)
      pen_down.drag = distance_above(this, p, MAP_DRAG_LIMIT);

    if(!pen_down.drag) {
      printf("left button released after click\n");

      object_t old_sel = selected.object;
      map_handle_click(this);

      if(old_sel.type != object_t::ILLEGAL && old_sel == selected.object) {
        printf("re-selected same item of type %s, pushing it to the bottom\n",
               old_sel.type_string());
        if(selected.item == nullptr) {
          printf("  item has no visible representation to push\n");
        } else {
          canvas->item_to_bottom(selected.item);

          /* update clicked item, to correctly handle the click */
          pen_down_item();

          map_handle_click(this);
        }
      }
    } else {
      printf("left button released after drag\n");

      /* just scroll if we didn't drag an selected item */
      if(!pen_down.on_selected_node) {
        osm2go_platform::screenpos d = pen_down.at - p;
        scroll_step(d);
      } else {
        printf("released after dragging node\n");
        hl_cursor_clear();

        /* now actually move the node */
        node_move(pen_down.on_item, p);
      }
    }
    break;

  case MAP_ACTION_NODE_ADD: {
    printf("released after NODE ADD\n");
    hl_cursor_clear();

    /* convert mouse position to canvas (world) position */
    lpos_t pos = canvas->window2world(p);

    node_t *node = nullptr;
    osm_t::ref osm = appdata.project->osm;
    if(!osm->bounds.contains(pos))
      outside_error();
    else {
      node = osm->node_new(pos);
      osm->node_attach(node);
      draw(node);
    }
    set_action(MAP_ACTION_IDLE);

    item_deselect();

    if(node != nullptr) {
      map_object_select(this, node);

      /* let the user specify some tags for the new node */
      info_selected();
    }
    break;
  }
  case MAP_ACTION_WAY_ADD:
    printf("released after WAY ADD\n");
    hl_cursor_clear();

    way_add_segment(canvas->window2world(p));
    break;

  case MAP_ACTION_WAY_NODE_ADD:
    printf("released after WAY NODE ADD\n");
    hl_cursor_clear();

    way_node_add(canvas->window2world(p));
    break;

  case MAP_ACTION_WAY_CUT:
    printf("released after WAY CUT\n");
    hl_cursor_clear();

    way_cut(canvas->window2world(p));
    break;

  default:
    assert_unreachable();
  }
}

/* move the background image (wms data) during wms adjustment */
void map_t::bg_adjust(const osm2go_platform::screenpos &p)
{
  osm_t::ref osm = appdata.project->osm;
  assert(osm);

  int x = p.x() + osm->bounds.min.x + bg_offset.x() - pen_down.at.x();
  int y = p.y() + osm->bounds.min.y + bg_offset.y() - pen_down.at.y();

  canvas->move_background(x, y);
}

void map_t::handle_motion(const osm2go_platform::screenpos &p)
{
  /* check if distance to press is above drag limit */
  if(!pen_down.drag)
    pen_down.drag = distance_above(this, p, MAP_DRAG_LIMIT);

  lpos_t pos;
  /* drag */
  switch(action.type) {
  case MAP_ACTION_BG_ADJUST:
    bg_adjust(p);
    break;

  case MAP_ACTION_IDLE:
    if(pen_down.drag) {
      /* just scroll if we didn't drag an selected item */
      if(!pen_down.on_selected_node)
        scroll_step(pen_down.at - p);
      else
        touchnode_update(canvas->window2world(p));
    }
    break;

  case MAP_ACTION_NODE_ADD:
    hl_cursor_draw(canvas->window2world(p), style->node.radius);
    break;

  case MAP_ACTION_WAY_ADD:
    touchnode_update(canvas->window2world(p));
    break;

  case MAP_ACTION_WAY_NODE_ADD:
    hl_cursor_clear();
    pos = canvas->window2world(p);
    way_node_add_highlight(item_at(pos), pos);
    break;

  case MAP_ACTION_WAY_CUT:
    hl_cursor_clear();
    pos = canvas->window2world(p);
    way_cut_highlight(item_at(pos), pos);
    break;

  default:
    assert_unreachable();
  }
}

map_t::_pd::_pd()
  : is(false)
  , drag(false)
  , on_item(nullptr)
  , at(0, 0)
  , on_selected_node(false)
{
}

map_t::map_t(appdata_t &a, canvas_t *c)
  : gps_item(nullptr)
  , appdata(a)
  , canvas(c)
  , touchnode(nullptr)
  , touchnode_node(nullptr)
  , bg_offset(0, 0)
  , style(appdata.style)
  , elements_drawn(0)
{
  memset(&action, 0, sizeof(action));
  action.type = MAP_ACTION_IDLE;
}

map_t::~map_t()
{
  // no need to clear background_items here, the items were all deleted when
  // destroying the canvas
  map_free_map_item_chains(appdata);
  cursor.release();
  appdata.map = nullptr;
}

void map_t::init() {
  const bounds_t &bounds = appdata.project->osm->bounds;

  /* update canvas background color */
  set_bg_color_from_style();

  // must be set before zoom so the valid dimension can be checked by canvas
  canvas->set_bounds(bounds.min, bounds.max);

  map_state_t &state = appdata.project->map_state;
  set_zoom(state.zoom, false);
  paint();

  printf("restore scroll position %f/%f\n",
         state.scroll_offset.x(), state.scroll_offset.y());

  state.scroll_offset = canvas->scroll_to(state.scroll_offset);
}

void map_t::clear(clearLayers layers) {
  printf("freeing map contents\n");

  unsigned int group_mask;
  switch(layers) {
  case MAP_LAYER_ALL:
    // add one so this is a usually illegal bitmask
    group_mask = ((1 << (CANVAS_GROUPS + 1)) - 1);
    remove_gps_position();
    break;
  case MAP_LAYER_OBJECTS_ONLY:
    group_mask = ((1 << CANVAS_GROUP_POLYGONS) |
                  (1 << CANVAS_GROUP_WAYS_HL) |
                  (1 << CANVAS_GROUP_WAYS_OL) |
                  (1 << CANVAS_GROUP_WAYS) |
                  (1 << CANVAS_GROUP_WAYS_INT) |
                  (1 << CANVAS_GROUP_NODES_HL) |
                  (1 << CANVAS_GROUP_NODES_IHL) |
                  (1 << CANVAS_GROUP_NODES) |
                  (1 << CANVAS_GROUP_WAYS_DIR));
    break;
  default:
    assert_unreachable();
  }

  // only clear the map, the items are deleted through the canvas
  background_items.clear();
  map_free_map_item_chains(appdata);

  /* remove a possibly existing highlight */
  item_deselect();

  canvas->erase(group_mask);
}

void map_t::paint() {
  osm_t::ref osm = appdata.project->osm;

  style->colorize_world(osm);

  assert(canvas != nullptr);

  printf("drawing ways ...\n");
  std::for_each(osm->ways.begin(), osm->ways.end(), map_way_draw_functor(this));

  printf("drawing single nodes ...\n");
  std::for_each(osm->nodes.begin(), osm->nodes.end(), map_node_draw_functor(this));

  printf("drawing frisket...\n");
  map_frisket_draw(this, osm->bounds);
}

/* called from several icons like e.g. "node_add" */
void map_t::set_action(map_action_t act) {
  printf("map action set to %d\n", act);

  action.type = act;

  /* enable/disable ok/cancel buttons */
  // MAP_ACTION_IDLE=0, NODE_ADD, BG_ADJUST, WAY_ADD, WAY_NODE_ADD, WAY_CUT
  bool ok_state = false;
  bool cancel_state = true;

  trstring::native_type statusbar_text;
  bool idle = false;

  switch(act) {
  case MAP_ACTION_BG_ADJUST:
    statusbar_text = _("Adjust background image position");
    ok_state = true;
    /* an existing selection only causes confusion ... */
    item_deselect();
    break;

  case MAP_ACTION_WAY_ADD: {
    statusbar_text = _("Place first node of new way");
    printf("starting new way\n");

    item_deselect();
    way_add_begin();
    break;
  }

  case MAP_ACTION_NODE_ADD:
    statusbar_text = _("Place a node");
    ok_state = true;
    item_deselect();
    break;

  case MAP_ACTION_IDLE:
    cancel_state = false;
    idle = true;
    break;

  case MAP_ACTION_WAY_CUT:
    statusbar_text = _("Select segment to cut way");
    break;

  case MAP_ACTION_WAY_NODE_ADD:
    statusbar_text = _("Place node on selected way");
    break;
  default:
    assert_unreachable();
  }

  appdata.iconbar->map_cancel_ok(cancel_state, ok_state);
  appdata.iconbar->map_action_idle(idle, selected.object);
  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_WMS_ADJUST, idle);

  appdata.uicontrol->showNotification(statusbar_text);
}


void map_t::action_ok() {
  /* reset action now as this erases the statusbar and some */
  /* of the actions may set it */
  map_action_t type = action.type;
  set_action(MAP_ACTION_IDLE);

  switch(type) {
  case MAP_ACTION_WAY_ADD:
    way_add_ok();
    break;

  case MAP_ACTION_BG_ADJUST:
    /* save changes to bg_offset in project */
    appdata.project->wms_offset.x = bg_offset.x();
    appdata.project->wms_offset.y = bg_offset.y();
    break;

  case MAP_ACTION_NODE_ADD:
    {
    pos_t pos = appdata.gps_state->get_pos();
    if(!pos.valid())
      break;

    node_t *node = nullptr;
    osm_t::ref osm = appdata.project->osm;

    if(!osm->bounds.ll.contains(pos)) {
      map_t::outside_error();
    } else {
      node = osm->node_new(pos);
      osm->node_attach(node);
      draw(node);
    }

    item_deselect();

    if(node != nullptr) {
      map_object_select(this, node);

      /* let the user specify some tags for the new node */
      info_selected();
    }
    }

  default:
    break;
  }
}

class node_deleted_from_ways {
  map_t * const map;
public:
  explicit inline node_deleted_from_ways(map_t *m) : map(m) { }
  void operator()(way_t *way);
};

/* redraw all affected ways */
void node_deleted_from_ways::operator()(way_t *way) {
  if(way->node_chain.size() == 1) {
    /* this way now only contains one node and thus isn't a valid */
    /* way anymore. So it'll also get deleted (which in turn may */
    /* cause other nodes to be deleted as well) */
    map->appdata.project->osm->way_delete(way, map);
  } else {
    map->redraw_item(way);
  }
}

class short_way {
  const node_t * const node;
public:
  explicit inline short_way(const node_t *n) : node(n) {}
  bool operator()(const std::pair<item_id_t, way_t *> &p) {
    const way_t *way = p.second;
    return way->node_chain.size() < 3 && way->contains_node(node);
  }
};

/* called from icon "trash" */
void map_t::delete_selected() {
  /* work on local copy since de-selecting destroys the selection */
  object_t sel = selected.object;

  const char *objtype = sel.type_string();
  if(!osm2go_platform::yes_no(trstring("Delete selected %1?").arg(objtype),
             trstring("Do you really want to delete the selected %1?").arg(objtype),
             MISC_AGAIN_ID_DELETE | MISC_AGAIN_FLAG_DONT_SAVE_NO))
    return;

  /* deleting the selected item de-selects it ... */
  item_deselect();

  printf("request to delete %s #" ITEM_ID_FORMAT "\n",
         objtype, sel.obj->id);

  osm_t::ref osm = appdata.project->osm;
  switch(sel.type) {
  case object_t::NODE: {
    /* check if this node is part of a way with two nodes only. */
    /* we cannot delete this as this would also delete the way */
    if(osm->find_way(short_way(sel.node)) != nullptr &&
       !osm2go_platform::yes_no(_("Delete node in short way(s)?"),
                                _("Deleting this node will also delete one or more ways "
                                "since they'll contain only one node afterwards. "
                                "Do you really want this?")))
      return;

    /* and mark it "deleted" in the database */
    const way_chain_t &chain = osm->node_delete(sel.node);
    std::for_each(chain.begin(), chain.end(), node_deleted_from_ways(this));

    break;
  }

  case object_t::WAY:
    osm->way_delete(sel.way, this);
    break;

  case object_t::RELATION:
    osm->relation_delete(sel.relation);
    break;

  default:
    assert_unreachable();
  }
}

/* ----------------------- track related stuff ----------------------- */

namespace {
/**
 * @brief allocate a point array and initialize it with screen coordinates
 * @param bounds screen boundary
 * @param point first track point to use
 * @param count number of points to use
 * @return point array
 */
std::vector<lpos_t>
canvas_points_init(const bounds_t &bounds, std::vector<track_point_t>::const_iterator point,
                   const unsigned int count)
{
  std::vector<lpos_t> points;
  points.reserve(count);
  lpos_t lpos;

  for(unsigned int i = 0; i < count; i++) {
    points.push_back(point->pos.toLpos(bounds));
    point++;
  }

  return points;
}

struct out_of_bounds {
  inline out_of_bounds(const bounds_t &b, bool inv) : bounds(b.ll), invert(inv) {}

  const pos_area &bounds;
  const bool invert;

  inline bool operator()(const track_point_t &pt) const
  { return invert == bounds.contains(pt.pos); }
};

} // namespace

void map_t::track_draw_seg(track_seg_t &seg) {
  const bounds_t &bounds = appdata.project->osm->bounds;

  /* a track_seg needs at least 2 points to be drawn */
  if (seg.track_points.empty())
    return;

  /* nothing should have been drawn by now ... */
  assert(seg.item_chain.empty());

  const std::vector<track_point_t>::const_iterator itEnd = seg.track_points.end();
  std::vector<track_point_t>::const_iterator it = seg.track_points.begin();
  while(it != itEnd) {
    /* skip all points not on screen */
    std::vector<track_point_t>::const_iterator tmp = std::find_if(it, itEnd, out_of_bounds(bounds, true));

    if(tmp == itEnd) {
      // the segment ends in a segment that is not on screen
      elements_drawn = 0;
      return;
    }
    /* actually start drawing with the last position that was offscreen */
    /* so the track nicely enters the viewing area */
    if (tmp != it)
      it = std::prev(tmp);

    /* count nodes that _are_ on screen */
    tmp = std::find_if(tmp, itEnd, out_of_bounds(bounds, false));
    unsigned int visible = std::distance(it, tmp);

    /* the last element is still on screen, so save the number of elements in
     * the point list to avoid recalculation on update */
    if(tmp == itEnd) {
      elements_drawn = visible;
    } else if(std::next(tmp) != itEnd) {
      /* also use last one that's offscreen to nicely leave the visible area */
      /* also determine the first item to use in the next loop */
      visible++;
      tmp++;
    } else {
      tmp = itEnd;
    }

    /* allocate space for nodes */
    printf("visible are %u\n", visible);
    std::vector<lpos_t> points = canvas_points_init(bounds, it, visible);
    it = tmp;

    canvas_item_t *item = canvas->polyline_new(CANVAS_GROUP_TRACK, points,
                                               style->track.width, style->track.color);
    seg.item_chain.push_back(item);
  }
}

/* update the last visible fragment of this segment since a */
/* gps position may have been added */
void map_t::track_update_seg(track_seg_t &seg) {
  const bounds_t &bounds = appdata.project->osm->bounds;

  printf("-- APPENDING TO TRACK --\n");
  assert(!seg.track_points.empty());

  /* there are two cases: either the second last point was on screen */
  /* or it wasn't. We'll have to start a new screen item if the latter */
  /* is the case */

  /* search last point */
  const std::vector<track_point_t>::const_iterator itEnd = seg.track_points.end();
  std::vector<track_point_t>::const_iterator last = std::prev(itEnd);
  /* check if the last and second_last points are visible */
  const bool last_is_visible = bounds.ll.contains(last->pos);
  const bool second_last_is_visible = (elements_drawn > 0);

  /* if both are invisible, then nothing has changed on screen */
  if(!last_is_visible && !second_last_is_visible) {
    printf("second_last and last entry are invisible -> doing nothing\n");
    elements_drawn = 0;
    return;
  }

  const std::vector<track_point_t>::const_iterator begin = // start of track to draw
                                                   second_last_is_visible
                                                   ? std::prev(itEnd, elements_drawn + 1)
                                                   : std::prev(itEnd, 2);

  /* since we are updating an existing track, it sure has at least two
   * points, second_last must be valid and its "next" (last) also */
  assert(begin != itEnd);
  assert(last != itEnd);

  /* count points to be placed */
  const size_t npoints = std::distance(begin, itEnd);
  assert_cmpnum_op(seg.track_points.size(), >=, npoints);
  elements_drawn = last_is_visible ? npoints : 0;

  lpos_t lpos = last->pos.toLpos(bounds);
  lpos_t lpos2 = (last - 1)->pos.toLpos(bounds);
  /* if both items appear on the screen in the same position (e.g. because they are
   * close to each other and a low zoom level) don't redraw as nothing would change
   * visually. */
  if(lpos == lpos2)
    return;

  std::vector<lpos_t> points = canvas_points_init(bounds, begin, npoints);

  if(second_last_is_visible) {
    /* there must be something already on the screen and there must */
    /* be visible nodes in the chain */
    assert(!seg.item_chain.empty());

    printf("second_last is visible -> updating last segment to %zu points\n", npoints);

    static_cast<canvas_item_polyline *>(seg.item_chain.back())->set_points(points);
  } else {
    assert(begin + 1 == last);
    assert(last_is_visible);

    printf("second last is invisible -> start new screen segment\n");

    canvas_item_t *item = canvas->polyline_new(CANVAS_GROUP_TRACK, points,
                                               style->track.width, style->track.color);
    seg.item_chain.push_back(item);
  }
}

class map_track_seg_draw_functor {
  map_t * const map;
public:
  explicit inline map_track_seg_draw_functor(map_t *m) : map(m) {}
  void operator()(track_seg_t &seg) {
    map->track_draw_seg(seg);
  }
};

void map_t::track_draw(TrackVisibility visibility, track_t &track) {
  if(unlikely(track.segments.empty()))
    return;

  track.clear();
  if(visibility < ShowPosition)
    remove_gps_position();

  canvas->erase(1 << CANVAS_GROUP_TRACK);

  switch(visibility) {
  case DrawAll:
    std::for_each(track.segments.begin(), track.segments.end(),
                  map_track_seg_draw_functor(this));
    break;
  case DrawCurrent:
    if(track.active)
      track_draw_seg(track.segments.back());
    break;
  default:
    break;
  }
}

void track_t::clear() {
  printf("removing track\n");

  std::for_each(segments.begin(), segments.end(), free_track_item_chain<true>);
}

void track_t::clear_current()
{
  assert(!segments.empty());
  free_track_item_chain<true>(segments.back());
  segments.back().track_points.clear();
}

/**
 * @brief show the marker item for the current GPS position
 */
void map_t::track_pos(const lpos_t lpos) {
  /* remove the old item */
  remove_gps_position();

  float radius = style->track.width / 2.0f;
  double zoom = canvas->get_zoom();
  if(zoom < GPS_RADIUS_LIMIT) {
    radius *= GPS_RADIUS_LIMIT;
    radius /= zoom;
  }

  gps_item = canvas->circle_new(CANVAS_GROUP_GPS, lpos, radius, 0,
                                style->track.gps_color);
}

/**
 * @brief remove the marker item for the current GPS position
 */
void map_t::remove_gps_position() {
  delete gps_item;
  gps_item = nullptr;
}

void map_t::action_cancel()
{
  switch(action.type) {
  case MAP_ACTION_WAY_ADD:
    way_add_cancel();
    break;

  case MAP_ACTION_BG_ADJUST: {
    /* undo all changes to bg_offset */
    bg_offset = osm2go_platform::screenpos(appdata.project->wms_offset.x,
                                           appdata.project->wms_offset.y);

    const bounds_t &bounds = appdata.project->osm->bounds;
    canvas->move_background(bounds.min.x + bg_offset.x(), bounds.min.y + bg_offset.y());
    break;
  }

  default:
    break;
  }

  set_action(MAP_ACTION_IDLE);
}

/* ------------------- map background ------------------ */

namespace {

void
setMenuEntries(const std::unique_ptr<MainUi> &uicontrol, bool en)
{
  uicontrol->setActionEnable(MainUi::MENU_ITEM_WMS_CLEAR, en);
  uicontrol->setActionEnable(MainUi::MENU_ITEM_WMS_ADJUST, en);
}

} // namespace

void map_t::remove_bg_image()
{
  cancel_bg_adjust();

  canvas->set_background(std::string());

  setMenuEntries(appdata.uicontrol, false);
}

void map_t::cancel_bg_adjust()
{
  if(action.type == MAP_ACTION_BG_ADJUST)
    action_cancel();
}

void map_t::set_bg_color_from_style()
{
  canvas->set_background(style->background.color);
}

bool map_t::set_bg_image(const std::string &filename, osm2go_platform::screenpos offset)
{
  const lpos_t min = appdata.project->osm->bounds.min;

  cancel_bg_adjust();

  bg_offset = offset;

  bool ret = canvas->set_background(filename);

  if (ret) {
    int x = min.x + bg_offset.x();
    int y = min.y + bg_offset.y();
    canvas->move_background(x, y);
  }

  setMenuEntries(appdata.uicontrol, ret);

  return ret;
}

/* -------- hide and show objects (for performance reasons) ------- */

void map_t::hide_selected() {
  if(selected.object.type != object_t::WAY) {
    printf("selected item is not a way\n");
    return;
  }

  way_t *way = selected.object.way;
  printf("hiding way #" ITEM_ID_FORMAT "\n", way->id);

  item_deselect();
  appdata.project->osm->waySetHidden(way);
  way->item_chain_destroy(this);

  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_MAP_SHOW_ALL, true);
}

class map_show_all_functor {
  map_t * const map;
public:
  explicit inline map_show_all_functor(map_t *m) : map(m) {}
  inline void operator()(way_t *way) const {
    map->draw(way);
  }
};

void map_t::show_all() {
  std::unordered_set<way_t *> ways;
  // the global table must be cleared, otherwise the call to map->draw()
  // inside the functor will do nothing as it sees the way as still hidden
  ways.swap(appdata.project->osm->hiddenWays);
  std::for_each(ways.begin(), ways.end(), map_show_all_functor(this));

  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_MAP_SHOW_ALL, false);
}

void map_t::detail_change(float detail, trstring::native_type_arg banner_msg)
{
  appdata.uicontrol->showNotification(banner_msg, MainUi::Busy);
  /* deselecting anything allows us not to care about automatic deselection */
  /* as well as items becoming invisible by the detail change */
  item_deselect();

  appdata.project->map_state.detail = detail;
  printf("changing detail factor to %f\n", detail);

  clear(MAP_LAYER_OBJECTS_ONLY);
  paint();

  appdata.uicontrol->showNotification(nullptr, MainUi::Busy);
}

void map_t::detail_increase() {
  detail_change(appdata.project->map_state.detail * MAP_DETAIL_STEP, _("Increasing detail level"));
}

void map_t::detail_decrease() {
  detail_change(appdata.project->map_state.detail / MAP_DETAIL_STEP, _("Decreasing detail level"));
}

void map_t::detail_normal() {
  detail_change(1.0, _("Restoring default detail level"));
}

node_t *map_t::touchnode_get_node() {
  if(touchnode == nullptr)
    return nullptr;
  node_t *ret = touchnode_node;
  touchnode_clear();
  return ret;
}

void map_t::touchnode_clear() {
  delete touchnode;
  touchnode = nullptr;
  touchnode_node = nullptr;
}

void map_t::info_selected()
{
  bool ret = info_dialog(appdata_t::window, this, appdata.project->osm, appdata.presets.get(), selected.object);

  /* since nodes being parts of ways but with no tags are invisible, */
  /* the result of editing them may have changed their visibility */
  if(ret) {
    switch(selected.object.type) {
    case object_t::NODE:
      redraw_item(selected.object.node);
      break;
    case object_t::WAY:
      redraw_item(selected.object.way);
      break;
    case object_t::RELATION:
      break;
    default:
      assert_unreachable();
    }
  }
}

map_state_t::map_state_t() noexcept
  : scroll_offset(0, 0)
{
  reset();
}

void map_state_t::reset() noexcept
{
  zoom = 0.25;
  detail = 1.0;

  scroll_offset = osm2go_platform::screenpos(0, 0);
}
