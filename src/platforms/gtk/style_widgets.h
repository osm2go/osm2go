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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef STYLE_WIDGETS_H
#define STYLE_WIDGETS_H

#include <gtk/gtk.h>
#include <string>

struct appdata_t;

#ifndef FREMANTLE
void style_select(appdata_t *appdata);
#else
GtkWidget *style_select_widget(const std::string &currentstyle);
#endif

#ifdef FREMANTLE
void style_change(appdata_t &appdata, GtkWidget *widget);
#endif

#endif // STYLE_WIDGETS_H
