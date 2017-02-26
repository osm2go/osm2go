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

#include "map.h"

#include "appdata.h"
#include "banner.h"
#include "diff.h"
#include "gps.h"
#include "iconbar.h"
#include "info.h"
#include "map_edit.h"
#include "map_hl.h"
#include "misc.h"
#include "statusbar.h"
#include "style.h"
#include "track.h"
#include "undo.h"

#include <algorithm>
#include <cstddef>
#include <gdk/gdkkeysyms.h>
#include <vector>

map_t::map_t()
{
  memset(this, 0, offsetof(map_t, last_node_tags));
}

static void osm_tag_members_free(tag_t tag) {
  g_free(tag.key);
  g_free(tag.value);
}

map_t::~map_t()
{
  /* free buffered tags */
  std::for_each(last_node_tags.begin(), last_node_tags.end(), osm_tag_members_free);
  std::for_each(last_way_tags.begin(), last_way_tags.end(), osm_tag_members_free);

  map_state_free(state);

  delete style;

  /* destroy existing highlight */
  delete highlight;
}

/* this is a chain of map_items which is attached to all entries */
/* in the osm tree (node_t, way_t, ...) to be able to get a link */
/* to the screen representation of a give node/way/etc */
struct map_item_chain_t {
  std::vector<map_item_t *> map_items;
  map_item_t *firstItem() const;
  canvas_item_t *firstCanvasItem() const;
};

map_item_t *map_item_chain_t::firstItem() const {
  if(map_items.empty())
    return 0;
  return map_items.front();
}

canvas_item_t *map_item_chain_t::firstCanvasItem() const {
  if(map_items.empty())
    return 0;
  return map_items.front()->item;
}

#undef DESTROY_WAIT_FOR_GTK

struct self_collision_functor {
  const std::vector<tag_t *> &tags;
  self_collision_functor(const std::vector<tag_t *> &t) : tags(t) {}
  bool operator()(const tag_t *tag) {
    return info_tag_key_collision(tags, tag) == TRUE;
  }
};

static void map_statusbar(map_t *map, map_item_t *map_item) {
  tag_t *tag = map_item->object.obj->tag;

  gboolean collision = FALSE;
  std::vector<tag_t *> tags;
  for(tag_t *t = tag; t; t = t->next)
    tags.push_back(t);

  collision = std::find_if(tags.begin(), tags.end(), self_collision_functor(tags)) !=
              tags.end();

  char *str = map_item->object.get_name();
  statusbar_set(map->appdata, str, collision);
  g_free(str);
}

void map_outside_error(appdata_t *appdata) {
  errorf(GTK_WIDGET(appdata->window),
	 _("Items must not be placed outside the working area!"));
}

static inline void map_item_destroy_canvas_item(map_item_t *m) {
  canvas_item_destroy(m->item);
}

void map_item_chain_destroy(map_item_chain_t **chainP) {
  if(!*chainP) {
    printf("nothing to destroy!\n");
    return;
  }

  std::for_each((*chainP)->map_items.begin(), (*chainP)->map_items.end(),
                map_item_destroy_canvas_item);

#ifdef DESTROY_WAIT_FOR_GTK
  /* wait until gtks event handling has actually destroyed this item */
  printf("waiting for item destruction ");
  while(gtk_events_pending() || *chainP) {
    putchar('.');
    gtk_main_iteration();
  }
  printf(" ok\n");

  /* the callback routine connected to this item should have been */
  /* called by now and it has set the chain to NULL */

#else
  delete *chainP;
  *chainP = NULL;
#endif
}

static void map_node_select(appdata_t *appdata, node_t *node) {
  map_t *map = appdata->map;
  map_item_t *map_item = &map->selected;

  g_assert(!map->highlight);

  map_item->object = node;
  map_item->highlight = FALSE;

  /* node may not have any visible representation at all */
  if(node->map_item_chain)
    map_item->item = node->map_item_chain->firstCanvasItem();
  else
    map_item->item = NULL;

  map_statusbar(map, map_item);
  icon_bar_map_item_selected(appdata, map_item, TRUE);

  /* highlight node */
  gint x = map_item->object.node->lpos.x, y = map_item->object.node->lpos.y;

  /* create a copy of this map item and mark it as being a highlight */
  map_item_t *new_map_item = g_new0(map_item_t, 1);
  memcpy(new_map_item, map_item, sizeof(map_item_t));
  new_map_item->highlight = TRUE;

  float radius = 0;
  if(node->icon_buf && map->style->icon.enable) {
    gint w = gdk_pixbuf_get_width(map_item->object.node->icon_buf);
    gint h = gdk_pixbuf_get_height(map_item->object.node->icon_buf);

    /* icons are technically square, so a radius slightly bigger */
    /* than sqrt(2)*MAX(w,h) should fit nicely */
    radius = 0.75 * map->style->icon.scale * MAX(w, h);
  } else {
    radius = map->style->highlight.width + map->style->node.radius;
    if(!node->ways) radius += map->style->node.border_radius;
  }

  radius *= map->state->detail;

  map_hl_circle_new(map, CANVAS_GROUP_NODES_HL, new_map_item,
		    x, y, radius, map->style->highlight.color);

  if(!map_item->item) {
    /* and draw a fake node */
    new_map_item = g_new0(map_item_t, 1);
    memcpy(new_map_item, map_item, sizeof(map_item_t));
    new_map_item->highlight = TRUE;
    map_hl_circle_new(map, CANVAS_GROUP_NODES_IHL, new_map_item,
		      x, y, map->style->node.radius,
		      map->style->highlight.node_color);
  }
}

struct set_point_pos {
  canvas_points_t * const points;
  gint node;
  set_point_pos(canvas_points_t *p) : points(p), node(0) {}
  void operator()(const node_t *n) {
    canvas_point_set_pos(points, node++, &n->lpos);
  }
};

/**
 * @brief create a canvas point array for a way
 * @param way the way to convert
 * @return canvas node array if at least 2 nodes were present
 * @retval NULL the way has less than 2 ways
 */
static canvas_points_t *
points_from_node_chain(const way_t *way)
{
  /* a way needs at least 2 points to be drawn */
  guint nodes = way->node_chain.size();
  if (nodes < 2)
    return NULL;

  /* allocate space for nodes */
  canvas_points_t *points = canvas_points_new(nodes);

  std::for_each(way->node_chain.begin(), way->node_chain.end(),
                set_point_pos(points));

  return points;
}

struct draw_selected_way_functor {
  node_t *last;
  const gint arrow_width;
  map_t * const map;
  way_t * const way;
  draw_selected_way_functor(gint a, map_t *m, way_t *w)
    : last(0), arrow_width(a), map(m), way(w) {}
  void operator()(node_t *node);
};

void draw_selected_way_functor::operator()(node_t* node)
{
  map_item_t item;
  item.object = node;

  /* draw an arrow between every two nodes */
  if(last) {
    struct { float x, y;} center, diff;
    center.x = (last->lpos.x + node->lpos.x)/2;
    center.y = (last->lpos.y + node->lpos.y)/2;
    diff.x = node->lpos.x - last->lpos.x;
    diff.y = node->lpos.y - last->lpos.y;

    /* only draw arrow if there's sufficient space */
    /* TODO: what if there's not enough space anywhere? */
    float len = sqrt(pow(diff.x, 2)+pow(diff.y, 2));
    if(len > map->style->highlight.arrow_limit * arrow_width) {
      /* create a new map item for every arrow */
      map_item_t *new_map_item = g_new0(map_item_t, 1);
      new_map_item->object = way;
      new_map_item->highlight = TRUE;

      len /= arrow_width;
      diff.x /= len;
      diff.y /= len;

      canvas_points_t *points = canvas_points_new(4);
      points->coords[2 * 0 + 0] = points->coords[2 * 3 + 0] = center.x + diff.x;
      points->coords[2 * 0 + 1] = points->coords[2 * 3 + 1] = center.y + diff.y;
      points->coords[2 * 1 + 0] = center.x + diff.y - diff.x;
      points->coords[2 * 1 + 1] = center.y - diff.x - diff.y;
      points->coords[2 * 2 + 0] = center.x - diff.y - diff.x;
      points->coords[2 * 2 + 1] = center.y + diff.x - diff.y;

      map_hl_polygon_new(map, CANVAS_GROUP_WAYS_DIR, new_map_item,
                         points, map->style->highlight.arrow_color);

      canvas_points_free(points);
    }
  }

  if(!map_hl_item_is_highlighted(map, &item)) {
    /* create a new map item for every node */
    map_item_t *new_map_item = g_new0(map_item_t, 1);
    new_map_item->object = node;
    new_map_item->highlight = TRUE;

    map_hl_circle_new(map, CANVAS_GROUP_NODES_IHL, new_map_item,
                      node->lpos.x, node->lpos.y,
                      map->style->node.radius * map->state->detail,
                      map->style->highlight.node_color);
  }

  last = node;
}

void map_way_select(appdata_t *appdata, way_t *way) {
  map_t *map = appdata->map;
  map_item_t *map_item = &map->selected;

  g_assert(!map->highlight);

  map_item->object = way;
  map_item->highlight = FALSE;
  map_item->item      = way->map_item_chain->firstCanvasItem();

  map_statusbar(map, map_item);
  icon_bar_map_item_selected(appdata, map_item, TRUE);
  gtk_widget_set_sensitive(appdata->menu_item_map_hide_sel, TRUE);

  gint arrow_width = ((map_item->object.way->draw.flags & OSM_DRAW_FLAG_BG)?
		      map->style->highlight.width + map_item->object.way->draw.bg.width/2:
		      map->style->highlight.width + map_item->object.way->draw.width/2)
    * map->state->detail;

  const node_chain_t &node_chain = map_item->object.way->node_chain;
  std::for_each(node_chain.begin(), node_chain.end(),
                draw_selected_way_functor(arrow_width, map, way));

  /* a way needs at least 2 points to be drawn */
  g_assert(map_item->object.way == way);
  canvas_points_t *points = points_from_node_chain(way);
  if(points != NULL) {
    /* create a copy of this map item and mark it as being a highlight */
    map_item_t *new_map_item = g_new(map_item_t, 1);
    *new_map_item = *map_item;
    new_map_item->highlight = TRUE;

    map_hl_polyline_new(map, CANVAS_GROUP_WAYS_HL, new_map_item, points,
		 ((way->draw.flags & OSM_DRAW_FLAG_BG)?
		 2*map->style->highlight.width + way->draw.bg.width:
		 2*map->style->highlight.width + way->draw.width)
		* map->state->detail, map->style->highlight.color);

    canvas_points_free(points);
  }
}

