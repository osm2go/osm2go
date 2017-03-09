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

typedef struct {
} undo_t;

#ifdef __cplusplus
extern "C" {
#endif

struct appdata_t;
static inline void undo_free(G_GNUC_UNUSED osm_t *osm, G_GNUC_UNUSED undo_t *undo) {}
static inline void undo(G_GNUC_UNUSED struct appdata_t *appdata) {}

#ifdef __cplusplus
}

static inline void undo_append_way(struct appdata_t *, undo_type_t, way_t *) {}
static inline void undo_append_node(struct appdata_t *, undo_type_t, node_t *) {}
static inline void undo_close_state(struct appdata_t *) {}

static inline void undo_open_new_state(struct appdata_t *, undo_type_t, object_t &) {}
static inline void undo_append_object(struct appdata_t *, undo_type_t, const object_t &) {}
#endif

#endif // UNDO_H
