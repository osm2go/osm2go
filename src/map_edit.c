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

#include "map_edit.h"

#include "appdata.h"
#include "banner.h"
#include "iconbar.h"
#include "info.h"
#include "map_hl.h"
#include "misc.h"
#include "statusbar.h"
#include "style.h"

#include <glib.h>

/* --------------------- misc local helper functions ---------------- */
struct relation_transfer {
  way_t * const dst;
  way_t * const src;
};

void relation_transfer_operator(relation_t* relation, struct relation_transfer *context)
{
  printf("way #" ITEM_ID_FORMAT " is part of relation #" ITEM_ID_FORMAT "\n",
         OSM_ID(context->src), OSM_ID(relation));

  /* make new member of the same relation */

  /* walk member chain. save role of way if its being found. */
  member_t **member = &relation->member;
  char *role = NULL;
  while(*member) {
    /* save role of way */
    if(((*member)->object.type == WAY) && ((*member)->object.way == context->src))
      role = (*member)->role;
    member = &(*member)->next;
  }

  printf("  adding way #" ITEM_ID_FORMAT " to relation\n", OSM_ID(context->dst));
  *member = g_new0(member_t, 1);
  (*member)->object.type = WAY;
  (*member)->object.way = context->dst;
  (*member)->role = g_strdup(role);
  member = &(*member)->next;

  OSM_FLAGS(relation) |= OSM_FLAG_DIRTY;
}

static void transfer_relations(osm_t *osm, way_t *dst, way_t *src) {

  /* transfer relation memberships from the src way to the dst one */
  GSList *rchain = osm_way_to_relation(osm, src);
  struct relation_transfer context = { .dst = dst, .src = src };

  g_slist_foreach(rchain, (GFunc)relation_transfer_operator, &context);

  g_slist_free(rchain);
}

/* combine tags from src to dst and combine them in a useful manner */
/* erase all source tags */
static gboolean combine_tags(tag_t **dst, tag_t *src) {
  gboolean conflict = FALSE;
  tag_t *dst_orig = *dst;

  /* ---------- transfer tags from way[1] to way[0] ----------- */
  while(*dst) dst = &((*dst)->next);  /* find end of target tag list */
  while(src) {
    /* check if same key but with different value is present, */
    /* ignoring the created_by tags */
    if(!osm_is_creator_tag(src) &&
       osm_tag_key_other_value_present(dst_orig, src))
      conflict = TRUE;

    /* don't copy "created_by" and "source" tag or tags that already */
    /* exist in identical form */
    if(osm_is_creator_tag(src) ||
       osm_tag_key_and_value_present(dst_orig, src)) {
      tag_t *next = src->next;
      osm_tag_free(src);
      src = next;
    } else {
      *dst = src;
      src = src->next;
      dst = &((*dst)->next);
      *dst = NULL;
    }
  }
  return conflict;
}

/* -------------------------- way_add ----------------------- */

void map_edit_way_add_begin(map_t *map, way_t *way_sel) {
  if(way_sel)
    printf("previously selected way is #" ITEM_ID_FORMAT "\n",
		     OSM_ID(way_sel));

  g_assert(!map->action.way);
  map->action.way = osm_way_new();
  map->action.extending = NULL;
}

