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

static void undo_state_free(undo_state_t *state) {
  g_free(state);
}

void undo_free(undo_state_t *state) {
  while(state) {
    undo_state_t *next = state->next;
    undo_state_free(state);
    state = next;
  }
}

static undo_state_t *undo_append_state(appdata_t *appdata) {
  undo_state_t *new_state = NULL;

  /* create new undo state at end of undo chain */
  int undo_chain_length = 0;
  undo_state_t **undo_stateP = &appdata->undo_state;
  while(*undo_stateP) {
    undo_chain_length++;
    undo_stateP = &(*undo_stateP)->next;
  }

  /* append new entry to chain */
  new_state = *undo_stateP = g_new0(undo_state_t, 1);

  /* delete first entry if the chain is too long */
  if(undo_chain_length >= UNDO_QUEUE_LEN) {
    undo_state_t *second = appdata->undo_state->next;
    undo_state_free(appdata->undo_state);
    appdata->undo_state = second;
  }

  printf("UNDO: current chain length = %d\n", undo_chain_length);

  return new_state;
}

void undo_remember_delete(appdata_t *appdata, type_t type, void *object) {
  printf("UNDO: remembering delete operation for %s\n", osm_type_string(type));

  undo_state_t *state = undo_append_state(appdata);

  /* fill out state */
}
