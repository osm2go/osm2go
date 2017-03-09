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

#ifndef RELATION_EDIT_H
#define RELATION_EDIT_H

#include <gtk/gtk.h>

#include "appdata.h"
#include "osm.h"

#ifdef __cplusplus
void relation_membership_dialog(GtkWidget *parent, appdata_t *appdata, object_t &object);

extern "C" {
#endif

void relation_list(GtkWidget *parent, appdata_t *appdata);
void relation_show_members(GtkWidget *parent, const relation_t *relation);

#ifdef __cplusplus
}
#endif

#endif // RELATION_EDIT_H
