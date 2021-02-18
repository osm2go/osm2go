/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "osm.h"
#include "style.h"

#include <libxml/tree.h>

#include <vector>

class color_t;
struct elemstyle_t;
class style_t;

// Ratio conversions

float scaledn_to_zoom(const float scaledn);

typedef enum {
  ES_TYPE_NONE = 0,
  ES_TYPE_AREA = 1,
  ES_TYPE_LINE = 2, ///< must not be combined with ES_TYPE_LINE_MOD
  ES_TYPE_LINE_MOD = 4 ///< must not be combined with ES_TYPE_LINE
} elemstyle_type_t;

#define DEFAULT_DASH_LENGTH 0

bool parse_color(xmlNode *a_node, const char *name, color_t &color);

class josm_elemstyle : public style_t {
public:
  josm_elemstyle() {}
  ~josm_elemstyle() override;

  bool load_elemstyles(const char *fname);

  void colorize(node_t *n) const override;
  void colorize(way_t *w) const override;

  std::vector<elemstyle_t *> elemstyles;
};
