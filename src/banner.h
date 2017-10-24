/*
 * Copyright (C) 2008 Andrew Chadwick <andrewc-osm2go@piffle.org>.
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

/*
 * Quick banners for short, non-critical messages.
 * For Hildon builds, show a HildonBanner.
 * For other builds, push the message onto the satausbar briefly.
 */

#ifndef BANNER_H
#define BANNER_H

struct appdata_t;

// Shows a brief info splash in a suitable way for the app environment being used
void banner_show_info(appdata_t &appdata, const char *text);

// Start, stop, and say "I'm still alive" to a busy message targetted at the
// app environment in use. This can be an animation for some builds, might be
// a static statusbar for others, a modal dialog for others.
void banner_busy_start(appdata_t &appdata, const char *text);
void banner_busy_stop(appdata_t &appdata);

#endif //BANNER_H
// vim:et:ts=8:sw=2:sts=2:ai
