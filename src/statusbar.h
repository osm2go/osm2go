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

#ifndef STATUSBAR_H
#define STATUSBAR_H

#define STATUSBAR_DEFAULT_BRIEF_TIME 3000

typedef struct statusbar_s {
  GtkWidget *widget, *eventbox;
  guint cid;
  guint mid;
  guint brief_mid;
  guint brief_handler_id;
} statusbar_t;

void statusbar_set(appdata_t *appdata, const char *msg, gboolean highlight);
void statusbar_brief(appdata_t *appdata, const char *msg, gint timeout);
GtkWidget *statusbar_new(appdata_t *appdata);
void statusbar_highlight(appdata_t *appdata, gboolean highlight);
void statusbar_free(statusbar_t *statusbar);

#endif // STATUSBAR_H