void map_edit_way_add_segment(map_t *map, gint x, gint y) {

  /* convert mouse position to canvas (world) position */
  canvas_window2world(map->canvas, x, y, &x, &y);

  /* check if this was a double click. This is the case if */
  /* the last node placed is less than 5 pixels from the current */
  /* position */
  const node_t *lnode = osm_way_get_last_node(map->action.way);
  if(lnode && (map->state->zoom * sqrt((lnode->lpos.x-x)*(lnode->lpos.x-x)+
		       (lnode->lpos.y-y)*(lnode->lpos.y-y))) < 5) {
#if 0
    printf("detected double click -> simulate ok click\n");
    map_hl_touchnode_clear(map);
    map_action_ok(map->appdata);
#else
    printf("detected double click -> ignore it as accidential\n");
#endif
  } else {

    /* use the existing node if one was touched */
    node_t *node = map_hl_touchnode_get_node(map);
    if(node) {
      printf("  re-using node #" ITEM_ID_FORMAT "\n", OSM_ID(node));
      map_hl_touchnode_clear(map);

      g_assert(map->action.way);

      /* check whether this node is first or last one of a different way */
      way_t *touch_way = NULL;
      way_chain_t *way_chain = osm_node_to_way(map->appdata->osm, node);
      way_chain_t *match = g_slist_find_custom(way_chain, node,
                                               (GCompareFunc)osm_way_ends_with_node);
      if(match)
        touch_way = match->data;
      g_slist_free(way_chain);

      /* remeber this way as this may be the last node placed */
      /* and we might want to join this with this other way */
      map->action.ends_on = touch_way;

      /* is this the first node the user places? */
      if(!map->action.way->node_chain) {
	map->action.extending = touch_way;

	if(map->action.extending) {
	  if(!yes_no_f(GTK_WIDGET(map->appdata->window),
	       map->appdata, MISC_AGAIN_ID_EXTEND_WAY, 0,
	       _("Extend way?"),
	       _("Do you want to extend the way present at this location?")))
	    map->action.extending = NULL;
	  else
	    /* there are immediately enough nodes for a valid way */
	    icon_bar_map_cancel_ok(map->appdata, TRUE, TRUE);
	}
      }

    } else {
      /* the current way doesn't end on another way if we are just placing */
      /* a new node */
      map->action.ends_on = NULL;

      if(!osm_position_within_bounds(map->appdata->osm, x, y))
	map_outside_error(map->appdata);
      else
	node = osm_node_new(map->appdata->osm, x, y);
    }

    if(node) {
      g_assert(map->action.way);
      osm_way_append_node(map->action.way, node);

      /* it's cheaper to count a short way multiple times than to
       * determine the length of a long way once as this trashes the
       * CPU cache. */
      if(osm_way_min_length(map->action.way, 1)) {
	if(osm_way_min_length(map->action.way, 2)) {
	  if(!osm_way_min_length(map->action.way, 3)) {
	    /* two nodes are enough for a valid way */
	    icon_bar_map_cancel_ok(map->appdata, TRUE, TRUE);
	  }
	} else {
	  /* replace "place first node..." message */
	  statusbar_set(map->appdata, _("Place next node of way"), FALSE);
	}
      }

      /* remove prior version of this way */
      map_item_chain_destroy(&map->action.way->map_item_chain);

      /* draw current way */
      josm_elemstyles_colorize_way(map->style, map->action.way);
      map_way_draw(map, map->action.way);
    }
  }
}

void map_edit_way_add_cancel(map_t *map) {
  osm_t *osm = map->appdata->osm;
  g_assert(osm);

  printf("  removing temporary way\n");
  g_assert(map->action.way);

  /* remove all nodes that have been created for this way */
  /* (their way count will be 0 after removing the way) */
  node_chain_t *chain = map->action.way->node_chain;
  while(chain) {
    node_chain_t *next = chain->next;
    node_t *node = chain->node;

    printf("    node #" ITEM_ID_FORMAT " (used by %d)\n",
	   OSM_ID(node), node->ways);

    node->ways--;
    if(!node->ways && (OSM_ID(node) == ID_ILLEGAL)) {
      printf("      -> freeing temp node\n");
      osm_node_free(osm, node);
    }
    g_free(chain);
    chain = next;
  }
  map->action.way->node_chain = NULL;

  /* remove ways visual representation */
  map_item_chain_destroy(&map->action.way->map_item_chain);

  osm_way_free(osm, map->action.way);
  map->action.way = NULL;
}

/**
 * @brief merge the two node chains
 * @param way the way to append to
 * @param nchain the tail of the new chain
 * @param reverse if way should be reversed afterwards
 *
 * The first node of nchain must be the last one of way. It will be
 * preserved in nchain, all other nodes will be moded to chain.
 */
static void merge_node_chains(way_t *way, node_chain_t *nchain, gboolean reverse)
{
  node_chain_t *chain = way->node_chain;
  g_assert(chain);

  /* search end of way to be extended */
  while(chain->next)
    chain = chain->next;

  /* skip first node of new way as its the same as the last one of the */
  /* way we are attaching it to */
  chain->next = nchain->next;

  /* terminate new way afer first node */
  nchain->next = NULL;

  /* and undo reversion of required */
  if(reverse)
    osm_way_reverse(way);
}

