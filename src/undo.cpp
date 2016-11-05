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

/*
 * TODO:
 *
 * + restore node memberships when restoring a way
 * o save restore relation membership
 * + handle permamently deleted objects (newly created, then deleted)
 */

#include "undo.h"

#include "appdata.h"
#include "banner.h"
#include "map.h"

#include <algorithm>
#include <vector>

#define UNDO_ENABLE_CHECK   if(!appdata->menu_item_map_undo) return;

/* the data required for an undo is the undo_state_t. It consists of one */
/* or more operations undo_op_t which are the atomic operations the state */
/* consist of. E.g. deleting a way causes all nodes it consists of to */
/* be deleted as well */

/* there is a type saved in the state as well as in every op. e.g. the */
/* deletion of a node (state type == DELETE) may result in a modification */
/* of all the ways the node was contained in. This would then be MODIFY */
/* operatins being part of the DELETE state */

class undo_op_t {
public:
  undo_op_t(undo_type_t t = UNDO_END);

  undo_type_t type;   /* the type of this particular database/map operation */
  object_t object;
  std::vector<item_id_chain_t> id_chain;       /* ref id list, e.g. for nodes of way */
  undo_op_t *next;
};

struct undo_state_t {
  undo_type_t type;   /* what the overall operation was */
  char *name;         /* the name of the "parent" object */
  undo_op_t *op;

  struct undo_state_t *next;
};

undo_op_t::undo_op_t(undo_type_t t)
  : type(t)
  , next(0)
{
  memset(&object, 0, sizeof(object));
}

/* return plain text of type */
static const char *undo_type_string(const undo_type_t type) {
  const struct { undo_type_t type; const char *name; } types[] = {
    { UNDO_DELETE, "deletion" },
    { UNDO_CREATE, "creation" },
    { UNDO_MODIFY, "modification" },
    { UNDO_END, NULL }
  };

  int i;
  for(i=0;types[i].name;i++)
    if(type == types[i].type)
      return types[i].name;

  return NULL;
}

static void undo_object_free(osm_t *osm, object_t *obj) {
  printf("free obj %p\n", obj);

  if(obj->ptr) {
    char *msg = obj->object_string();
    printf("   free object %s\n", msg);
    g_free(msg);

    switch(obj->type) {
    case NODE:
      osm_node_free(osm, obj->node);
      break;

    case WAY:
      osm_way_free(osm, obj->way);
      break;

    case RELATION:
      osm_relation_free(obj->relation);
      break;

    default:
      printf("ERROR: unsupported object %s\n",
             obj->type_string());
      g_assert_not_reached();
      break;
    }
  } else
    printf("   free object %s\n", obj->type_string());
}

static void undo_op_free(osm_t *osm, undo_op_t *op) {
  printf("  free op: %s\n", undo_type_string(op->type));
  undo_object_free(osm, &op->object);
  delete op;
}

static void undo_state_free(osm_t *osm, undo_state_t *state) {
  printf(" free state: %s\n", undo_type_string(state->type));

  g_free(state->name);

  undo_op_t *op = state->op;
  while(op) {
    undo_op_t *next = op->next;
    undo_op_free(osm, op);
    op = next;
  }

  g_free(state);
}

/* free all undo states, thus forgetting the entire history */
/* called at program exit or e.g. at project change */
void undo_free(osm_t *osm, undo_t *undo) {
  undo_state_t *state = undo->state;
  printf("Freeing all UNDO states:\n");

  while(state) {
    undo_state_t *next = state->next;
    undo_state_free(osm, state);
    state = next;
  }
}

/* append a new state to the chain of undo states */
static undo_state_t *undo_append_state(appdata_t *appdata) {
  undo_state_t *new_state = NULL;

  /* create new undo state at end of undo chain */
  int undo_chain_length = 0;
  undo_state_t **undo_stateP = &appdata->undo.state;
  while(*undo_stateP) {
    undo_chain_length++;
    undo_stateP = &(*undo_stateP)->next;
  }

  /* append new entry to chain */
  new_state = *undo_stateP = g_new0(undo_state_t, 1);

  /* delete first entry if the chain is too long */
  if(undo_chain_length >= UNDO_QUEUE_LEN) {
    undo_state_t *second = appdata->undo.state->next;
    undo_state_free(appdata->osm, appdata->undo.state);
    appdata->undo.state = second;
  }

  printf("UNDO: current chain length = %d\n", undo_chain_length);

  return new_state;
}

