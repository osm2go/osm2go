/*
 * Copyright (C) 2009 Till Harbaum <till@harbaum.org>.
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

#ifndef CONFIG_H
#define CONFIG_H

#ifdef FREMANTLE
#include <hildon/hildon-defines.h>
/* only maemo devices up to version 4 have a fullscreen button */
#define OSM_GPS_MAP_KEY_FULLSCREEN  'f'
#define OSM_GPS_MAP_KEY_ZOOMIN      HILDON_HARDKEY_INCREASE
#define OSM_GPS_MAP_KEY_ZOOMOUT     HILDON_HARDKEY_DECREASE
#else
#define OSM_GPS_MAP_KEY_FULLSCREEN  GDK_F11
#define OSM_GPS_MAP_KEY_ZOOMIN      '+'
#define OSM_GPS_MAP_KEY_ZOOMOUT     '-'
#endif

#define OSM_GPS_MAP_KEY_UP          GDK_Up
#define OSM_GPS_MAP_KEY_DOWN        GDK_Down
#define OSM_GPS_MAP_KEY_LEFT        GDK_Left
#define OSM_GPS_MAP_KEY_RIGHT       GDK_Right

#define OSD_DOUBLE_BUFFER    // render osd/map together offscreen

#endif // CONFIG_H
