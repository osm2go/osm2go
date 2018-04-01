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

#include "map_edit.h"

#include "appdata.h"
#include "iconbar.h"
#include "info.h"
#include "map.h"
#include "map_hl.h"
#include "misc.h"
#include "notifications.h"
#include "project.h"
#include "style.h"
#include "uicontrol.h"

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>

#include <algorithm>
#include <cassert>
#include <cmath>

/* -------------------------- way_add ----------------------- */

void map_edit_way_add_begin(map_t *map) {
  assert_null(map->action.way);
  map->action.way = new way_t(0);
  map->action.extending = nullptr;
}

struct check_first_last_node {
  const node_t * const node;
  explicit check_first_last_node(const node_t *n) : node(n) {}
  bool operator()(const std::pair<item_id_t, way_t *> &p) {
    return p.second->ends_with_node(node);
  }
};

void map_edit_way_add_segment(map_t *map, int x, int y) {

  /* convert mouse position to canvas (world) position */
  lpos_t pos = map->canvas->window2world(x, y);

  /* check if this was a double click. This is the case if */
  /* the last node placed is less than 5 pixels from the current */
  /* position */
  const node_t *lnode = map->action.way->last_node();
  if(lnode && (map->state.zoom * std::sqrt((lnode->lpos.x - pos.x) * (lnode->lpos.x - pos.x) +
                                           (lnode->lpos.y - pos.y) * (lnode->lpos.y - pos.y))) < 5) {
#if 0
    printf("detected double click -> simulate ok click\n");
    map->touchnode_clear();
    map_action_ok(map->appdata);
#else
    printf("detected double click -> ignore it as accidential\n");
#endif
  } else {

    /* use the existing node if one was touched */
    node_t *node = map->touchnode_get_node();
    osm_t::ref osm = map->appdata.project->osm;
    if(node != nullptr) {
      printf("  re-using node #" ITEM_ID_FORMAT "\n", node->id);

      assert(map->action.way != nullptr);

      /* check whether this node is first or last one of a different way */
      way_t *touch_way = nullptr;
      touch_way = osm->find_way(check_first_last_node(node));

      /* remeber this way as this may be the last node placed */
      /* and we might want to join this with this other way */
      map->action.ends_on = touch_way;

      /* is this the first node the user places? */
      if(map->action.way->node_chain.empty()) {
	map->action.extending = touch_way;

	if(map->action.extending) {
          if(!yes_no_f(nullptr, MISC_AGAIN_ID_EXTEND_WAY, _("Extend way?"),
	       _("Do you want to extend the way present at this location?")))
	    map->action.extending = nullptr;
	  else
	    /* there are immediately enough nodes for a valid way */
	    map->appdata.iconbar->map_cancel_ok(true, true);
	}
      }

    } else {
      /* the current way doesn't end on another way if we are just placing */
      /* a new node */
      map->action.ends_on = nullptr;

      if(!osm->bounds.contains(pos))
        map_t::outside_error();
      else
        node = osm->node_new(pos);
    }

    if(node) {
      assert(map->action.way != nullptr);
      map->action.way->append_node(node);

      switch(map->action.way->node_chain.size()) {
      case 1:
        /* replace "place first node..." message */
        map->appdata.uicontrol->showNotification(_("Place next node of way"));
        break;
      case 2:
        /* two nodes are enough for a valid way */
        map->appdata.iconbar->map_cancel_ok(true, true);
        break;
      }

      /* remove prior version of this way */
      map_item_chain_destroy(map->action.way->map_item_chain);

      /* draw current way */
      map->style->colorize_way(map->action.way);
      map->draw(map->action.way);
    }
  }
}

struct map_unref_ways {
  osm_t::ref osm;
  explicit map_unref_ways(osm_t::ref o) : osm(o) {}
  void operator()(node_t *node);
};

void map_unref_ways::operator()(node_t* node)
{
  printf("    node #" ITEM_ID_FORMAT " (used by %u)\n",
         node->id, node->ways);

  assert_cmpnum_op(node->ways, >, 0);
  node->ways--;
  if(!node->ways && (node->id == ID_ILLEGAL)) {
    printf("      -> freeing temp node\n");
    delete node;
  }
}