void map_edit_way_add_ok(map_t *map) {
  osm_t *osm = map->appdata->osm;

  g_assert(osm);
  g_assert(map->action.way);

  /* transfer all nodes that have been created for this way */
  /* into the node chain */

  /* (their way count will be 0 after removing the way) */
  node_chain_t *chain = map->action.way->node_chain;
  while(chain) {
    node_t *node = chain->node;
    printf("    node #" ITEM_ID_FORMAT " (used by %d)\n",
	   OSM_ID(node), node->ways);

    /* a node may have been a stand-alone node before, so remove its */
    /* visual representation as its now drawn as part of the way */
    /* (if at all) */
    if(OSM_ID(node) != ID_ILLEGAL)
      map_item_chain_destroy(&node->map_item_chain);

    map_node_draw(map, node);

    /* we can be sure that no node gets inserted twice (even if twice in */
    /* the ways chain) because it gets assigned a non-ID_ILLEGAL id when */
    /* being moved to the osm node chain */
    if(OSM_ID(node) == ID_ILLEGAL)
      osm_node_attach(map->appdata->osm, node);

    chain = chain->next;
  }

  /* attach to existing way if the user requested so */
  gboolean reverse = FALSE;
  if(map->action.extending) {
    node_t *nfirst = map->action.way->node_chain->node;

    printf("  request to extend way #" ITEM_ID_FORMAT "\n",
	   OSM_ID(map->action.extending));

    if(osm_way_get_first_node(map->action.extending) == nfirst) {
      printf("  need to prepend\n");
      osm_way_reverse(map->action.extending);
      reverse = TRUE;
    } else
      printf("  need to append\n");

    /* search end of way to be extended */
    merge_node_chains(map->action.extending, map->action.way->node_chain, reverse);

    /* erase and free new way (now only containing the first node anymore) */
    map_item_chain_destroy(&map->action.way->map_item_chain);
    osm_way_free(osm, map->action.way);

    map->action.way = map->action.extending;
    OSM_FLAGS(map->action.way) |= OSM_FLAG_DIRTY;
  } else {
    /* now move the way itself into the main data structure */
    osm_way_attach(map->appdata->osm, map->action.way);
  }

  /* we might already be working on the "ends_on" way as we may */
  /* be extending it. Joining the same way doesn't make sense. */
  if(map->action.ends_on && (map->action.ends_on == map->action.way)) {
    printf("  the new way ends on itself -> don't join itself\n");
    map->action.ends_on = NULL;
  }

  if(map->action.ends_on)
    if(!yes_no_f(GTK_WIDGET(map->appdata->window),
		 map->appdata, MISC_AGAIN_ID_EXTEND_WAY_END, 0,
		 _("Join way?"),
		 _("Do you want to join the way present at this location?")))
      map->action.ends_on = NULL;

  if(map->action.ends_on) {
    printf("  this new way ends on another way\n");

    /* If reverse is true the node in question is the first one */
    /* of the newly created way. Thus it is reversed again before */
    /* attaching and the result is finally reversed once more */

    /* this is slightly more complex as this time two full tagged */
    /* ways may be involved as the new way may be an extended existing */
    /* way being connected to another way. This happens if you connect */
    /* two existing ways using a new way between them */

    if (reverse) osm_way_reverse(map->action.way);

    /* and open dialog to resolve tag collisions if necessary */
    if(combine_tags(&OSM_TAG(map->action.way), OSM_TAG(map->action.ends_on)))
      messagef(GTK_WIDGET(map->appdata->window), _("Way tag conflict"),
	       _("The resulting way contains some conflicting tags. "
		 "Please solve these."));

    OSM_TAG(map->action.ends_on) = NULL;

    /* make way member of all relations ends_on already is */
    transfer_relations(map->appdata->osm, map->action.way, map->action.ends_on);

    /* check if we have to reverse (again?) to match the way order */
    if(osm_way_is_closed(map->action.way)) {

      printf("  need to prepend ends_on\n");

      /* need to reverse ends_on way */
      osm_way_reverse(map->action.ends_on);
      reverse = !reverse;
    }

    merge_node_chains(map->action.way, map->action.ends_on->node_chain, reverse);

    /* erase and free ends_on (now only containing the first node anymore) */
    map_way_delete(map->appdata, map->action.ends_on);
  }

  /* remove prior version of this way */
  map_item_chain_destroy(&map->action.way->map_item_chain);

  /* draw the updated way */
  map_way_draw(map, map->action.way);

  map_way_select(map->appdata, map->action.way);

  map->action.way = NULL;

  /* let the user specify some tags for the new way */
  info_dialog(GTK_WIDGET(map->appdata->window), map->appdata, NULL);
}

