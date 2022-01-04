/*
 * SPDX-FileCopyrightText: 2017-2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "josm_elemstyles.h"

#include <cstring>
#include <cstdint>
#include <string>
#include <variant>

#include <osm2go_cpp.h>

class base_object_t;

struct elemstyle_condition_t {
    elemstyle_condition_t(const char *k, const char *v);
    elemstyle_condition_t(const char *k, bool b);

    const char * const key;
    const std::variant<bool, const char *> value;

#if __cplusplus < 201103L
    elemstyle_condition_t &operator=(const elemstyle_condition_t &other)
    {
      memcpy(this, &other, sizeof(*this));
      return *this;
    }
#else
    elemstyle_condition_t(const elemstyle_condition_t &other) = default;
    ~elemstyle_condition_t() = default;
    elemstyle_condition_t &operator=(const elemstyle_condition_t &other) = delete;
#endif

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

  unsigned int width;
  color_t color;
  unsigned short dash_length_on;
  unsigned short dash_length_off;
  int priority;

  struct {
    bool valid : 8;
    unsigned int width : 24;
  } real;

  struct {
    bool valid: 8;
    unsigned int width : 24;
    color_t color;
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
  uint8_t width;
} __attribute__ ((packed));

struct elemstyle_line_mod_t {
  inline elemstyle_line_mod_t()
    : color(0)
  {
  }

  elemstyle_width_mod_t line, bg;
  color_t color;
} __attribute__ ((packed));

struct elemstyle_area_t {
  elemstyle_area_t()
    : color(0)
  {
  }

  color_t color;
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
    , zoom_max(0.0f)
  {
  }

  std::vector<elemstyle_condition_t> conditions;

  unsigned int type; ///< combination of elemstyle_type_t

  elemstyle_line_mod_t line_mod;
  std::unique_ptr<elemstyle_line_t> line;
  elemstyle_area_t area;

  float zoom_max;
  elemstyle_icon_t icon;
};
