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
#include "iconbar.h"
#include "notifications.h"
#include "project.h"
#include "style.h"
#include "uicontrol.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <utility>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_platform.h>

/* -------------------------- way_add ----------------------- */

void map_t::way_add_begin() {
  assert_null(action.way);
  action.way = new way_t(0);
  action.extending = nullptr;
}

namespace {

class check_first_last_node {
  const node_t * const node;
public:
  explicit check_first_last_node(const node_t *n) : node(n) {}
  bool operator()(const std::pair<item_id_t, way_t *> &p) {
    return p.second->ends_with_node(node);
  }
};

} // namespace

void map_t::way_add_segment(lpos_t pos) {
  /* check if this was a double click. This is the case if */
  /* the last node placed is less than 5 pixels from the current */
  /* position */
  const node_t *lnode = action.way->last_node();
  if(lnode != nullptr) {
    const lpos_t lnpos = lnode->lpos;

    if(appdata.project->map_state.zoom * std::sqrt(static_cast<float>((lnpos.x - pos.x) * (lnpos.x - pos.x) +
                              (lnpos.y - pos.y) * (lnpos.y - pos.y))) < 5.0f) {
#if 0
      printf("detected double click -> simulate ok click\n");
      touchnode_clear();
      action_ok();
#else
      printf("detected double click -> ignore it as accidential\n");
#endif
      return;
    }
  }

  /* use the existing node if one was touched */
  node_t *node = touchnode_get_node();
  osm_t::ref osm = appdata.project->osm;
  if(node != nullptr) {
    printf("  re-using node #" ITEM_ID_FORMAT "\n", node->id);

    assert(action.way != nullptr);

    /* check whether this node is first or last one of a different way */
    way_t *touch_way = osm->find_way(check_first_last_node(node));

    /* remember this way as this may be the last node placed */
    /* and we might want to join this with this other way */
    action.ends_on = touch_way;

    /* is this the first node the user places? */
    if(action.way->node_chain.empty() && touch_way != nullptr &&
       osm2go_platform::yes_no(_("Extend way?"),
                               _("Do you want to extend the way present at this location?"),
                               MISC_AGAIN_ID_EXTEND_WAY)) {
      /* there are immediately enough nodes for a valid way */
      action.extending = touch_way;
      appdata.iconbar->map_cancel_ok(true, true);
    }
  } else {
    /* the current way doesn't end on another way if we are just placing */
    /* a new node */
    action.ends_on = nullptr;

    if(!osm->bounds.contains(pos)) {
      map_t::outside_error();
      return;
    }
    node = osm->node_new(pos);
  }

  assert(node != nullptr);
  assert(action.way != nullptr);
  action.way->append_node(node);

  switch(action.way->node_chain.size()) {
  case 1:
    /* replace "place first node..." message */
    appdata.uicontrol->showNotification(_("Place next node of way"));
    break;
  case 2:
    /* two nodes are enough for a valid way */
    appdata.iconbar->map_cancel_ok(true, true);
    break;
  }

  /* draw current way */
  style->colorize(action.way);
  draw(action.way);
}

static void map_unref_ways(node_t *node)
{
  printf("    node #" ITEM_ID_FORMAT " (used by %u)\n",
         node->id, node->ways);

  assert_cmpnum_op(node->ways, >, 0);
  node->ways--;
  if(node->ways == 0 && node->id == ID_ILLEGAL) {
    printf("      -> freeing temp node\n");
    delete node;
  }
}

void map_t::way_add_cancel() {
  printf("  removing temporary way\n");
  assert(action.way != nullptr);

  appdata.project->osm->way_delete(action.way, nullptr, map_unref_ways);
  action.way = nullptr;
}

class map_draw_nodes {
  map_t * const map;
public:
  explicit inline map_draw_nodes(map_t *m) : map(m) {}
  void operator()(node_t *node);
};

void map_draw_nodes::operator()(node_t* node)
{
  printf("    node #" ITEM_ID_FORMAT " (used by %u)\n",
         node->id, node->ways);

  if(node->id == ID_ILLEGAL) {
    /* we can be sure that no node gets inserted twice (even if twice in */
    /* the ways chain) because it gets assigned a non-ID_ILLEGAL id when */
    /* being moved to the osm node chain */
    map->appdata.project->osm->node_attach(node);
  }

  map->draw(node);
}

