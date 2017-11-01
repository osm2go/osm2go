/*
 * Copyright (C) 2008 Andrew Chadwick <andrewc-osm2go@piffle.org>.
 * Copyright (C) 2008-2009 Till Harbaum <till@harbaum.org>.
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

#include "statusbar.h"
#include "appdata.h"
#include "osm2go_platform.h"

#include <hildon/hildon.h>

#include <osm2go_cpp.h>

static GdkColor color_red() {
  GdkColor color;
  gdk_color_parse("#ff0000", &color);
  return color;
}

class statusbar_fremantle : public statusbar_t {
public:
  statusbar_fremantle();

  GtkWidget *banner;

  virtual void set(const char *msg, bool highlight) O2G_OVERRIDE;
  virtual void banner_show_info(appdata_t &appdata, const char *text) O2G_OVERRIDE;
  virtual void banner_busy_start(appdata_t &appdata, const char *text) O2G_OVERRIDE;
  virtual void banner_busy_stop(appdata_t &appdata) O2G_OVERRIDE;

  void setBanner(appdata_t &appdata, GtkWidget *b);
};

statusbar_fremantle::statusbar_fremantle()
  : statusbar_t(gtk_label_new(O2G_NULLPTR))
  , banner(O2G_NULLPTR)
{
  /* why the heck does hildon show this by default? It's useless!! */
  g_object_set(widget, "has-resize-grip", FALSE, O2G_NULLPTR);
}

void statusbar_fremantle::banner_busy_stop(appdata_t &appdata) {
  GtkWidget *win = appdata.window;
  if(G_UNLIKELY(win == O2G_NULLPTR || banner == O2G_NULLPTR))
    return;
  gtk_grab_remove(widget);
  gtk_widget_set_sensitive(win, TRUE);
  gtk_widget_destroy(banner);
  g_object_unref(banner);
  banner = O2G_NULLPTR;
}

// Cancel any animations currently going, and show a brief text message.

void statusbar_fremantle::banner_show_info(appdata_t &appdata, const char *text) {
  if(!appdata.window)
    return;
  setBanner(appdata, hildon_banner_show_information(appdata.window, O2G_NULLPTR, text));
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

void statusbar_fremantle::banner_busy_start(appdata_t &appdata, const char *text) {
  GtkWidget *win = appdata.window;
  if(G_UNLIKELY(win == O2G_NULLPTR))
    return;
  setBanner(appdata, hildon_banner_show_progress(win, O2G_NULLPTR, text));
  gtk_widget_set_sensitive(win, FALSE);
  gtk_grab_add(widget);
  osm2go_platform::process_events();
}

void statusbar_fremantle::set(const char *msg, bool highlight) {
  static const GdkColor color = color_red();
  const GdkColor *col = highlight ? &color : O2G_NULLPTR;

  gtk_widget_modify_fg(widget, GTK_STATE_NORMAL, col);
  gtk_widget_modify_text(widget, GTK_STATE_NORMAL, col);

  printf("statusbar_set: %s\n", msg);

  gtk_label_set_text(GTK_LABEL(widget), msg);
}

void statusbar_fremantle::setBanner(appdata_t &appdata, GtkWidget *b)
{
  banner_busy_stop(appdata);
  banner = b;
  g_object_ref(banner);
  gtk_widget_show(banner);
}

statusbar_t *statusbar_t::create()
{
  return new statusbar_fremantle();
}

// vim:et:ts=8:sw=2:sts=2:ai
