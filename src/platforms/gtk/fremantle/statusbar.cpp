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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "statusbar.h"

#include <appdata.h>
#include <misc.h>
#include <osm2go_platform.h>

#include <hildon/hildon.h>
#include <memory>

#include <osm2go_cpp.h>
#include <osm2go_stl.h>

static GdkColor color_red() {
  GdkColor color;
  gdk_color_parse("#ff0000", &color);
  return color;
}

class statusbar_fremantle : public statusbar_t {
public:
  statusbar_fremantle();

  std::unique_ptr<GtkWidget, g_object_deleter> banner;

  virtual void set(const char *msg, bool highlight) override;
  virtual void banner_show_info(const char *text) override;
  virtual void banner_busy_start(const char *text) override;
  virtual void banner_busy_stop() override;

  void setBanner(GtkWidget *b);
};

statusbar_fremantle::statusbar_fremantle()
  : statusbar_t(gtk_label_new(nullptr))
{
  /* why the heck does hildon show this by default? It's useless!! */
  g_object_set(widget, "has-resize-grip", FALSE, nullptr);
}

void statusbar_fremantle::banner_busy_stop() {
  GtkWidget *win = appdata_t::window;
  if(G_UNLIKELY(win == nullptr || !banner))
    return;
  gtk_grab_remove(widget);
  gtk_widget_set_sensitive(win, TRUE);
  gtk_widget_destroy(banner.get());
  banner.reset();
}

// Cancel any animations currently going, and show a brief text message.

void statusbar_fremantle::banner_show_info(const char *text) {
  if(G_UNLIKELY(appdata_t::window == nullptr))
    return;
  setBanner(hildon_banner_show_information(appdata_t::window, nullptr, text));
}

/*
 * Start a spinner animation going to demonstrate that something's happening
 * behind the scenes. If `grab` is true, use the Yeti trick to grab the pointer
 * during the animation: this gives the impression that the app is doing
 * something while blocking the rest of the UI. banner_busy_stop() and
 * banner_clear() will ungrab if grab is set.
 *
 * Yeti mode:
 *   https://mail.gnome.org/archives/gtk-app-devel-list/2006-May/msg00020.html
 */

void statusbar_fremantle::banner_busy_start(const char *text) {
  GtkWidget *win = appdata_t::window;
  if(G_UNLIKELY(win == nullptr))
    return;
  setBanner(hildon_banner_show_progress(win, nullptr, text));
  gtk_widget_set_sensitive(win, FALSE);
  gtk_grab_add(widget);
  osm2go_platform::process_events();
}

void statusbar_fremantle::set(const char *msg, bool highlight) {
  static const GdkColor color = color_red();
  const GdkColor *col = highlight ? &color : nullptr;

  gtk_widget_modify_fg(widget, GTK_STATE_NORMAL, col);
  gtk_widget_modify_text(widget, GTK_STATE_NORMAL, col);

  g_debug("%s: %s", __PRETTY_FUNCTION__, msg);

  gtk_label_set_text(GTK_LABEL(widget), msg);
}

void statusbar_fremantle::setBanner(GtkWidget *b)
{
  banner_busy_stop();
  banner.reset(b);
  g_object_ref(b);
  gtk_widget_show(b);
}

statusbar_t *statusbar_t::create()
{
  return new statusbar_fremantle();
}

// vim:et:ts=8:sw=2:sts=2:ai
