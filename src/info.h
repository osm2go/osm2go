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

#include "osm.h"

#include <gtk/gtk.h>
#include <string>
#include <vector>

class map_t;
struct presets_items;

class tag_context_t {
public:
  explicit tag_context_t(map_t *m, osm_t *os, presets_items *p, const object_t &o);
  ~tag_context_t();

  map_t * const map;
  osm_t * const osm;
  presets_items * const presets;
  GtkWidget *dialog, *list;
  GtkListStore *store;
  object_t object;
  osm_t::TagMap tags;

  void info_tags_replace();
  void update_collisions(const std::string &k);
};

void info_dialog(GtkWidget *parent, map_t *map, osm_t *osm, presets_items *presets);
bool info_dialog(GtkWidget *parent, map_t *map, osm_t *osm, presets_items *presets, object_t &object);

#endif // INFO_H
