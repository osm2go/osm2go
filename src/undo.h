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

#include "osm.h"

/* remember the last X operations for undo */
#define UNDO_QUEUE_LEN  4

typedef enum {
  UNDO_DELETE = 0,
  UNDO_CREATE,
  UNDO_MODIFY

} undo_type_t;

typedef struct undo_state_s undo_state_t;

typedef struct {
  undo_state_t *state;   /* pointer to first state in chain */
  undo_state_t *open;    /* pointer to open state (NULL if none) */
} undo_t;

struct appdata_s;
void undo_open_new_state(struct appdata_s *ad, undo_type_t typ, object_t *obj);
void undo_append_object(struct appdata_s *ad, undo_type_t type, const object_t *obj);
void undo_append_way(struct appdata_s *ad, undo_type_t type, way_t *way);
void undo_append_node(struct appdata_s *ad, undo_type_t type, node_t *node);
void undo_close_state(struct appdata_s *appdata);

void undo_free(osm_t *osm, undo_t *undo);
void undo(struct appdata_s *appdata);

#endif // UNDO_H