struct relation_select_functor {
  map_highlight_t &hl;
  map_t * const map;
  relation_select_functor(map_highlight_t &h, map_t *m) : hl(h), map(m) {}
  void operator()(member_t &member);
};

void relation_select_functor::operator()(member_t& member)
{
  canvas_item_t *item = 0;

  switch(member.object.type) {
  case NODE: {
    node_t *node = member.object.node;
    printf("  -> node " ITEM_ID_FORMAT "\n", node->id);

    item = canvas_circle_new(map->canvas, CANVAS_GROUP_NODES_HL,
                             node->lpos.x, node->lpos.y,
                             map->style->highlight.width + map->style->node.radius,
                             0, map->style->highlight.color, NO_COLOR);
    break;
    }
  case WAY: {
    way_t *way = member.object.way;
    /* a way needs at least 2 points to be drawn */
    canvas_points_t *points = points_from_node_chain(way);
    if(points != NULL) {
      if(way->draw.flags & OSM_DRAW_FLAG_AREA)
        item = canvas_polygon_new(map->canvas, CANVAS_GROUP_WAYS_HL, points, 0, 0,
                                  map->style->highlight.color);
      else
        item = canvas_polyline_new(map->canvas, CANVAS_GROUP_WAYS_HL, points,
                                   (way->draw.flags & OSM_DRAW_FLAG_BG) ?
                                     2 * map->style->highlight.width + way->draw.bg.width :
                                     2 * map->style->highlight.width + way->draw.width,
                                   map->style->highlight.color);

      canvas_points_free(points);
    }
    break;
    }

  default:
    break;
  }

  /* attach item to item chain */
  if(item)
    hl.items.push_back(item);
}


void map_relation_select(appdata_t *appdata, relation_t *relation) {
  map_t *map = appdata->map;

  printf("highlighting relation " ITEM_ID_FORMAT "\n", relation->id);

  map_highlight_t *hl = map->highlight;
  if(hl) {
    g_assert(hl->items.empty());
  } else {
    hl = map->highlight = new map_highlight_t();
  }

  map_item_t *map_item = &map->selected;
  map_item->object = relation;
  map_item->highlight = FALSE;
  map_item->item      = NULL;

  map_statusbar(map, map_item);
  icon_bar_map_item_selected(appdata, map_item, TRUE);

  /* process all members */
  relation_select_functor fc(*hl, map);
  std::for_each(relation->members.begin(), relation->members.end(), fc);
}

static void map_object_select(appdata_t *appdata, object_t &object) {
  switch(object.type) {
  case NODE:
    map_node_select(appdata, object.node);
    break;
  case WAY:
    map_way_select(appdata, object.way);
    break;
  case RELATION:
    map_relation_select(appdata, object.relation);
    break;
  default:
    g_assert_not_reached();
    break;
  }
}

void map_item_deselect(appdata_t *appdata) {

  /* save tags for "last" function in info dialog */
  if(appdata->map->selected.object.obj && appdata->map->selected.object.obj->tag) {
    std::vector<tag_t> clear;
    if(appdata->map->selected.object.type == NODE) {
      clear.swap(appdata->map->last_node_tags);

      appdata->map->last_node_tags =
        osm_tags_list_copy(appdata->map->selected.object.obj->tag);
    } else if(appdata->map->selected.object.type == WAY) {
      clear.swap(appdata->map->last_way_tags);

      appdata->map->last_way_tags =
        osm_tags_list_copy(appdata->map->selected.object.obj->tag);
    }
    std::for_each(clear.begin(), clear.end(), osm_tag_members_free);
  }

  /* remove statusbar message */
  statusbar_set(appdata, NULL, FALSE);

  /* disable/enable icons in icon bar */
  icon_bar_map_item_selected(appdata, NULL, FALSE);
  gtk_widget_set_sensitive(appdata->menu_item_map_hide_sel, FALSE);

  /* remove highlight */
  map_hl_remove(appdata);

  /* forget about selection */
  appdata->map->selected.object.type = ILLEGAL;
}

/* called whenever a map item is to be destroyed */
static gint map_item_destroy_event(G_GNUC_UNUSED GtkWidget *widget, gpointer data) {
  map_item_t *map_item = (map_item_t*)data;

  //  printf("destroying map_item @ %p\n", map_item);

#ifdef DESTROY_WAIT_FOR_GTK
  /* remove item from nodes/ways map_item_chain */
  map_item_chain_t *chain = 0;
  if(map_item->object.type == NODE)
    chain = map_item->object.node->map_item_chain;
  else if(map_item->object.type == WAY)
    chain = map_item->object.way->map_item_chain;

  /* there must be a chain with content, otherwise things are broken */
  g_assert(chain);

  /* search current map_item, ... */
  std::vector<map_item_t *>::iterator it = std::find(chain->map_items.begin(),
                                                     chain->map_items.end(),
                                                     map_item);

  g_assert(it != chain->map_items.end());

  /* ... remove it from chain and free it */
  chain->map_items.erase(it);
#endif

  g_free(map_item);
  return FALSE;
}

static canvas_item_t *map_node_new(map_t *map, node_t *node, gint radius,
		   gint width, canvas_color_t fill, canvas_color_t border) {

  map_item_t *map_item = g_new0(map_item_t, 1);
  map_item->object = node;

  if(!node->icon_buf || !map->style->icon.enable)
    map_item->item = canvas_circle_new(map->canvas, CANVAS_GROUP_NODES,
       node->lpos.x, node->lpos.y, radius, width, fill, border);
  else
    map_item->item = canvas_image_new(map->canvas, CANVAS_GROUP_NODES,
      node->icon_buf, node->lpos.x, node->lpos.y,
		      map->state->detail * map->style->icon.scale,
		      map->state->detail * map->style->icon.scale);

  canvas_item_set_zoom_max(map_item->item,
			   node->zoom_max / (2 * map->state->detail));

  /* attach map_item to nodes map_item_chain */
  if(!node->map_item_chain)
    node->map_item_chain = new map_item_chain_t();
  node->map_item_chain->map_items.push_back(map_item);

  canvas_item_set_user_data(map_item->item, map_item);

  canvas_item_destroy_connect(map_item->item,
          G_CALLBACK(map_item_destroy_event), map_item);

  return map_item->item;
}

/* in the rare case that a way consists of only one node, it is */
/* drawn as a circle. This e.g. happens when drawing a new way */
static map_item_t *map_way_single_new(map_t *map, way_t *way, gint radius,
		   gint width, canvas_color_t fill, canvas_color_t border) {

  map_item_t *map_item = g_new0(map_item_t, 1);
  map_item->object = way;
  map_item->item = canvas_circle_new(map->canvas, CANVAS_GROUP_WAYS,
	  way->node_chain.front()->lpos.x, way->node_chain.front()->lpos.y,
				     radius, width, fill, border);

  // TODO: decide: do we need canvas_item_set_zoom_max() here too?

  canvas_item_set_user_data(map_item->item, map_item);

  canvas_item_destroy_connect(map_item->item,
          G_CALLBACK(map_item_destroy_event), map_item);

  return map_item;
}

static map_item_t *map_way_new(map_t *map, canvas_group_t group,
	  way_t *way, canvas_points_t *points, gint width,
	  canvas_color_t color, canvas_color_t fill_color) {
  map_item_t *map_item = g_new0(map_item_t, 1);
  map_item->object = way;

  if(way->draw.flags & OSM_DRAW_FLAG_AREA) {
    if(map->style->area.color & 0xff)
      map_item->item = canvas_polygon_new(map->canvas, group, points,
					  width, color, fill_color);
    else
      map_item->item = canvas_polyline_new(map->canvas, group, points,
					   width, color);
  } else {
    map_item->item = canvas_polyline_new(map->canvas, group, points, width, color);
  }

  canvas_item_set_zoom_max(map_item->item,
			   way->draw.zoom_max / (2 * map->state->detail));

  /* a ways outline itself is never dashed */
  if (group != CANVAS_GROUP_WAYS_OL)
    if (way->draw.dashed)
      canvas_item_set_dashed(map_item->item, width, way->draw.dash_length);

  canvas_item_set_user_data(map_item->item, map_item);

  canvas_item_destroy_connect(map_item->item,
	      G_CALLBACK(map_item_destroy_event), map_item);

  return map_item;
}

void map_show_node(map_t *map, node_t *node) {
  map_node_new(map, node, map->style->node.radius, 0,
	       map->style->node.color, 0);
}

struct map_way_draw_functor {
  map_t * const map;
  map_way_draw_functor(map_t *m) : map(m) {}
  void operator()(way_t *way);
  void operator()(std::pair<item_id_t, way_t *> pair) {
    operator()(pair.second);
  }
};

