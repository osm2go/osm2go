/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * osm-gps-map-types.h
 * Copyright (C) Marcus Bauer 2008 <marcus.bauer@gmail.com>
 * Copyright (C) John Stowers 2009 <john.stowers@gmail.com>
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

#include <gdk/gdk.h>
#include "osm-gps-map.h"

#define TILESIZE 256
#define MAX_ZOOM 20
#define MIN_ZOOM 0

#ifdef FREMANTLE
#define OSM_REPO_URI        "http://tile.openstreetmap.org/#Z/#X/#Y.png"
#else
#define OSM_REPO_URI        "https://tile.openstreetmap.org/#Z/#X/#Y.png"
#endif
#define OSM_MIN_ZOOM        1
#define OSM_MAX_ZOOM        19
#define OSM_IMAGE_FORMAT    "png"

#define URI_MARKER_X    "#X"
#define URI_MARKER_Y    "#Y"
#define URI_MARKER_Z    "#Z"

#define URI_HAS_X   (1 << 0)
#define URI_HAS_Y   (1 << 1)
#define URI_HAS_Z   (1 << 2)
#define URI_FLAG_END (1 << 3)

/* equatorial radius in meters */
#define OSM_EQ_RADIUS   (6378137.0)

typedef struct {
    int x1;
    int y1;
    int x2;
    int y2;
} bbox_pixel_t;

typedef struct {
    int x;
    int y;
    int zoom;
} tile_t;

typedef struct {
    OsmGpsMapPoint pt;
    GdkPixbuf *image;
    int w;
    int h;
} image_t;

#endif /* _OSM_GPS_MAP_TYPES_H_ */
