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

#include "appdata.h"
#include "pos.h"
#include "settings.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>

struct map_state_t;

typedef struct project_t {
#ifdef __cplusplus
  project_t(const char *n, const char *base_path);
  ~project_t();
#endif

  const char *server; /**< the server string used, either rserver or settings->server */

  char *wms_server;
  char *wms_path;
  struct { gint x, y; } wms_offset;

  struct map_state_t *map_state;

  pos_t min, max;

  gboolean data_dirty;     /* needs to download new data */
#ifdef __cplusplus
  const std::string name;
  const std::string path;
  std::string desc;
  std::string osm;
  std::string rserver;
#endif
} project_t;

#ifdef __cplusplus
extern "C" {
#endif

gboolean project_exists(settings_t *settings, const char *name);
gboolean project_save(GtkWidget *parent, project_t *project);
gboolean project_load(appdata_t *appdata, const char *name);
gboolean project_check_demo(GtkWidget *parent, project_t *project);

void project_free(project_t *project);

osm_t *project_parse_osm(const project_t *project, struct icon_t **icons);
/**
 * @brief return project->name.c_str()
 */
const char *project_name(const project_t *project);

#ifdef __cplusplus
}
#endif

#endif // PROJECT_H