void map_way_draw_functor::operator()(way_t *way)
{
  /* don't draw a way that's not there anymore */
  if(way->flags & (OSM_FLAG_DELETED | OSM_FLAG_HIDDEN))
    return;

  /* attach map_item to ways map_item_chain */
  if(!way->map_item_chain)
    way->map_item_chain = new map_item_chain_t();
  std::vector<map_item_t *> &chain = way->map_item_chain->map_items;

  /* allocate space for nodes */
  /* a way needs at least 2 points to be drawn */
  canvas_points_t *points = points_from_node_chain(way);
  if(points == NULL) {
    /* draw a single dot where this single node is */
    chain.push_back(map_way_single_new(map, way, map->style->node.radius, 0,
                                       map->style->node.color, 0));
  } else {
    /* draw way */
    float width = way->draw.width * map->state->detail;

    if(way->draw.flags & OSM_DRAW_FLAG_AREA) {
      chain.push_back(map_way_new(map, CANVAS_GROUP_POLYGONS, way, points,
                                  width, way->draw.color, way->draw.area.color));
    } else {
      if(way->draw.flags & OSM_DRAW_FLAG_BG) {
        chain.push_back(map_way_new(map, CANVAS_GROUP_WAYS_INT, way, points,
                                    width, way->draw.color, NO_COLOR));

        chain.push_back(map_way_new(map, CANVAS_GROUP_WAYS_OL, way, points,
                                    way->draw.bg.width * map->state->detail,
                                    way->draw.bg.color, NO_COLOR));

      } else
        chain.push_back(map_way_new(map, CANVAS_GROUP_WAYS, way, points,
                                    width, way->draw.color, NO_COLOR));
    }
    canvas_points_free(points);
  }
}

void map_way_draw(map_t *map, way_t *way) {
  map_way_draw_functor m(map);
  m(way);
}

struct map_node_draw_functor {
  map_t * const map;
  map_node_draw_functor(map_t *m) : map(m) {}
  void operator()(node_t *node);
  void operator()(std::pair<item_id_t, node_t *> pair) {
    operator()(pair.second);
  }
};

void map_node_draw_functor::operator()(node_t *node)
{
  /* don't draw a node that's not there anymore */
  if(node->flags & OSM_FLAG_DELETED)
    return;

  if(!node->ways)
    map_node_new(map, node,
		 map->style->node.radius * map->state->detail,
		 map->style->node.border_radius * map->state->detail,
		 map->style->node.fill_color,
		 map->style->node.color);

  else if(map->style->node.show_untagged || node->has_tag())
    map_node_new(map, node,
		 map->style->node.radius * map->state->detail, 0,
		 map->style->node.color, 0);
}

void map_node_draw(map_t *map, node_t *node) {
  map_node_draw_functor m(map);
  m(node);
}

static void map_item_draw(map_t *map, map_item_t *map_item) {
  switch(map_item->object.type) {
  case NODE:
    map_node_draw(map, map_item->object.node);
    break;
  case WAY:
    map_way_draw(map, map_item->object.way);
    break;
  default:
    g_assert_not_reached();
  }
}

static void map_item_remove(map_item_t *map_item) {
  map_item_chain_t **chainP = NULL;

  switch(map_item->object.type) {
  case NODE:
    chainP = &map_item->object.node->map_item_chain;
    break;
  case WAY:
    chainP = &map_item->object.way->map_item_chain;
    break;
  default:
    g_assert_not_reached();
  }

  map_item_chain_destroy(chainP);
}

static void map_item_init(style_t *style, map_item_t *map_item) {
  switch (map_item->object.type){
    case WAY:
      josm_elemstyles_colorize_way(style, map_item->object.way);
      break;
    case NODE:
      josm_elemstyles_colorize_node(style, map_item->object.node);
      break;
    default:
      g_assert_not_reached();
  }
}

void map_item_redraw(appdata_t *appdata, map_item_t *map_item) {
  map_item_t item = *map_item;

  /* a relation cannot be redraws as it doesn't have a visual */
  /* representation */
  if(map_item->object.type == RELATION)
    return;

  /* check if the item to be redrawn is the selected one */
  gboolean is_selected = FALSE;
  if(map_item->object.obj == appdata->map->selected.object.obj) {
    map_item_deselect(appdata);
    is_selected = TRUE;
  }

  map_item_remove(&item);
  map_item_init(appdata->map->style, &item);
  map_item_draw(appdata->map, &item);

  /* restore selection if there was one */
  if(is_selected)
    map_object_select(appdata, item.object);
}

static void map_frisket_rectangle(canvas_points_t *points,
				  gint x0, gint x1, gint y0, gint y1) {
  points->coords[2*0+0] = points->coords[2*3+0] = points->coords[2*4+0] = x0;
  points->coords[2*1+0] = points->coords[2*2+0] = x1;
  points->coords[2*0+1] = points->coords[2*1+1] = points->coords[2*4+1] = y0;
  points->coords[2*2+1] = points->coords[2*3+1] = y1;
}

/* Draw the frisket area which masks off areas it'd be unsafe to edit,
 * plus its inner edge marker line */
static void map_frisket_draw(map_t *map, const bounds_t *bounds) {
  canvas_points_t *points = canvas_points_new(5);

  /* don't draw frisket at all if it's completely transparent */
  if(map->style->frisket.color & 0xff) {
    elemstyle_color_t color = map->style->frisket.color;

    float mult = map->style->frisket.mult;

    /* top rectangle */
    map_frisket_rectangle(points, mult*bounds->min.x, mult*bounds->max.x,
			  mult*bounds->min.y, bounds->min.y);
    canvas_polygon_new(map->canvas, CANVAS_GROUP_FRISKET, points,
		       1, NO_COLOR, color);

    /* bottom rectangle */
    map_frisket_rectangle(points, mult*bounds->min.x, mult*bounds->max.x,
			  bounds->max.y, mult*bounds->max.y);
    canvas_polygon_new(map->canvas, CANVAS_GROUP_FRISKET, points,
		       1, NO_COLOR, color);

    /* left rectangle */
    map_frisket_rectangle(points, mult*bounds->min.x, bounds->min.x,
			  mult*bounds->min.y, mult*bounds->max.y);
    canvas_polygon_new(map->canvas, CANVAS_GROUP_FRISKET, points,
		       1, NO_COLOR, color);

    /* right rectangle */
    map_frisket_rectangle(points, bounds->max.x, mult*bounds->max.x,
			  mult*bounds->min.y, mult*bounds->max.y);
    canvas_polygon_new(map->canvas, CANVAS_GROUP_FRISKET, points,
		       1, NO_COLOR, color);

  }

  if(map->style->frisket.border.present) {
    // Edge marker line
    gint ew2 = map->style->frisket.border.width/2;
    map_frisket_rectangle(points,
			  bounds->min.x-ew2, bounds->max.x+ew2,
			  bounds->min.y-ew2, bounds->max.y+ew2);

    canvas_polyline_new(map->canvas, CANVAS_GROUP_FRISKET, points,
			map->style->frisket.border.width,
			map->style->frisket.border.color);

  }
  canvas_points_free(points);
}

static void map_draw(map_t *map, osm_t *osm) {
  g_assert(map->canvas);

  printf("drawing ways ...\n");
  std::for_each(osm->ways.begin(), osm->ways.end(), map_way_draw_functor(map));

  printf("drawing single nodes ...\n");
  std::for_each(osm->nodes.begin(), osm->nodes.end(), map_node_draw_functor(map));

  printf("drawing frisket...\n");
  map_frisket_draw(map, osm->bounds);
}

void map_state_free(map_state_t *state) {
  if(!state) return;

  /* free state of noone else references it */
  if(state->refcount > 1)
    state->refcount--;
  else
    delete state;
}

template<typename T> void free_map_item_chain(std::pair<item_id_t, T *> pair) {
  delete pair.second->map_item_chain;
  pair.second->map_item_chain = NULL;
}

template<bool b> void free_track_item_chain(track_seg_t &seg) {
  if(b)
    std::for_each(seg.item_chain.begin(), seg.item_chain.end(), canvas_item_destroy);
  seg.item_chain.clear();
}

void map_free_map_item_chains(appdata_t *appdata) {
  if(!appdata->osm) return;

#ifndef DESTROY_WAIT_FOR_GTK
  /* free all map_item_chains */
  std::for_each(appdata->osm->nodes.begin(), appdata->osm->nodes.end(),
                free_map_item_chain<node_t>);

  std::for_each(appdata->osm->ways.begin(), appdata->osm->ways.end(),
                free_map_item_chain<way_t>);

  if (appdata->track.track) {
    /* remove all segments */
    std::for_each(appdata->track.track->segments.begin(),
                  appdata->track.track->segments.end(),
                  free_track_item_chain<false>);
  }
#endif
}

static gint map_destroy_event(G_GNUC_UNUSED GtkWidget *widget, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  map_t *map = appdata->map;

  gtk_timeout_remove(map->autosave_handler_id);

  printf("destroying entire map\n");

  map_free_map_item_chains(appdata);

  appdata->map = NULL;
  delete map;

  return FALSE;
}

/* get the item at position x, y */
map_item_t *map_item_at(map_t *map, gint x, gint y) {
  printf("map check at %d/%d\n", x, y);

  canvas_window2world(map->canvas, x, y, &x, &y);

  printf("world check at %d/%d\n", x, y);

  canvas_item_t *item = canvas_get_item_at(map->canvas, x, y);

  if(!item) {
    printf("  there's no item\n");
    return NULL;
  }

  printf("  there's an item (%p)\n", item);

  map_item_t *map_item = (map_item_t*)canvas_item_get_user_data(item);

  if(!map_item) {
    printf("  item has no user data!\n");
    return NULL;
  }

  if(map_item->highlight)
    printf("  item is highlight\n");

  printf("  item is %s #" ITEM_ID_FORMAT "\n",
	 map_item->object.type_string(),
	 map_item->object.obj->id);

  return map_item;
}

/* get the real item (no highlight) at x, y */
map_item_t *map_real_item_at(map_t *map, gint x, gint y) {
  map_item_t *map_item = map_item_at(map, x, y);

  /* no item or already a real one */
  if(!map_item || !map_item->highlight) return map_item;

  /* get the item (parent) this item is the highlight of */
  map_item_t *parent = NULL;
  switch(map_item->object.type) {

  case NODE:
    if(map_item->object.node->map_item_chain)
      parent = map_item->object.node->map_item_chain->firstItem();

    if(parent)
      printf("  using parent item node #" ITEM_ID_FORMAT "\n",
	     parent->object.obj->id);
    break;

  case WAY:
    if(map_item->object.way->map_item_chain)
      parent = map_item->object.way->map_item_chain->firstItem();

    if(parent)
      printf("  using parent item way #" ITEM_ID_FORMAT "\n",
	     parent->object.obj->id);
    break;

  default:
    g_assert_not_reached();
    break;
  }

  if(parent)
    map_item = parent;
  else
    printf("  no parent, working on highlight itself\n");

  return map_item;
}

