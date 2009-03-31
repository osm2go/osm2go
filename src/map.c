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

#include "appdata.h"

#include <gdk/gdkkeysyms.h>


#undef DESTROY_WAIT_FOR_GTK

static void map_statusbar(map_t *map, map_item_t *map_item) {
  char *item_str = NULL;
  item_id_t id = ID_ILLEGAL;
  tag_t *tag = NULL;
  char *str = NULL;
  
  switch(map_item->object.type) {
  case NODE:
    item_str = "Node";
    id = map_item->object.node->id;
    tag = map_item->object.node->tag;
    break;

  case WAY:
    item_str = "Way";
    id = map_item->object.way->id;
    tag = map_item->object.way->tag;
    break;

  case RELATION:
    item_str = "Relation";
    id = map_item->object.relation->id;
    tag = map_item->object.relation->tag;
    break;

  default:
    break;
  }

  gboolean collision = FALSE;
  tag_t *tags = tag;

  if(id == ID_ILLEGAL) 
    str = g_strdup_printf(_("Unknown item"));
  else {
    str = g_strdup_printf("%s #%ld", item_str, id);

    /* add some tags ... */
    /*
     *  XXX Should we just try to present only the name or the ref (or the
     *  alt_name, old_name, whatever) here?  Hurling a load of tags in the
     *  user's face in some unpredictable, uninformative order isn't very
     *  friendly.
     *
     *  Actually, a tag_short_desc() function would be useful in dialogs
     *  nd user messages too.
     */
    while(tag) {
      if(!collision && info_tag_key_collision(tags, tag))
	collision = TRUE;

      /* we don't have much space, so ignore created_by tag */
      if(!osm_is_creator_tag(tag)) {
	char *old = str;
	str = g_strdup_printf("%s, %s=%s", old, tag->key, tag->value);
	g_free(old);
      }
      tag = tag->next;
    }
  }
  
  statusbar_set(map->appdata, str, collision);
  g_free(str);
}

void map_outside_error(appdata_t *appdata) {
  errorf(GTK_WIDGET(appdata->window), 
	 _("Items must not be placed outside the working area!"));
}

void map_item_chain_destroy(map_item_chain_t **chainP) {
  if(!*chainP) {
    printf("nothing to destroy!\n");
    return;
  }

#ifdef DESTROY_WAIT_FOR_GTK
  map_item_chain_t *chain = *chainP;
  while(chain) {
    map_item_chain_t *next = chain->next;
    canvas_item_destroy(chain->map_item->item);
    chain = next;
  }

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
  map_item_chain_t *chain = *chainP;
  while(chain) {
    map_item_chain_t *next = chain->next;
    canvas_item_destroy(chain->map_item->item);

    g_free(chain);
    chain = next;
  }
  *chainP = NULL;
#endif
}

