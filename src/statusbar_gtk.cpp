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

#ifdef FREMANTLE
#include <hildon/hildon.h>
#endif

#include <osm2go_cpp.h>

static GdkColor color_red() {
  GdkColor color;
  gdk_color_parse("#ff0000", &color);
  return color;
}

class statusbar_gtk : public statusbar_t {
public:
  statusbar_gtk();

  guint brief_handler_id;
  guint brief_mid;
  guint cid;
  guint mid;

  /**
   * @brief flash up a brief, temporary message.
   * @param msg the message to show
   * @param timeout the timeout in seconds
   *
   * Once the message disappears, drop back to any persistent message set
   * with set().
   *
   * If @msg is nullptr, clear the message and don't establish a handler.
   *
   * If timeout is negative, don't establish a handler. You'll have to clear it
   * yourself later. If it's zero, use the default.
   */
  void brief(const char *msg, int timeout);

  virtual void set(const char *msg, bool highlight) O2G_OVERRIDE;
  virtual void banner_show_info(appdata_t &appdata, const char *text) O2G_OVERRIDE;
  virtual void banner_busy_start(appdata_t &appdata, const char *text) O2G_OVERRIDE;
  virtual void banner_busy_stop(appdata_t &appdata) O2G_OVERRIDE;
};

/*
 * For non-Hildon builds, use the "brief" message in the statusbar to show
 * what's happening.
 */

void statusbar_gtk::banner_busy_stop(appdata_t &appdata) {
  brief(O2G_NULLPTR, 0);
  gtk_widget_set_sensitive(appdata.window, TRUE);
  gtk_grab_remove(widget);
}

void statusbar_gtk::banner_show_info(appdata_t &appdata, const char *text) {
  banner_busy_stop(appdata);
  brief(text, 0);
}

void statusbar_gtk::banner_busy_start(appdata_t &appdata, const char *text) {
  banner_busy_stop(appdata);
  brief(text, -1);
  gtk_widget_set_sensitive(appdata.window, FALSE);
  gtk_grab_add(widget);
}

static void statusbar_highlight(statusbar_t *statusbar, bool highlight) {
  GtkWidget * const w = GTK_STATUSBAR(statusbar->widget)->label;
  static const GdkColor color = color_red();
  const GdkColor *col = highlight ? &color : O2G_NULLPTR;

  gtk_widget_modify_fg(w, GTK_STATE_NORMAL, col);
  gtk_widget_modify_text(w, GTK_STATE_NORMAL, col);
}

void statusbar_gtk::set(const char *msg, bool highlight) {
  statusbar_highlight(this, highlight);

  printf("statusbar_set: %s\n", msg);

  if (mid) {
    gtk_statusbar_remove(GTK_STATUSBAR(widget), cid, mid);
    mid = 0;
  }

  if (msg)
    mid = gtk_statusbar_push(GTK_STATUSBAR(widget), cid, msg);
}

// Clear any brief message currently set, dropping back to the persistent one.

static gboolean statusbar_brief_clear(gpointer data) {
  statusbar_gtk *statusbar = static_cast<statusbar_gtk *>(data);
  if (statusbar->brief_mid) {
    gtk_statusbar_remove(GTK_STATUSBAR(statusbar->widget),
                         statusbar->cid, statusbar->brief_mid);
    statusbar->brief_mid = 0;
    statusbar_highlight(statusbar, false);
  }
  return FALSE;
}

void statusbar_gtk::brief(const char *msg, int timeout)
{
  printf("%s: %s\n", __PRETTY_FUNCTION__, msg);
  if (brief_handler_id) {
    g_source_remove(brief_handler_id);
    brief_handler_id = 0;
  }
  statusbar_brief_clear(this);
  if (msg) {
    statusbar_highlight(this, true);
    brief_mid = gtk_statusbar_push(GTK_STATUSBAR(widget), cid, msg);
    if (brief_mid && (timeout >= 0)) {
      if (timeout == 0) {
        timeout = STATUSBAR_DEFAULT_BRIEF_TIME;
      }
      brief_handler_id = g_timeout_add_seconds(timeout, statusbar_brief_clear, this);
    }
  }
}

statusbar_gtk::statusbar_gtk()
  : statusbar_t(gtk_statusbar_new())
  , brief_handler_id(0)
  , brief_mid(0)
  , cid(gtk_statusbar_get_context_id(GTK_STATUSBAR(widget), "Msg"))
  , mid(0)
{
}

statusbar_t *statusbar_t::create()
{
  return new statusbar_gtk();
}

// vim:et:ts=8:sw=2:sts=2:ai
