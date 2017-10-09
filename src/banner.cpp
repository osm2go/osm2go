/*
 * Copyright (C) 2008 Andrew Chadwick <andrewc-osm2go@piffle.org>.
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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "banner.h"

#include "appdata.h"
#include "osm2go_platform.h"
#include "statusbar.h"

#include <gtk/gtk.h>

#define YETI_PASSIVE_WIDGET appdata.statusbar->widget

#include <osm2go_cpp.h>

#ifdef FREMANTLE
#include <hildon/hildon.h>

// Clear any current animations.

void banner_clear(appdata_t &appdata) {
  if(!appdata.window || !appdata.banner)
    return;
  gtk_grab_remove(YETI_PASSIVE_WIDGET);
  GtkWidget *win = GTK_WIDGET(appdata.window);
  gtk_widget_set_sensitive(win, TRUE);
  gtk_widget_destroy(appdata.banner);
  g_object_unref(appdata.banner);
  appdata.banner = O2G_NULLPTR;
}

// Cancel any animations currently going, and show a brief text message.

void banner_show_info(appdata_t &appdata, const char *text) {
  if(!appdata.window)
    return;
  banner_clear(appdata);
  appdata.banner = hildon_banner_show_information(
    GTK_WIDGET(appdata.window), O2G_NULLPTR, text);
  g_object_ref(appdata.banner);
  gtk_widget_show(appdata.banner);
}

/*
 * Start a spinner animation going to demonstrate that something's happening
 * behind the scenes. If `grab` is true, use the Yeti trick to grab the pointer
 * during the animation: this gives the impression that the app is doing
 * something while blocking the rest of the UI. banner_busy_stop() and
 * banner_clear() will ungrab if grab is set.
 *
 * Yeti mode:
 *   http://mail.gnome.org/archives/gtk-app-devel-list/2006-May/msg00020.html
 */

void banner_busy_start(appdata_t &appdata, const char *text) {
  if(!appdata.window)
    return;
  banner_clear(appdata);
  appdata.banner = hildon_banner_show_animation(
    GTK_WIDGET(appdata.window), O2G_NULLPTR, text);
  g_object_ref(appdata.banner);
  gtk_widget_show(appdata.banner);
  GtkWidget *win = GTK_WIDGET(appdata.window);
  gtk_widget_set_sensitive(win, FALSE);
  gtk_grab_add(YETI_PASSIVE_WIDGET);
  osm2go_platform::process_events();
}

#else  // FREMANTLE

/*
 * For non-Hildon builds, use the "brief" message in the statusbar to show
 * what's happening.
 */

#include "statusbar.h"

void banner_show_info(appdata_t &appdata, const char *text) {
  banner_clear(appdata);
  appdata.statusbar->brief(text, 0);
}

void banner_busy_start(appdata_t &appdata, const char *text) {
  banner_clear(appdata);
  appdata.statusbar->brief(text, -1);
  gtk_widget_set_sensitive(appdata.window, FALSE);
  gtk_grab_add(YETI_PASSIVE_WIDGET);
}

void banner_clear(appdata_t &appdata) {
  appdata.statusbar->brief(O2G_NULLPTR, 0);
  gtk_widget_set_sensitive(appdata.window, TRUE);
  gtk_grab_remove(YETI_PASSIVE_WIDGET);
}

#endif //FREMANTLE

// Just an alias right now

void banner_busy_stop(appdata_t &appdata) {
  banner_clear(appdata);
}
