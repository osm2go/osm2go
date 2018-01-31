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

#include "osm2go_platform.h"
#include "osm2go_platform_gtk.h"

#include <algorithm>
#include <cstdio>
#include <gtk/gtk.h>

#include <osm2go_annotations.h>

void osm2go_platform::process_events(bool tick)
{
  while(gtk_events_pending()) {
    if(tick)
      putchar('.');
    gtk_main_iteration();
  }
}

void osm2go_platform::Timer::restart(unsigned int seconds, GSourceFunc callback, void *data)
{
  assert_cmpnum(id, 0);
  id = g_timeout_add_seconds(seconds, callback, data);
}

void osm2go_platform::Timer::stop()
{
  if(likely(id != 0)) {
    g_source_remove(id);
    id = 0;
  }
}

struct combo_add_string {
  GtkWidget * const cbox;
  explicit combo_add_string(GtkWidget *w) : cbox(w) {}
  void operator()(const std::string &entry) {
    osm2go_platform::combo_box_append_text(cbox, entry.c_str());
  }
};

GtkWidget *osm2go_platform::string_select_widget(const char *title, const std::vector<std::string> &entries, int match) {
  GtkWidget *cbox = osm2go_platform::combo_box_new(title);

  /* fill combo box with entries */
  std::for_each(entries.begin(), entries.end(), combo_add_string(cbox));

  if(match >= 0)
    osm2go_platform::combo_box_set_active(cbox, match);

  return cbox;
}