/* Limitations on the amount by which we can scroll. Keeps part of the
 * map visible at all times */
static void map_limit_scroll(map_t *map, canvas_unit_t unit,
			     gint *sx, gint *sy) {

  /* get scale factor for pixel->meter conversion. set to 1 if */
  /* given coordinates are already in meters */
  gdouble scale = (unit == CANVAS_UNIT_METER)?1.0:canvas_get_zoom(map->canvas);

  /* convert pixels to meters if necessary */
  gdouble sx_cu = *sx / scale;
  gdouble sy_cu = *sy / scale;

  /* get size of visible area in canvas units (meters) */
  gint aw_cu = canvas_get_viewport_width(map->canvas, CANVAS_UNIT_METER)/2;
  gint ah_cu = canvas_get_viewport_height(map->canvas, CANVAS_UNIT_METER)/2;

  // Data rect minimum and maximum
  gint min_x, min_y, max_x, max_y;
  min_x = map->appdata->osm->bounds->min.x;
  min_y = map->appdata->osm->bounds->min.y;
  max_x = map->appdata->osm->bounds->max.x;
  max_y = map->appdata->osm->bounds->max.y;

  // limit stops - prevent scrolling beyond these
  gint min_sy_cu = 0.95*(min_y - ah_cu);
  gint min_sx_cu = 0.95*(min_x - aw_cu);
  gint max_sy_cu = 0.95*(max_y + ah_cu);
  gint max_sx_cu = 0.95*(max_x + aw_cu);
  if (sy_cu < min_sy_cu) { *sy = min_sy_cu * scale; }
  if (sx_cu < min_sx_cu) { *sx = min_sx_cu * scale; }
  if (sy_cu > max_sy_cu) { *sy = max_sy_cu * scale; }
  if (sx_cu > max_sx_cu) { *sx = max_sx_cu * scale; }
}


/* Limit a proposed zoom factor to sane ranges.
 * Specifically the map is allowed to be no smaller than the viewport. */
static gboolean map_limit_zoom(map_t *map, gdouble *zoom) {
    // Data rect minimum and maximum
    gint min_x, min_y, max_x, max_y;
    min_x = map->appdata->osm->bounds->min.x;
    min_y = map->appdata->osm->bounds->min.y;
    max_x = map->appdata->osm->bounds->max.x;
    max_y = map->appdata->osm->bounds->max.y;

    /* get size of visible area in pixels and convert to meters of intended */
    /* zoom by deviding by zoom (which is basically pix/m) */
    gint aw_cu =
      canvas_get_viewport_width(map->canvas, CANVAS_UNIT_PIXEL) / *zoom;
    gint ah_cu =
      canvas_get_viewport_height(map->canvas, CANVAS_UNIT_PIXEL) / *zoom;

    gdouble oldzoom = *zoom;
    if (ah_cu < aw_cu) {
        gint lim_h = ah_cu*0.95;
        if (max_y-min_y < lim_h) {
            gdouble corr = ((gdouble)max_y-min_y) / (gdouble)lim_h;
            *zoom /= corr;
        }
    }
    else {
        gint lim_w = aw_cu*0.95;
        if (max_x-min_x < lim_w) {
            gdouble corr = ((gdouble)max_x-min_x) / (gdouble)lim_w;
            *zoom /= corr;
        }
    }
    if (*zoom != oldzoom) {
        printf("Can't zoom further out (%f)\n", *zoom);
        return 1;
    }
    return 0;
}


/*
 * Scroll the map to a point if that point is currently offscreen.
 * Return true if this was possible, false if position is outside
 * working area
 */
gboolean map_scroll_to_if_offscreen(map_t *map, const lpos_t *lpos) {

  // Ignore anything outside the working area
  if (!(map && map->appdata && map->appdata->osm)) {
    return FALSE;
  }
  gint min_x, min_y, max_x, max_y;
  min_x = map->appdata->osm->bounds->min.x;
  min_y = map->appdata->osm->bounds->min.y;
  max_x = map->appdata->osm->bounds->max.x;
  max_y = map->appdata->osm->bounds->max.y;
  if (   (lpos->x > max_x) || (lpos->x < min_x)
      || (lpos->y > max_y) || (lpos->y < min_y)) {
    printf("cannot scroll to (%d, %d): outside the working area\n",
	   lpos->x, lpos->y);
    return FALSE;
  }

  // Viewport dimensions in canvas space

  /* get size of visible area in canvas units (meters) */
  gdouble pix_per_meter = canvas_get_zoom(map->canvas);
  gdouble aw = canvas_get_viewport_width(map->canvas, CANVAS_UNIT_METER);
  gdouble ah = canvas_get_viewport_height(map->canvas, CANVAS_UNIT_METER);

  // Is the point still onscreen?
  gboolean vert_recentre_needed = FALSE;
  gboolean horiz_recentre_needed = FALSE;
  gint sx, sy;
  canvas_scroll_get(map->canvas, CANVAS_UNIT_METER, &sx, &sy);
  gint viewport_left   = sx-aw/2;
  gint viewport_right  = sx+aw/2;
  gint viewport_top    = sy-ah/2;
  gint viewport_bottom = sy+ah/2;

  if (lpos->x > viewport_right) {
    printf("** off right edge (%d > %d)\n", lpos->x, viewport_right);
    horiz_recentre_needed = TRUE;
  }
  if (lpos->x < viewport_left) {
    printf("** off left edge (%d < %d)\n", lpos->x, viewport_left);
    horiz_recentre_needed = TRUE;
  }
  if (lpos->y > viewport_bottom) {
    printf("** off bottom edge (%d > %d)\n", lpos->y, viewport_bottom);
    vert_recentre_needed = TRUE;
  }
  if (lpos->y < viewport_top) {
    printf("** off top edge (%d < %d)\n", lpos->y, viewport_top);
    vert_recentre_needed = TRUE;
  }

  if (horiz_recentre_needed || vert_recentre_needed) {
    gint new_sx, new_sy;

    // Just centre both at once
    new_sx = pix_per_meter * lpos->x; // XXX (lpos->x - (aw/2));
    new_sy = pix_per_meter * lpos->y; // XXX (lpos->y - (ah/2));

    map_limit_scroll(map, CANVAS_UNIT_PIXEL, &new_sx, &new_sy);
    canvas_scroll_to(map->canvas, CANVAS_UNIT_PIXEL, new_sx, new_sy);
  }
  return TRUE;
}

/* Deselects the current way or node if its zoom_max
 * means that it's not going to render at the current map zoom. */
void map_deselect_if_zoom_below_zoom_max(map_t *map) {
    if (map->selected.object.type == WAY) {
        printf("will deselect way if zoomed below %f\n",
               map->selected.object.way->draw.zoom_max);
        if (map->state->zoom < map->selected.object.way->draw.zoom_max) {
            printf("  deselecting way!\n");
            map_item_deselect(map->appdata);
        }
    }
    else if (map->selected.object.type == NODE) {
        printf("will deselect node if zoomed below %f\n",
               map->selected.object.node->zoom_max);
        if (map->state->zoom < map->selected.object.node->zoom_max) {
            printf("  deselecting node!\n");
            map_item_deselect(map->appdata);
        }
    }
}

#define GPS_RADIUS_LIMIT  3.0

void map_set_zoom(map_t *map, double zoom,
		  gboolean update_scroll_offsets) {
  gboolean at_zoom_limit = 0;
  at_zoom_limit = map_limit_zoom(map, &zoom);

  map->state->zoom = zoom;
  canvas_set_zoom(map->canvas, map->state->zoom);

  map_deselect_if_zoom_below_zoom_max(map);

  if(update_scroll_offsets) {
    if (!at_zoom_limit) {
      /* zooming affects the scroll offsets */
      gint sx, sy;
      canvas_scroll_get(map->canvas, CANVAS_UNIT_PIXEL, &sx, &sy);
      map_limit_scroll(map, CANVAS_UNIT_PIXEL, &sx, &sy);

      // keep the map visible
      canvas_scroll_to(map->canvas, CANVAS_UNIT_PIXEL, sx, sy);
    }

    canvas_scroll_get(map->canvas, CANVAS_UNIT_METER,
		      &map->state->scroll_offset.x,
		      &map->state->scroll_offset.y);
  }

  if(map->appdata->track.gps_item) {
    float radius = map->style->track.width/2.0;
    if(zoom < GPS_RADIUS_LIMIT) {
      radius *= GPS_RADIUS_LIMIT;
      radius /= zoom;

      canvas_item_set_radius(map->appdata->track.gps_item, radius);
    }
  }
}

static gboolean map_scroll_event(G_GNUC_UNUSED GtkWidget *widget, GdkEventScroll *event,
				 gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  if(!appdata->osm) return FALSE;

  if(event->type == GDK_SCROLL && appdata->map && appdata->map->state) {
    if(event->direction)
      map_set_zoom(appdata->map,
		   appdata->map->state->zoom / ZOOM_FACTOR_WHEEL, TRUE);
    else
      map_set_zoom(appdata->map,
		   appdata->map->state->zoom * ZOOM_FACTOR_WHEEL, TRUE);
  }

  return TRUE;
}

static gboolean distance_above(map_t *map, gint x, gint y, gint limit) {
  gint sx, sy;

  /* add offsets generated by mouse within map and map scrolling */
  sx = (x-map->pen_down.at.x);
  sy = (y-map->pen_down.at.y);

  return(sx*sx + sy*sy > limit*limit);
}

/* scroll with respect to two screen positions */
static void map_do_scroll(map_t *map, gint x, gint y) {
  gint sx, sy;

  canvas_scroll_get(map->canvas, CANVAS_UNIT_PIXEL, &sx, &sy);
  sx -= x-map->pen_down.at.x;
  sy -= y-map->pen_down.at.y;
  map_limit_scroll(map, CANVAS_UNIT_PIXEL, &sx, &sy);
  canvas_scroll_to(map->canvas, CANVAS_UNIT_PIXEL, sx, sy);

  canvas_scroll_get(map->canvas, CANVAS_UNIT_METER,
		    &map->state->scroll_offset.x,
		    &map->state->scroll_offset.y);
}