void map_edit_way_add_cancel(map_t *map) {
  osm_t::ref osm = map->appdata.project->osm;
  assert(osm);

  printf("  removing temporary way\n");
  assert(map->action.way != nullptr);

  /* remove all nodes that have been created for this way */
  /* (their way count will be 0 after removing the way) */
  node_chain_t &chain = map->action.way->node_chain;
  std::for_each(chain.begin(), chain.end(), map_unref_ways(osm));
  chain.clear();

  /* remove ways visual representation */
  map_item_chain_destroy(map->action.way->map_item_chain);

  osm->way_free(map->action.way);
  map->action.way = nullptr;
}

struct map_draw_nodes {
  map_t * const map;
  explicit map_draw_nodes(map_t *m) : map(m) {}
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
    map->appdata.project->osm->node_attach(node);
  }

  map->draw(node);
}

void map_edit_way_add_ok(map_t *map) {
  osm_t::ref osm = map->appdata.project->osm;

  assert(osm);
  assert(map->action.way != nullptr);

  /* transfer all nodes that have been created for this way */
  /* into the node chain */

  /* (their way count will be 0 after removing the way) */
  node_chain_t &chain = map->action.way->node_chain;
  std::for_each(chain.begin(), chain.end(), map_draw_nodes(map));

  /* attach to existing way if the user requested so */
  if(map->action.extending) {
    // this is triggered when the user started with extending an existing way
    // since the merged way is a temporary one there are no relation memberships
    map->action.extending->merge(map->action.way, osm);

    map->action.way = map->action.extending;
  } else {
    /* now move the way itself into the main data structure */
    osm->way_attach(map->action.way);
  }

  /* we might already be working on the "ends_on" way as we may */
  /* be extending it. Joining the same way doesn't make sense. */
  if(map->action.ends_on && (map->action.ends_on == map->action.way)) {
    printf("  the new way ends on itself -> don't join itself\n");
    map->action.ends_on = nullptr;
  }

  if(map->action.ends_on &&
     yes_no_f(nullptr, MISC_AGAIN_ID_EXTEND_WAY_END, _("Join way?"),
              _("Do you want to join the way present at this location?"))) {
    printf("  this new way ends on another way\n");
    // this is triggered when the new way ends on an existing way, this can
    // happen even if an existing way was extended before

    /* this is slightly more complex as this time two full tagged */
    /* ways may be involved as the new way may be an extended existing */
    /* way being connected to another way. This happens if you connect */
    /* two existing ways using a new way between them */

    bool conflict;
    map->action.way = osm->mergeWays(map->action.way, map->action.ends_on, conflict);
    map->action.ends_on = nullptr;

    if(conflict)
      message_dlg(_("Way tag conflict"),
                  _("The resulting way contains some conflicting tags. Please solve these."));
  }

  /* remove prior version of this way */
  map_item_chain_destroy(map->action.way->map_item_chain);

  /* draw the updated way */
  map->draw(map->action.way);

  map->select_way(map->action.way);

  map->action.way = nullptr;

  /* let the user specify some tags for the new way */
  info_dialog(appdata_t::window, map, osm, map->appdata.presets.get());
}

/* -------------------------- way_node_add ----------------------- */

void map_edit_way_node_add_highlight(map_t *map, map_item_t *item,
                                     int x, int y) {
  if(map->item_is_selected_way(item)) {
    lpos_t pos = map->canvas->window2world(x, y);
    if(item->get_segment(pos) >= 0)
      map_hl_cursor_draw(map, pos, map->style->node.radius);
  }
}

