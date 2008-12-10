/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
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

#include "appdata.h"

void statusbar_highlight(appdata_t *appdata, gboolean highlight) {
  if(highlight) {
    GdkColor color;
    gdk_color_parse("red", &color); 
    gtk_widget_modify_bg(appdata->statusbar->eventbox, GTK_STATE_NORMAL, &color);
    gtk_widget_modify_base(appdata->statusbar->eventbox, GTK_STATE_NORMAL, &color);
    gtk_widget_modify_fg(appdata->statusbar->eventbox, GTK_STATE_NORMAL, &color);
  } else
    gtk_widget_modify_bg(appdata->statusbar->eventbox, GTK_STATE_NORMAL, NULL);
}

void statusbar_set(appdata_t *appdata, const char *msg, gboolean highlight) {
  statusbar_highlight(appdata, highlight);

  printf("statusbar set: %s\n", msg);

  static guint mid = 0;
  if(mid)
    gtk_statusbar_pop(GTK_STATUSBAR(appdata->statusbar->widget),
		      appdata->statusbar->cid);

  if(msg)
    mid = gtk_statusbar_push(GTK_STATUSBAR(appdata->statusbar->widget),
			   appdata->statusbar->cid, msg);
}

GtkWidget *statusbar_new(appdata_t *appdata) {
  appdata->statusbar = (statusbar_t*)g_new0(statusbar_t, 1);

  appdata->statusbar->eventbox = gtk_event_box_new();
  appdata->statusbar->widget = gtk_statusbar_new();

#ifdef USE_HILDON
  /* why the heck does hildon show this by default? It's useless!! */
  g_object_set(appdata->statusbar->widget,
	       "has-resize-grip", FALSE, 
	       NULL );
#endif
  gtk_container_add(GTK_CONTAINER(appdata->statusbar->eventbox), 
		    appdata->statusbar->widget);

  appdata->statusbar->cid = gtk_statusbar_get_context_id(
		GTK_STATUSBAR(appdata->statusbar->widget), "Msg");

  return appdata->statusbar->eventbox;
}

void statusbar_free(statusbar_t *statusbar) {
  if(statusbar)
    g_free(statusbar);
}