/* -------------------------- way_node_add ----------------------- */

void map_edit_way_node_add_highlight(map_t *map, map_item_t *item,
				     gint x, gint y) {
  if(map_item_is_selected_way(map, item)) {
    gint nx, ny;
    canvas_window2world(map->canvas, x, y, &nx, &ny);
    if(canvas_item_get_segment(item->item, nx, ny) >= 0)
      map_hl_cursor_draw(map, x, y, FALSE, map->style->node.radius);
  }
}

void map_edit_way_node_add(map_t *map, gint x, gint y) {
  /* check if we are still hovering above the selected way */
  map_item_t *item = map_item_at(map, x, y);
  if(item && map_item_is_selected_way(map, item)) {
    /* convert mouse position to canvas (world) position */
    canvas_window2world(map->canvas, x, y, &x, &y);
    gint insert_after = canvas_item_get_segment(item->item, x, y)+1;
    if(insert_after > 0) {
      /* create new node */
      node_t* node = osm_node_new(map->appdata->osm, x, y);
      osm_node_attach(map->appdata->osm, node);

      /* insert it into ways chain of nodes */
      way_t *way = item->object.way;

      /* search correct position */
      node_chain_t **chain = &way->node_chain;
      while(insert_after--) {
	g_assert(*chain);
	chain = &(*chain)->next;
      }

      /* and actually do the insertion */
      node_chain_t *new_chain_item = g_new0(node_chain_t, 1);
      new_chain_item->node = node;
      new_chain_item->next = *chain;
      *chain = new_chain_item;

      /* clear selection */
      map_item_deselect(map->appdata);

      /* remove prior version of this way */
      map_item_chain_destroy(&way->map_item_chain);

      /* draw the updated way */
      map_way_draw(map, way);

      /* remember that this node is contained in one way */
      node->ways=1;

      /* and now draw the node */
      map_node_draw(map, node);

      /* and that the way needs to be uploaded */
      OSM_FLAGS(way) |= OSM_FLAG_DIRTY;

      /* put gui into idle state */
      map_action_set(map->appdata, MAP_ACTION_IDLE);

      /* and redo it */
      map_way_select(map->appdata, way);
    }
  }
}

/* -------------------------- way_node_cut ----------------------- */

void map_edit_way_cut_highlight(map_t *map, map_item_t *item, gint x, gint y) {

  if(map_item_is_selected_way(map, item)) {
    gint nx, ny, seg;
    canvas_window2world(map->canvas, x, y, &nx, &ny);
    seg = canvas_item_get_segment(item->item, nx, ny);
    if(seg >= 0) {
      gint x0, y0, x1, y1;
      canvas_item_get_segment_pos(item->item, seg, &x0, &y0, &x1, &y1);

      gint width = (item->object.way->draw.flags &
		    OSM_DRAW_FLAG_BG)?
	2*item->object.way->draw.bg.width:
	3*item->object.way->draw.width;
      map_hl_segment_draw(map, width, x0, y0, x1, y1);
    }
  } else if(map_item_is_selected_node(map, item)) {
    /* cutting a way at its first or last node doesn't make much sense ... */
    if(!osm_way_ends_with_node(map->selected.object.way, item->object.node))
      map_hl_cursor_draw(map, item->object.node->lpos.x, item->object.node->lpos.y,
			 TRUE, 2*map->style->node.radius);
  }
}