/* copy the base object parts to another object */
static void undo_object_copy_base(object_t *dst, const object_t &src) {
  OBJECT_ID(*dst)      = OBJECT_ID(src);
  OBJECT_VERSION(*dst) = OBJECT_VERSION(src);
  OBJECT_TIME(*dst)    = OBJECT_TIME(src);
  OBJECT_USER(*dst)    = OBJECT_USER(src);
  OBJECT_FLAGS(*dst)   = OBJECT_FLAGS(src);
  OBJECT_VISIBLE(*dst) = OBJECT_VISIBLE(src);
  OBJECT_TAG(*dst)     = osm_tags_copy(OBJECT_TAG(src));
}

/* create a local copy of the entire object */
static bool undo_object_save(const object_t &object,
                                 undo_op_t *op) {
  object_t *ob = &op->object;
  ob->type = object.type;

  switch(object.type) {
  case NODE:
    /* fields ignored in this copy operation: */
    /* ways, icon_buf, map_item_chain, next */

    ob->node = g_new0(node_t, 1);
    undo_object_copy_base(ob, object);

    /* copy all important parts, omitting icon pointers etc. */
    ob->node->lpos = object.node->lpos;
    ob->node->pos = object.node->pos;
    ob->node->zoom_max = object.node->zoom_max;

    return true;

  case WAY: {
    /* fields ignored in this copy operation: */
    /* next (XXX: incomplete) */

    ob->way = g_new0(way_t, 1);
    undo_object_copy_base(ob, object);

    /* the nodes are saved by reference, since they may also be */
    /* deleted and restored and thus their address may change */
    node_chain_t *node_chain = object.way->node_chain;
    const node_chain_t::const_iterator itEnd = node_chain->end();
    for(node_chain_t::const_iterator it = node_chain->begin(); it != itEnd; it++)
      op->id_chain.push_back(item_id_chain_t(NODE, OSM_ID(*it)));

    return true;
    }

  case RELATION: {
    /* fields ignored in this copy operation: */
    /* next */

    ob->relation = new relation_t();
    undo_object_copy_base(ob, object);

    /* save members reference */
    const std::vector<member_t>::const_iterator mitEnd = object.relation->members.end();
    for(std::vector<member_t>::const_iterator member = object.relation->members.begin();
        member != mitEnd; member++) {
      item_id_chain_t id(member->object.type, member->object.get_id());
      op->id_chain.push_back(id);
    }

    return true;
    }

  default:
    printf("unsupported object of type %s\n",
           object.type_string());

    return false;
  }
}

struct undo_append_obj_modify {
  appdata_t * const appdata;
  undo_append_obj_modify(appdata_t *a) : appdata(a) {}
  void operator()(relation_t *relation) {
    undo_append_object(appdata, UNDO_MODIFY, object_t(relation));
  }
};

void undo_append_object(appdata_t *appdata, undo_type_t type,
                        const object_t &object) {

  UNDO_ENABLE_CHECK;

  undo_state_t *state = appdata->undo.open;
  g_assert(state);

  /* deleting an object will affect all relations it's part of */
  /* therefore handle them first and save their state to undo  */
  /* the modifications */
  if(type == UNDO_DELETE) {
    const relation_chain_t &rchain =
      osm_object_to_relation(appdata->osm, object);

    /* store relation modification as undo operation by recursing */
    /* into objects */
    std::for_each(rchain.begin(), rchain.end(), undo_append_obj_modify(appdata));
  }

  /* a simple stand-alone node deletion is just a single */
  /* operation on the database/map so only one undo_op is saved */

  /* check if this object already is in operaton chain */
  undo_op_t *op = state->op;
  while(op) {
    if(osm_object_is_same(&op->object, object)) {
      /* this must be the same operation!! */
      g_assert(op->type == type);

      printf("UNDO: object %s already in undo_state: ignoring\n",
             object.object_string());
      return;
    }
    op = op->next;
  }

  printf("UNDO: saving \"%s\" operation for object %s\n",
	 undo_type_string(type), object.object_string());

  /* create new operation for main object */
  op = new undo_op_t(type);
  if(undo_object_save(object, op)) {
    /* prepend operation to chain, so that the undo works in reverse order */
    op->next = state->op;
    state->op = op;
  } else {
    delete op;
    return;
  }

  /* if the deleted object is a way, then check if this affects */
  /* a node */
  if((type == UNDO_DELETE) && (object.type == WAY)) {
    node_chain_t *chain = object.way->node_chain;
    const node_chain_t::const_iterator itEnd = chain->end();
    for(node_chain_t::const_iterator it = chain->begin(); it != itEnd; it++) {
      /* this node must only be part of this way */
      if(!osm_node_in_other_way(appdata->osm, object.way, *it))
        undo_append_node(appdata, UNDO_DELETE, *it);
    }
  }
}