/* scroll a certain step */
static void map_do_scroll_step(map_t *map, gint x, gint y) {
  gint sx, sy;
  canvas_scroll_get(map->canvas, CANVAS_UNIT_PIXEL, &sx, &sy);
  sx += x;
  sy += y;
  map_limit_scroll(map, CANVAS_UNIT_PIXEL, &sx, &sy);
  canvas_scroll_to(map->canvas, CANVAS_UNIT_PIXEL, sx, sy);

  canvas_scroll_get(map->canvas, CANVAS_UNIT_METER,
		    &map->state->scroll_offset.x,
		    &map->state->scroll_offset.y);
}

gboolean map_item_is_selected_node(map_t *map, map_item_t *map_item) {
  printf("check if item is a selected node\n");

  if(!map_item) {
    printf("  no item requested\n");
    return FALSE;
  }

  if(map->selected.object.type == ILLEGAL) {
    printf("  nothing is selected\n");
    return FALSE;
  }

  /* clicked the highlight directly */
  if(map_item->object.type != NODE) {
    printf("  didn't click node\n");
    return FALSE;
  }

  if(map->selected.object.type == NODE) {
    printf("  selected item is a node\n");

    if(map_item->object.node == map->selected.object.node) {
      printf("  requested item is a selected node\n");
      return TRUE;
    }
    printf("  but it's not the requested one\n");
    return FALSE;

  } else if(map->selected.object.type == WAY) {
    printf("  selected item is a way\n");

    if(map->selected.object.way->contains_node(map_item->object.node)) {
      printf("  requested item is part of selected way\n");
      return TRUE;
    }
    printf("  but it doesn't include the requested node\n");
    return FALSE;

  } else {
    printf("  selected item is unknown\n");
    return FALSE;
  }
}

/* return true if the item given is the currenly selected way */
/* also return false if nothing is selected or the selection is no way */
gboolean map_item_is_selected_way(map_t *map, map_item_t *map_item) {
  printf("check if item is the selected way\n");

  if(!map_item) {
    printf("  no item requested\n");
    return FALSE;
  }

  if(map->selected.object.type == ILLEGAL) {
    printf("  nothing is selected\n");
    return FALSE;
  }

  /* clicked the highlight directly */
  if(map_item->object.type != WAY) {
    printf("  didn't click way\n");
    return FALSE;
  }

  if(map->selected.object.type == WAY) {
    printf("  selected item is a way\n");

    if(map_item->object.way == map->selected.object.way) {
      printf("  requested item is a selected way\n");
      return TRUE;
    }
    printf("  but it's not the requested one\n");
    return FALSE;
  }

  printf("  selected item is not a way\n");
  return FALSE;
}


void map_highlight_refresh(appdata_t *appdata) {
  map_t *map = appdata->map;
  object_t old = map->selected.object;

  printf("type to refresh is %d\n", old.type);
  if(old.type == ILLEGAL)
    return;

  map_item_deselect(appdata);
  map_object_select(appdata, old);
}

void map_way_delete(appdata_t *appdata, way_t *way) {
  printf("deleting way #" ITEM_ID_FORMAT " from map and osm\n", way->id);

  undo_append_way(appdata, UNDO_DELETE, way);

  /* remove it visually from the screen */
  map_item_chain_destroy(&way->map_item_chain);

  /* and mark it "deleted" in the database */
  appdata->osm->remove_from_relations(way);

  appdata->osm->way_delete(way, false);
}

static void map_handle_click(appdata_t *appdata, map_t *map) {

  /* problem: on_item may be the highlight itself! So store it! */
  map_item_t map_item;
  if(map->pen_down.on_item) map_item = *map->pen_down.on_item;
  else                      map_item.object.type = ILLEGAL;

  /* if we aready have something selected, then de-select it */
  map_item_deselect(appdata);

  /* select the clicked item (if there was one) */
  if(map_item.object.type != ILLEGAL) {
    switch(map_item.object.type) {
    case NODE:
      map_node_select(appdata, map_item.object.node);
      break;

    case WAY:
      map_way_select(appdata, map_item.object.way);
      break;

    default:
      g_assert_not_reached();
      break;
    }
  }
}

struct hl_nodes {
  const node_t * const cur_node;
  const gint x, y;
  map_t * const map;
  hl_nodes(const node_t *c, gint px, gint py, map_t *m)
    : cur_node(c), x(px), y(py), map(m) {}
  void operator()(const std::pair<item_id_t, node_t *> &p);
  void operator()(node_t *node);
};

void hl_nodes::operator()(const std::pair<item_id_t, node_t *> &p)
{
  node_t * const node = p.second;

  if((node != cur_node) && (!(node->flags & OSM_FLAG_DELETED)))
    operator()(node);
}

void hl_nodes::operator()(node_t* node)
{
  gint nx = abs(x - node->lpos.x);
  gint ny = abs(y - node->lpos.y);

  if((nx < map->style->node.radius) && (ny < map->style->node.radius) &&
     (nx*nx + ny*ny < map->style->node.radius * map->style->node.radius))
    map_hl_touchnode_draw(map, node);
}

static void map_touchnode_update(appdata_t *appdata, gint x, gint y) {
  map_t *map = appdata->map;

  map_hl_touchnode_clear(map);

  const node_t *cur_node = NULL;

  /* the "current node" which is the one we are working on and which */
  /* should not be highlighted depends on the action */
  switch(map->action.type) {

    /* in idle mode the dragged node is not highlighted */
  case MAP_ACTION_IDLE:
    g_assert(map->pen_down.on_item);
    g_assert(map->pen_down.on_item->object.type == NODE);
    cur_node = map->pen_down.on_item->object.node;
    break;

  default:
    break;
  }

  /* check if we are close to one of the other nodes */
  canvas_window2world(appdata->map->canvas, x, y, &x, &y);
  hl_nodes fc(cur_node, x, y, map);
  std::for_each(appdata->osm->nodes.begin(), appdata->osm->nodes.end(), fc);

  /* during way creation also nodes of the new way */
  /* need to be searched */
  if(!map->touchnode && map->action.way && map->action.way->node_chain.size() > 1) {
    const node_chain_t &chain = map->action.way->node_chain;
    std::for_each(chain.begin(), chain.end() - 1, fc);
  }
}

static void map_button_press(map_t *map, gint x, gint y) {

  printf("left button pressed\n");
  map->pen_down.is = TRUE;

  /* save press position */
  map->pen_down.at.x = x;
  map->pen_down.at.y = y;
  map->pen_down.drag = FALSE;     // don't assume drag yet

  /* determine wether this press was on an item */
  map->pen_down.on_item = map_real_item_at(map, x, y);

  /* check if the clicked item is a highlighted node as the user */
  /* might want to drag that */
  map->pen_down.on_selected_node = FALSE;
  if(map->pen_down.on_item)
    map->pen_down.on_selected_node =
      map_item_is_selected_node(map, map->pen_down.on_item);

  /* button press */
  switch(map->action.type) {

  case MAP_ACTION_WAY_NODE_ADD:
    map_edit_way_node_add_highlight(map, map->pen_down.on_item, x, y);
    break;

  case MAP_ACTION_WAY_CUT:
    map_edit_way_cut_highlight(map, map->pen_down.on_item, x, y);
    break;

  case MAP_ACTION_NODE_ADD:
    map_hl_cursor_draw(map, x, y, false, map->style->node.radius);
    break;

  case MAP_ACTION_WAY_ADD:
    map_hl_cursor_draw(map, x, y, false, map->style->node.radius);
    map_touchnode_update(map->appdata, x, y);
    break;

  default:
    break;
  }
}

/* move the background image (wms data) during wms adjustment */
static void map_bg_adjust(map_t *map, gint x, gint y) {
  g_assert(map->appdata);
  g_assert(map->appdata->osm);
  g_assert(map->appdata->osm->bounds);

  x += map->appdata->osm->bounds->min.x + map->bg.offset.x -
    map->pen_down.at.x;
  y += map->appdata->osm->bounds->min.y + map->bg.offset.y -
    map->pen_down.at.y;

  canvas_image_move(map->bg.item, x, y, map->bg.scale.x, map->bg.scale.y);
}

static void map_button_release(map_t *map, gint x, gint y) {
  map->pen_down.is = FALSE;

  /* before button release is handled */
  switch(map->action.type) {
  case MAP_ACTION_BG_ADJUST:
    map_bg_adjust(map, x, y);
    map->bg.offset.x += x - map->pen_down.at.x;
    map->bg.offset.y += y - map->pen_down.at.y;
    break;

  case MAP_ACTION_IDLE:
    /* check if distance to press is above drag limit */
    if(!map->pen_down.drag)
      map->pen_down.drag = distance_above(map, x, y, MAP_DRAG_LIMIT);

    if(!map->pen_down.drag) {
      printf("left button released after click\n");

      map_item_t old_sel = map->selected;
      map_handle_click(map->appdata, map);

      if((old_sel.object.type != ILLEGAL) &&
	 (old_sel.object == map->selected.object)) {
	printf("re-selected same item of type %d, "
	       "pushing it to the bottom\n", old_sel.object.type);

	if(!map->selected.item) {
	  printf("  item has no visible representation to push\n");
	} else {
	  canvas_item_to_bottom(map->selected.item);

	  /* update clicked item, to correctly handle the click */
	  map->pen_down.on_item =
	    map_real_item_at(map, map->pen_down.at.x, map->pen_down.at.y);

	  map_handle_click(map->appdata, map);
	}
      }
    } else {
      printf("left button released after drag\n");

      /* just scroll if we didn't drag an selected item */
      if(!map->pen_down.on_selected_node)
	map_do_scroll(map, x, y);
      else {
	printf("released after dragging node\n");
	map_hl_cursor_clear(map);

	/* now actually move the node */
	map_edit_node_move(map->appdata, map->pen_down.on_item, x, y);
      }
    }
    break;

  case MAP_ACTION_NODE_ADD: {
    printf("released after NODE ADD\n");
    map_hl_cursor_clear(map);

    /* convert mouse position to canvas (world) position */
    canvas_window2world(map->canvas, x, y, &x, &y);

    node_t *node = NULL;
    if(!map->appdata->osm->position_within_bounds(x, y))
      map_outside_error(map->appdata);
    else {
      node = map->appdata->osm->node_new(x, y);
      map->appdata->osm->node_attach(node);
      map_node_draw(map, node);
    }
    map_action_set(map->appdata, MAP_ACTION_IDLE);

    map_item_deselect(map->appdata);

    if(node) {
      map_node_select(map->appdata, node);

      /* let the user specify some tags for the new node */
      info_dialog(GTK_WIDGET(map->appdata->window), map->appdata, NULL);
    }
    break;
  }
  case MAP_ACTION_WAY_ADD:
    printf("released after WAY ADD\n");
    map_hl_cursor_clear(map);

    map_edit_way_add_segment(map, x, y);
    break;

  case MAP_ACTION_WAY_NODE_ADD:
    printf("released after WAY NODE ADD\n");
    map_hl_cursor_clear(map);

    map_edit_way_node_add(map, x, y);
    break;

  case MAP_ACTION_WAY_CUT:
    printf("released after WAY CUT\n");
    map_hl_cursor_clear(map);

    map_edit_way_cut(map, x, y);
    break;


  default:
    break;
  }
}

