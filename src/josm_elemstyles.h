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

#ifndef JOSM_ELEMSTYLES_H
#define JOSM_ELEMSTYLES_H

#include "color.h"
#include "osm.h"

#include <libxml/tree.h>

#include <vector>

// Ratio conversions

struct elemstyle_t;
struct style_t;

float scaledn_to_zoom(const float scaledn);

typedef enum {
  ES_TYPE_NONE = 0,
  ES_TYPE_AREA = 1,
  ES_TYPE_LINE = 2, ///< must not be combined with ES_TYPE_LINE_MOD
  ES_TYPE_LINE_MOD = 4 ///< must not be combined with ES_TYPE_LINE
} elemstyle_type_t;

#define DEFAULT_DASH_LENGTH 0

std::vector<elemstyle_t *> josm_elemstyles_load(const char *name);
void josm_elemstyles_free(std::vector<elemstyle_t *> &elemstyles);
bool parse_color(xmlNode *a_node, const char *name, color_t &color);

void josm_elemstyles_colorize_node(style_t *style, node_t *node);
void josm_elemstyles_colorize_way(const style_t *style, way_t *way);
void josm_elemstyles_colorize_world(style_t *style, osm_t *osm);

#endif // JOSM_ELEMSTYLES_H

// vim:et:ts=8:sw=2:sts=2:ai