void undo_append_way(appdata_t *appdata, undo_type_t type, way_t *way) {
  undo_append_object(appdata, type, object_t(way));
}

void undo_append_node(appdata_t *appdata, undo_type_t type, node_t *node) {
  undo_append_object(appdata, type, object_t(node));
}

void undo_open_new_state(struct appdata_t *appdata, undo_type_t type,
                         object_t &object) {

  UNDO_ENABLE_CHECK;

  g_assert(!appdata->undo.open);

  printf("UNDO: open new state for %s\n", object.object_string());

  /* create a new undo state */
  appdata->undo.open = undo_append_state(appdata);
  appdata->undo.open->type = type;

  appdata->undo.open->name = object.get_name();
  printf("   name: %s\n", appdata->undo.open->name);
}

void undo_close_state(appdata_t *appdata) {
  UNDO_ENABLE_CHECK;

  g_assert(appdata->undo.open);

  printf("UNDO: closing state\n");

  appdata->undo.open = NULL;
}


/* --------------------- restoring ---------------------- */

/* restore state of an object (or even restore it after deletion) */
static void undo_operation_object_restore(appdata_t *appdata, object_t &obj,
					  std::vector<item_id_chain_t> &id_chain) {

  gchar *msg = obj.object_string();
  printf("UNDO deletion of object %s\n", msg);
  g_free(msg);

  switch(obj.type) {
  case NODE: {
    /* there must be an "deleted" entry which needs to be */
    /* removed or no entry at all (since a new one has been deleted) */
    node_t *orig = osm_get_node_by_id(appdata->osm, OBJECT_ID(obj));
    if(orig) {
      g_assert(OSM_FLAGS(orig) & OSM_FLAG_DELETED);

      /* permanently remove the node marked as "deleted" */
      const way_chain_t &wchain = osm_node_delete(appdata->osm, orig, true, true);

      /* the deleted node must not have been part of any way */
      g_assert(wchain.empty());
    }

    /* then restore old node */
    osm_node_restore(appdata->osm, obj.node);

    obj.ptr = NULL;
  } break;

  case WAY: {
    /* there must be an "deleted" entry which needs to be */
    /* removed or no entry at all (since a new one has been deleted) */
    way_t *orig = osm_get_way_by_id(appdata->osm, OBJECT_ID(obj));
    if(orig) {
      g_assert(OSM_FLAGS(orig) & OSM_FLAG_DELETED);

      /* permanently remove the way marked as "deleted" */
      osm_way_delete(appdata->osm, orig, TRUE);
    }

    osm_way_restore(appdata->osm, obj.way, id_chain);
    id_chain.clear();

    obj.ptr = NULL;
  } break;

  default:
    printf("Unsupported object type\n");
    g_assert_not_reached();
    break;
  }
}

/* undo a single operation */
static void undo_operation(appdata_t *appdata, undo_op_t *op) {
  printf("UNDO operation: %s\n", undo_type_string(op->type));

  switch(op->type) {
  case UNDO_DELETE:
    undo_operation_object_restore(appdata, op->object, op->id_chain);
    break;

  default:
    printf("unsupported UNDO operation\n");
    g_assert_not_reached();
    break;
  }
}

/* undo the last undo_state */
void undo(appdata_t *appdata) {
  undo_state_t *state = appdata->undo.state;
  printf("user selected undo\n");

  /* search last (newest) entry */
  while(state && state->next) state = state->next;

  if(!state) {
    banner_show_info(appdata, _("No further undo data"));
    return;
  }


  printf("UNDO state: %s\n", undo_type_string(state->type));

  char *msg = g_strdup_printf(_("Undoing %s of %s"),
			      undo_type_string(state->type),
			      state->name);
  banner_show_info(appdata, msg);
  g_free(msg);

  /* since the operations list was built by prepending new */
  /* entries, just going through the list will run the operations */
  /* in reverse order. That's exactly what we want! */
  undo_op_t *op = state->op;
  while(op) {
    undo_operation(appdata, op);
    op = op->next;
  }

  /* remove this entry from chain */
  undo_state_t **stateP = &appdata->undo.state;
  while(*stateP && (*stateP)->next) stateP = &(*stateP)->next;

  undo_state_free(appdata->osm, *stateP);
  *stateP = NULL;

  /* redraw entire map */
  map_clear(appdata, MAP_LAYER_ALL);
  map_paint(appdata);
}