static gboolean map_button_event(G_GNUC_UNUSED GtkWidget *widget, GdkEventButton *event,
				       gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  map_t *map = appdata->map;

  if(!appdata->osm) return FALSE;

  if(event->button == 1) {
    gint x = event->x, y = event->y;

    if(event->type == GDK_BUTTON_PRESS)
      map_button_press(map, x, y);

    if(event->type == GDK_BUTTON_RELEASE)
      map_button_release(map, x, y);
  }

  return FALSE;  /* forward to further processing */
}

static gboolean map_motion_notify_event(G_GNUC_UNUSED GtkWidget *widget,
                             GdkEventMotion *event, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  map_t *map = appdata->map;
  gint x, y;
  GdkModifierType state;

  if(!appdata->osm) return FALSE;

#if 0 // def USE_HILDON
  /* reduce update frequency on hildon to keep screen update fluid */
  static guint32 last_time = 0;

  if(event->time - last_time < 250) return FALSE;
  last_time = event->time;
#endif

  if(gtk_events_pending())
    return FALSE;

  if(!map->pen_down.is)
    return FALSE;

  /* handle hints */
  if(event->is_hint)
    gdk_window_get_pointer(event->window, &x, &y, &state);
  else {
    x = event->x;
    y = event->y;
    state = (GdkModifierType)event->state;
  }

  /* check if distance to press is above drag limit */
  if(!map->pen_down.drag)
    map->pen_down.drag = distance_above(map, x, y, MAP_DRAG_LIMIT);

  /* drag */
  switch(map->action.type) {
  case MAP_ACTION_BG_ADJUST:
    map_bg_adjust(map, x, y);
    break;

  case MAP_ACTION_IDLE:
    if(map->pen_down.drag) {
      /* just scroll if we didn't drag an selected item */
      if(!map->pen_down.on_selected_node)
	map_do_scroll(map, x, y);
      else {
	map_hl_cursor_draw(map, x, y, false, map->style->node.radius);
	map_touchnode_update(appdata, x, y);
      }
    }
    break;

  case MAP_ACTION_NODE_ADD:
    map_hl_cursor_draw(map, x, y, false, map->style->node.radius);
    break;

  case MAP_ACTION_WAY_ADD:
    map_hl_cursor_draw(map, x, y, false, map->style->node.radius);
    map_touchnode_update(appdata, x, y);
    break;

  case MAP_ACTION_WAY_NODE_ADD: {
    map_hl_cursor_clear(map);
    map_item_t *item = map_item_at(map, x, y);
    if(item) map_edit_way_node_add_highlight(map, item, x, y);
    break;
  }

  case MAP_ACTION_WAY_CUT: {
    map_hl_cursor_clear(map);
    map_item_t *item = map_item_at(map, x, y);
    if(item) map_edit_way_cut_highlight(map, item, x, y);
    break;
  }

  default:
    break;
  }


  return FALSE;  /* forward to further processing */
}

gboolean map_key_press_event(appdata_t *appdata, GdkEventKey *event) {

  if(!appdata->osm) return FALSE;

  /* map needs to be there to handle buttons */
  if(!appdata->map->canvas)
    return FALSE;

  if(event->type == GDK_KEY_PRESS) {
    gdouble zoom = 0;
    switch(event->keyval) {

    case GDK_Left:
      map_do_scroll_step(appdata->map, -50, 0);
      break;

    case GDK_Right:
      map_do_scroll_step(appdata->map, +50, 0);
      break;

    case GDK_Up:
      map_do_scroll_step(appdata->map, 0, -50);
      break;

    case GDK_Down:
      map_do_scroll_step(appdata->map, 0, +50);
      break;

    case GDK_Return:   // same as HILDON_HARDKEY_SELECT
      /* if the ok button is enabled, call its function */
      if(GTK_WIDGET_FLAGS(appdata->iconbar->ok) & GTK_SENSITIVE)
	map_action_ok(appdata);
      /* otherwise if info is enabled call that */
      else if(GTK_WIDGET_FLAGS(appdata->iconbar->info) & GTK_SENSITIVE)
	info_dialog(GTK_WIDGET(appdata->window), appdata, NULL);
      break;

    case GDK_Escape:   // same as HILDON_HARDKEY_ESC
      /* if the cancel button is enabled, call its function */
      if(GTK_WIDGET_FLAGS(appdata->iconbar->cancel) & GTK_SENSITIVE)
	map_action_cancel(appdata);
      break;

    case GDK_Delete:
      /* if the delete button is enabled, call its function */
      if(GTK_WIDGET_FLAGS(appdata->iconbar->trash) & GTK_SENSITIVE)
	map_delete_selected(appdata);
      break;

#ifdef USE_HILDON
    case HILDON_HARDKEY_INCREASE:
#else
    case '+':
    case GDK_KP_Add:
#endif
      zoom = appdata->map->state->zoom;
      zoom *= ZOOM_FACTOR_BUTTON;
      map_set_zoom(appdata->map, zoom, TRUE);
      printf("zoom is now %f (1:%d)\n", zoom, (int)zoom_to_scaledn(zoom));
      return TRUE;
      break;

#ifdef USE_HILDON
    case HILDON_HARDKEY_DECREASE:
#else
    case '-':
    case GDK_KP_Subtract:
#endif
      zoom = appdata->map->state->zoom;
      zoom /= ZOOM_FACTOR_BUTTON;
      map_set_zoom(appdata->map, zoom, TRUE);
      printf("zoom is now %f (1:%d)\n", zoom, (int)zoom_to_scaledn(zoom));
      return TRUE;
      break;

    default:
      printf("key event %d\n", event->keyval);
      break;
    }
  }
  return FALSE;
}

static gboolean map_autosave(gpointer data) {
  map_t *map = (map_t*)data;

  /* only do this if root window has focus as otherwise */
  /* a dialog may be open and modifying the basic structures */
  if(gtk_window_is_active(GTK_WINDOW(map->appdata->window))) {
    printf("autosave ...\n");

    if(map->appdata->project && map->appdata->osm) {
      track_save(map->appdata->project, map->appdata->track.track);
      diff_save(map->appdata->project, map->appdata->osm);
      //      banner_show_info(map->appdata, _("Autosave"));
    }
  } else
    printf("autosave supressed\n");

  return TRUE;
}

GtkWidget *map_new(appdata_t *appdata) {
  map_t *map = new map_t();

  map->style = style_load(appdata);
  if(!map->style) {
    errorf(NULL, _("Unable to load valid style, terminating."));
    delete map;
    return NULL;
  }
  appdata->map = map;

  if(appdata->project && appdata->project->map_state) {
    printf("Using projects map state\n");
    map->state = appdata->project->map_state;
    map->state->refcount++;
  } else {
    printf("Creating new map state\n");
    map->state = new map_state_t();
  }


  map->pen_down.at.x = -1;
  map->pen_down.at.y = -1;
  map->appdata = appdata;
  map->action.type = MAP_ACTION_IDLE;

  map->canvas = canvas_new();

  GtkWidget *canvas_widget = canvas_get_widget(map->canvas);

  gtk_widget_set_events(canvas_widget,
                          GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK
			| GDK_SCROLL_MASK
			| GDK_POINTER_MOTION_MASK
			| GDK_POINTER_MOTION_HINT_MASK);

  /* autosave happens every two minutes */
  map->autosave_handler_id = gtk_timeout_add(120*1000, map_autosave, map);

  gtk_signal_connect(GTK_OBJECT(canvas_widget),
     "button_press_event", G_CALLBACK(map_button_event), appdata);
  gtk_signal_connect(GTK_OBJECT(canvas_widget),
     "button_release_event", G_CALLBACK(map_button_event), appdata);
  gtk_signal_connect(GTK_OBJECT(canvas_widget),
     "motion_notify_event", G_CALLBACK(map_motion_notify_event), appdata);
  gtk_signal_connect(GTK_OBJECT(canvas_widget),
     "scroll_event", G_CALLBACK(map_scroll_event), appdata);

  gtk_signal_connect(GTK_OBJECT(canvas_widget),
     "destroy", G_CALLBACK(map_destroy_event), appdata);

  return canvas_widget;
}

