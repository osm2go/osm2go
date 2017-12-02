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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef JOSM_ELEMSTYLES_P_H
#define JOSM_ELEMSTYLES_P_H

#include "josm_elemstyles.h"

#include <cstring>
#include <cstdint>

#include <osm2go_cpp.h>

struct elemstyle_condition_t {
    elemstyle_condition_t(const char *k, const char *v)
      : key(strdup(k)), value(v ? strdup(v) : O2G_NULLPTR), isBool(false) {}
    elemstyle_condition_t(const char *k, bool b)
      : key(strdup(k)), boolValue(b), isBool(true) {}
    elemstyle_condition_t &operator=(const elemstyle_condition_t &other)
    {
      memcpy(this, &other, sizeof(*this));
      return *this;
    }

    char * const key;
#if __cplusplus < 201103L
    // a special version of the union, as the old compiler chokes
    // on the constness in the constructor
    union {
      char *value;
      bool boolValue;
    };
#else
    union {
      char * const value;
      const bool boolValue;
    };
#endif
    const bool isBool;

    bool matches(const base_object_t &obj) const;
};

/* from elemstyles.xml:
 *  line attributes
 *  - width absolute width in pixel in every zoom level
 *  - realwidth relative width which will be scaled in meters, integer
 *  - colour
 */

struct elemstyle_line_t {
  elemstyle_line_t() {
    memset(this, 0, sizeof(*this));
  }

  int priority;
  int width;
  elemstyle_color_t color;
  unsigned short dash_length_on;
  unsigned short dash_length_off;

  struct {
    bool valid : 8;
    int width : 24;
  } real;

  struct {
    bool valid: 8;
    int width : 24;
    elemstyle_color_t color;
  } bg;
};

/* attribute modifiers */
typedef enum {
  ES_MOD_NONE = 0,  // don't change attribute
  ES_MOD_ADD,       // add constant value
  ES_MOD_SUB,       // subtract constant value
  ES_MOD_PERCENT    // scale by x percent
} elemstyle_mod_mode_t;

/* a width with modifier */
struct elemstyle_width_mod_t {
  inline elemstyle_width_mod_t()
    : mod(ES_MOD_NONE)
    , width(0)
  {
  }

  elemstyle_mod_mode_t mod:8;
  int8_t width;
} __attribute__ ((packed));

struct elemstyle_line_mod_t {
  inline elemstyle_line_mod_t()
    : priority(0)
    , color(0)
  {
  }

  int priority;
  elemstyle_width_mod_t line, bg;
  elemstyle_color_t color;
} __attribute__ ((packed));

struct elemstyle_area_t {
  elemstyle_area_t()
    : priority(0)
    , color(0)
  {
  }

  int priority;
  elemstyle_color_t color;
};

struct elemstyle_icon_t {
  elemstyle_icon_t()
    : priority(0)
    , annotate(false)
  {
  }

  int priority;
  bool annotate;
  std::string filename;
};

struct elemstyle_t {
  elemstyle_t()
    : type(ES_TYPE_NONE)
    , line(O2G_NULLPTR)
    , zoom_max(0.0f)
  {
  }
  ~elemstyle_t();

  std::vector<elemstyle_condition_t> conditions;

  unsigned int type; ///< combination of elemstyle_type_t

  elemstyle_line_mod_t line_mod;
  elemstyle_line_t *line;
  elemstyle_area_t area;

  float zoom_max;
  elemstyle_icon_t icon;
};

#endif // JOSM_ELEMSTYLES_P_H
