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

#ifndef OSM_API_H
#define OSM_API_H

#include "osm.h"

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

struct appdata_t;
struct project_t;
struct settings_t;

gboolean osm_download(GtkWidget *parent, struct settings_t *settings,
                      struct project_t *project);
void osm_upload(struct appdata_t *appdata, osm_t *osm, struct project_t *project);

#ifdef __cplusplus
}
#endif

#endif // OSM_API_H
