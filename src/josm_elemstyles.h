/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "osm.h"

#include <libxml/tree.h>

#include <vector>

class color_t;
struct elemstyle_t;
struct style_t;

// Ratio conversions

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

void josm_elemstyles_colorize_node(const style_t *style, node_t *node);
void josm_elemstyles_colorize_way(const style_t *style, way_t *way);
void josm_elemstyles_colorize_world(const style_t *styles, osm_t::ref osm);
