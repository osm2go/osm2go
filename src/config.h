

// config.h, only used by osm-gps-map

#define USE_CAIRO
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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CONFIG_H
#define CONFIG_H

#define ENABLE_OSD

/* only maemo devices up to version 4 have a fullscreen button */
#ifdef USE_HILDON
#define OSD_DIAMETER  40
#include <hildon/hildon-defines.h>
#if (MAEMO_VERSION_MAJOR < 5)
#define OSM_GPS_MAP_KEY_FULLSCREEN  HILDON_HARDKEY_FULLSCREEN
#else
#define OSM_GPS_MAP_KEY_FULLSCREEN  'f'
#endif
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

/* specify OSD colors explicitely. Otherwise gtk default */
/* colors are used. fremantle always uses gtk defaults */
#if !defined(USE_HILDON) || (MAEMO_VERSION_MAJOR < 5)
#define OSD_COLOR_BG         1, 1, 1         // white background
#define OSD_COLOR            0.5, 0.5, 1     // light blue border and controls
#define OSD_COLOR_DISABLED   0.8, 0.8, 0.8   // light grey disabled controls
#define OSD_SHADOW_ENABLE
#endif

#define OSD_DOUBLE_BUFFER    // render osd/map together offscreen
#define OSD_GPS_BUTTON       // display a GPS button

#endif // CONFIG_H
