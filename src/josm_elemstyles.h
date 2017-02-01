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

#ifndef JOSM_ELEMSTYLES_H
#define JOSM_ELEMSTYLES_H

#ifdef __cplusplus

#include "osm.h"

#include <glib.h>
#include <libxml/tree.h>

#include <vector>

// Ratio conversions

float scaledn_to_zoom(const float scaledn);
float zoom_to_scaledn(const float zoom);

typedef enum {
  ES_TYPE_NONE = 0,
  ES_TYPE_LINE,
  ES_TYPE_AREA,
  ES_TYPE_LINE_MOD
} elemstyle_type_t;

#define DEFAULT_DASH_LENGTH 0

typedef guint elemstyle_color_t;

struct elemstyle_t;

std::vector<elemstyle_t *> josm_elemstyles_load(const char *name);
void josm_elemstyles_free(std::vector<elemstyle_t *> &elemstyles);
bool parse_color(xmlNode *a_node, const char *name, elemstyle_color_t &color);

struct style_t;

void josm_elemstyles_colorize_node(const style_t *style, node_t *node);
void josm_elemstyles_colorize_way(const style_t *style, way_t *way);
void josm_elemstyles_colorize_world(const style_t *style, osm_t *osm);

#endif

#endif // JOSM_ELEMSTYLES_H

// vim:et:ts=8:sw=2:sts=2:ai
