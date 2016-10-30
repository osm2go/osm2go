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

#ifndef INFO_H
#define INFO_H

#include "appdata.h"
#include "osm.h"

#include <gtk/gtk.h>

typedef struct {
  appdata_t *appdata;
  GtkWidget *dialog, *list;
  GtkListStore *store;
  object_t object;
  tag_t **tag;
  int presets_type;
} tag_context_t;


gboolean info_dialog(GtkWidget *parent, appdata_t *appdata,
		     object_t *object);
void info_tags_replace(tag_context_t *context);
gboolean info_tag_key_collision(const tag_t *tags, const tag_t *tag);

#endif // INFO_H
