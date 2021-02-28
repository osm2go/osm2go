/*
 * SPDX-FileCopyrightText: 2018 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <map.h>

#include "osm2go_platform_gtk.h"

#include <glib.h>

class map_gtk : public map_t {
public:
  explicit map_gtk(appdata_t &a);
  inline ~map_gtk() override {}

  void set_autosave(bool enable) override;
  gboolean key_press_event(unsigned int keyval);

private:
  osm2go_platform::Timer autosave;

  static gboolean map_motion_notify_event(GtkWidget *, GdkEventMotion *event, map_gtk *map);
  static gboolean map_button_event(map_gtk *map, GdkEventButton *event);
};
