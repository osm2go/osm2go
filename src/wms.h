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

#ifndef WMS_H
#define WMS_H

#include <glib.h>
#include <string>

#include <osm2go_cpp.h>

struct wms_server_t {
  inline wms_server_t() : next(O2G_NULLPTR) {}
  std::string name, server, path;
  struct wms_server_t *next;
};

struct appdata_t;
struct project_t;

void wms_import(appdata_t *appdata);
void wms_load(appdata_t *appdata);
void wms_remove(appdata_t *appdata);
void wms_remove_file(project_t *project);

wms_server_t *wms_server_get_default(void);
void wms_servers_free(wms_server_t *wms_server);

#endif // WMS_H