/* cut the currently selected way at the current cursor position */
void map_edit_way_cut(map_t *map, gint x, gint y) {

  /* check if we are still hovering above the selected way */
  map_item_t *item = map_item_at(map, x, y);
  if(item && (map_item_is_selected_way(map, item) ||
	      map_item_is_selected_node(map, item))) {
    gboolean cut_at_node = map_item_is_selected_node(map, item);

    /* convert mouse position to canvas (world) position */
    canvas_window2world(map->canvas, x, y, &x, &y);

    gint cut_at = -1;
    way_t *way = NULL;
    if(cut_at_node) {
      printf("  cut at node\n");

      /* node must not be first or last node of way */
      g_assert(map->selected.object.type == WAY);

      if(!osm_way_ends_with_node(map->selected.object.way,
                                 item->object.node)) {
	way = map->selected.object.way;

	cut_at = 0;
	node_chain_t *chain = way->node_chain;
	while(chain && chain->node != item->object.node) {
	  chain = chain->next;
	  cut_at++;
	}

      } else
	printf("  won't cut as it's last or first node\n");

    } else {
      printf("  cut at segment\n");
      cut_at = canvas_item_get_segment(item->item, x, y);
      if(cut_at >= 0) way = item->object.way;
    }

    if(way) {
      /* create a duplicate of the currently selected way */
      way_t *neww = osm_way_new();

      /* if this is a closed way, reorder (rotate) it, so the */
      /* place to cut is adjecent to the begin/end of the way. */
      /* this prevents a cut polygon to be split into two ways */
      g_assert(way->node_chain);
      if(osm_way_is_closed(way)) {
	printf("CLOSED WAY -> rotate by %d\n", cut_at);
	osm_way_rotate(way, cut_at);
	cut_at = 0;
      }

      /* ------------  copy all tags ------------- */
      OSM_TAG(neww) = osm_tags_copy(OSM_TAG(way));

      /* ---- transfer relation membership from way to new ----- */
      transfer_relations(map->appdata->osm, neww, way);

      /* move parts of node_chain to the new way */
      printf("  moving everthing after segment %d to new way\n", cut_at);

      node_chain_t *chain = way->node_chain;
      while(cut_at--) {
	g_assert(chain);
	chain = chain->next;
      }

      /* attach remaining nodes to new way */
      neww->node_chain = chain->next;

      /* terminate remainig chain on old way */
      chain->next = NULL;

      /* if we cut at a node, this node is now part of both ways. so */
      /* create a copy of the last node of the old way and prepend it to */
      /* the new way */
      if(cut_at_node) {
	node_chain_t *first = g_new0(node_chain_t, 1);
	first->next = neww->node_chain;
	first->node = (node_t*)osm_way_get_last_node(way);
	first->node->ways++;
	neww->node_chain = first;
      }

      /* now move the way itself into the main data structure */
      osm_way_attach(map->appdata->osm, neww);

      /* clear selection */
      map_item_deselect(map->appdata);

      /* remove prior version of this way */
      printf("remove visible version of way #" ITEM_ID_FORMAT "\n", OSM_ID(way));
      map_item_chain_destroy(&way->map_item_chain);

      /* swap chains if the old way is to be destroyed due to a lack */
      /* of nodes */
      if(!osm_way_min_length(way, 2)) {
	printf("swapping ways to avoid destruction of original way\n");
	node_chain_t *tmp = way->node_chain;
	way->node_chain = neww->node_chain;
	neww->node_chain = tmp;
	map_way_delete(map->appdata, neww);
	neww = NULL;
      } else if(!osm_way_min_length(neww, 2)) {
	printf("new way has less than 2 nodes, deleting it\n");
	map_way_delete(map->appdata, neww);
	neww = NULL;
      }

      /* the way may still only consist of a single node. */
      /* remove it then */
      if(!osm_way_min_length(way, 2)) {
	printf("original way has less than 2 nodes left, deleting it\n");
	map_way_delete(map->appdata, way);
	item = NULL;
      } else {
	printf("original way still has %d nodes\n",
	       osm_way_number_of_nodes(way));

	/* draw the updated old way */
	josm_elemstyles_colorize_way(map->style, way);
	map_way_draw(map, way);

	/* remember that the way needs to be uploaded */
	OSM_FLAGS(way) |= OSM_FLAG_DIRTY;
      }

      if(neww != NULL) {
	/* colorize the new way before drawing */
	josm_elemstyles_colorize_way(map->style, neww);
	map_way_draw(map, neww);
      }

      /* put gui into idle state */
      map_action_set(map->appdata, MAP_ACTION_IDLE);

      /* and redo selection if way still exists */
      if(item)
	map_way_select(map->appdata, way);
      else if(neww)
	map_way_select(map->appdata, neww);
    }
  }
}

