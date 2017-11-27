/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * osm-gps-map.h
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

#ifndef _OSM_GPS_MAP_SOURCE_H_
#define _OSM_GPS_MAP_SOURCE_H_

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    OSM_GPS_MAP_SOURCE_NULL = 0,
    OSM_GPS_MAP_SOURCE_OPENSTREETMAP,
    OSM_GPS_MAP_SOURCE_OPENSTREETMAP_RENDERER,
    OSM_GPS_MAP_SOURCE_OPENCYCLEMAP,
    OSM_GPS_MAP_SOURCE_OSM_PUBLIC_TRANSPORT,
    OSM_GPS_MAP_SOURCE_GOOGLE_STREET,
    OSM_GPS_MAP_SOURCE_GOOGLE_SATELLITE,
    OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_STREET,
    OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_SATELLITE,
    OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_HYBRID,

    /* entries below are currently unusable */
    OSM_GPS_MAP_SOURCE_MAPS_FOR_FREE,    /* not enough details */
    OSM_GPS_MAP_SOURCE_GOOGLE_HYBRID,    /* disabled by google */
    OSM_GPS_MAP_SOURCE_YAHOO_STREET,     /* not implemented yet */
    OSM_GPS_MAP_SOURCE_YAHOO_SATELLITE,  /* not implemented yet */
    OSM_GPS_MAP_SOURCE_YAHOO_HYBRID,     /* not implemented yet */
    OSM_GPS_MAP_SOURCE_OSMC_TRAILS       /* only for germany */
} OsmGpsMapSource_t;

#define OSM_GPS_MAP_SOURCE_LAST  (OSM_GPS_MAP_SOURCE_VIRTUAL_EARTH_HYBRID)

const char* osm_gps_map_source_get_friendly_name (OsmGpsMapSource_t source);
const char* osm_gps_map_source_get_repo_uri (OsmGpsMapSource_t source);
const char* osm_gps_map_source_get_image_format (OsmGpsMapSource_t source);
int osm_gps_map_source_get_min_zoom (OsmGpsMapSource_t source);
int osm_gps_map_source_get_max_zoom (OsmGpsMapSource_t source);

G_END_DECLS

#endif /* _OSM_GPS_MAP_SOURCE_H_ */
