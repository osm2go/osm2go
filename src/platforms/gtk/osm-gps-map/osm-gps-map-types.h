/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * osm-gps-map-types.h
 * SPDX-FileCopyrightText: Marcus Bauer 2008 <marcus.bauer@gmail.com>
 * SPDX-FileCopyrightText: John Stowers 2009 <john.stowers@gmail.com>
 *
 * Contributions by
 * Everaldo Canuto 2009 <everaldo.canuto@gmail.com>
 *
 * osm-gps-map.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * osm-gps-map.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _OSM_GPS_MAP_TYPES_H_
#define _OSM_GPS_MAP_TYPES_H_

#define TILESIZE 256

#define URI_MARKER_X    "#X"
#define URI_MARKER_Y    "#Y"
#define URI_MARKER_Z    "#Z"

#ifdef FREMANTLE
#define OSM_REPO_PROTOCOL   "http://"
#else
#define OSM_REPO_PROTOCOL   "https://"
#endif
#define OSM_REPO_URI        OSM_REPO_PROTOCOL "tile.openstreetmap.org/" URI_MARKER_Z "/" URI_MARKER_X "/" URI_MARKER_Y ".png"
#define OSM_MIN_ZOOM        1
#define OSM_MAX_ZOOM        19
#define OSM_IMAGE_FORMAT    "png"

#define URI_HAS_X   (1 << 0)
#define URI_HAS_Y   (1 << 1)
#define URI_HAS_Z   (1 << 2)
#define URI_FLAG_END (1 << 3)

/* equatorial radius in meters */
#define OSM_EQ_RADIUS   (6378137.0)

#endif /* _OSM_GPS_MAP_TYPES_H_ */