static void member_merge_operator(relation_t *relation, way_t *other)
{
  printf("way[1] is part of relation #" ITEM_ID_FORMAT "\n",
         OSM_ID(relation));

  /* make way[0] member of the same relation */

  /* walk member chain. save role of way[1] if its being found. */
  /* end search either at end of chain or if way[0] was found */
  /* as it's already a member of that relation */
  member_t **member;
  const char *role = NULL;
  for(member = &relation->member; *member; member = &(*member)->next) {
    /* save role of way[1] */
    if(((*member)->object.type == WAY) &&
       ((*member)->object.way == other)) {
      role = (*member)->role;
      break;
    }
  }

  if(*member) {
    printf("  both ways were members of this relation\n");
  } else {
    printf("  adding way[0] to relation\n");
    *member = g_new0(member_t, 1);
    (*member)->object.type = WAY;
    (*member)->object.way = other;
    (*member)->role = g_strdup(role);
    member = &(*member)->next;

    OSM_FLAGS(relation) |= OSM_FLAG_DIRTY;
  }
}

void map_edit_node_move(appdata_t *appdata, map_item_t *map_item,
			  gint ex, gint ey) {

  map_t *map = appdata->map;
  osm_t *osm = appdata->osm;

  g_assert(map_item->object.type == NODE);
  node_t *node = map_item->object.node;

  printf("released dragged node #" ITEM_ID_FORMAT "\n", OSM_ID(node));
  printf("  was at %d %d (%f %f)\n",
	 node->lpos.x, node->lpos.y,
	 node->pos.lat, node->pos.lon);


  /* check if it was dropped onto another node */
  node_t *touchnode = map_hl_touchnode_get_node(map);
  gboolean joined_with_touchnode = FALSE;

  if(touchnode) {
    map_hl_touchnode_clear(map);

    printf("  dropped onto node #" ITEM_ID_FORMAT "\n", OSM_ID(touchnode));

    if(yes_no_f(GTK_WIDGET(appdata->window),
		appdata, MISC_AGAIN_ID_JOIN_NODES, 0,
		_("Join nodes?"),
		_("Do you want to join the dragged node with the one "
		  "you dropped it on?"))) {

      /* the touchnode vanishes and is replaced by the node the */
      /* user dropped onto it */
      joined_with_touchnode = TRUE;

      /* use touchnodes position */
      node->lpos = touchnode->lpos;
      node->pos = touchnode->pos;

      way_t *way = appdata->osm->way;
      while(way) {
	node_chain_t *chain = way->node_chain;
	while(chain) {
	  if(chain->node == touchnode) {
	    printf("  found node in way #" ITEM_ID_FORMAT "\n", OSM_ID(way));

	    /* replace by node */
	    chain->node = node;

	    /* and adjust way references of both nodes */
	    node->ways++;
	    touchnode->ways--;

	    OSM_FLAGS(way) |= OSM_FLAG_DIRTY;
	  }
	  chain = chain->next;
	}
	way = way->next;
      }

      /* replace node in relations */
      relation_t *relation = appdata->osm->relation;
      while(relation) {
	member_t *member = relation->member;
	while(member) {
	  if(member->object.type == NODE && member->object.node == touchnode) {
	    printf("  found node in relation #" ITEM_ID_FORMAT "\n",
		   OSM_ID(relation));

	    /* replace by node */
	    member->object.node = node;

	    OSM_FLAGS(relation) |= OSM_FLAG_DIRTY;
	  }
	  member = member->next;
	}
	relation = relation->next;
      }

      gboolean conflict = combine_tags(&OSM_TAG(node), OSM_TAG(touchnode));
      OSM_TAG(touchnode) = NULL;

      /* touchnode must not have any references to ways anymore */
      g_assert_cmpint(touchnode->ways, ==, 0);

      /* delete touchnode */
      /* remove it visually from the screen */
      map_item_chain_destroy(&touchnode->map_item_chain);

      /* and remove it from the data structures */
      osm_node_remove_from_relation(appdata->osm, touchnode);
      osm_node_delete(appdata->osm, touchnode, FALSE, TRUE);

      /* and open dialog to resolve tag collisions if necessary */
      if(conflict)
	messagef(GTK_WIDGET(appdata->window), _("Node tag conflict"),
		 _("The resulting node contains some conflicting tags. "
		   "Please solve these."));

      /* check whether this will also join two ways */
      printf("  checking if node is end of way\n");
      guint ways2join_cnt = 0;
      way_t *ways2join[2] = { NULL, NULL };
      way = appdata->osm->way;
      while(way) {
	if(osm_way_ends_with_node(way, node)) {
	  if(ways2join_cnt < 2)
	    ways2join[ways2join_cnt] = way;

	  printf("  way #" ITEM_ID_FORMAT " ends with this node\n", OSM_ID(way));
	  ways2join_cnt++;
	}
	way = way->next;
      }

      if(ways2join_cnt > 2) {
	messagef(GTK_WIDGET(appdata->window), _("Too many ways to join"),
		 _("More than two ways now end on this node. Joining more "
		   "than two ways is not yet implemented, sorry"));

      } else if(ways2join_cnt == 2) {
	if(yes_no_f(GTK_WIDGET(appdata->window),
		    appdata, MISC_AGAIN_ID_JOIN_WAYS, 0,
		    _("Join ways?"),
		    _("Do you want to join the dragged way with the one "
		      "you dropped it on?"))) {

	  printf("  about to join ways #" ITEM_ID_FORMAT " and #"
	      ITEM_ID_FORMAT "\n", OSM_ID(ways2join[0]), OSM_ID(ways2join[1]));

	  /* way[1] gets destroyed and attached to way[0] */
	  /* so check if way[1] is selected and exchainge ways then */
	  /* so that way may stay selected */
	  if((map->selected.object.type == WAY) &&
	     (map->selected.object.way == ways2join[1])) {
	    printf("  swapping ways to keep selected one alive\n");
	    way_t *tmp = ways2join[1];
	    ways2join[1] = ways2join[0];
	    ways2join[0] = tmp;
	  }

	  /* take all nodes from way[1] and append them to way[0] */
	  /* check if we have to append or prepend to way[0] */
	  if(ways2join[0]->node_chain->node == node) {
	    /* make "prepend" to be "append" by reversing way[0] */
	    printf("  target prepend -> reverse\n");
	    osm_way_reverse(ways2join[0]);
	  }

	  /* verify the common node is last in the target way */
	  node_chain_t *chain = ways2join[0]->node_chain;
	  while(chain->next) chain = chain->next;
	  g_assert(chain->node == node);
	  g_assert(!chain->next);

	  /* common node must be first in the chain to attach */
	  if(ways2join[1]->node_chain->node != node) {
	    printf("  source reverse\n");
	    osm_way_reverse(ways2join[1]);
	  }

	  /* verify the common node is first in the source way */
	  g_assert(ways2join[1]->node_chain->node == node);

	  /* finally append source chain to target */
	  g_assert(!chain->next);
	  chain->next = ways2join[1]->node_chain->next;

	  ways2join[1]->node_chain->next = NULL;

	  /* transfer tags from touchnode to node */

	  /* ---------- transfer tags from way[1] to way[0] ----------- */
	  gboolean conflict =
	    combine_tags(&OSM_TAG(ways2join[0]), OSM_TAG(ways2join[1]));
	  OSM_TAG(ways2join[1]) = NULL;

	  /* ---- transfer relation membership from way[1] to way[0] ----- */
	  GSList *rchain =
	    osm_way_to_relation(appdata->osm, ways2join[1]);

          g_slist_foreach(rchain, (GFunc)member_merge_operator, ways2join[0]);
	  g_slist_free(rchain);

	  /* and open dialog to resolve tag collisions if necessary */
	  if(conflict)
	    messagef(GTK_WIDGET(appdata->window), _("Way tag conflict"),
		     _("The resulting way contains some conflicting tags. "
		       "Please solve these."));

	  OSM_FLAGS(ways2join[0]) |= OSM_FLAG_DIRTY;
	  map_way_delete(appdata, ways2join[1]);
	}
      }
    }
  }

  /* the node either wasn't dropped into another one (touchnode) or */
  /* the user didn't want to join the nodes */
  if(!joined_with_touchnode) {

    /* finally update dragged nodes position */

    /* convert mouse position to canvas (world) position */
    gint x, y;
    canvas_window2world(map->canvas, ex, ey, &x, &y);
    if(!osm_position_within_bounds(appdata->osm, x, y)) {
      map_outside_error(appdata);
      return;
    }

    node->lpos.x = x;
    node->lpos.y = y;

    /* convert screen position to lat/lon */
    lpos2pos(osm->bounds, &node->lpos, &node->pos);

    /* convert pos back to lpos to see rounding errors */
    pos2lpos(osm->bounds, &node->pos, &node->lpos);

    printf("  now at %d %d (%f %f)\n",
	   node->lpos.x, node->lpos.y, node->pos.lat, node->pos.lon);
  }

  /* now update the visual representation of the node */

  map_item_chain_destroy(&node->map_item_chain);
  map_node_draw(map, node);

  /* visually update ways, node is part of */
  way_t *way = osm->way;
  while(way) {
    if(osm_node_in_way(way, node)) {
      printf("  node is part of way #" ITEM_ID_FORMAT ", redraw!\n",
	     OSM_ID(way));

      /* remove prior version of this way */
      map_item_chain_destroy(&way->map_item_chain);

      /* draw current way */
      josm_elemstyles_colorize_way(map->style, way);
      map_way_draw(map, way);
    }

    way = way->next;
  }

  /* and mark the node as dirty */
  OSM_FLAGS(node) |= OSM_FLAG_DIRTY;

  /* update highlight */
  map_highlight_refresh(appdata);
}

