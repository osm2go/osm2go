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

#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <osm2go_platform.h>

void error_dlg(const char *msg, osm2go_platform::Widget *parent = nullptr) __attribute__((nonnull(1)));
void errorf(osm2go_platform::Widget *parent, const char *fmt, ...) __attribute__((format (printf, 2, 3))) __attribute__((nonnull(2)));
void warning_dlg(const char *msg, osm2go_platform::Widget *parent = nullptr) __attribute__((nonnull(1)));
void warningf(const char *fmt, ...) __attribute__((format (printf, 1, 2))) __attribute__((nonnull(1)));
void message_dlg(const char *title, const char *msg, osm2go_platform::Widget *parent = nullptr) __attribute__((nonnull(1,2)));

#endif // NOTIFICATIONS_H
