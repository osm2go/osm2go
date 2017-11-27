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

#ifndef RELATION_EDIT_H
#define RELATION_EDIT_H

#include <gtk/gtk.h>

class map_t;
struct object_t;
struct osm_t;
struct presets_items;
class relation_t;

void relation_membership_dialog(GtkWidget *parent, const presets_items *presets,
                                osm_t *osm, object_t &object);

void relation_list(GtkWidget *parent, map_t *map, osm_t *osm, presets_items *presets);
void relation_show_members(GtkWidget *parent, const relation_t *relation);

#endif // RELATION_EDIT_H
