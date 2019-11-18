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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>
#include <vector>

struct wms_server_t {
  explicit inline wms_server_t() {}
  explicit inline wms_server_t(const char *n, const char *s) __attribute__ ((nonnull(2,3)))
    : name(n), server(s) {}
  std::string name, server;
};

struct appdata_t;
struct project_t;

void wms_import(appdata_t &appdata);
void wms_load(appdata_t &appdata);
void wms_remove(appdata_t &appdata);
void wms_remove_file(project_t &project);

std::vector<wms_server_t *> wms_server_get_default(void);
