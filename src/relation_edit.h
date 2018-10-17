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

#include "osm.h"

#include <osm2go_platform.h>

class map_t;
class presets_items;

void relation_membership_dialog(osm2go_platform::Widget *parent, const presets_items *presets,
                                osm_t::ref osm, object_t &object);

void relation_list(osm2go_platform::Widget *parent, map_t *map, osm_t::ref osm, presets_items *presets);
void relation_show_members(osm2go_platform::Widget *parent, const relation_t *relation, osm_t::ref osm);
