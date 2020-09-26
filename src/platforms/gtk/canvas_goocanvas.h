/*
 * SPDX-FileCopyrightText: 2019 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <canvas.h>

#include <pos.h>

#include <array>
#include <goocanvas.h>
#include <gtk/gtk.h>
#include <memory>

#include "osm2go_platform_gtk.h"

struct canvas_dimensions;

class canvas_goocanvas : public canvas_t {
public:
  canvas_goocanvas();

  std::array<GooCanvasItem *, CANVAS_GROUPS> group;

  struct canvas_bounds {
    inline canvas_bounds() : min(lpos_t(0, 0)), max(lpos_t(0, 0)) {}
    lpos_t min, max;
  } bounds;

  struct {
    struct { float x, y; } scale;
    std::unique_ptr<GdkPixbuf, g_object_deleter> pix;
  } bg;

  canvas_dimensions get_viewport_dimensions() const;
  bool isVisible(const lpos_t lpos) const;
};
