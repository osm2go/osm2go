/*
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

#include <osm2go_cpp.h>

static GdkColor color_red() {
  GdkColor color;
  gdk_color_parse("#ff0000", &color);
  return color;
}

class statusbar_internal : public statusbar_t {
public:
  statusbar_internal();

#ifndef FREMANTLE
#ifndef USE_HILDON
  guint brief_handler_id;
  guint brief_mid;

  void brief(int timeout, const char *msg);
#endif /* USE_HILDON */
  guint cid;
  guint mid;

  inline void setMsg(const char *msg);
#endif /* FREMANTLE */
};

static void statusbar_highlight(statusbar_t *statusbar, bool highlight) {
  GtkWidget * const w =
#ifndef FREMANTLE
      GTK_STATUSBAR(statusbar->widget)->label;
#else
      statusbar->widget;
#endif
  static const GdkColor color = color_red();
  const GdkColor *col = highlight ? &color : O2G_NULLPTR;

  gtk_widget_modify_fg(w, GTK_STATE_NORMAL, col);
  gtk_widget_modify_text(w, GTK_STATE_NORMAL, col);
}

#ifndef FREMANTLE
void statusbar_internal::setMsg(const char *msg) {
  if (mid) {
    gtk_statusbar_remove(GTK_STATUSBAR(widget), cid, mid);
    mid = 0;
  }

  if (msg)
    mid = gtk_statusbar_push(GTK_STATUSBAR(widget), cid, msg);
}
#endif

void statusbar_t::set(const char *msg, bool highlight) {
  statusbar_highlight(this, highlight);

  printf("statusbar_set: %s\n", msg);

#ifndef FREMANTLE
  static_cast<statusbar_internal *>(this)->setMsg(msg);
#else
  gtk_label_set_text(GTK_LABEL(widget), msg);
#endif
}

#ifndef USE_HILDON
// Clear any brief message currently set, dropping back to the persistent one.

static gboolean statusbar_brief_clear(gpointer data) {
  statusbar_internal *statusbar = static_cast<statusbar_internal *>(data);
  if (statusbar->brief_mid) {
    gtk_statusbar_remove(GTK_STATUSBAR(statusbar->widget),
                         statusbar->cid, statusbar->brief_mid);
    statusbar->brief_mid = 0;
    statusbar_highlight(statusbar, false);
  }
  return FALSE;
}

void statusbar_t::brief(const char *msg, gint timeout) {
  printf("%s: %s\n", __PRETTY_FUNCTION__, msg);
  static_cast<statusbar_internal *>(this)->brief(timeout, msg);
}

void statusbar_internal::brief(int timeout, const char* msg)
{
  if (brief_handler_id) {
    g_source_remove(brief_handler_id);
    brief_handler_id = 0;
  }
  statusbar_brief_clear(this);
  guint mid = 0;
  if (msg) {
    statusbar_highlight(this, true);
    mid = gtk_statusbar_push(GTK_STATUSBAR(widget), cid, msg);
    if (mid) {
      brief_mid = mid;
    }
  }
  if (mid && (timeout >= 0)) {
    if (timeout == 0) {
      timeout = STATUSBAR_DEFAULT_BRIEF_TIME;
    }
    brief_handler_id
      = g_timeout_add_seconds(timeout, statusbar_brief_clear, this);
  }
}
#endif

statusbar_t::statusbar_t()
#ifndef FREMANTLE
  : widget(gtk_statusbar_new())
#else
  : widget(gtk_label_new(O2G_NULLPTR))
#endif
{
#ifdef USE_HILDON
  /* why the heck does hildon show this by default? It's useless!! */
  g_object_set(widget, "has-resize-grip", FALSE, O2G_NULLPTR);
#endif
}

statusbar_internal::statusbar_internal()
  : statusbar_t()
#ifndef FREMANTLE
#ifndef USE_HILDON
  , brief_handler_id(0)
  , brief_mid(0)
#endif
  , cid(gtk_statusbar_get_context_id(GTK_STATUSBAR(widget), "Msg"))
  , mid(0)
#endif
{
}

statusbar_t *statusbar_t::create()
{
  return new statusbar_internal();
}

// vim:et:ts=8:sw=2:sts=2:ai
