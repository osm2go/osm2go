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
  UNDO_CREATE
  
} undo_type_t;

typedef struct undo_op_s {

  struct undo_op_s *next;
} undo_op_t;

typedef struct undo_state_s {
  undo_op_t *op;

  struct undo_state_s *next;
} undo_state_t;

void undo_remember_delete(appdata_t *appdata, type_t type, void *object);
void undo_free(undo_state_t *state);

#endif // UNDO_H
