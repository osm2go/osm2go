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

#include <statusbar.h>

#include <appdata.h>
#include <misc.h>

#include <osm2go_platform.h>
#include <osm2go_platform_gtk.h>
#include <osm2go_cpp.h>

class statusbar_gtk : public statusbar_t {
public:
  statusbar_gtk();

  osm2go_platform::Timer brief_handler;
  guint brief_mid;
  guint cid;
  guint mid;

  void clear_message();
  /**
   * @brief flash up a brief, temporary message.
   * @param msg the message to show
   * @param timeout if the message should time out
   *
   * Once the message disappears, drop back to any persistent message set
   * with set().
   *
   * If timeout is false, don't establish a handler. You'll have to clear it
   * yourself later. If it's true, use STATUSBAR_DEFAULT_BRIEF_TIME.
   */
  void brief(const char *msg, bool timeout);

  virtual void set(const char *msg, bool highlight) override;
  virtual void banner_show_info(const char *text) override;
  virtual void banner_busy_start(const char *text) override;
  virtual void banner_busy_stop() override;
};

/*
 * For non-Hildon builds, use the "brief" message in the statusbar to show
 * what's happening.
 */

void statusbar_gtk::banner_busy_stop() {
  clear_message();
  gtk_widget_set_sensitive(appdata_t::window, TRUE);
  gtk_grab_remove(widget);
}

void statusbar_gtk::banner_show_info(const char *text) {
  banner_busy_stop();
  brief(text, true);
}

void statusbar_gtk::banner_busy_start(const char *text) {
  banner_busy_stop();
  brief(text, false);
  gtk_widget_set_sensitive(appdata_t::window, FALSE);
  gtk_grab_add(widget);
}

static void statusbar_highlight(statusbar_t *statusbar, bool highlight) {
  GtkWidget * const w = GTK_STATUSBAR(statusbar->widget)->label;
  const GdkColor *col = highlight ? osm2go_platform::invalid_text_color() : nullptr;

  gtk_widget_modify_fg(w, GTK_STATE_NORMAL, col);
  gtk_widget_modify_text(w, GTK_STATE_NORMAL, col);
  g_object_set(statusbar->widget, "has-resize-grip", FALSE, nullptr);
}

void statusbar_gtk::set(const char *msg, bool highlight) {
  statusbar_highlight(this, highlight);

  g_debug("%s: %s", __PRETTY_FUNCTION__, msg);

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

void statusbar_gtk::clear_message()
{
  brief_handler.stop();
  statusbar_brief_clear(this);
}

void statusbar_gtk::brief(const char *msg, bool timeout)
{
  clear_message();
  g_debug("%s: %s", __PRETTY_FUNCTION__, msg);
  statusbar_highlight(this, true);
  brief_mid = gtk_statusbar_push(GTK_STATUSBAR(widget), cid, msg);
  if (brief_mid && timeout)
    brief_handler.restart(STATUSBAR_DEFAULT_BRIEF_TIME, statusbar_brief_clear, this);
}

statusbar_gtk::statusbar_gtk()
  : statusbar_t(gtk_statusbar_new())
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