static void map_node_select(appdata_t *appdata, node_t *node) {
  map_t *map = appdata->map;
  map_item_t *map_item = &map->selected;
  
  g_assert(!map->highlight);

  map_item->object.type      = NODE;
  map_item->object.node      = node;
  map_item->highlight = FALSE;

  /* node may not have any visible representation at all */
  if(node->map_item_chain) 
    map_item->item = node->map_item_chain->map_item->item;
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

  float radius = map->style->highlight.width + map->style->node.radius;
  if(!node->ways) radius += map->style->node.border_radius;
  if(node->icon_buf && map->style->icon.enable && 
     !appdata->settings->no_icons) {
    gint w = gdk_pixbuf_get_width(map_item->object.node->icon_buf);
    gint h = gdk_pixbuf_get_height(map_item->object.node->icon_buf);
    /* icons are technically square, so a radius slightly bigger */
    /* than sqrt(2)*MAX(w,h) should fit nicely */
    radius =  0.75 * map->style->icon.scale * ((w>h)?w:h);
  }

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

void map_way_select(appdata_t *appdata, way_t *way) {
  map_t *map = appdata->map;
  map_item_t *map_item = &map->selected;
  
  g_assert(!map->highlight);

  map_item->object.type      = WAY;
  map_item->object.way       = way;
  map_item->highlight = FALSE;
  map_item->item      = way->map_item_chain->map_item->item;

  map_statusbar(map, map_item);
  icon_bar_map_item_selected(appdata, map_item, TRUE);
  gtk_widget_set_sensitive(appdata->menu_item_map_hide_sel, TRUE);

  gint arrow_width = (map_item->object.way->draw.flags & OSM_DRAW_FLAG_BG)?
    map->style->highlight.width + map_item->object.way->draw.bg.width/2:
    map->style->highlight.width + map_item->object.way->draw.width/2;
  
  node_chain_t *node_chain = map_item->object.way->node_chain;
  node_t *last = NULL;
  while(node_chain) {
    map_item_t item;
    item.object.type = NODE;
    item.object.node = node_chain->node;

    /* draw an arrow between every two nodes */
    if(last) {
      /* create a new map item for every arrow */
      map_item_t *new_map_item = g_new0(map_item_t, 1);
      new_map_item->object.type = WAY;
      new_map_item->object.way = way;
      new_map_item->highlight = TRUE;

      struct { float x, y;} center, diff;
      center.x = (last->lpos.x + node_chain->node->lpos.x)/2;
      center.y = (last->lpos.y + node_chain->node->lpos.y)/2;
      diff.x = node_chain->node->lpos.x - last->lpos.x;
      diff.y = node_chain->node->lpos.y - last->lpos.y;

      /* only draw arrow if there's sufficient space */
      /* TODO: what if there's not enough space anywhere? */
      float len = sqrt(pow(diff.x, 2)+pow(diff.y, 2));
      if(len > map->style->highlight.arrow_limit*arrow_width) {
	len /= arrow_width;
	diff.x = diff.x / len; 
	diff.y = diff.y / len; 

	canvas_points_t *points = canvas_points_new(4);
	points->coords[2*0+0] = points->coords[2*3+0] = center.x + diff.x;
	points->coords[2*0+1] = points->coords[2*3+1] = center.y + diff.y;
	points->coords[2*1+0] = center.x + diff.y - diff.x;
	points->coords[2*1+1] = center.y - diff.x - diff.y;
	points->coords[2*2+0] = center.x - diff.y - diff.x;
	points->coords[2*2+1] = center.y + diff.x - diff.y;
	
	map_hl_polygon_new(map, CANVAS_GROUP_WAYS_DIR, new_map_item, 
			   points, map->style->highlight.arrow_color);
	
	canvas_points_free(points);
      }
    }
      
    if(!map_hl_item_is_highlighted(map, &item)) {

      /* create a new map item for every node */
      map_item_t *new_map_item = g_new0(map_item_t, 1);
      new_map_item->object.type = NODE;
      new_map_item->object.node = node_chain->node;
      new_map_item->highlight = TRUE;
    
      gint x = node_chain->node->lpos.x;
      gint y = node_chain->node->lpos.y;

      map_hl_circle_new(map, CANVAS_GROUP_NODES_IHL, new_map_item, 
			x, y, map->style->node.radius, 
			map->style->highlight.node_color);
    }

    last = node_chain->node;
    node_chain = node_chain->next;
  }

  /* a way needs at least 2 points to be drawn */
  guint nodes = osm_way_number_of_nodes(way);
  if(nodes > 1) {
    
    /* allocate space for nodes */
    canvas_points_t *points = canvas_points_new(nodes);

    int node = 0;
    node_chain = map_item->object.way->node_chain;
    while(node_chain) {
      canvas_point_set_pos(points, node++, &node_chain->node->lpos);
      node_chain = node_chain->next;
    }
    
    /* create a copy of this map item and mark it as being a highlight */
    map_item_t *new_map_item = g_new0(map_item_t, 1);
    memcpy(new_map_item, map_item, sizeof(map_item_t));
    new_map_item->highlight = TRUE;
    
    map_hl_polyline_new(map, CANVAS_GROUP_WAYS_HL, new_map_item, points, 
		(map_item->object.way->draw.flags & OSM_DRAW_FLAG_BG)?
		2*map->style->highlight.width + map_item->object.way->draw.bg.width:
		2*map->style->highlight.width + map_item->object.way->draw.width, 
		map->style->highlight.color);

    canvas_points_free(points);
  }
}

void map_relation_select(appdata_t *appdata, relation_t *relation) {
  map_t *map = appdata->map;

  printf("highlighting relation %ld\n", relation->id);

  g_assert(!map->highlight);
  map_highlight_t **hl = &map->highlight;

  map_item_t *map_item = &map->selected;
  map_item->object.type      = RELATION;
  map_item->object.relation  = relation;
  map_item->highlight = FALSE;
  map_item->item      = NULL;

  map_statusbar(map, map_item);
  icon_bar_map_item_selected(appdata, map_item, TRUE);

  /* process all members */
  member_t *member = relation->member;
  while(member) {
    canvas_item_t *item = NULL;

    switch(member->object.type) {

    case NODE: {
      node_t *node = member->object.node;
      printf("  -> node %ld\n", node->id);

      item = canvas_circle_new(map->canvas, CANVAS_GROUP_NODES_HL, 
			node->lpos.x, node->lpos.y, 
			map->style->highlight.width + map->style->node.radius, 
			0, map->style->highlight.color, NO_COLOR);
      } break;

    case WAY: {
      way_t *way = member->object.way;
      /* a way needs at least 2 points to be drawn */
      guint nodes = osm_way_number_of_nodes(way);
      if(nodes > 1) {
	
	/* allocate space for nodes */
	canvas_points_t *points = canvas_points_new(nodes);
	
	int node = 0;
	node_chain_t *node_chain = way->node_chain;
	while(node_chain) {
	  canvas_point_set_pos(points, node++, &node_chain->node->lpos);
	  node_chain = node_chain->next;
	}

	if(way->draw.flags & OSM_DRAW_FLAG_AREA) 
	  item = canvas_polygon_new(map->canvas, CANVAS_GROUP_WAYS_HL, points, 0, 0,
				    map->style->highlight.color);
	else
	  item = canvas_polyline_new(map->canvas, CANVAS_GROUP_WAYS_HL, points,
			      (way->draw.flags & OSM_DRAW_FLAG_BG)?
			      2*map->style->highlight.width + way->draw.bg.width:
			      2*map->style->highlight.width + way->draw.width, 
			      map->style->highlight.color);

	canvas_points_free(points);
      } } break;
      
    default:
      break;
    }

    /* attach item to item chain */
    if(item) {
      *hl = g_new0(map_highlight_t, 1);
      (*hl)->item = item;
      hl = &(*hl)->next;
    }

    member = member->next;
  }
}

static void map_object_select(appdata_t *appdata, object_t *object) {
  switch(object->type) {
  case NODE:
    map_node_select(appdata, object->node);
    break;
  case WAY:
    map_way_select(appdata, object->way);
    break;
  case RELATION:
    map_relation_select(appdata, object->relation);
    break;
  default:
    g_assert((object->type == NODE)||(object->type == RELATION)||
	     (object->type == WAY));
    break;
  }
}

void map_item_deselect(appdata_t *appdata) {

  /* save tags for "last" function in info dialog */
  if(appdata->map->selected.object.type == NODE) {
    if(appdata->map->last_node_tags) 
      osm_tags_free(appdata->map->last_node_tags);

    appdata->map->last_node_tags = 
      osm_tags_copy(appdata->map->selected.object.node->tag, FALSE);
  } else if(appdata->map->selected.object.type == WAY) {
    if(appdata->map->last_way_tags) 
      osm_tags_free(appdata->map->last_way_tags);

    appdata->map->last_way_tags = 
      osm_tags_copy(appdata->map->selected.object.way->tag, FALSE);
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
static gint map_item_destroy_event(GtkWidget *widget, gpointer data) {
  map_item_t *map_item = (map_item_t*)data;

  //  printf("destroying map_item @ %p\n", map_item);

#ifdef DESTROY_WAIT_FOR_GTK
  /* remove item from nodes/ways map_item_chain */
  map_item_chain_t **chain = NULL;
  if(map_item->object.type == NODE)
    chain = &map_item->object.node->map_item_chain;
  else if(map_item->object.type == WAY)
    chain = &map_item->object.way->map_item_chain;

  /* there must be a chain with content, otherwise things are broken */
  g_assert(chain);
  g_assert(*chain);

  /* search current map_item, ... */
  while(*chain && (*chain)->map_item != map_item)
    chain = &(*chain)->next;

  g_assert(*chain);

  /* ... remove it from chain and free it */
  map_item_chain_t *tmp = *chain;
  *chain = (*chain)->next;

  g_free(tmp);
#endif

  g_free(map_item);
  return FALSE;
}

static canvas_item_t *map_node_new(map_t *map, node_t *node, gint radius, 
		   gint width, canvas_color_t fill, canvas_color_t border) {

  map_item_t *map_item = g_new0(map_item_t, 1);
  map_item->object.type = NODE;
  map_item->object.node = node;

  if(!node->icon_buf || !map->style->icon.enable || 
     map->appdata->settings->no_icons) 
    map_item->item = canvas_circle_new(map->canvas, CANVAS_GROUP_NODES, 
       node->lpos.x, node->lpos.y, radius, width, fill, border);
  else
    map_item->item = canvas_image_new(map->canvas, CANVAS_GROUP_NODES, 
      node->icon_buf, 
      node->lpos.x - map->style->icon.scale/2 * 
		      gdk_pixbuf_get_width(node->icon_buf), 
      node->lpos.y - map->style->icon.scale/2 * 
		      gdk_pixbuf_get_height(node->icon_buf), 
	      map->style->icon.scale,map->style->icon.scale);
 
  canvas_item_set_zoom_max(map_item->item, node->zoom_max);

  /* attach map_item to nodes map_item_chain */
  map_item_chain_t **chain = &node->map_item_chain;
  while(*chain) chain = &(*chain)->next;
  *chain = g_new0(map_item_chain_t, 1);
  (*chain)->map_item = map_item;

  canvas_item_set_user_data(map_item->item, map_item);

  canvas_item_destroy_connect(map_item->item, 
          G_CALLBACK(map_item_destroy_event), map_item);

  return map_item->item;
}

/* in the rare case that a way consists of only one node, it is */
/* drawn as a circle. This e.g. happens when drawing a new way */
static canvas_item_t *map_way_single_new(map_t *map, way_t *way, gint radius, 
		   gint width, canvas_color_t fill, canvas_color_t border) {

  map_item_t *map_item = g_new0(map_item_t, 1);
  map_item->object.type = WAY;
  map_item->object.way = way;
  map_item->item = canvas_circle_new(map->canvas, CANVAS_GROUP_WAYS, 
	  way->node_chain->node->lpos.x, way->node_chain->node->lpos.y, 
				     radius, width, fill, border);

  // TODO: decide: do we need canvas_item_set_zoom_max() here too?

  /* attach map_item to nodes map_item_chain */
  map_item_chain_t **chain = &way->map_item_chain;
  while(*chain) chain = &(*chain)->next;
  *chain = g_new0(map_item_chain_t, 1);
  (*chain)->map_item = map_item;

  canvas_item_set_user_data(map_item->item, map_item);

  canvas_item_destroy_connect(map_item->item, 
          G_CALLBACK(map_item_destroy_event), map_item);

  return map_item->item;
}

static canvas_item_t *map_way_new(map_t *map, canvas_group_t group, 
	  way_t *way, canvas_points_t *points, gint width, 
	  canvas_color_t color, canvas_color_t fill_color) {
  map_item_t *map_item = g_new0(map_item_t, 1);
  map_item->object.type = WAY;
  map_item->object.way = way;

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

  canvas_item_set_zoom_max(map_item->item, way->draw.zoom_max);

  /* a ways outline itself is never dashed */
  if (group != CANVAS_GROUP_WAYS_OL)
    if (way->draw.dashed)
      canvas_item_set_dashed(map_item->item, width, way->draw.dash_length);

  /* attach map_item to ways map_item_chain */
  map_item_chain_t **chain = &way->map_item_chain;
  while(*chain) chain = &(*chain)->next;
  *chain = g_new0(map_item_chain_t, 1);
  (*chain)->map_item = map_item;

  canvas_item_set_user_data(map_item->item, map_item);

  canvas_item_destroy_connect(map_item->item, 
	      G_CALLBACK(map_item_destroy_event), map_item);

  return map_item->item;
}

void map_show_node(map_t *map, node_t *node) {
  map_node_new(map, node, map->style->node.radius, 0,
	       map->style->node.color, 0);
}

void map_way_draw(map_t *map, way_t *way) {

  /* don't draw a way that's not there anymore */
  if(way->flags & (OSM_FLAG_DELETED | OSM_FLAG_HIDDEN))
    return;

  /* allocate space for nodes */
  /* a way needs at least 2 points to be drawn */
  guint nodes = osm_way_number_of_nodes(way);
  if(nodes == 1) {
    /* draw a single dot where this single node is */
    map_way_single_new(map, way, map->style->node.radius, 0, 
		       map->style->node.color, 0);
  } else {
    canvas_points_t *points = canvas_points_new(nodes);
    
    int node = 0;
    node_chain_t *node_chain = way->node_chain;
    while(node_chain) {
      canvas_point_set_pos(points, node++, &node_chain->node->lpos);
      node_chain = node_chain->next;
    }
    
    /* draw way */
    if(way->draw.flags & OSM_DRAW_FLAG_AREA) {
      map_way_new(map, CANVAS_GROUP_POLYGONS, way, points, 
		  way->draw.width, way->draw.color, way->draw.area.color);
    } else {
      
      if(way->draw.flags & OSM_DRAW_FLAG_BG) {
	map_way_new(map, CANVAS_GROUP_WAYS_INT, way, points, 
		    way->draw.width, way->draw.color, NO_COLOR);

	map_way_new(map, CANVAS_GROUP_WAYS_OL, way, points, 
		    way->draw.bg.width, way->draw.bg.color, NO_COLOR);

      } else
	map_way_new(map, CANVAS_GROUP_WAYS, way, points, 
		    way->draw.width, way->draw.color, NO_COLOR);
    }
    canvas_points_free(points);
  }
}

void map_node_draw(map_t *map, node_t *node) {
  /* don't draw a node that's not there anymore */
  if(node->flags & OSM_FLAG_DELETED)
    return;

  if(!node->ways) 
    map_node_new(map, node, 
		 map->style->node.radius,
		 map->style->node.border_radius,
		 map->style->node.fill_color, 
		 map->style->node.color);
  
  else if(map->style->node.show_untagged || osm_node_has_tag(node)) 
    map_node_new(map, node, 
		 map->style->node.radius, 0,
		 map->style->node.color, 0);
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
    g_assert((map_item->object.type == NODE) ||
	     (map_item->object.type == WAY));
  }
}

static void map_item_remove(map_t *map, map_item_t *map_item) {
  map_item_chain_t **chainP = NULL;

  switch(map_item->object.type) {
  case NODE:
    chainP = &map_item->object.node->map_item_chain;
    break;
  case WAY:
    chainP = &map_item->object.way->map_item_chain;
    break;
  default:
    g_assert((map_item->object.type == NODE) ||
	     (map_item->object.type == WAY));
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
      g_assert((map_item->object.type == NODE) ||
	           (map_item->object.type == WAY));
  }
}

void map_item_redraw(appdata_t *appdata, map_item_t *map_item) {
  map_item_t item = *map_item;

  /* check if the item to be redrawn is the selected one */
  gboolean is_selected = FALSE;
  if(map_item->object.ptr == appdata->map->selected.object.ptr) {
    map_item_deselect(appdata);
    is_selected = TRUE;
  }

  map_item_remove(appdata->map, &item);
  map_item_init(appdata->map->style, &item);
  map_item_draw(appdata->map, &item);

  /* restore selection if there was one */
  if(is_selected) 
    map_object_select(appdata, &item.object);
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
void map_frisket_draw(map_t *map, bounds_t *bounds) {
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
  way_t *way = osm->way;
  while(way) {
    map_way_draw(map, way);
    way = way->next;
  }

  printf("drawing single nodes ...\n");
  node_t *node = osm->node;
  while(node) {
    map_node_draw(map, node);
    node = node->next;
  }

  printf("drawing frisket...\n");
  map_frisket_draw(map, osm->bounds);
}

void map_state_free(map_state_t *state) {
  if(!state) return;
  
  /* free state of noone else references it */
  if(state->refcount > 1) 
    state->refcount--;
  else
    g_free(state);
}

void map_free_map_item_chains(appdata_t *appdata) {
  if(!appdata->osm) return;

#ifndef DESTROY_WAIT_FOR_GTK
  printf("  DESTROY_WAIT_FOR_GTK not set, removing all chains now\n");

  /* free all map_item_chains */
  node_t *node = appdata->osm->node;
  while(node) {
    map_item_chain_t *chain = node->map_item_chain;
    while(chain) {
      map_item_chain_t *next = chain->next;
      g_free(chain);
      chain = next;
    }
    node->map_item_chain = NULL;
    node = node->next;
  }
  
  way_t *way = appdata->osm->way;
  while(way) {
    map_item_chain_t *chain = way->map_item_chain;
    while(chain) {
      map_item_chain_t *next = chain->next;
      g_free(chain);
      chain = next;
    }
    way->map_item_chain = NULL;
    way = way->next;
  }
#endif
}

static gint map_destroy_event(GtkWidget *widget, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  map_t *map = appdata->map;

  printf("destroying entire map\n");
  
  map_free_map_item_chains(appdata);

  /* free buffered tags */
  if(map->last_node_tags) osm_tags_free(map->last_node_tags);
  if(map->last_way_tags) osm_tags_free(map->last_way_tags);

  map_state_free(map->state);

  if(map->style)
    style_free(map->style);

  /* destroy existing highlight */
  if(map->highlight) {
    printf("removing highlight\n");

    map_highlight_t *hl = map->highlight;
    while(hl) {
      map_highlight_t *next = hl->next;
      g_free(hl);
      hl = next;
    }
  }

  g_free(map);

  appdata->map = NULL;

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

  map_item_t *map_item = canvas_item_get_user_data(item);

  if(!map_item) {
    printf("  item has no user data!\n");
    return NULL;
  }
    
  if(map_item->highlight) 
    printf("  item is highlight\n");    

  switch(map_item->object.type) {
  case NODE:
    printf("  item is node #%ld\n", map_item->object.node->id);
    break;
  case WAY:
    printf("  item is way #%ld\n", map_item->object.way->id);
    break;
  default:
    printf("  unknown item\n");
    break;
  }

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
      parent = map_item->object.node->map_item_chain->map_item;

    if(parent)
      printf("  using parent item node #%ld\n", parent->object.node->id);      
    break;

  case WAY:
    if(map_item->object.way->map_item_chain)
      parent = map_item->object.way->map_item_chain->map_item;

    if(parent)
      printf("  using parent item way #%ld\n", parent->object.way->id);      
    break;

  default:
    g_assert((map_item->object.type == NODE) ||
	     (map_item->object.type == WAY)); 
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
  gint aw_cu = canvas_get_viewport_width(map->canvas, CANVAS_UNIT_METER);
  gint ah_cu = canvas_get_viewport_height(map->canvas, CANVAS_UNIT_METER);
  
  // Data rect minimum and maximum
  gint min_x, min_y, max_x, max_y;
  min_x = map->appdata->osm->bounds->min.x;
  min_y = map->appdata->osm->bounds->min.y;
  max_x = map->appdata->osm->bounds->max.x;
  max_y = map->appdata->osm->bounds->max.y;
  
  // limit stops - prevent scrolling beyond these
  gint min_sy_cu = 0.95*(min_y - ah_cu);
  gint min_sx_cu = 0.95*(min_x - aw_cu);
  gint max_sy_cu = 0.95*(max_y);
  gint max_sx_cu = 0.95*(max_x);
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
        printf("Can't zoom further out\n");
        return 1;
    }
    return 0;
}


/*
 * Scroll the map to a point if that point is currently offscreen.
 */
void map_scroll_to_if_offscreen(map_t *map, lpos_t *lpos) {

  // Ignore anything outside the working area
  if (!(map && map->appdata && map->appdata->osm)) {
    return;
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
    return;
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
  canvas_scroll_get(map->canvas, CANVAS_UNIT_PIXEL, &sx, &sy);
  gint viewport_left   = (sx/pix_per_meter);
  gint viewport_right  = (sx/pix_per_meter)+aw;
  gint viewport_top    = (sy/pix_per_meter);
  gint viewport_bottom = (sy/pix_per_meter)+ah;
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
    new_sx = pix_per_meter * (lpos->x - (aw/2));
    new_sy = pix_per_meter * (lpos->y - (ah/2));

    map_limit_scroll(map, CANVAS_UNIT_PIXEL, &new_sx, &new_sy);
    canvas_scroll_to(map->canvas, CANVAS_UNIT_PIXEL, new_sx, new_sy);
  }
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
}

static gboolean map_scroll_event(GtkWidget *widget, GdkEventScroll *event,
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

    node_chain_t *node_chain = map->selected.object.way->node_chain;
    while(node_chain) {
      if(node_chain->node == map_item->object.node) {
	printf("  requested item is part of selected way\n");
	return TRUE;
      } 
      node_chain = node_chain->next;
    }
    printf("  but it doesn't include the requested node\n");
    return FALSE;
    
  } else {
    printf("  selected item is unknown\n");
    return FALSE;
  }

  g_assert(0);
  return TRUE;
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
  map_object_select(appdata, &old);
}

void map_way_delete(appdata_t *appdata, way_t *way) {
  printf("deleting way #%ld from map and osm\n", way->id);

  /* remove it visually from the screen */
  map_item_chain_destroy(&way->map_item_chain);

  /* also remove the visible representation of all nodes (nodes with tags) */
  node_chain_t *chain = way->node_chain;
  while(chain) {
    if(chain->node->map_item_chain) 
      map_item_chain_destroy(&chain->node->map_item_chain);
      
    chain = chain->next;
  }

  /* and mark it "deleted" in the database */
  osm_way_remove_from_relation(appdata->osm, way);
  osm_way_delete(appdata->osm, &appdata->icon, way, FALSE);
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
      g_assert((map_item.object.type == NODE) ||
	       (map_item.object.type == WAY));
      break;
    }
  }
}