/* -------------------------- way_reverse ----------------------- */

/* called from the "reverse" icon */
void map_edit_way_reverse(appdata_t *appdata) {
  /* work on local copy since de-selecting destroys the selection */
  map_item_t item = appdata->map->selected;

  /* deleting the selected item de-selects it ... */
  map_item_deselect(appdata);

  g_assert(item.object.type == WAY);

  osm_way_reverse(item.object.way);
  guint n_tags_flipped =
    osm_way_reverse_direction_sensitive_tags(item.object.way);
  guint n_roles_flipped =
    osm_way_reverse_direction_sensitive_roles(appdata->osm, item.object.way);

  OBJECT_FLAGS(item.object) |= OSM_FLAG_DIRTY;
  map_way_select(appdata, item.object.way);

  // Flash a message about any side-effects
  char *msg = NULL;
  if (n_tags_flipped && !n_roles_flipped) {
    msg = g_strdup_printf(ngettext("%d tag updated", "%d tags updated",
                                   n_tags_flipped),
                          n_tags_flipped);
  }
  else if (!n_tags_flipped && n_roles_flipped) {
    msg = g_strdup_printf(ngettext("%d relation updated",
                                   "%d relations updated",
                                   n_roles_flipped),
                          n_roles_flipped);
  }
  else if (n_tags_flipped && n_roles_flipped) {
    char *msg1 = g_strdup_printf(ngettext("%d tag", "%d tags",
                                          n_tags_flipped),
                                 n_tags_flipped);
    char *msg2 = g_strdup_printf(ngettext("%d relation", "%d relations",
                                          n_roles_flipped),
                                 n_roles_flipped);
    msg = g_strdup_printf(_("%s & %s updated"), msg1, msg2);
    g_free(msg1);
    g_free(msg2);
  }
  if (msg) {
    banner_show_info(appdata, msg);
    g_free(msg);
  }
}

// vim:et:ts=8:sw=2:sts=2:ai
