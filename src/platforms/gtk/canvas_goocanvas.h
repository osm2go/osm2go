/*
 * Copyright (C) 2019 Rolf Eike Beer <eike@sf-mail.de>.
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