void map_t::way_add_ok() {
  osm_t::ref osm = appdata.project->osm;

  assert(osm);
  assert(action.way != nullptr);

  /* transfer all nodes that have been created for this way */
  /* into the node chain */

  /* (their way count will be 0 after removing the way) */
  node_chain_t &chain = action.way->node_chain;
  std::for_each(chain.begin(), chain.end(), map_draw_nodes(this));

  /* attach to existing way if the user requested so */
  if(action.extending != nullptr) {
    // this is triggered when the user started with extending an existing way
    // since the merged way is a temporary one there are no relation memberships
    // and no background item
    assert(background_items.find(action.way) == background_items.end());
    action.extending->merge(action.way, osm, this);

    action.way = action.extending;
  } else {
    /* now move the way itself into the main data structure */
    osm->way_attach(action.way);
  }

  /* we might already be working on the "ends_on" way as we may */
  /* be extending it. Joining the same way doesn't make sense. */
  if(action.ends_on == action.way) {
    printf("  the new way ends on itself -> don't join itself\n");
    action.ends_on = nullptr;
  } else if(action.ends_on != nullptr && osm2go_platform::yes_no(_("Join way?"),
                                           _("Do you want to join the way present at this location?"),
                                                          MISC_AGAIN_ID_EXTEND_WAY_END)) {
    printf("  this new way ends on another way\n");
    // this is triggered when the new way ends on an existing way, this can
    // happen even if an existing way was extended before

    /* this is slightly more complex as this time two full tagged */
    /* ways may be involved as the new way may be an extended existing */
    /* way being connected to another way. This happens if you connect */
    /* two existing ways using a new way between them */

    osm_t::mergeResult<way_t> mr = osm->mergeWays(action.way, action.ends_on, this);
    action.way = mr.obj;
    action.ends_on = nullptr;

    if(mr.conflict)
      message_dlg(_("Way tag conflict"),
                  _("The resulting way contains some conflicting tags. Please solve these."));
  }

  /* draw the updated way */
  draw(action.way);

  select_way(action.way);

  action.way = nullptr;

  /* let the user specify some tags for the new way */
  info_selected();
}

/* -------------------------- way_node_add ----------------------- */

void map_t::way_node_add_highlight(map_item_t *item, lpos_t pos) {
  if(item_is_selected_way(item) && canvas->get_item_segment(item->item, pos) >= 0)
    hl_cursor_draw(pos, style->node.radius);
}

void map_t::way_node_add(lpos_t pos) {
  /* check if we are still hovering above the selected way */
  map_item_t *item = item_at(pos);
  if(item_is_selected_way(item)) {
    /* convert mouse position to canvas (world) position */
    const int insert_after = canvas->get_item_segment(item->item, pos);
    if(insert_after >= 0) {
      /* insert it into ways chain of nodes */
      way_t *way = item->object.way;

      /* create new node */
      node_t* node = way->insert_node(appdata.project->osm, insert_after + 1, pos);

      /* clear selection */
      item_deselect();

      /* draw the updated way */
      draw(way);

      /* and now draw the node */
      draw(node);

      /* put gui into idle state */
      set_action(MAP_ACTION_IDLE);

      /* and redo it */
      select_way(way);
    }
  }
}

/* -------------------------- way_node_cut ----------------------- */

void map_t::way_cut_highlight(map_item_t *item, lpos_t pos) {

  if(item_is_selected_way(item)) {
    int seg = canvas->get_item_segment(item->item, pos);
    if(seg >= 0) {
      unsigned int width = (item->object.way->draw.flags & OSM_DRAW_FLAG_BG) ?
                           2 * item->object.way->draw.bg.width :
                           3 * item->object.way->draw.width;
      std::vector<lpos_t> coords(2);
      coords[0] = item->object.way->node_chain[seg]->lpos;
      coords[1] = item->object.way->node_chain[seg + 1]->lpos;
      cursor = canvas->polyline_new(CANVAS_GROUP_DRAW, coords, width, style->highlight.node_color);
    }
  } else if(item_is_selected_node(item)) {
    /* cutting a way at its first or last node doesn't make much sense ... */
    if(!selected.object.way->ends_with_node(item->object.node))
      hl_cursor_draw(item->object.node->lpos, 2 * style->node.radius);
  }
}

/* cut the currently selected way at the current cursor position */
void map_t::way_cut(lpos_t pos) {
  /* check if we are still hovering above the selected way */
  map_item_t *item = item_at(pos);
  bool cut_at_node = item_is_selected_node(item);

  if(!item_is_selected_way(item) && !cut_at_node)
    return;

  /* convert mouse position to canvas (world) position */

  node_chain_t::iterator cut_at;
  way_t *way = nullptr;
  if(cut_at_node) {
    printf("  cut at node\n");

    /* node must not be first or last node of way */
    assert(selected.object.type == object_t::WAY);

    if(!selected.object.way->ends_with_node(item->object.node)) {
      way = selected.object.way;

      cut_at = std::find(way->node_chain.begin(), way->node_chain.end(),
                         item->object.node);
    } else {
      printf("  won't cut as it's last or first node\n");
      return;
    }

  } else {
    printf("  cut at segment\n");
    int c = canvas->get_item_segment(item->item, pos);
    if(c < 0)
      return;
    way = item->object.way;
    // add one since to denote the end of the segment
    cut_at = std::next(way->node_chain.begin(), c + 1);
  }

  assert(way != nullptr);
  assert_cmpnum_op(way->node_chain.size(), >, 2);

  /* move parts of node_chain to the new way */
  printf("  moving everthing after segment %zi to new way\n",
         cut_at - way->node_chain.begin());

  /* clear selection */
  item_deselect();

  /* create a duplicate of the currently selected way */
  way_t * const neww = way->split(appdata.project->osm, cut_at, cut_at_node);

  printf("original way still has %zu nodes\n", way->node_chain.size());

  /* draw the updated old way */
  style->colorize(way);
  draw(way);

  if(neww != nullptr) {
    /* colorize the new way before drawing */
    style->colorize(neww);
    draw(neww);
  }

  /* put gui into idle state */
  set_action(MAP_ACTION_IDLE);

  /* and redo selection if way still exists */
  if(item != nullptr)
    select_way(way);
  else if(neww != nullptr)
    select_way(neww);
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

  /* draw current way */
  map->style->colorize(way);
  map->draw(way);
}

