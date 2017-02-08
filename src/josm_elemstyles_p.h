/*
 * Copyright (C) 2017 Rolf Eike Beer <eike@sf-mail.de>.
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

#ifndef JOSM_ELEMSTYLES_P_H
#define JOSM_ELEMSTYLES_P_H

#include "josm_elemstyles.h"
#if __cplusplus >= 201103L
#include <cstdint>
#else
#include <stdint.h>
#endif

struct elemstyle_condition_t {
    elemstyle_condition_t(xmlChar *k, xmlChar *v) : key(k), value(v) {}
    xmlChar *key;
    xmlChar *value;
};

/* from elemstyles.xml:
 *  line attributes
 *  - width absolute width in pixel in every zoom level
 *  - realwidth relative width which will be scaled in meters, integer
 *  - colour
 */

struct elemstyle_line_t {
  gint width;
  elemstyle_color_t color;
  gboolean dashed: 8;
  gint dash_length: 24;  // <= 0 means dash length is based on the width

  struct {
    gboolean valid : 8;
    gint width : 24;
  } real;

  struct {
    gboolean valid: 8;
    gint width : 24;
    elemstyle_color_t color;
  } bg;
};

G_STATIC_ASSERT(sizeof(elemstyle_line_t) == 6*4);
G_STATIC_ASSERT(sizeof(reinterpret_cast<elemstyle_line_t *>(0)->bg) == 8);

/* attribute modifiers */
typedef enum {
  ES_MOD_NONE = 0,  // don't change attribute
  ES_MOD_ADD,       // add constant value
  ES_MOD_SUB,       // subtract constant value
  ES_MOD_PERCENT    // scale by x percent
} elemstyle_mod_mode_t;

/* a width with modifier */
struct elemstyle_width_mod_t {
  elemstyle_mod_mode_t mod:8;
  int8_t width;
} __attribute__ ((packed));

struct elemstyle_line_mod_t {
  elemstyle_width_mod_t line, bg;
} __attribute__ ((packed));

G_STATIC_ASSERT(sizeof(elemstyle_line_mod_t) == 4);

struct elemstyle_area_t {
  elemstyle_color_t color;
};

struct elemstyle_icon_t {
  elemstyle_icon_t()
    : annotate(false)
    , filename(0)
  {
  }
  ~elemstyle_icon_t()
  {
    g_free(filename);
  }

  bool annotate;
  char *filename;
};

struct elemstyle_t {
  elemstyle_t()
    : type(ES_TYPE_NONE)
    , line(0)
    , zoom_max(0.0f)
  {
  }
  ~elemstyle_t();

  std::vector<elemstyle_condition_t> conditions;

  elemstyle_type_t type;

  union {
    elemstyle_line_mod_t line_mod;
    elemstyle_line_t *line;
    elemstyle_area_t area;
  };

  float zoom_max;
  elemstyle_icon_t icon;
};

#endif // JOSM_ELEMSTYLES_P_H
