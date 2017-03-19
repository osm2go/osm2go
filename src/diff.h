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

#ifndef DIFF_H
#define DIFF_H

#include "osm.h"

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct project_t;

void diff_save(const struct project_t *project, const osm_t *osm);
void diff_restore(struct appdata_t *appdata, struct project_t *project, osm_t *osm);
gboolean diff_present(const struct project_t *project);
void diff_remove(const struct project_t *project);
gboolean diff_is_clean(const osm_t *osm, gboolean honor_hidden_flags);

#ifdef __cplusplus
}
#endif

#endif // DIFF_H