void map_init(appdata_t *appdata) {
  map_t *map = appdata->map;

  /* update canvas background color */
  canvas_set_background(map->canvas, map->style->background.color);

  /* set initial zoom */
  map_set_zoom(map, map->state->zoom, FALSE);
  josm_elemstyles_colorize_world(map->style, appdata->osm);

  map_draw(map, appdata->osm);

  float mult = appdata->map->style->frisket.mult;
  canvas_set_bounds(map->canvas,
		    mult*appdata->osm->bounds->min.x,
		    mult*appdata->osm->bounds->min.y,
		    mult*appdata->osm->bounds->max.x,
		    mult*appdata->osm->bounds->max.y);

  printf("restore scroll position %d/%d\n",
	 map->state->scroll_offset.x, map->state->scroll_offset.y);

  map_limit_scroll(map, CANVAS_UNIT_METER,
	   &map->state->scroll_offset.x, &map->state->scroll_offset.y);
  canvas_scroll_to(map->canvas, CANVAS_UNIT_METER,
	   map->state->scroll_offset.x, map->state->scroll_offset.y);
}


void map_clear(appdata_t *appdata, gint group_mask) {
  map_t *map = appdata->map;

  printf("freeing map contents\n");

  if(group_mask == MAP_LAYER_ALL)
    map_track_remove_pos(appdata);

  map_free_map_item_chains(appdata);

  /* remove a possibly existing highlight */
  map_item_deselect(appdata);

  canvas_erase(map->canvas, group_mask);
}

void map_paint(appdata_t *appdata) {
  map_t *map = appdata->map;

  josm_elemstyles_colorize_world(map->style, appdata->osm);
  map_draw(map, appdata->osm);
}

/* called from several icons like e.g. "node_add" */
void map_action_set(appdata_t *appdata, map_action_t action) {
  printf("map action set to %d\n", action);

  appdata->map->action.type = action;

  /* enable/disable ok/cancel buttons */
  // MAP_ACTION_IDLE=0, NODE_ADD, BG_ADJUST, WAY_ADD, WAY_NODE_ADD, WAY_CUT
  const gboolean ok_state[] = { FALSE, TRUE, TRUE, FALSE, FALSE, FALSE };
  const gboolean cancel_state[] = { FALSE, TRUE, TRUE, TRUE, TRUE, TRUE };

  g_assert_cmpint(MAP_ACTION_NUM, ==, sizeof(ok_state)/sizeof(gboolean));
  g_assert_cmpint(action, <, sizeof(ok_state)/sizeof(gboolean));

  icon_bar_map_cancel_ok(appdata, cancel_state[action], ok_state[action]);

  switch(action) {
  case MAP_ACTION_BG_ADJUST:
    /* an existing selection only causes confusion ... */
    map_item_deselect(appdata);
    break;

  case MAP_ACTION_WAY_ADD: {
    printf("starting new way\n");

    /* remember if there was a way selected */
    way_t *way_sel = NULL;
    if(appdata->map->selected.object.type == WAY)
      way_sel = appdata->map->selected.object.way;

    map_item_deselect(appdata);
    map_edit_way_add_begin(appdata->map, way_sel);
    break;
  }

  case MAP_ACTION_NODE_ADD:
    map_item_deselect(appdata);
    break;

  default:
    break;
  }

  icon_bar_map_action_idle(appdata, action == MAP_ACTION_IDLE);
  gtk_widget_set_sensitive(appdata->menu_item_wms_adjust,
			   action == MAP_ACTION_IDLE);

  const char *str_state[] = {
    NULL,
    _("Place a node"),
    _("Adjust background image position"),
    _("Place first node of new way"),
    _("Place node on selected way"),
    _("Select segment to cut way"),
  };

  g_assert_cmpint(MAP_ACTION_NUM, ==, sizeof(str_state)/sizeof(char*));

  statusbar_set(appdata, str_state[action], FALSE);
}


void map_action_cancel(appdata_t *appdata) {
  map_t *map = appdata->map;

  switch(map->action.type) {
  case MAP_ACTION_WAY_ADD:
    map_edit_way_add_cancel(map);
    break;

  case MAP_ACTION_BG_ADJUST: {
    /* undo all changes to bg_offset */
    map->bg.offset.x = appdata->project->wms_offset.x;
    map->bg.offset.y = appdata->project->wms_offset.y;

    gint x = appdata->osm->bounds->min.x + map->bg.offset.x;
    gint y = appdata->osm->bounds->min.y + map->bg.offset.y;
    canvas_image_move(map->bg.item, x, y, map->bg.scale.x, map->bg.scale.y);
    break;
  }

  default:
    break;
  }

  map_action_set(appdata, MAP_ACTION_IDLE);
}

void map_action_ok(appdata_t *appdata) {
  map_t *map = appdata->map;

  /* reset action now as this erases the statusbar and some */
  /* of the actions may set it */
  map_action_t type = map->action.type;
  map_action_set(appdata, MAP_ACTION_IDLE);

  switch(type) {
  case MAP_ACTION_WAY_ADD:
    map_edit_way_add_ok(map);
    break;

  case MAP_ACTION_BG_ADJUST:
    /* save changes to bg_offset in project */
    appdata->project->wms_offset.x = map->bg.offset.x;
    appdata->project->wms_offset.y = map->bg.offset.y;
    break;

  case MAP_ACTION_NODE_ADD:
    {
    pos_t pos;
    if(!gps_get_pos(appdata, &pos, NULL))
      break;

    node_t *node = NULL;

    if(!osm_position_within_bounds_ll(&appdata->osm->bounds->ll_min,
                                      &appdata->osm->bounds->ll_max, &pos)) {
      map_outside_error(appdata);
    } else {
      node = appdata->osm->node_new(&pos);
      appdata->osm->node_attach(node);
      map_node_draw(map, node);
    }
    map_action_set(appdata, MAP_ACTION_IDLE);

    map_item_deselect(appdata);

    if(node) {
      map_node_select(appdata, node);

      /* let the user specify some tags for the new node */
      info_dialog(GTK_WIDGET(appdata->window), appdata, NULL);
    }
    }

  default:
    break;
  }
}

struct node_deleted_from_ways {
  appdata_t * const appdata;
  node_deleted_from_ways(appdata_t *a) : appdata(a) { }
  void operator()(way_t *way);
};

/* redraw all affected ways */
void node_deleted_from_ways::operator()(way_t *way) {
  if(way->node_chain.size() == 1) {
    /* this way now only contains one node and thus isn't a valid */
    /* way anymore. So it'll also get deleted (which in turn may */
    /* cause other nodes to be deleted as well) */
    map_way_delete(appdata, way);
  } else {
    map_item_t item;
    item.object = way;
    undo_append_object(appdata, UNDO_MODIFY, item.object);
    map_item_redraw(appdata, &item);
  }
}

static bool short_way(const way_t *way) {
  return way->node_chain.size() < 3;
}

/* called from icon "trash" */
void map_delete_selected(appdata_t *appdata) {
  map_t *map = appdata->map;

  if(!yes_no_f(GTK_WIDGET(appdata->window),
	       appdata, MISC_AGAIN_ID_DELETE, MISC_AGAIN_FLAG_DONT_SAVE_NO,
	       _("Delete selected object?"),
	       _("Do you really want to delete the selected object?")))
    return;

  /* work on local copy since de-selecting destroys the selection */
  map_item_t item = map->selected;

  /* deleting the selected item de-selects it ... */
  map_item_deselect(appdata);

  undo_open_new_state(appdata, UNDO_DELETE, item.object);

  switch(item.object.type) {
  case NODE: {
    printf("request to delete node #" ITEM_ID_FORMAT "\n",
	   item.object.obj->id);

    undo_append_object(appdata, UNDO_DELETE, item.object);

    /* check if this node is part of a way with two nodes only. */
    /* we cannot delete this as this would also delete the way */
    const way_chain_t &way_chain = appdata->osm->node_to_way(item.object.node);
    if(!way_chain.empty()) {

      const way_chain_t::const_iterator it =
          std::find_if(way_chain.begin(), way_chain.end(), short_way);

      if(it != way_chain.end()) {
	if(!yes_no_f(GTK_WIDGET(appdata->window), NULL, 0, 0,
		     _("Delete node in short way(s)?"),
		     _("Deleting this node will also delete one or more ways "
		       "since they'll contain only one node afterwards. "
		       "Do you really want this?")))
	  return;
      }
    }

    /* and mark it "deleted" in the database */
    appdata->osm->remove_from_relations(item.object.node);
    const way_chain_t &chain = appdata->osm->node_delete(
                                        item.object.node, false, true);
    std::for_each(chain.begin(), chain.end(), node_deleted_from_ways(appdata));

    break;
  }

  case WAY:
    printf("request to delete way #" ITEM_ID_FORMAT "\n",
	   item.object.obj->id);
    map_way_delete(appdata, item.object.way);
    break;

  default:
    g_assert_not_reached();
    break;
  }
  undo_close_state(appdata);
}

/* ----------------------- track related stuff ----------------------- */

static bool track_pos2lpos(const bounds_t *bounds, const pos_t &pos, lpos_t &lpos) {
  pos2lpos(bounds, &pos, &lpos);

  /* check if point is within bounds */
  return ((lpos.x >= bounds->min.x) && (lpos.x <= bounds->max.x) &&
          (lpos.y >= bounds->min.y) && (lpos.y <= bounds->max.y));
}

/**
 * @brief allocate a point array and initialize it with screen coordinates
 * @param bounds screen boundary
 * @param point first track point to use
 * @param count number of points to use
 * @return point array
 */
static canvas_points_t *canvas_points_init(const bounds_t *bounds,
                                           std::vector<track_point_t>::const_iterator point,
                                           const gint count) {
  canvas_points_t *points = canvas_points_new(count);
  lpos_t lpos;

  for(gint i = 0; i < count; i++) {
    track_pos2lpos(bounds, point->pos, lpos);
    canvas_point_set_pos(points, i, &lpos);
    point++;
  }

  return points;
}

