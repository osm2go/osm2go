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

#include <color.h>
#include <pos.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <osm2go_annotations.h>

void osm2go_platform::gtk_widget_deleter::operator()(GtkWidget *mem) {
  gtk_widget_destroy(mem);
}

void osm2go_platform::process_events()
{
  while(gtk_events_pending())
    gtk_main_iteration();
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

void osm2go_platform::dialog_size_hint(GtkWindow *window, osm2go_platform::DialogSizeHint hint)
{
  static const gint dialog_sizes[][2] = {
#ifdef FREMANTLE
    { 400, 100 },  // SMALL
    /* in maemo5 most dialogs are full screen */
    { 800, 480 },  // MEDIUM
    { 790, 380 },  // LARGE
    { 640, 100 },  // WIDE
    { 450, 480 },  // HIGH
#else
    { 300, 100 },  // SMALL
    { 400, 300 },  // MEDIUM
    { 500, 350 },  // LARGE
    { 450, 100 },  // WIDE
    { 200, 350 },  // HIGH
#endif
  };

  gtk_window_set_default_size(window, dialog_sizes[hint][0], dialog_sizes[hint][1]);
}

void osm2go_platform::MappedFile::reset()
{
  if(likely(map != nullptr)) {
#if GLIB_CHECK_VERSION(2,22,0)
    g_mapped_file_unref(map);
#else
    g_mapped_file_free(map);
#endif
    map = nullptr;
  }
}

bool osm2go_platform::parse_color_string(const char *str, color_t &color)
{
  /* we parse this directly since gdk_color_parse doesn't cope */
  /* with the alpha channel that may be present */
  if (strlen(str + 1) == 8) {
    char *err;

    color = strtoul(str + 1, &err, 16);

    return (*err == '\0');
  } else {
    GdkColor gdk_color;
    if(gdk_color_parse(str, &gdk_color)) {
      color = color_t(gdk_color.red, gdk_color.green, gdk_color.blue);
      return true;
    }
    return false;
  }
}

static GdkColor parseRed()
{
  GdkColor color;
  gdk_color_parse("red", &color);
  return color;
}

const GdkColor *osm2go_platform::invalid_text_color()
{
  static const GdkColor red = parseRed();

  return &red;
}

double osm2go_platform::string_to_double(const char *str)
{
  if(likely(str != nullptr))
    return g_ascii_strtod(str, nullptr);
  else
    return NAN;
}
