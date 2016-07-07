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

  float zoom_max;   // XXX probably belongs in elemstyle_s
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
  float zoom_max;   // XXX probably belongs in elemstyle_s
} elemstyle_area_t;

typedef struct {
  gboolean annotate;
  char *filename;
  float zoom_max;   // XXX probably belongs in elemstyle_s
} elemstyle_icon_t;

typedef struct elemstyle_condition_s {
    char *key;
    char *value;
    struct elemstyle_condition_s *next;
} elemstyle_condition_t;

typedef struct elemstyle_s {
  elemstyle_condition_t *condition;

  elemstyle_type_t type;

  union {
    elemstyle_line_mod_t *line_mod;
    elemstyle_line_t *line;
    elemstyle_area_t *area;
  };

  elemstyle_icon_t *icon;

  struct elemstyle_s *next;
} elemstyle_t;

elemstyle_t *josm_elemstyles_load(char *name);
void josm_elemstyles_free(elemstyle_t *elemstyles);
gboolean parse_color(xmlNode *a_node, char *name, elemstyle_color_t *color);

void josm_elemstyles_colorize_node(struct style_s *style, node_t *node);
void josm_elemstyles_colorize_way(struct style_s *style, way_t *way);
void josm_elemstyles_colorize_world(struct style_s *style, osm_t *osm);

#endif // JOSM_ELEMSTYLES_H

// vim:et:ts=8:sw=2:sts=2:ai
