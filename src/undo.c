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

/* return plain text of type */
char *undo_type_string(type_t type) {
  const struct { undo_type_t type; char *name; } types[] = {
    { UNDO_DELETE, "DELETE" },
    { UNDO_CREATE, "CREATE" },
    { UNDO_MODIFY, "MODIFY" },
    { 0, NULL }
  };

  int i;
  for(i=0;types[i].name;i++) 
    if(type == types[i].type)
      return types[i].name;

  return NULL;
}

static void undo_object_free(object_t *obj) {
  if(obj->ptr) {
    char *msg = osm_object_string(obj);
    printf("   object %s\n", msg);
    g_free(msg);
  } else
    printf("   object %s\n", osm_object_type_string(obj));
	   
  if(obj->ptr) {

    switch(obj->type) {
    case NODE:
      osm_node_free(NULL, obj->node);
      break;

    case WAY:
      osm_way_free(obj->way);
      break;
      
    default:
      printf("ERROR: unsupported object %s\n", 
	     osm_object_type_string(obj));
      g_assert(0);
      break;
    }
  }

  g_free(obj);
}

static void undo_op_free(undo_op_t *op) {
  printf("  op: %s\n", undo_type_string(op->type));
  if(op->object) undo_object_free(op->object);
  g_free(op);
}

static void undo_state_free(undo_state_t *state) {
  printf(" state: %s\n", undo_type_string(state->type));

  if(state->object)
    undo_object_free(state->object);

  undo_op_t *op = state->op;
  while(op) {
    undo_op_t *next = op->next;
    undo_op_free(op);
    op = next;
  }

  g_free(state);
}

/* free all undo states, thus forgetting the entire history */
/* called at program exit or e.g. at project change */
void undo_free(undo_state_t *state) {
  printf("Freeing all UNDO states:\n");

  while(state) {
    undo_state_t *next = state->next;
    undo_state_free(state);
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
    undo_state_free(appdata->undo.state);
    appdata->undo.state = second;
  }

  printf("UNDO: current chain length = %d\n", undo_chain_length);

  return new_state;
}

/* create a local copy of the entire object */
static object_t *undo_object_copy(object_t *object) {
  switch(object->type) {
  case NODE: {
    object_t *ob = g_new0(object_t, 1);
    ob->type = object->type;

    /* fields ignored in this copy operation: */
    /* ways, icon_buf, map_item_chain, next */

    ob->node = g_new0(node_t, 1);
    /* copy all important parts, omitting icon pointers etc. */
    ob->node->id = object->node->id;
    ob->node->lpos = object->node->lpos;
    ob->node->pos = object->node->pos;
    /* user is a pointer, but since the users list */
    /* is never touched it's ok */
    ob->node->user = object->node->user;
    ob->node->visible = object->node->visible;
    ob->node->time = object->node->time;
    ob->node->tag = osm_tags_copy(object->node->tag);
    ob->node->flags = object->node->flags; 
    ob->node->zoom_max = object->node->zoom_max; 

    return ob;
    } break;

  case WAY: {
    object_t *ob = g_new0(object_t, 1);
    ob->type = object->type;

    /* fields ignored in this copy operation: */

    ob->way = g_new0(way_t, 1);
    /* copy all important parts */
    ob->way->id = object->way->id;
    /* user is a pointer, but since the users list */
    /* is never touched it's ok */
    ob->way->user = object->way->user;
    ob->way->visible = object->way->visible;
    ob->way->time = object->way->time;
    ob->way->tag = osm_tags_copy(object->way->tag);
    ob->way->flags = object->way->flags; 

    return ob;
    } break;

  default:
    printf("UNDO WARNING: ignoring unsupported object %s\n", 
	   osm_object_type_string(object));
    break;
  }

  return NULL;
}

void undo_append_object(appdata_t *appdata, undo_type_t type, 
			object_t *object) {

  /* don't do anything if undo isn't enabled */
  if(!appdata->menu_item_map_undo)
    return;

  g_assert(appdata->undo.open);

  printf("UNDO: saving %s operation for %s\n", 
	 undo_type_string(type),
	 osm_object_type_string(object));

  /* a simple stand-alone node deletion is just a single */
  /* operation on the database/map so only one undo_op is saved */

  /* append new undo operation */
  undo_op_t **op = &(appdata->undo.open->op);
  while(*op) op = &(*op)->next;

  *op = g_new0(undo_op_t, 1);
  (*op)->type = type;
  (*op)->object = undo_object_copy(object);
}

void undo_append_way(appdata_t *appdata, undo_type_t type, way_t *way) {
  object_t obj;
  obj.type = WAY;
  obj.way = way;

  undo_append_object(appdata, type, &obj);
}

void undo_open_new_state(struct appdata_s *appdata, undo_type_t type, 
			 object_t *object) {
  g_assert(!appdata->undo.open);

  printf("UNDO: open new state for %s\n",
	 osm_object_string(object));

  /* don't do anything if undo isn't enabled */
  if(!appdata->menu_item_map_undo)
    return;

  /* create a new undo state */
  appdata->undo.open = undo_append_state(appdata);
  appdata->undo.open->type = type;

  appdata->undo.open->object = undo_object_copy(object);
}

void undo_close_state(appdata_t *appdata) {
  g_assert(appdata->undo.open);

  printf("UNDO: closing state\n");

  appdata->undo.open = NULL;
}


/* --------------------- restoring ---------------------- */

/* undo the deletion of an object */
static void undo_operation_object_delete(appdata_t *appdata, object_t *obj) {

  char *msg = osm_object_string(obj);
  printf("UNDO deletion of object %s\n", msg);
  g_free(msg);
  
  switch(obj->type) {
  case NODE: {
    /* there must be an "deleted" entry which needs to be */
    /* removed */
    node_t *orig = osm_get_node_by_id(appdata->osm, obj->node->id);
    g_assert(orig);
    g_assert(orig->flags & OSM_FLAG_DELETED);
    way_chain_t *wchain = 
      osm_node_delete(appdata->osm, &appdata->icon, orig, TRUE, TRUE);
    g_assert(!wchain);

    /* then restore old node */
    osm_node_dump(obj->node);
    osm_node_restore(appdata->osm, obj->node);
    josm_elemstyles_colorize_node(appdata->map->style, obj->node);
    map_node_draw(appdata->map, obj->node);
    obj->ptr = NULL;
  } break;

  case WAY: {
    way_t *orig = osm_get_way_by_id(appdata->osm, obj->way->id);
    g_assert(orig);
    g_assert(orig->flags & OSM_FLAG_DELETED);
  } break;

  default:
    printf("Unsupported object type\n");
    g_assert(0);
    break;
  }
}

/* undo a single operation */
static void undo_operation(appdata_t *appdata, undo_op_t *op) {
  printf("UNDO operation: %s\n", undo_type_string(op->type));

  switch(op->type) {
  case UNDO_DELETE:
    undo_operation_object_delete(appdata, op->object);
    break;

  default:
    printf("unsupported UNDO operation\n");
    g_assert(0);
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

  undo_state_free(*stateP);
  *stateP = NULL;
}
