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

#ifndef ICON_H
#define ICON_H

#include <gtk/gtk.h>

typedef struct icon_t icon_t;

#ifdef __cplusplus
extern "C" {
#endif

GdkPixbuf *icon_load(icon_t **icon, const char *name);
void icon_free(icon_t **icons, GdkPixbuf *buf);
void icon_free_all(icon_t **icons);
GtkWidget *icon_widget_load(icon_t **icon, const char *name);

#ifdef __cplusplus
}
#endif

#endif // ICON_H