void map_t::node_move(map_item_t *map_item, const osm2go_platform::screenpos &p)
{
  osm_t::ref osm = appdata.project->osm;

  assert(map_item->object.type == object_t::NODE);
  node_t *node = map_item->object.node;

  printf("released dragged node #" ITEM_ID_FORMAT "\n", node->id);
  printf("  was at %d %d (%f %f)\n",
	 node->lpos.x, node->lpos.y,
	 node->pos.lat, node->pos.lon);

  /* check if it was dropped onto another node */
  bool joined_with_touchnode = false;

  if(touchnode != nullptr) {
    node_t *tn = touchnode_get_node();

    printf("  dropped onto node #" ITEM_ID_FORMAT "\n", tn->id);

    if(osm2go_platform::yes_no(_("Join nodes?"),
                               _("Do you want to join the dragged node with the one you dropped it on?"),
                               MISC_AGAIN_ID_JOIN_NODES)) {
      /* the touchnode vanishes and is replaced by the node the */
      /* user dropped onto it */
      joined_with_touchnode = true;
      unsigned int ways2join_cnt = 0;

      // only offer to join ways if they come from the different nodes, not
      // if e.g. one node has 2 ways and the other has none
      std::array<way_t *, 2> ways2join;
      if(node->ways > 0 && tn->ways > 0)
        ways2join_cnt = node->ways + tn->ways;

      osm_t::mergeResult<node_t> mr = osm->mergeNodes(node, tn, ways2join);
      // make sure the object marked as selected is the surviving node
      selected.object = node = mr.obj;

      /* and open dialog to resolve tag collisions if necessary */
      if(mr.conflict)
        message_dlg(_("Node tag conflict"),
                    _("The resulting node contains some conflicting tags. Please solve these."));

      /* check whether this will also join two ways */
      printf("  checking if node is end of way\n");

      if(ways2join_cnt > 2) {
        message_dlg(_("Too many ways to join"),
                    _("More than two ways that contain this node. Joining more "
                      "than two ways is not yet implemented, sorry"));
      } else if(ways2join_cnt == 2 && ways2join[0] != nullptr &&
                osm2go_platform::yes_no(_("Join ways?"),
                         _("Do you want to join the dragged way with the one you dropped it on?"),
                                        MISC_AGAIN_ID_JOIN_WAYS)) {
        printf("  about to join ways #" ITEM_ID_FORMAT " and #" ITEM_ID_FORMAT "\n",
               ways2join[0]->id, ways2join[1]->id);

        if(osm->mergeWays(ways2join[0], ways2join[1], this).conflict)
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
    lpos_t pos = canvas->window2world(p);
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
  draw(node);

  /* visually update ways, node is part of */
  if(node->ways > 0)
    std::for_each(osm->ways.begin(), osm->ways.end(), redraw_way(node, this));

  /* and mark the node as dirty */
  node->flags |= OSM_FLAG_DIRTY;

  /* update highlight */
  highlight_refresh();
}

/* -------------------------- way_reverse ----------------------- */

/* called from the "reverse" icon */
void map_t::way_reverse() {
  /* work on local copy since de-selecting destroys the selection */
  object_t sel = selected.object;

  /* deleting the selected item de-selects it ... */
  item_deselect();

  assert(sel.type == object_t::WAY);

  unsigned int n_tags_flipped;
  unsigned int n_roles_flipped;
  sel.way->reverse(appdata.project->osm, n_tags_flipped, n_roles_flipped);

  select_way(sel.way);

  if (n_tags_flipped == 0 && n_roles_flipped == 0)
    return;

  // Flash a message about any side-effects
  trstring tags, rels;
  if (n_tags_flipped != 0)
    tags = trstring(ngettext("%n tag", "%n tags", n_tags_flipped), nullptr, n_tags_flipped);
  if (n_roles_flipped != 0)
    rels = trstring(ngettext("%n relation", "%n relations", n_roles_flipped), nullptr, n_roles_flipped);

  trstring msg;
  if (n_tags_flipped != 0 && n_roles_flipped != 0) {
    msg = trstring("%1 & %2 updated").arg(tags).arg(rels);
  } else {
    msg = trstring("%1 updated").arg(tags.isEmpty() ? rels : tags);
  }

  appdata.uicontrol->showNotification(msg, MainUi::Brief);
}
