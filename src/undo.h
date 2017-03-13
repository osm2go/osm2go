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

typedef enum {
  UNDO_DELETE = 0,
  UNDO_CREATE,
  UNDO_MODIFY,
  UNDO_END = -1
} undo_type_t;

struct undo_t;

struct appdata_t;
static inline void undo_free(osm_t *, undo_t *) {}
static inline void undo(struct appdata_t *) {}

static inline void undo_append_way(struct appdata_t *, undo_type_t, way_t *) {}
static inline void undo_append_node(struct appdata_t *, undo_type_t, node_t *) {}
static inline void undo_close_state(struct appdata_t *) {}

static inline void undo_open_new_state(struct appdata_t *, undo_type_t, object_t &) {}
static inline void undo_append_object(struct appdata_t *, undo_type_t, const object_t &) {}

#endif // UNDO_H
