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

#include "statusbar.h"

#include <gtk/gtk.h>

#define YETI_PASSIVE_WIDGET appdata->statusbar->widget

#ifdef USE_HILDON
#include <hildon/hildon.h>

// Clear any current animations.

void banner_clear(appdata_t *appdata) {
  if (! (appdata->window && appdata->banner))
    return;
  if (appdata->banner_is_grabby) {
    gtk_grab_remove(YETI_PASSIVE_WIDGET);
    GtkWidget *win = GTK_WIDGET(appdata->window);
#if MAEMO_VERSION_MAJOR < 5
    GtkWidget *menu = GTK_WIDGET(hildon_window_get_menu(HILDON_WINDOW(win)));
    GtkWidget *menu_att = gtk_menu_get_attach_widget(
		  hildon_window_get_menu(HILDON_WINDOW(win)));
#endif
    gtk_widget_set_sensitive(win, TRUE);
#if MAEMO_VERSION_MAJOR < 5
    gtk_widget_set_sensitive(menu, TRUE);
    gtk_widget_set_sensitive(menu_att, TRUE);
#endif
  }
  gtk_widget_destroy(appdata->banner);
  g_object_unref(appdata->banner);
  appdata->banner = NULL;
}


// Cancel any animations currently going, and show a brief text message.

void banner_show_info(appdata_t *appdata, char *text) {
  if (!appdata->window)
    return;
  banner_clear(appdata);
  appdata->banner = hildon_banner_show_information(
    GTK_WIDGET(appdata->window), NULL, text);
  g_object_ref(appdata->banner);
  gtk_widget_show(appdata->banner);
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

void banner_busy_start(appdata_t *appdata, gboolean grab, char *text) {
  if (!appdata->window)
    return;
  banner_clear(appdata);
  appdata->banner = hildon_banner_show_animation(
    GTK_WIDGET(appdata->window), NULL, text);
  g_object_ref(appdata->banner);
  gtk_widget_show(appdata->banner);
  appdata->banner_is_grabby = grab;
  if (appdata->banner_is_grabby) {
    GtkWidget *win = GTK_WIDGET(appdata->window);
#if MAEMO_VERSION_MAJOR < 5
    GtkWidget *menu = GTK_WIDGET(hildon_window_get_menu(HILDON_WINDOW(win)));
    GtkWidget *menu_att = gtk_menu_get_attach_widget(
		  hildon_window_get_menu(HILDON_WINDOW(win)));
#endif
    gtk_widget_set_sensitive(win, FALSE);
#if MAEMO_VERSION_MAJOR < 5
    gtk_widget_set_sensitive(menu, FALSE);
    gtk_widget_set_sensitive(menu_att, FALSE);
#endif
    gtk_grab_add(YETI_PASSIVE_WIDGET);
  }
  banner_busy_tick();
}


#else  // USE_HILDON

/*
 * For non-Hildon builds, use the "brief" message in the statusbar to show
 * what's happening.
 */

#include "statusbar.h"

void banner_show_info(appdata_t *appdata, char *text) {
  banner_clear(appdata);
  statusbar_brief(appdata, text, 0);
}

void banner_busy_start(appdata_t *appdata, gboolean grab, char *text) {
  banner_clear(appdata);
  statusbar_brief(appdata, text, -1);
  appdata->banner_is_grabby = grab;
  if (appdata->banner_is_grabby) {
    GtkWidget *win;
    win = GTK_WIDGET(appdata->window);
    gtk_widget_set_sensitive(win, FALSE);
    gtk_grab_add(YETI_PASSIVE_WIDGET);
  }
}

void banner_clear(appdata_t *appdata) {
  statusbar_brief(appdata, NULL, 0);
  if (appdata->banner_is_grabby) {
    GtkWidget *win;
    win = GTK_WIDGET(appdata->window);
    gtk_widget_set_sensitive(win, TRUE);
    gtk_grab_remove(YETI_PASSIVE_WIDGET);
  }
}


#endif //USE_HILDON


// Just an alias right now

void banner_busy_stop(appdata_t *appdata) {
  banner_clear(appdata);
}


/*
 * Process any outstanding GTK events to make the app look more responsive
 * while still allowing long-running things to process in the mainloop.
 * This could perhaps be generalised; it isn't banner-specific.
 */

void banner_busy_tick() {
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }
}

// vim:et:ts=8:sw=2:sts=2:ai
