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

#if !defined(USE_HILDON) || (MAEMO_VERSION_MAJOR < 5)
void statusbar_highlight(statusbar_t *statusbar, gboolean highlight) {
  GtkStatusbar *bar = GTK_STATUSBAR(statusbar->widget);

  if(highlight) {
    GdkColor color;
    gdk_color_parse("red", &color);
    gtk_widget_modify_fg(bar->label, GTK_STATE_NORMAL, &color);
    gtk_widget_modify_text(bar->label, GTK_STATE_NORMAL, &color);
  } else {
    gtk_widget_modify_fg(bar->label, GTK_STATE_NORMAL, NULL);
    gtk_widget_modify_text(bar->label, GTK_STATE_NORMAL, NULL);
  }
}

// Set the persistent message, replacing anything currently there.
void statusbar_set(statusbar_t *statusbar, const char *msg, gboolean highlight) {
  statusbar_highlight(statusbar, highlight);

  printf("statusbar_set: %s\n", msg);

  if (statusbar->mid) {
    gtk_statusbar_remove(GTK_STATUSBAR(statusbar->widget),
                         statusbar->cid, statusbar->mid);
    statusbar->mid = 0;
  }

  if (msg) {
    guint mid = gtk_statusbar_push(GTK_STATUSBAR(statusbar->widget),
                                   statusbar->cid, msg);
    statusbar->mid = mid;
  }
}

#ifndef USE_HILDON
// Clear any brief message currently set, dropping back to the persistent one.

static gboolean statusbar_brief_clear(gpointer data) {
  statusbar_t *statusbar = (statusbar_t *)data;
  if (statusbar->brief_mid) {
    gtk_statusbar_remove(GTK_STATUSBAR(statusbar->widget),
                         statusbar->cid, statusbar->brief_mid);
    statusbar->brief_mid = 0;
    statusbar_highlight(statusbar, FALSE);
  }
  return FALSE;
}

// Flash up a brief, temporary message. Once it disappears, drop back to any
// persistent message set with statusbar_set().
//
// If msg is NULL, clear the message and don't establish a handler.
//
// If timeout is negative, don't establish a handler. You'll have to clear it
// yourself later. If it's zero, use the default.

void statusbar_brief(statusbar_t *statusbar, const char *msg, gint timeout) {
  printf("statusbar_brief: %s\n", msg);
  if (statusbar->brief_handler_id) {
    gtk_timeout_remove(statusbar->brief_handler_id);
    statusbar->brief_handler_id = 0;
  }
  statusbar_brief_clear(statusbar);
  guint mid = 0;
  if (msg) {
    statusbar_highlight(statusbar, TRUE);
    mid = gtk_statusbar_push(GTK_STATUSBAR(statusbar->widget),
                                 statusbar->cid, msg);
    if (mid) {
      statusbar->brief_mid = mid;
    }
  }
  if (mid && (timeout >= 0)) {
    if (timeout == 0) {
      timeout = STATUSBAR_DEFAULT_BRIEF_TIME;
    }
    statusbar->brief_handler_id
      = gtk_timeout_add(timeout, statusbar_brief_clear, statusbar);
  }
}
#endif

statusbar_t *statusbar_new(void) {
  statusbar_t *statusbar = (statusbar_t*)g_new0(statusbar_t, 1);

  statusbar->widget = gtk_statusbar_new();

#ifdef USE_HILDON
  /* why the heck does hildon show this by default? It's useless!! */
  g_object_set(statusbar->widget,
	       "has-resize-grip", FALSE,
	       NULL );
#endif

  statusbar->cid = gtk_statusbar_get_context_id(
		GTK_STATUSBAR(statusbar->widget), "Msg");

  return statusbar;
}

#else
void statusbar_highlight(statusbar_t *statusbar, gboolean highlight) {
  if(highlight) {
    GdkColor color;
    gdk_color_parse("red", &color);
    gtk_widget_modify_fg(statusbar->widget, GTK_STATE_NORMAL, &color);
    gtk_widget_modify_text(statusbar->widget, GTK_STATE_NORMAL, &color);
  } else {
    gtk_widget_modify_fg(statusbar->widget, GTK_STATE_NORMAL, NULL);
    gtk_widget_modify_text(statusbar->widget, GTK_STATE_NORMAL, NULL);
  }
}


// Set the persistent message, replacing anything currently there.
void statusbar_set(statusbar_t *statusbar, const char *msg, gboolean highlight) {
  statusbar_highlight(statusbar, highlight);

  printf("statusbar_set: %s\n", msg);

  if(!msg)
    gtk_label_set_text(GTK_LABEL(statusbar->widget), msg);
  else
    gtk_label_set_text(GTK_LABEL(statusbar->widget), msg);
}

statusbar_t *statusbar_new(void) {
  statusbar_t *statusbar = (statusbar_t*)g_new0(statusbar_t, 1);

  statusbar->widget = gtk_label_new(NULL);
  return statusbar;
}

#endif

void statusbar_free(statusbar_t *statusbar) {
  g_free(statusbar);
}

// vim:et:ts=8:sw=2:sts=2:ai
