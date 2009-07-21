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

#ifndef UNDO_H
#define UNDO_H

/* remember the last X operations for undo */
#define UNDO_QUEUE_LEN  4

typedef enum {
  UNDO_DELETE = 0,
  UNDO_CREATE,
  UNDO_MODIFY
  
} undo_type_t;

/* the data required for an undo is the undo_state_t. It consists of one */
/* or more operations undo_op_t which are the atomic operations the state */
/* consist of. E.g. deleting a way causes all nodes it consists of to */
/* be deleted as well */

/* there is a type saved in the state as well as in every op. e.g. the */
/* deletion of a node (state type == DELETE) may result in a modification */
/* of all the ways the node was contained in. This would then be MODIFY */
/* operatins being part of the DELETE state */

typedef struct undo_op_s {
  undo_type_t type;   /* the type of this particular database/map operation */
  object_t *object;
  struct undo_op_s *next;
} undo_op_t;

typedef struct undo_state_s {
  undo_type_t type;   /* what the overall operation was */
  object_t *object;   /* the "parent" object that this undo state is for */
  undo_op_t *op;

  struct undo_state_s *next;
} undo_state_t;

typedef struct {
  undo_state_t *state;   /* pointer to first state in chain */
  undo_state_t *open;    /* pointer to open state (NULL if none) */
} undo_t;

struct appdata_s;
void undo_open_new_state(struct appdata_s *ad, undo_type_t typ, object_t *obj);
void undo_append_object(struct appdata_s *ad, undo_type_t type, object_t *obj);
void undo_append_way(struct appdata_s *ad, undo_type_t type, way_t *way);
void undo_close_state(struct appdata_s *appdata);

void undo_free(undo_state_t *state);
void undo(struct appdata_s *appdata);


#endif // UNDO_H
