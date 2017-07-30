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

#include <osm2go_cpp.h>

#include <algorithm>
#include <cmath>

/* -------------------------- way_add ----------------------- */

void map_edit_way_add_begin(map_t *map, way_t *way_sel) {
  if(way_sel)
    printf("previously selected way is #" ITEM_ID_FORMAT "\n",
		     way_sel->id);

  g_assert_null(map->action.way);
  map->action.way = new way_t(1);
  map->action.extending = O2G_NULLPTR;
}

struct check_first_last_node {
  const node_t * const node;
  check_first_last_node(const node_t *n) : node(n) {}
  bool operator()(const way_t *way) {
    return way->ends_with_node(node);
  }
};

void map_edit_way_add_segment(map_t *map, gint x, gint y) {

  /* convert mouse position to canvas (world) position */
  canvas_window2world(map->canvas, x, y, &x, &y);

  /* check if this was a double click. This is the case if */
  /* the last node placed is less than 5 pixels from the current */
  /* position */
  const node_t *lnode = map->action.way->last_node();
  if(lnode && (map->state.zoom * std::sqrt((lnode->lpos.x - x) * (lnode->lpos.x - x) +
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
      printf("  re-using node #" ITEM_ID_FORMAT "\n", node->id);
      map_hl_touchnode_clear(map);

      g_assert_nonnull(map->action.way);

      /* check whether this node is first or last one of a different way */
      way_t *touch_way = O2G_NULLPTR;
      const way_chain_t &way_chain = map->appdata.osm->node_to_way(node);
      const way_chain_t::const_iterator it =
        std::find_if(way_chain.begin(), way_chain.end(), check_first_last_node(node));
      if(it != way_chain.end())
        touch_way = *it;

      /* remeber this way as this may be the last node placed */
      /* and we might want to join this with this other way */
      map->action.ends_on = touch_way;

      /* is this the first node the user places? */
      if(map->action.way->node_chain.empty()) {
	map->action.extending = touch_way;

	if(map->action.extending) {
	  if(!yes_no_f(GTK_WIDGET(map->appdata.window),
	       map->appdata, MISC_AGAIN_ID_EXTEND_WAY, 0,
	       _("Extend way?"),
	       _("Do you want to extend the way present at this location?")))
	    map->action.extending = O2G_NULLPTR;
	  else
	    /* there are immediately enough nodes for a valid way */
	    map->appdata.iconbar->map_cancel_ok(TRUE, TRUE);
	}
      }

    } else {
      /* the current way doesn't end on another way if we are just placing */
      /* a new node */
      map->action.ends_on = O2G_NULLPTR;

      if(!map->appdata.osm->position_within_bounds(x, y))
	map_outside_error(map->appdata);
      else
        node = map->appdata.osm->node_new(lpos_t(x, y));
    }

    if(node) {
      g_assert_nonnull(map->action.way);
      map->action.way->append_node(node);

      switch(map->action.way->node_chain.size()) {
      case 1:
        /* replace "place first node..." message */
        map->appdata.statusbar->set(_("Place next node of way"), FALSE);
        break;
      case 2:
        /* two nodes are enough for a valid way */
        map->appdata.iconbar->map_cancel_ok(TRUE, TRUE);
        break;
      }

      /* remove prior version of this way */
      map_item_chain_destroy(map->action.way->map_item_chain);

      /* draw current way */
      josm_elemstyles_colorize_way(map->style, map->action.way);
      map->draw(map->action.way);
    }
  }
}

struct map_unref_ways {
  osm_t * const osm;
  map_unref_ways(osm_t *o) : osm(o) {}
  void operator()(node_t *node);
};

void map_unref_ways::operator()(node_t* node)
{
  printf("    node #" ITEM_ID_FORMAT " (used by %u)\n",
         node->id, node->ways);

  g_assert_cmpuint(node->ways, >, 0);
  node->ways--;
  if(!node->ways && (node->id == ID_ILLEGAL)) {
    printf("      -> freeing temp node\n");
    delete node;
  }
}

void map_edit_way_add_cancel(map_t *map) {
  osm_t *osm = map->appdata.osm;
  g_assert_nonnull(osm);

  printf("  removing temporary way\n");
  g_assert_nonnull(map->action.way);

  /* remove all nodes that have been created for this way */
  /* (their way count will be 0 after removing the way) */
  node_chain_t &chain = map->action.way->node_chain;
  std::for_each(chain.begin(), chain.end(), map_unref_ways(osm));
  chain.clear();

  /* remove ways visual representation */
  map_item_chain_destroy(map->action.way->map_item_chain);

  osm->way_free(map->action.way);
  map->action.way = O2G_NULLPTR;
}

struct map_draw_nodes {
  map_t * const map;
  map_draw_nodes(map_t *m) : map(m) {}
  void operator()(node_t *node);
};

void map_draw_nodes::operator()(node_t* node)
{
  printf("    node #" ITEM_ID_FORMAT " (used by %u)\n",
         node->id, node->ways);

  if(node->id != ID_ILLEGAL) {
    /* a node may have been a stand-alone node before, so remove its */
    /* visual representation as its now drawn as part of the way */
    /* (if at all) */
    map_item_chain_destroy(node->map_item_chain);
  } else {
    /* we can be sure that no node gets inserted twice (even if twice in */
    /* the ways chain) because it gets assigned a non-ID_ILLEGAL id when */
    /* being moved to the osm node chain */
    map->appdata.osm->node_attach(node);
  }

  map->draw(node);
}

void map_edit_way_add_ok(map_t *map) {
  osm_t *osm = map->appdata.osm;

  g_assert_nonnull(osm);
  g_assert_nonnull(map->action.way);

  /* transfer all nodes that have been created for this way */
  /* into the node chain */

  /* (their way count will be 0 after removing the way) */
  node_chain_t &chain = map->action.way->node_chain;
  std::for_each(chain.begin(), chain.end(), map_draw_nodes(map));

  /* attach to existing way if the user requested so */
  if(map->action.extending) {
    // this is triggered when the user started with extending an existing way
    // since the merged way is a temporary one there are no relation memberships
    map->action.extending->merge(map->action.way, osm, false);

    map->action.way = map->action.extending;
  } else {
    /* now move the way itself into the main data structure */
    map->appdata.osm->way_attach(map->action.way);
  }

  /* we might already be working on the "ends_on" way as we may */
  /* be extending it. Joining the same way doesn't make sense. */
  if(map->action.ends_on && (map->action.ends_on == map->action.way)) {
    printf("  the new way ends on itself -> don't join itself\n");
    map->action.ends_on = O2G_NULLPTR;
  }

  if(map->action.ends_on &&
     yes_no_f(GTK_WIDGET(map->appdata.window),
              map->appdata, MISC_AGAIN_ID_EXTEND_WAY_END, 0,
              _("Join way?"),
              _("Do you want to join the way present at this location?"))) {
    printf("  this new way ends on another way\n");
    // this is triggered when the new way ends on an existing way, this can
    // happen even if an existing way was extended before

    /* this is slightly more complex as this time two full tagged */
    /* ways may be involved as the new way may be an extended existing */
    /* way being connected to another way. This happens if you connect */
    /* two existing ways using a new way between them */

    /* and open dialog to resolve tag collisions if necessary */
    if(map->action.way->tags.merge(map->action.ends_on->tags))
      messagef(GTK_WIDGET(map->appdata.window), _("Way tag conflict"),
	       _("The resulting way contains some conflicting tags. "
		 "Please solve these."));

    map->action.way->merge(map->action.ends_on, osm, true);
    map->action.ends_on = O2G_NULLPTR;
  }

  /* remove prior version of this way */
  map_item_chain_destroy(map->action.way->map_item_chain);

  /* draw the updated way */
  map->draw(map->action.way);

  map->select_way(map->action.way);

  map->action.way = O2G_NULLPTR;

  /* let the user specify some tags for the new way */
  info_dialog(GTK_WIDGET(map->appdata.window), map->appdata);
}

/* -------------------------- way_node_add ----------------------- */

void map_edit_way_node_add_highlight(map_t *map, map_item_t *item,
				     gint x, gint y) {
  if(map->item_is_selected_way(item)) {
    gint nx, ny;
    canvas_window2world(map->canvas, x, y, &nx, &ny);
    if(canvas_item_get_segment(item->item, nx, ny) >= 0)
      map_hl_cursor_draw(map, x, y, false, map->style->node.radius);
  }
}

void map_edit_way_node_add(map_t *map, gint x, gint y) {
  /* check if we are still hovering above the selected way */
  map_item_t *item = map_item_at(map, x, y);
  if(map->item_is_selected_way(item)) {
    /* convert mouse position to canvas (world) position */
    canvas_window2world(map->canvas, x, y, &x, &y);
    gint insert_after = canvas_item_get_segment(item->item, x, y)+1;
    if(insert_after > 0) {
      /* create new node */
      node_t* node = map->appdata.osm->node_new(lpos_t(x, y));
      map->appdata.osm->node_attach(node);

      /* insert it into ways chain of nodes */
      way_t *way = item->object.way;

      /* search correct position */
      way->node_chain.insert(way->node_chain.begin() + insert_after + 1, node);

      /* clear selection */
      map->item_deselect();

      /* remove prior version of this way */
      map_item_chain_destroy(way->map_item_chain);

      /* draw the updated way */
      map->draw(way);

      /* remember that this node is contained in one way */
      node->ways=1;

      /* and now draw the node */
      map->draw(node);

      /* and that the way needs to be uploaded */
      way->flags |= OSM_FLAG_DIRTY;

      /* put gui into idle state */
      map->set_action(MAP_ACTION_IDLE);

      /* and redo it */
      map->select_way(way);
    }
  }
}

/* -------------------------- way_node_cut ----------------------- */

void map_edit_way_cut_highlight(map_t *map, map_item_t *item, gint x, gint y) {

  if(map->item_is_selected_way(item)) {
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
  } else if(map->item_is_selected_node(item)) {
    /* cutting a way at its first or last node doesn't make much sense ... */
    if(!map->selected.object.way->ends_with_node(item->object.node))
      map_hl_cursor_draw(map, item->object.node->lpos.x, item->object.node->lpos.y,
			 true, 2*map->style->node.radius);
  }
}

/* cut the currently selected way at the current cursor position */
void map_edit_way_cut(map_t *map, gint x, gint y) {

  /* check if we are still hovering above the selected way */
  map_item_t *item = map_item_at(map, x, y);
  bool cut_at_node = map->item_is_selected_node(item);

  if(!map->item_is_selected_way(item) && !cut_at_node)
    return;

  /* convert mouse position to canvas (world) position */
  canvas_window2world(map->canvas, x, y, &x, &y);

  node_chain_t::iterator cut_at;
  way_t *way = O2G_NULLPTR;
  if(cut_at_node) {
    printf("  cut at node\n");

    /* node must not be first or last node of way */
    g_assert(map->selected.object.type == WAY);

    if(!map->selected.object.way->ends_with_node(item->object.node)) {
      way = map->selected.object.way;

      cut_at = std::find(way->node_chain.begin(), way->node_chain.end(),
                         item->object.node);
    } else {
      printf("  won't cut as it's last or first node\n");
      return;
    }

  } else {
    printf("  cut at segment\n");
    gint c = canvas_item_get_segment(item->item, x, y);
    if(c < 0)
      return;
    way = item->object.way;
    // add one since to denote the end of the segment
    cut_at = way->node_chain.begin() + c + 1;
  }

  g_assert_nonnull(way);

  /* if this is a closed way, reorder (rotate) it, so the */
  /* place to cut is adjacent to the begin/end of the way. */
  /* this prevents a cut polygon to be split into two ways */
  if(way->is_closed()) {
    printf("CLOSED WAY -> rotate by %zi\n", cut_at - way->node_chain.begin());
    way->rotate(cut_at);
    cut_at = way->node_chain.begin();
  }

  /* move parts of node_chain to the new way */
  printf("  moving everthing after segment %zi to new way\n",
         cut_at - way->node_chain.begin());

  /* clear selection */
  map->item_deselect();

  /* remove prior version of this way */
  printf("remove visible version of way #" ITEM_ID_FORMAT "\n", way->id);
  map_item_chain_destroy(way->map_item_chain);

  /* create a duplicate of the currently selected way */
  way_t *neww = way->split(map->appdata.osm, cut_at, cut_at_node);

  /* remove new way if it is too short */
  if(neww->node_chain.size() < 2) {
    printf("new way has less than 2 nodes, deleting it\n");
    map->delete_way(neww);
    neww = O2G_NULLPTR;
  }

  /* the way may still only consist of a single node. */
  /* remove it then */
  if(way->node_chain.size() < 2) {
    printf("original way has less than 2 nodes left, deleting it\n");
    map->delete_way(way);
    item = O2G_NULLPTR;
  } else {
    printf("original way still has %zu nodes\n", way->node_chain.size());

    /* draw the updated old way */
    josm_elemstyles_colorize_way(map->style, way);
    map->draw(way);
  }

  if(neww != O2G_NULLPTR) {
    /* colorize the new way before drawing */
    josm_elemstyles_colorize_way(map->style, neww);
    map->draw(neww);
  }

  /* put gui into idle state */
  map->set_action(MAP_ACTION_IDLE);

  /* and redo selection if way still exists */
  if(item)
    map->select_way(way);
  else if(neww)
    map->select_way(neww);
}

struct member_merge {
  const object_t other;
  member_merge(way_t *o) : other(o) {}
  void operator()(relation_t *relation);
};

void member_merge::operator()(relation_t *relation)
{
  printf("way[1] is part of relation #" ITEM_ID_FORMAT "\n",
         relation->id);

  /* make way[0] member of the same relation */

  /* walk member chain. save role of way[1] if its being found. */
  /* end search either at end of chain or if way[0] was found */
  /* as it's already a member of that relation */
  std::vector<member_t>::iterator mit =
               relation->find_member_object(other);

  if(mit != relation->members.end()) {
    printf("  both ways were members of this relation\n");
  } else {
    printf("  adding way[0] to relation\n");
    member_t member(other);
    member.role = g_strdup(mit->role);
    relation->members.push_back(member);

    relation->flags |= OSM_FLAG_DIRTY;
  }
}

struct redraw_way {
  node_t * const node;
  map_t * const map;
  redraw_way(node_t *n, map_t *m) : node(n), map(m) {}
  void operator()(const std::pair<item_id_t, way_t *> &p);
};

void redraw_way::operator()(const std::pair<item_id_t, way_t *> &p)
{
  way_t * const way = p.second;
  if(!way->contains_node(node))
    return;

  printf("  node is part of way #" ITEM_ID_FORMAT ", redraw!\n", way->id);

  /* remove prior version of this way */
  map_item_chain_destroy(way->map_item_chain);

  /* draw current way */
  josm_elemstyles_colorize_way(map->style, way);
  map->draw(way);
}

struct find_way_ends {
  unsigned int &ways2join_cnt;
  const node_t * const node;
  way_t ** const ways2join;
  find_way_ends(unsigned int &w, const node_t *n, way_t **j)
    : ways2join_cnt(w), node(n), ways2join(j) {}
  void operator()(const std::pair<item_id_t, way_t *> &p);
};

void find_way_ends::operator()(const std::pair<item_id_t, way_t *>& p)
{
  way_t * const way = p.second;
  if(!way->ends_with_node(node))
    return;

  if(ways2join_cnt < 2)
    ways2join[ways2join_cnt] = way;

  printf("  way #" ITEM_ID_FORMAT " ends with this node\n", way->id);
  ways2join_cnt++;
}

void map_edit_node_move(map_t *map, map_item_t *map_item, gint ex, gint ey) {
  osm_t *osm = map->appdata.osm;

  g_assert(map_item->object.type == NODE);
  node_t *node = map_item->object.node;

  printf("released dragged node #" ITEM_ID_FORMAT "\n", node->id);
  printf("  was at %d %d (%f %f)\n",
	 node->lpos.x, node->lpos.y,
	 node->pos.lat, node->pos.lon);


  /* check if it was dropped onto another node */
  node_t *touchnode = map_hl_touchnode_get_node(map);
  bool joined_with_touchnode = false;

  if(touchnode) {
    map_hl_touchnode_clear(map);

    printf("  dropped onto node #" ITEM_ID_FORMAT "\n", touchnode->id);

    if(yes_no_f(GTK_WIDGET(map->appdata.window),
		map->appdata, MISC_AGAIN_ID_JOIN_NODES, 0,
		_("Join nodes?"),
		_("Do you want to join the dragged node with the one "
		  "you dropped it on?"))) {

      /* the touchnode vanishes and is replaced by the node the */
      /* user dropped onto it */
      joined_with_touchnode = true;
      bool conflict;

      node = osm->mergeNodes(node, touchnode, conflict);

      const std::map<item_id_t, way_t *>::iterator witEnd = osm->ways.end();

      /* and open dialog to resolve tag collisions if necessary */
      if(conflict)
        messagef(GTK_WIDGET(map->appdata.window), _("Node tag conflict"),
		 _("The resulting node contains some conflicting tags. "
		   "Please solve these."));

      /* check whether this will also join two ways */
      printf("  checking if node is end of way\n");
      unsigned int ways2join_cnt = 0;
      way_t *ways2join[2] = { O2G_NULLPTR, O2G_NULLPTR };
      std::for_each(osm->ways.begin(), witEnd,
                    find_way_ends(ways2join_cnt, node, ways2join));

      if(ways2join_cnt > 2) {
        messagef(GTK_WIDGET(map->appdata.window), _("Too many ways to join"),
		 _("More than two ways now end on this node. Joining more "
		   "than two ways is not yet implemented, sorry"));

      } else if(ways2join_cnt == 2) {
        if(yes_no_f(GTK_WIDGET(map->appdata.window),
                    map->appdata, MISC_AGAIN_ID_JOIN_WAYS, 0,
		    _("Join ways?"),
		    _("Do you want to join the dragged way with the one "
		      "you dropped it on?"))) {

	  printf("  about to join ways #" ITEM_ID_FORMAT " and #"
	      ITEM_ID_FORMAT "\n", ways2join[0]->id, ways2join[1]->id);

	  /* way[1] gets destroyed and attached to way[0] */
	  /* so check if way[1] is selected and exchainge ways then */
	  /* so that way may stay selected */
	  if((map->selected.object == ways2join[1])) {
	    printf("  swapping ways to keep selected one alive\n");
	    way_t *tmp = ways2join[1];
	    ways2join[1] = ways2join[0];
	    ways2join[0] = tmp;
	  }

	  /* take all nodes from way[1] and append them to way[0] */
	  /* check if we have to append or prepend to way[0] */
	  if(ways2join[0]->node_chain.front() == node) {
	    /* make "prepend" to be "append" by reversing way[0] */
	    printf("  target prepend -> reverse\n");
	    ways2join[0]->reverse();
	  }

	  /* verify the common node is last in the target way */
	  node_chain_t &chain = ways2join[0]->node_chain;
	  g_assert(chain.back() == node);

	  /* common node must be first in the chain to attach */
	  if(ways2join[1]->node_chain.front() != node) {
	    printf("  source reverse\n");
	    ways2join[1]->reverse();
	  }

	  /* verify the common node is first in the source way */
	  g_assert(ways2join[1]->node_chain.front() == node);

	  /* finally append source chain to target */
	  chain.insert(chain.end(), ways2join[1]->node_chain.begin()++, ways2join[1]->node_chain.end());

	  ways2join[1]->node_chain.resize(1);

	  /* ---------- transfer tags from way[1] to way[0] ----------- */
	  bool conflict = ways2join[0]->tags.merge(ways2join[1]->tags);

	  /* ---- transfer relation membership from way[1] to way[0] ----- */
          const relation_chain_t &rchain = osm->to_relation(ways2join[1]);

	  std::for_each(rchain.begin(), rchain.end(), member_merge(ways2join[0]));

	  /* and open dialog to resolve tag collisions if necessary */
	  if(conflict)
            messagef(GTK_WIDGET(map->appdata.window), _("Way tag conflict"),
		     _("The resulting way contains some conflicting tags. "
		       "Please solve these."));

	  ways2join[0]->flags |= OSM_FLAG_DIRTY;
          map->delete_way(ways2join[1]);
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
    if(!osm->position_within_bounds(x, y)) {
      map_outside_error(map->appdata);
      return;
    }

    node->lpos.x = x;
    node->lpos.y = y;

    /* convert screen position to lat/lon */
    node->pos = node->lpos.toPos(*(osm->bounds));

    /* convert pos back to lpos to see rounding errors */
    node->lpos = node->pos.toLpos(*(osm->bounds));

    printf("  now at %d %d (%f %f)\n",
	   node->lpos.x, node->lpos.y, node->pos.lat, node->pos.lon);
  }

  /* now update the visual representation of the node */

  map_item_chain_destroy(node->map_item_chain);
  map->draw(node);

  /* visually update ways, node is part of */
  std::for_each(osm->ways.begin(), osm->ways.end(), redraw_way(node, map));

  /* and mark the node as dirty */
  node->flags |= OSM_FLAG_DIRTY;

  /* update highlight */
  map->highlight_refresh();
}

/* -------------------------- way_reverse ----------------------- */

/* called from the "reverse" icon */
void map_edit_way_reverse(map_t *map) {
  /* work on local copy since de-selecting destroys the selection */
  map_item_t item = map->selected;

  /* deleting the selected item de-selects it ... */
  map->item_deselect();

  g_assert(item.object.type == WAY);

  item.object.way->reverse();
  unsigned int n_tags_flipped =
    item.object.way->reverse_direction_sensitive_tags();
  unsigned int n_roles_flipped =
    item.object.way->reverse_direction_sensitive_roles(map->appdata.osm);

  item.object.obj->flags |= OSM_FLAG_DIRTY;
  map->select_way(item.object.way);

  // Flash a message about any side-effects
  gchar *msg = O2G_NULLPTR;
  if (n_tags_flipped && !n_roles_flipped) {
    msg = g_strdup_printf(ngettext("%u tag updated", "%u tags updated",
                                   n_tags_flipped),
                          n_tags_flipped);
  }
  else if (!n_tags_flipped && n_roles_flipped) {
    msg = g_strdup_printf(ngettext("%u relation updated",
                                   "%u relations updated",
                                   n_roles_flipped),
                          n_roles_flipped);
  }
  else if (n_tags_flipped && n_roles_flipped) {
    gchar *msg1 = g_strdup_printf(ngettext("%u tag", "%u tags",
                                          n_tags_flipped),
                                 n_tags_flipped);
    gchar *msg2 = g_strdup_printf(ngettext("%u relation", "%u relations",
                                          n_roles_flipped),
                                 n_roles_flipped);
    msg = g_strdup_printf(_("%s & %s updated"), msg1, msg2);
    g_free(msg1);
    g_free(msg2);
  }
  if (msg) {
    banner_show_info(map->appdata, msg);
    g_free(msg);
  }
}

// vim:et:ts=8:sw=2:sts=2:ai