void map_edit_way_node_add(map_t *map, int px, int py) {
  /* check if we are still hovering above the selected way */
  map_item_t *item = map->item_at(px, py);
  if(map->item_is_selected_way(item)) {
    /* convert mouse position to canvas (world) position */
    lpos_t pos = map->canvas->window2world(px, py);
    int insert_after = item->get_segment(pos) + 1;
    if(insert_after > 0) {
      /* create new node */
      node_t* node = map->appdata.project->osm->node_new(pos);
      map->appdata.project->osm->node_attach(node);

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

void map_edit_way_cut_highlight(map_t *map, map_item_t *item, int x, int y) {

  if(map->item_is_selected_way(item)) {
    lpos_t pos = map->canvas->window2world(x, y);
    int seg = item->get_segment(pos);
    if(seg >= 0) {
      unsigned int width = (item->object.way->draw.flags & OSM_DRAW_FLAG_BG) ?
	2*item->object.way->draw.bg.width:
	3*item->object.way->draw.width;
      std::vector<lpos_t> coords(2);
      coords[0] = item->object.way->node_chain[seg]->lpos;
      coords[1] = item->object.way->node_chain[seg + 1]->lpos;
      map_hl_segment_draw(map, width, coords);
    }
  } else if(map->item_is_selected_node(item)) {
    /* cutting a way at its first or last node doesn't make much sense ... */
    if(!map->selected.object.way->ends_with_node(item->object.node))
      map_hl_cursor_draw(map, item->object.node->lpos, 2 * map->style->node.radius);
  }
}

/* cut the currently selected way at the current cursor position */
void map_edit_way_cut(map_t *map, int px, int py) {

  /* check if we are still hovering above the selected way */
  map_item_t *item = map->item_at(px, py);
  bool cut_at_node = map->item_is_selected_node(item);

  if(!map->item_is_selected_way(item) && !cut_at_node)
    return;

  /* convert mouse position to canvas (world) position */
  lpos_t pos = map->canvas->window2world(px, py);

  node_chain_t::iterator cut_at;
  way_t *way = nullptr;
  if(cut_at_node) {
    printf("  cut at node\n");

    /* node must not be first or last node of way */
    assert(map->selected.object.type == object_t::WAY);

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
    int c = item->get_segment(pos);
    if(c < 0)
      return;
    way = item->object.way;
    // add one since to denote the end of the segment
    cut_at = way->node_chain.begin() + c + 1;
  }

  assert(way != nullptr);
  assert_cmpnum_op(way->node_chain.size(), >, 2);

  /* move parts of node_chain to the new way */
  printf("  moving everthing after segment %zi to new way\n",
         cut_at - way->node_chain.begin());

  /* clear selection */
  map->item_deselect();

  /* remove prior version of this way */
  printf("remove visible version of way #" ITEM_ID_FORMAT "\n", way->id);
  map_item_chain_destroy(way->map_item_chain);

  /* create a duplicate of the currently selected way */
  way_t * const neww = way->split(map->appdata.project->osm, cut_at, cut_at_node);

  printf("original way still has %zu nodes\n", way->node_chain.size());

  /* draw the updated old way */
  map->style->colorize_way(way);
  map->draw(way);

  if(neww != nullptr) {
    /* colorize the new way before drawing */
    map->style->colorize_way(neww);
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
  map->style->colorize_way(way);
  map->draw(way);
}

struct find_way_ends {
  const node_t * const node;
  explicit find_way_ends(const node_t *n) : node(n) {}
  bool operator()(const std::pair<item_id_t, way_t *> &p) {
    return p.second->ends_with_node(node);
  }
};

void map_edit_node_move(map_t *map, map_item_t *map_item, int ex, int ey) {
  osm_t::ref osm = map->appdata.project->osm;

  assert(map_item->object.type == object_t::NODE);
  node_t *node = map_item->object.node;

  printf("released dragged node #" ITEM_ID_FORMAT "\n", node->id);
  printf("  was at %d %d (%f %f)\n",
	 node->lpos.x, node->lpos.y,
	 node->pos.lat, node->pos.lon);


  /* check if it was dropped onto another node */
  node_t *touchnode = map->touchnode_get_node();
  bool joined_with_touchnode = false;

  if(touchnode != nullptr) {
    printf("  dropped onto node #" ITEM_ID_FORMAT "\n", touchnode->id);

    if(yes_no_f(nullptr, MISC_AGAIN_ID_JOIN_NODES, _("Join nodes?"),
                _("Do you want to join the dragged node with the one you dropped it on?"))) {
      /* the touchnode vanishes and is replaced by the node the */
      /* user dropped onto it */
      joined_with_touchnode = true;
      bool conflict;
      unsigned int ways2join_cnt = 0;

      // only offer to join ways if they come from the different nodes, not
      // if e.g. one node has 2 ways and the other has none
      way_t *ways2join[2] = { nullptr, nullptr };
      if(node->ways > 0 && touchnode->ways > 0) {
        ways2join_cnt = node->ways + touchnode->ways;
        if(ways2join_cnt == 2) {
          const std::map<item_id_t, way_t *>::iterator witEnd = osm->ways.end();
          const std::map<item_id_t, way_t *>::iterator witBegin = osm->ways.begin();
          const std::map<item_id_t, way_t *>::iterator way0It = std::find_if(witBegin, witEnd,
                                                                             find_way_ends(node));
          const std::map<item_id_t, way_t *>::iterator way1It = std::find_if(witBegin, witEnd,
                                                                             find_way_ends(touchnode));
          assert(way0It != witEnd);
          assert(way1It != witEnd);
          ways2join[0] = way0It->second;
          ways2join[1] = way1It->second;
        }
      }

      node = osm->mergeNodes(node, touchnode, conflict);
      // make sure the object marked as selected is the surviving node
      map->selected.object = node;

      /* and open dialog to resolve tag collisions if necessary */
      if(conflict)
        message_dlg(_("Node tag conflict"),
                    _("The resulting node contains some conflicting tags. Please solve these."));

      /* check whether this will also join two ways */
      printf("  checking if node is end of way\n");

      if(ways2join_cnt > 2) {
        message_dlg(_("Too many ways to join"),
                    _("More than two ways now end on this node. Joining more "
                      "than two ways is not yet implemented, sorry"));
      } else if(ways2join_cnt == 2 &&
                yes_no_f(nullptr, MISC_AGAIN_ID_JOIN_WAYS, _("Join ways?"),
                         _("Do you want to join the dragged way with the one you dropped it on?"))) {
        printf("  about to join ways #" ITEM_ID_FORMAT " and #" ITEM_ID_FORMAT "\n",
               ways2join[0]->id, ways2join[1]->id);

        osm->mergeWays(ways2join[0], ways2join[1], conflict);
        if(conflict)
          message_dlg(_("Way tag conflict"),
                      _("The resulting way contains some conflicting tags. Please solve these."));
      }
    }
  }

  /* the node either wasn't dropped into another one (touchnode) or */
  /* the user didn't want to join the nodes */
  if(!joined_with_touchnode) {

    /* finally update dragged nodes position */

    /* convert mouse position to canvas (world) position */
    lpos_t pos = map->canvas->window2world(ex, ey);
    if(!osm->bounds.contains(pos)) {
      map_t::outside_error();
      return;
    }

    /* convert screen position to lat/lon */
    node->pos = pos.toPos(osm->bounds);

    /* convert pos back to lpos to see rounding errors */
    node->lpos = node->pos.toLpos(osm->bounds);

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

  assert(item.object.type == object_t::WAY);

  unsigned int n_tags_flipped;
  unsigned int n_roles_flipped;
  item.object.way->reverse(map->appdata.project->osm, n_tags_flipped, n_roles_flipped);

  map->select_way(item.object.way);

  // Flash a message about any side-effects
  g_string msg;
  if (n_tags_flipped && !n_roles_flipped) {
    msg.reset(g_strdup_printf(ngettext("%u tag updated", "%u tags updated",
                                       n_tags_flipped), n_tags_flipped));
  }
  else if (!n_tags_flipped && n_roles_flipped) {
    msg.reset(g_strdup_printf(ngettext("%u relation updated", "%u relations updated",
                                       n_roles_flipped), n_roles_flipped));
  }
  else if (n_tags_flipped && n_roles_flipped) {
    g_string msg1(g_strdup_printf(ngettext("%u tag", "%u tags",
                                           n_tags_flipped), n_tags_flipped));
    g_string msg2(g_strdup_printf(ngettext("%u relation", "%u relations",
                                          n_roles_flipped), n_roles_flipped));
    msg.reset(g_strdup_printf(_("%s & %s updated"), msg1.get(), msg2.get()));
  }
  if (msg)
    map->appdata.uicontrol->showNotification(msg.get(), MainUi::Brief);
}

// vim:et:ts=8:sw=2:sts=2:ai
