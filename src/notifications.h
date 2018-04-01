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

void errorf(osm2go_platform::Widget *parent, const char *fmt, ...) __attribute__((format (printf, 2, 3)));
void warningf(osm2go_platform::Widget *parent, const char *fmt, ...) __attribute__((format (printf, 2, 3)));
void messagef(osm2go_platform::Widget *parent, const char *title, const char *fmt, ...) __attribute__((format (printf, 3, 4)));

#endif // NOTIFICATIONS_H
