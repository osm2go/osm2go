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
#include <string>

typedef struct icon_t icon_t;

void icon_free(icon_t **icons, GdkPixbuf *buf);

/**
 * @brief load an icon from disk, limited to given dimensions
 * @param icon the global icon cache
 * @param sname the name of the icon
 * @param limit the maximum dimensions of the image
 * @return the scaled pixbuf
 *
 * The image is only scaled down to the given dimensions, not enlarged.
 * The limit is only applied if the icon is not already cached.
 */
GdkPixbuf *icon_load(icon_t **icon, const std::string &sname, int limit = -1);

GtkWidget *icon_widget_load(icon_t **icon, const std::string &name, int limit = -1);

void icon_free_all(icon_t *icons);

#endif // ICON_H
