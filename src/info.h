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

#include <gtk/gtk.h>

#ifdef __cplusplus
#include <vector>

extern "C" {
#endif

gboolean info_dialog(GtkWidget *parent, appdata_t *appdata,
		     object_t *object);
#ifdef __cplusplus
}

#include "osm.h"

class tag_context_t {
public:
  explicit tag_context_t(appdata_t *a);

  appdata_t * const appdata;
  GtkWidget *dialog, *list;
  GtkListStore *store;
  object_t object;
  int presets_type;
  std::vector<tag_t *> tags;

  void info_tags_replace();
  void update_collisions();
};

gboolean info_tag_key_collision(const std::vector<tag_t *> &tags, const tag_t *tag);

#endif

#endif // INFO_H