void map_track_draw_seg(map_t *map, track_seg_t &seg) {
  const bounds_t *bounds = map->appdata->osm->bounds;

  /* a track_seg needs at least 2 points to be drawn */
  if (seg.track_points.empty())
    return;

  /* nothing should have been drawn by now ... */
  g_assert(seg.item_chain.empty());

  const std::vector<track_point_t>::const_iterator itEnd = seg.track_points.end();
  std::vector<track_point_t>::const_iterator it = seg.track_points.begin();
  while(it != itEnd) {
    lpos_t lpos;

    /* skip all points not on screen */
    std::vector<track_point_t>::const_iterator last = itEnd;
    while(it != itEnd && !track_pos2lpos(bounds, it->pos, lpos)) {
      last = it;
      it++;
    }

    int visible = 0;

    /* count nodes that _are_ on screen */
    std::vector<track_point_t>::const_iterator tmp = it;
    while(tmp != itEnd && track_pos2lpos(bounds, tmp->pos, lpos)) {
      tmp++;
      visible++;
    }

    /* actually start drawing with the last position that was offscreen */
    /* so the track nicely enters the viewing area */
    if(last != itEnd) {
      it = last;
      visible++;
    }

    /* also use last one that's offscreen to nicely leave the visible area */
    /* also determine the first item to use in the next loop */
    if(tmp != itEnd && tmp + 1 != itEnd) {
      visible++;
      tmp++;
    } else {
      tmp = itEnd;
    }

    /* allocate space for nodes */
    printf("visible are %d\n", visible);
    canvas_points_t *points = canvas_points_init(bounds, it, visible);
    it = tmp;

    canvas_item_t *item = canvas_polyline_new(map->canvas, CANVAS_GROUP_TRACK,
		 points, map->style->track.width, map->style->track.color);
    seg.item_chain.push_back(item);

    canvas_points_free(points);
  }
}

/* update the last visible fragment of this segment since a */
/* gps position may have been added */
void map_track_update_seg(map_t *map, track_seg_t &seg) {
  const bounds_t *bounds = map->appdata->osm->bounds;

  printf("-- APPENDING TO TRACK --\n");

  /* there are two cases: either the second last point was on screen */
  /* or it wasn't. We'll have to start a new screen item if the latter */
  /* is the case */

  /* search last point */
  const std::vector<track_point_t>::const_iterator itEnd = seg.track_points.end();
  std::vector<track_point_t>::const_iterator begin = itEnd - 2, // start of track to draw
                                             last = itEnd - 1;
  lpos_t lpos;
  /* check if the last and second_last points are visible */
  bool last_is_visible = track_pos2lpos(bounds, last->pos, lpos);
  bool second_last_is_visible = track_pos2lpos(bounds, begin->pos, lpos);

  /* if both are invisible, then nothing has changed on screen */
  if(!last_is_visible && !second_last_is_visible) {
    printf("second_last and last entry are invisible -> doing nothing\n");
    return;
  }

  const std::vector<track_point_t>::const_iterator itBegin = seg.track_points.begin();
  if(second_last_is_visible) {
    for(begin--; begin > itBegin; begin--) {
      // if this position is not visible start drawing here to get a
      // track entering the screen the correct way
      if(!track_pos2lpos(bounds, begin->pos, lpos))
        break;
    }
  }

  /* since we are updating an existing track, it sure has at least two */
  /* points, second_last must be valid and its "next" (last) also */
  g_assert(begin != itEnd);
  g_assert(last != itEnd);

  if(second_last_is_visible) {
    /* there must be something already on the screen and there must */
    /* be visible nodes in the chain */
    g_assert(!seg.item_chain.empty());

    printf("second_last is visible -> append\n");

    /* count points to be placed */
    gint npoints = itEnd - begin;

    printf("updating last segment to %d points\n", npoints);

    canvas_points_t *points = canvas_points_init(bounds, begin, npoints);

    canvas_item_t *item = seg.item_chain.back();
    canvas_item_set_points(item, points);
    canvas_points_free(points);

  } else {
    printf("second last is invisible -> start new screen segment\n");

    g_assert(begin + 1 == last);

    /* count points to be placed */
    gint npoints = itEnd - begin;

    printf("attaching new segment with %d points\n", npoints);

    canvas_points_t *points = canvas_points_init(bounds, begin, npoints);

    canvas_item_t *item = canvas_polyline_new(map->canvas, CANVAS_GROUP_TRACK,
		 points, map->style->track.width, map->style->track.color);
    seg.item_chain.push_back(item);

    canvas_points_free(points);
  }
}

struct map_track_seg_draw_functor {
  map_t * const map;
  map_track_seg_draw_functor(map_t *m) : map(m) {}
  void operator()(track_seg_t &seg) {
    map_track_draw_seg(map, seg);
  }
};

void map_track_draw(map_t *map, track_t *track) {
  std::for_each(track->segments.begin(), track->segments.end(),
                map_track_seg_draw_functor(map));
}

void map_track_remove(appdata_t *appdata) {
  track_t *track = appdata->track.track;

  printf("removing track\n");

  g_assert(track);

  /* remove all segments */
  std::for_each(track->segments.begin(), track->segments.end(),
                free_track_item_chain<true>);
}

/**
 * @brief show the marker item for the current GPS position
 */
void map_track_pos(appdata_t *appdata, const lpos_t *lpos) {
  /* remove the old item */
  map_track_remove_pos(appdata);

  float radius = appdata->map->style->track.width / 2.0;
  gdouble zoom = canvas_get_zoom(appdata->map->canvas);
  if(zoom < GPS_RADIUS_LIMIT) {
    radius *= GPS_RADIUS_LIMIT;
    radius /= zoom;
  }

  appdata->track.gps_item =
    canvas_circle_new(appdata->map->canvas, CANVAS_GROUP_GPS,
                      lpos->x, lpos->y, radius, 0,
                      appdata->map->style->track.gps_color, NO_COLOR);
}

/**
 * @brief remove the marker item for the current GPS position
 */
void map_track_remove_pos(appdata_t *appdata) {
  if(appdata->track.gps_item) {
    canvas_item_destroy(appdata->track.gps_item);
    appdata->track.gps_item = NULL;
  }
}

/* ------------------- map background ------------------ */

void map_remove_bg_image(map_t *map) {
  if(!map) return;

  if(map->bg.item) {
    canvas_item_destroy(map->bg.item);
    map->bg.item = NULL;
  }
}

static gint map_bg_item_destroy_event(G_GNUC_UNUSED GtkWidget *widget, gpointer data) {
  map_t *map = (map_t*)data;

  /* destroying background item */

  map->bg.item = NULL;
  if(map->bg.pix) {
    printf("destroying background item\n");
    g_object_unref(map->bg.pix);
    map->bg.pix = NULL;
  }
  return FALSE;
}

void map_set_bg_image(map_t *map, const char *filename) {
  const bounds_t *bounds = map->appdata->osm->bounds;

  map_remove_bg_image(map);

  map->bg.pix = gdk_pixbuf_new_from_file(filename, NULL);

  /* calculate required scale factor */
  map->bg.scale.x = (float)(bounds->max.x - bounds->min.x)/
    (float)gdk_pixbuf_get_width(map->bg.pix);
  map->bg.scale.y = (float)(bounds->max.y - bounds->min.y)/
    (float)gdk_pixbuf_get_height(map->bg.pix);

  map->bg.item = canvas_image_new(map->canvas, CANVAS_GROUP_BG, map->bg.pix,
	  bounds->min.x, bounds->min.y, map->bg.scale.x, map->bg.scale.y);

  canvas_item_destroy_connect(map->bg.item,
          G_CALLBACK(map_bg_item_destroy_event), map);
}


/* -------- hide and show objects (for performance reasons) ------- */

void map_hide_selected(appdata_t *appdata) {
  map_t *map = appdata->map;
  if(!map) return;

  if(map->selected.object.type != WAY) {
    printf("selected item is not a way\n");
    return;
  }

  way_t *way = map->selected.object.way;
  printf("hiding way #" ITEM_ID_FORMAT "\n", way->id);

  map_item_deselect(appdata);
  way->flags |= OSM_FLAG_HIDDEN;
  map_item_chain_destroy(&way->map_item_chain);

  gtk_widget_set_sensitive(appdata->menu_item_map_show_all, TRUE);
}

struct map_show_all_functor {
  map_t * const map;
  map_show_all_functor(map_t *m) : map(m) {}
  void operator()(std::pair<item_id_t, way_t *> pair);
};

void map_show_all_functor::operator()(std::pair<item_id_t, way_t *> pair)
{
  way_t * const way = pair.second;
  if(way->flags & OSM_FLAG_HIDDEN) {
    way->flags &= ~OSM_FLAG_HIDDEN;
    map_way_draw(map, way);
  }
}

void map_show_all(appdata_t *appdata) {
  map_t *map = appdata->map;
  if(!map) return;

  std::for_each(appdata->osm->ways.begin(), appdata->osm->ways.end(),
                map_show_all_functor(map));

  gtk_widget_set_sensitive(appdata->menu_item_map_show_all, FALSE);
}

void map_detail_change(map_t *map, float detail) {
  appdata_t *appdata = map->appdata;

  /* deselecting anything allows us not to care about automatic deselection */
  /* as well as items becoming invisible by the detail change */
  map_item_deselect(appdata);

  map->state->detail = detail;
  printf("changing detail factor to %f\n", map->state->detail);

  map_clear(appdata, MAP_LAYER_OBJECTS_ONLY);
  map_paint(appdata);
}

void map_detail_increase(map_t *map) {
  if(!map) return;
  banner_busy_start(map->appdata, 1, _("Increasing detail level"));
  map_detail_change(map, map->state->detail * MAP_DETAIL_STEP);
  banner_busy_stop(map->appdata);
}

void map_detail_decrease(map_t *map) {
  if(!map) return;
  banner_busy_start(map->appdata, 1, _("Decreasing detail level"));
  map_detail_change(map, map->state->detail / MAP_DETAIL_STEP);
  banner_busy_stop(map->appdata);
}

void map_detail_normal(map_t *map) {
  if(!map) return;
  banner_busy_start(map->appdata, 1, _("Restoring default detail level"));
  map_detail_change(map, 1.0);
  banner_busy_stop(map->appdata);
}

map_state_t::map_state_t()
  : refcount(1)
{
  reset();
}

void map_state_t::reset() {
  zoom = 0.25;
  detail = 1.0;

  scroll_offset.x = 0;
  scroll_offset.y = 0;
}

// vim:et:ts=8:sw=2:sts=2:ai
