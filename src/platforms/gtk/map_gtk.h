/*
 * Copyright (C) 2018 Rolf Eike Beer <eike@sf-mail.de>.
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

#include <map.h>

#include "osm2go_platform_gtk.h"

#include <glib.h>

class map_gtk : public map_t {
public:
  explicit map_gtk(appdata_t &a);
  ~map_gtk() override {}

  void set_autosave(bool enable) override;
  gboolean key_press_event(unsigned int keyval);

private:
  osm2go_platform::Timer autosave;

  static gboolean map_motion_notify_event(GtkWidget *, GdkEventMotion *event, map_gtk *map);
  static gboolean map_button_event(map_gtk *map, GdkEventButton *event);
};
