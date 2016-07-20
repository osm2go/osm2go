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

#ifndef PROJECT_H
#define PROJECT_H

#include "map.h"

typedef struct project_s {
  char *name;
  char *path;

  char *desc;
  char *server;
  char *osm;

  char *wms_server;
  char *wms_path;
  struct { gint x, y; } wms_offset;

  map_state_t *map_state;

  pos_t min, max;

  gboolean data_dirty;     /* needs to download new data */

  struct project_s *next;  /* for chaining projects (e.g. during scan) */
} project_t;

gboolean project_exists(settings_t *settings, const char *name);
gboolean project_open(appdata_t *appdata, char *name);
gboolean project_save(GtkWidget *parent, project_t *project);
gboolean project_load(appdata_t *appdata, char *name);
gboolean project_close(appdata_t *appdata);
gboolean project_check_demo(GtkWidget *parent, project_t *project);
void project_wizard(appdata_t *appdata);

void project_free(project_t *project);

#endif // PROJECT_H
