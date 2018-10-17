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

#pragma once

#include <osm2go_platform.h>

class trstring;

void error_dlg(const char *msg, osm2go_platform::Widget *parent = nullptr) __attribute__((nonnull(1)));
void error_dlg(const trstring &msg, osm2go_platform::Widget *parent = nullptr);
void warning_dlg(const char *msg, osm2go_platform::Widget *parent = nullptr) __attribute__((nonnull(1)));
void warning_dlg(const trstring &msg, osm2go_platform::Widget *parent = nullptr);
void message_dlg(const char *title, const char *msg, osm2go_platform::Widget *parent = nullptr) __attribute__((nonnull(1,2)));
void message_dlg(const char *title, const trstring &msg, GtkWidget *parent) __attribute__((nonnull(1)));