static void map_touchnode_update(appdata_t *appdata, gint x, gint y) {
  map_t *map = appdata->map;

  map_hl_touchnode_clear(map);

  node_t *cur_node = NULL;

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
  node_t *node = appdata->osm->node;
  while(!map->touchnode && node) {
    /* don't highlight the dragged node itself and don't highlight */
    /* deleted ones */
    if((node != cur_node) && (!(node->flags & OSM_FLAG_DELETED))) {
      gint nx = x - node->lpos.x;
      gint ny = y - node->lpos.y;

      if((nx < map->style->node.radius) && (ny < map->style->node.radius) &&
	 (nx*nx + ny*ny < map->style->node.radius * map->style->node.radius))
	map_hl_touchnode_draw(map, node);
    }
    node = node->next;
  }

  /* during way creation also nodes of the new way */
  /* need to be searched */
  if(!map->touchnode && map->action.way) {
    node_chain_t *chain = map->action.way->node_chain;
    while(!map->touchnode && chain && chain->next) {
      gint nx = x - chain->node->lpos.x;
      gint ny = y - chain->node->lpos.y;
	  
      if((nx < map->style->node.radius) && (ny < map->style->node.radius) &&
	 (nx*nx + ny*ny < map->style->node.radius * map->style->node.radius))
	map_hl_touchnode_draw(map, chain->node);

      chain = chain->next;
    }
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
    map_hl_cursor_draw(map, x, y, FALSE, map->style->node.radius);
    break;

  case MAP_ACTION_WAY_ADD:
    map_hl_cursor_draw(map, x, y, FALSE, map->style->node.radius);
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
	 (old_sel.object.type == map->selected.object.type) &&
	 (old_sel.object.ptr == map->selected.object.ptr)) {
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

  case MAP_ACTION_NODE_ADD:
    printf("released after NODE ADD\n");
    map_hl_cursor_clear(map);

    /* convert mouse position to canvas (world) position */
    canvas_window2world(map->canvas, x, y, &x, &y);

    node_t *node = NULL;
    if(!osm_position_within_bounds(map->appdata->osm, x, y)) 
      map_outside_error(map->appdata);
    else {
      node = osm_node_new(map->appdata->osm, x, y);
      osm_node_attach(map->appdata->osm, node);
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

static gboolean map_button_event(GtkWidget *widget, GdkEventButton *event,
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

static gboolean map_motion_notify_event(GtkWidget *widget, 
                             GdkEventMotion *event, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  map_t *map = appdata->map;
  gint x, y;
  GdkModifierType state;

  if(!appdata->osm) return FALSE;

#ifdef USE_HILDON
  /* reduce update frequency on hildon to keep screen update fluid */
  static guint32 last_time = 0;

  if(event->time - last_time < 250) return FALSE;
  last_time = event->time;
#endif

  if(!map->pen_down.is) 
    return FALSE;

#ifndef USE_GOOCANVAS
  /* handle hints, hints are handled by goocanvas directly */
  if(event->is_hint)
    gdk_window_get_pointer(event->window, &x, &y, &state);
  else 
#endif
  {
    x = event->x;
    y = event->y;
    state = event->state;
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
	map_hl_cursor_draw(map, x, y, FALSE, map->style->node.radius);
	map_touchnode_update(appdata, x, y);
      }
    }
    break;
    
  case MAP_ACTION_NODE_ADD:
    map_hl_cursor_draw(map, x, y, FALSE, map->style->node.radius);
    break;

  case MAP_ACTION_WAY_ADD:
    map_hl_cursor_draw(map, x, y, FALSE, map->style->node.radius);
    map_touchnode_update(appdata, x, y);
    break;

  case MAP_ACTION_WAY_NODE_ADD:
    map_hl_cursor_clear(map);
    map_item_t *item = map_item_at(map, x, y);
    if(item) map_edit_way_node_add_highlight(map, item, x, y);
    break;

  case MAP_ACTION_WAY_CUT:
    map_hl_cursor_clear(map);
    item = map_item_at(map, x, y);
    if(item) map_edit_way_cut_highlight(map, item, x, y);
    break;

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

#ifdef USE_HILDON
    case HILDON_HARDKEY_INCREASE:
#else
    case '+':
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

GtkWidget *map_new(appdata_t *appdata) {
  map_t *map = appdata->map = g_new0(map_t, 1);

  map->style = style_load(appdata, appdata->settings->style);
  if(!map->style) {
    errorf(NULL, _("Unable to load valid style, terminating."));
    g_free(map);
    return NULL;
  }

  if(appdata->project && appdata->project->map_state) {
    printf("Using projects map state\n");
    map->state = appdata->project->map_state;
  } else {
    printf("Creating new map state\n");
    map->state = g_new0(map_state_t, 1);
    map->state->zoom = 0.25;
  }
  
  map->state->refcount++;

  map->pen_down.at.x = -1;
  map->pen_down.at.y = -1;
  map->appdata = appdata;
  map->action.type = MAP_ACTION_IDLE;

  map->canvas = canvas_new(map->style->background.color);
  canvas_set_antialias(map->canvas, !appdata->settings->no_antialias);

  GtkWidget *canvas_widget = canvas_get_widget(map->canvas);

  gtk_widget_set_events(canvas_widget,
                          GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK
			| GDK_SCROLL_MASK
			| GDK_POINTER_MOTION_MASK
    			| GDK_POINTER_MOTION_HINT_MASK);

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

  map_free_map_item_chains(appdata);

  /* remove a possibly existing highlight */
  map_item_deselect(appdata);
  
  canvas_erase(map->canvas, group_mask);
}

void map_paint(appdata_t *appdata) {
  map_t *map = appdata->map;

  /* user may have changed antialias settings */
  canvas_set_antialias(map->canvas, !appdata->settings->no_antialias);

  josm_elemstyles_colorize_world(map->style, appdata->osm);
  map_draw(map, appdata->osm);
}

/* called from several icons like e.g. "node_add" */
void map_action_set(appdata_t *appdata, map_action_t action) {
  printf("map action set to %d\n", action);

  appdata->map->action.type = action;

  /* enable/disable ok/cancel buttons */
  // MAP_ACTION_IDLE=0, NODE_ADD, BG_ADJUST, WAY_ADD, WAY_NODE_ADD, WAY_CUT
  const gboolean ok_state[] = { FALSE, FALSE, TRUE, FALSE, FALSE, FALSE };
  const gboolean cancel_state[] = { FALSE, TRUE, TRUE, TRUE, TRUE, TRUE };

  g_assert(MAP_ACTION_NUM == sizeof(ok_state)/sizeof(gboolean));
  g_assert(action < sizeof(ok_state)/sizeof(gboolean));

  icon_bar_map_cancel_ok(appdata, cancel_state[action], ok_state[action]);

  switch(action) {
  case MAP_ACTION_BG_ADJUST:
    /* an existing selection only causes confusion ... */
    map_item_deselect(appdata);
    break;

  case MAP_ACTION_WAY_ADD:
    printf("starting new way\n");

    /* remember if there was a way selected */
    way_t *way_sel = NULL;
    if(appdata->map->selected.object.type == WAY)
      way_sel = appdata->map->selected.object.way;

    map_item_deselect(appdata);
    map_edit_way_add_begin(appdata->map, way_sel);
    break;

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

  g_assert(MAP_ACTION_NUM == sizeof(str_state)/sizeof(char*));

  statusbar_set(appdata, str_state[action], FALSE);
}


void map_action_cancel(appdata_t *appdata) {
  map_t *map = appdata->map;

  switch(map->action.type) {
  case MAP_ACTION_WAY_ADD:
    map_edit_way_add_cancel(map);
    break;

  case MAP_ACTION_BG_ADJUST:
    /* undo all changes to bg_offset */
    map->bg.offset.x = appdata->project->wms_offset.x;
    map->bg.offset.y = appdata->project->wms_offset.y;

    gint x = appdata->osm->bounds->min.x + map->bg.offset.x;
    gint y = appdata->osm->bounds->min.y + map->bg.offset.y;
    canvas_image_move(map->bg.item, x, y, map->bg.scale.x, map->bg.scale.y);
    break;

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
    appdata->project->dirty = TRUE;
    break;

  default:
    break;
  }
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

  undo_remember_delete(appdata, &item.object);

  switch(item.object.type) {
  case NODE:
    printf("request to delete node #%ld\n", item.object.node->id);

    /* check if this node is part of a way with two nodes only. */
    /* we cannot delete this as this would also delete the way */
    way_chain_t *way_chain = osm_node_to_way(appdata->osm, item.object.node);
    if(way_chain) {
      gboolean short_way = FALSE;
      
      /* free the chain of ways */
      while(way_chain && !short_way) {
	way_chain_t *next = way_chain->next;

	if(osm_way_number_of_nodes(way_chain->way) <= 2) 
	  short_way = TRUE;

	g_free(way_chain);
	way_chain = next;
      }

      if(short_way) {
	if(!yes_no_f(GTK_WIDGET(appdata->window), NULL, 0, 0,
		     _("Delete node in short way(s)?"),
		     _("Deleting this node will also delete one or more ways "
		       "since they'll contain only one node afterwards. "
		       "Do you really want this?")))
	  return;
      }
    }

    /* remove it visually from the screen */
    map_item_chain_destroy(&item.object.node->map_item_chain);

    /* and mark it "deleted" in the database */
    osm_node_remove_from_relation(appdata->osm, item.object.node);
    way_chain_t *chain = osm_node_delete(appdata->osm, 
			 &appdata->icon, item.object.node, FALSE, TRUE);

    /* redraw all affected ways */
    while(chain) {
      way_chain_t *next = chain->next;

      if(osm_way_number_of_nodes(chain->way) == 1) {
	/* this way now only contains one node and thus isn't a valid */
	/* way anymore. So it'll also get deleted (which in turn may */
	/* cause other nodes to be deleted as well) */
	map_way_delete(appdata, chain->way);
      } else {
	map_item_t item;
	item.object.type = WAY;
	item.object.way = chain->way;
	map_item_redraw(appdata, &item);
      }

      g_free(chain);

      chain = next;
    }

    break;

  case WAY:
    printf("request to delete way #%ld\n", item.object.way->id);
    map_way_delete(appdata, item.object.way);
    break;

  default:
    g_assert((item.object.type == NODE) ||
	     (item.object.type == WAY));
    break;
  }
}

/* ----------------------- track related stuff ----------------------- */

void map_track_draw_seg(map_t *map, track_seg_t *seg) {
  /* a track_seg needs at least 2 points to be drawn */
  guint pnum = track_seg_points(seg);
  printf("seg of length %d\n", pnum);

  if(pnum == 1) {
    g_assert(!seg->item);

    seg->item = canvas_circle_new(map->canvas, CANVAS_GROUP_TRACK,
	  seg->track_point->lpos.x, seg->track_point->lpos.y, 
	  map->style->track.width/2.0, 0, map->style->track.color, NO_COLOR);
  }

  if(pnum > 1) {
    
    /* allocate space for nodes */
    canvas_points_t *points = canvas_points_new(pnum);
    
    int point = 0;
    track_point_t *track_point = seg->track_point;
    while(track_point) {
      points->coords[point++] = track_point->lpos.x;
      points->coords[point++] = track_point->lpos.y;
      track_point = track_point->next;
    }
    
    /* there may be a circle (one point line) */
    if(seg->item)
      canvas_item_destroy(seg->item);

    seg->item = canvas_polyline_new(map->canvas, CANVAS_GROUP_TRACK,
	  points, map->style->track.width, map->style->track.color);

    canvas_points_free(points);
  }
}

void map_track_update_seg(map_t *map, track_seg_t *seg) {
  /* a track_seg needs at least 2 points to be drawn */
  guint pnum = track_seg_points(seg);
  printf("seg of length %d\n", pnum);

  if(pnum > 1) {
    
    /* allocate space for nodes */
    canvas_points_t *points = canvas_points_new(pnum);
    
    int point = 0;
    track_point_t *track_point = seg->track_point;
    while(track_point) {
      canvas_point_set_pos(points, point++, &track_point->lpos);
      track_point = track_point->next;
    }
    
    g_assert(seg->item);
    canvas_item_set_points(seg->item, points);
    canvas_points_free(points);
  }
}

void map_track_draw(map_t *map, track_t *track) {
  track_seg_t *seg = track->track_seg;

  /* draw all segments */
  while(seg) {
    map_track_draw_seg(map, seg);
    seg = seg->next;
  }
}

void map_track_remove(appdata_t *appdata) {
  track_t *track = appdata->track.track;

  printf("removing track\n");

  g_assert(track);

  /* remove all segments */
  track_seg_t *seg = track->track_seg;
  while(seg) {
    if(seg->item) {
      canvas_item_destroy(seg->item);
      seg->item = NULL;
    }

    seg = seg->next;
  }
}

void map_track_pos(appdata_t *appdata, lpos_t *lpos) {
  if(appdata->track.gps_item) {
    canvas_item_destroy(appdata->track.gps_item);
    appdata->track.gps_item = NULL;
  }

  if(lpos)
    appdata->track.gps_item = 
      canvas_circle_new(appdata->map->canvas, CANVAS_GROUP_GPS, 
	lpos->x, lpos->y, appdata->map->style->track.width/2.0, 0, 
			appdata->map->style->track.gps_color, NO_COLOR);
}

/* ------------------- map background ------------------ */

void map_remove_bg_image(map_t *map) {
  if(!map) return;

  if(map->bg.item) {
    canvas_item_destroy(map->bg.item);
    map->bg.item = NULL;
  }
}

static gint map_bg_item_destroy_event(GtkWidget *widget, gpointer data) {
  map_t *map = (map_t*)data;

  /* destroying background item */

  map->bg.item = NULL;
  if(map->bg.pix) {
    printf("destroying background item\n");
    gdk_pixbuf_unref(map->bg.pix);
    map->bg.pix = NULL;
  }
  return FALSE;
}

void map_set_bg_image(map_t *map, char *filename) {
  bounds_t *bounds = map->appdata->osm->bounds;

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
  printf("hiding way #%ld\n", way->id);

  map_item_deselect(appdata);
  way->flags |= OSM_FLAG_HIDDEN;
  map_item_chain_destroy(&way->map_item_chain);

  gtk_widget_set_sensitive(appdata->menu_item_map_show_all, TRUE);
}

void map_show_all(appdata_t *appdata) {
  map_t *map = appdata->map;
  if(!map) return;

  way_t *way = appdata->osm->way;
  while(way) {
    if(way->flags & OSM_FLAG_HIDDEN) {
      way->flags &= ~OSM_FLAG_HIDDEN;
      map_way_draw(map, way);
    }

    way = way->next;
  }

  gtk_widget_set_sensitive(appdata->menu_item_map_show_all, FALSE);
}

// vim:et:ts=8:sw=2:sts=2:ai
