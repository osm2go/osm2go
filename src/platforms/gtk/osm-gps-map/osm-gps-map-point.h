/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * osm-gps-map.h
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

#ifndef _OSM_GPS_MAP_POINT_H_
#define _OSM_GPS_MAP_POINT_H_

#include <glib.h>

G_BEGIN_DECLS

typedef struct _OsmGpsMapPoint
{
    /* radians */
    float  rlat;
    float  rlon;
} OsmGpsMapPoint;

G_END_DECLS

#endif /* _OSM_GPS_MAP_POINT_H_ */
