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

  void free_data(osm_t *osm);
};

struct undo_state_t {
  undo_state_t();
  ~undo_state_t();

  undo_type_t type;   /* what the overall operation was */
  char *name;         /* the name of the "parent" object */

  std::vector<undo_op_t> ops;

  struct undo_state_t *next;
};

undo_op_t::undo_op_t(undo_type_t t)
  : type(t)
{
  memset(&object, 0, sizeof(object));
}

undo_state_t::undo_state_t()
  : type(UNDO_END)
  , name(0)
  , next(0)
{
}

undo_state_t::~undo_state_t()
{
  g_free(name);
}

/* return plain text of type */
static const char *undo_type_string(const undo_type_t type) {
  const struct { undo_type_t type; const char *name; } types[] = {
    { UNDO_DELETE, "deletion" },
    { UNDO_CREATE, "creation" },
    { UNDO_MODIFY, "modification" },
    { UNDO_END, NULL }
  };

  for(int i = 0; types[i].name; i++)
    if(type == types[i].type)
      return types[i].name;

  return NULL;
}

static void undo_object_free(osm_t *osm, object_t *obj) {
  printf("free obj %p\n", obj);

  if(obj->obj) {
    char *msg = obj->object_string();
    printf("   free object %s\n", msg);
    g_free(msg);

    switch(obj->type) {
    case NODE:
      osm->node_free(obj->node);
      break;

    case WAY:
      osm->way_free(obj->way);
      break;

    case RELATION:
      osm->relation_free(obj->relation);
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

void undo_op_t::free_data(osm_t *osm) {
  printf("  free op: %s\n", undo_type_string(type));
  undo_object_free(osm, &object);
}

struct free_op_data {
  osm_t * const osm;
  free_op_data(osm_t *o) : osm(o) {}
  void operator()(undo_op_t &u) {
    u.free_data(osm);
  }
};

static void undo_state_free(osm_t *osm, undo_state_t *state) {
  printf(" free state: %s\n", undo_type_string(state->type));

  std::for_each(state->ops.begin(), state->ops.end(), free_op_data(osm));

  delete state;
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
  new_state = *undo_stateP = new undo_state_t();

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
  dst->obj->id      = src.obj->id;
  dst->obj->version = src.obj->version;
  dst->obj->time    = src.obj->time;
  dst->obj->user    = src.obj->user;
  dst->obj->flags   = src.obj->flags;
  dst->obj->visible = src.obj->visible;
  dst->obj->tag     = osm_tags_copy(src.obj->tag);
}

struct object_append {
  std::vector<item_id_chain_t> &id_chain;
  object_append(std::vector<item_id_chain_t> &c) : id_chain(c) {}
  void operator()(const node_t *node) {
    id_chain.push_back(item_id_chain_t(NODE, node->id));
  }
  void operator()(const member_t &member) {
    id_chain.push_back(item_id_chain_t(member.object.type, member.object.get_id()));
  }
};

/* create a local copy of the entire object */
static bool undo_object_save(const object_t &object,
                             undo_op_t &op) {
  object_t *ob = &op.object;
  ob->type = object.type;

  switch(object.type) {
  case NODE:
    /* fields ignored in this copy operation: */
    /* ways, icon_buf, map_item_chain, next */

    ob->node = new node_t();
    undo_object_copy_base(ob, object);

    /* copy all important parts, omitting icon pointers etc. */
    ob->node->lpos = object.node->lpos;
    ob->node->pos = object.node->pos;
    ob->node->zoom_max = object.node->zoom_max;

    return true;

  case WAY: {
    /* fields ignored in this copy operation: */
    /* next (XXX: incomplete) */

    ob->way = new way_t();
    undo_object_copy_base(ob, object);

    /* the nodes are saved by reference, since they may also be */
    /* deleted and restored and thus their address may change */
    const node_chain_t &node_chain = object.way->node_chain;
    std::for_each(node_chain.begin(), node_chain.end(), object_append(op.id_chain));

    return true;
    }

  case RELATION: {
    /* fields ignored in this copy operation: */
    /* next */

    ob->relation = new relation_t();
    undo_object_copy_base(ob, object);

    /* save members reference */
    const std::vector<member_t> &chain = object.relation->members;
    std::for_each(chain.begin(), chain.end(), object_append(op.id_chain));

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

struct find_undo_obj {
  const object_t &object;
  find_undo_obj(const object_t &o) : object(o) {}
  bool operator()(const undo_op_t &u) {
    return u.object == object;
  }
};

struct find_node_of_this_way {
  const osm_t * const osm;
  const way_t * const way;
  find_node_of_this_way(const osm_t *o, const way_t *w) : osm(o), way(w) {}
  bool operator()(const node_t *node) {
    return !osm_node_in_other_way(osm, way, node);
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
      appdata->osm->to_relation(object);

    /* store relation modification as undo operation by recursing */
    /* into objects */
    std::for_each(rchain.begin(), rchain.end(), undo_append_obj_modify(appdata));
  }

  /* a simple stand-alone node deletion is just a single */
  /* operation on the database/map so only one undo_op is saved */

  /* check if this object already is in operation chain */
  if(std::find_if(state->ops.begin(), state->ops.end(), find_undo_obj(object)) !=
     state->ops.end()) {
    printf("UNDO: object %s already in undo_state: ignoring\n",
           object.object_string());
    return;
  }

  printf("UNDO: saving \"%s\" operation for object %s\n",
	 undo_type_string(type), object.object_string());

  /* create new operation for main object */
  undo_op_t op(type);
  if(undo_object_save(object, op)) {
    state->ops.push_back(op);
  } else {
    return;
  }

  /* if the deleted object is a way, then check if this affects */
  /* a node */
  if((type == UNDO_DELETE) && (object.type == WAY)) {
    const node_chain_t::const_iterator itEnd = object.way->node_chain.end();
    node_chain_t::const_iterator it = object.way->node_chain.begin();
    /* this node must only be part of this way */
    while((it = std::find_if(it, itEnd,
           find_node_of_this_way(appdata->osm, object.way))) != itEnd) {
      undo_append_node(appdata, UNDO_DELETE, *it);
      it++;
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
    node_t *orig = appdata->osm->node_by_id(obj.obj->id);
    if(orig) {
      g_assert(orig->flags & OSM_FLAG_DELETED);

      /* permanently remove the node marked as "deleted" */
      const way_chain_t &wchain = appdata->osm->node_delete(orig, true, true);

      /* the deleted node must not have been part of any way */
      g_assert(wchain.empty());
    }

    /* then restore old node */
    appdata->osm->node_restore(obj.node);

    obj.obj = 0;
  } break;

  case WAY: {
    /* there must be an "deleted" entry which needs to be */
    /* removed or no entry at all (since a new one has been deleted) */
    way_t *orig = appdata->osm->way_by_id(obj.obj->id);
    if(orig) {
      g_assert(orig->flags & OSM_FLAG_DELETED);

      /* permanently remove the way marked as "deleted" */
      appdata->osm->way_delete(orig, true);
    }

    appdata->osm->way_restore(obj.way, id_chain);
    id_chain.clear();

    obj.obj = 0;
  } break;

  default:
    printf("Unsupported object type\n");
    g_assert_not_reached();
    break;
  }
}

/* undo a single operation */
static void undo_operation(appdata_t *appdata, undo_op_t &op) {
  printf("UNDO operation: %s\n", undo_type_string(op.type));

  switch(op.type) {
  case UNDO_DELETE:
    undo_operation_object_restore(appdata, op.object, op.id_chain);
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

  /* run the operations in reverse order */
  const std::vector<undo_op_t>::reverse_iterator itEnd = state->ops.rend();
  for(std::vector<undo_op_t>::reverse_iterator it = state->ops.rbegin(); it != itEnd; it++) {
    undo_operation(appdata, *it);
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
