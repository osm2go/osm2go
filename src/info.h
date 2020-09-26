/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "osm.h"

#include <osm2go_platform.h>

#include <string>
#include <vector>

class map_t;
class presets_items;

class tag_context_t {
protected:
  tag_context_t(const object_t &o, const osm_t::TagMap &t, osm2go_platform::Widget *dlg);
public:
  tag_context_t() O2G_DELETED_FUNCTION;
  tag_context_t(const tag_context_t &) O2G_DELETED_FUNCTION;
  tag_context_t &operator=(const tag_context_t &) O2G_DELETED_FUNCTION;
#if __cplusplus >= 201103L
  tag_context_t(tag_context_t &&) = delete;
  tag_context_t &operator=(tag_context_t &&) = delete;
#endif

  osm2go_platform::DialogGuard dialog;
  object_t object;
  const osm_t::TagMap &tags;

  void info_tags_replace(const osm_t::TagMap &ntags);
};

bool info_dialog(osm2go_platform::Widget *parent, map_t *map, osm_t::ref osm, presets_items *presets, object_t &object);
