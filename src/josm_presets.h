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

#ifndef JOSM_PRESETS_H
#define JOSM_PRESETS_H

#include <set>
#include <string>

#include <osm2go_platform.h>

struct object_t;
class relation_t;
class tag_context_t;

class presets_items {
protected:
  presets_items() {}
public:
  virtual ~presets_items() {}

  static presets_items *load(void);
  virtual std::set<std::string> roles(const relation_t *relation, const object_t &obj) const = 0;
};

std::string josm_icon_name_adjust(const char *name);

osm2go_platform::Widget *josm_build_presets_button(presets_items *presets, tag_context_t *tag_context);

#endif // JOSM_PRESETS_H
