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

#include "osm.h"

#include <glib.h>
#include <libxml/tree.h>

#ifdef __cplusplus
extern "C" {
#endif

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

typedef gulong elemstyle_color_t;

/* from elemstyles.xml:
 *  line attributes
 *  - width absolute width in pixel in every zoom level
 *  - realwidth relative width which will be scaled in meters, integer
 *  - colour
 */

typedef struct {
  gint width;
  elemstyle_color_t color;
  gboolean dashed;
  gint dash_length;  // <= 0 means dash length is based on the width

  struct {
    gboolean valid;
    gint width;
  } real;

  struct {
    gboolean valid;
    gint width;
    elemstyle_color_t color;
  } bg;

  float zoom_max;   // XXX probably belongs in elemstyle_t
} elemstyle_line_t;

/* attribute modifiers */
typedef enum {
  ES_MOD_NONE = 0,  // don't change attribute
  ES_MOD_ADD,       // add constant value
  ES_MOD_SUB,       // subtract constant value
  ES_MOD_PERCENT    // scale by x percent
} elemstyle_mod_mode_t;

/* a width with modifier */
typedef struct {
  elemstyle_mod_mode_t mod;
  gint width;
} elemstyle_width_mod_t;


typedef struct {
  elemstyle_width_mod_t line, bg;
} elemstyle_line_mod_t;

typedef struct {
  elemstyle_color_t color;
  float zoom_max;   // XXX probably belongs in elemstyle_t
} elemstyle_area_t;

typedef struct {
  gboolean annotate;
  char *filename;
  float zoom_max;   // XXX probably belongs in elemstyle_t
} elemstyle_icon_t;

typedef struct elemstyle_t elemstyle_t;

elemstyle_t *josm_elemstyles_load(const char *name);
void josm_elemstyles_free(elemstyle_t *elemstyles);
gboolean parse_color(xmlNode *a_node, const char *name, elemstyle_color_t *color);

struct style_t;

void josm_elemstyles_colorize_node(const struct style_t *style, node_t *node);
void josm_elemstyles_colorize_way(const struct style_t *style, way_t *way);
void josm_elemstyles_colorize_world(const struct style_t *style, osm_t *osm);

#ifdef __cplusplus
}
#endif

#endif // JOSM_ELEMSTYLES_H

// vim:et:ts=8:sw=2:sts=2:ai
