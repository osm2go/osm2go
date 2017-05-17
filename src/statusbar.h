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

#include <gtk/gtk.h>

#define STATUSBAR_DEFAULT_BRIEF_TIME 3

typedef struct statusbar_t {
  GtkWidget * const widget;
#ifdef __cplusplus
  statusbar_t();

#if !defined(USE_HILDON) || (MAEMO_VERSION_MAJOR < 5)
  guint cid;
  guint mid;
#endif
#ifndef USE_HILDON
  guint brief_handler_id;
  guint brief_mid;
#endif

  void set(const char *msg, gboolean highlight);
#endif
} statusbar_t;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef USE_HILDON
void statusbar_brief(statusbar_t *statusbar, const char *msg, gint timeout);
#endif

#ifdef __cplusplus
}
#endif

#endif // STATUSBAR_H
