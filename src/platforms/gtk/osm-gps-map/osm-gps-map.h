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

#ifndef _OSM_GPS_MAP_H_
#define _OSM_GPS_MAP_H_

#include <osm-gps-map-widget.h>

#include <glib.h>
#include <glib-object.h>

#ifdef __cplusplus

#include <utility>

#endif

G_BEGIN_DECLS

typedef enum {
    OSD_NONE = 0,
    OSD_BG,
    OSD_UP,
    OSD_DOWN,
    OSD_LEFT,
    OSD_RIGHT,
    OSD_IN,
    OSD_OUT,
    OSD_CUSTOM   // first custom buttom
} osd_button_t;

typedef void (*OsmGpsMapOsdCallback)(osd_button_t but, gpointer data);
#define	OSM_GPS_MAP_OSD_CALLBACK(f) ((OsmGpsMapOsdCallback) (f))

#ifdef __cplusplus
/**
 * @brief add the area of the active project
 * The previous area marker is removed. map takes ownership of track.
 */
void osm_gps_map_add_track (OsmGpsMap *map, const std::pair<OsmGpsMapPoint, OsmGpsMapPoint> &track);
void osm_gps_map_add_bounds(OsmGpsMap *map, const std::pair<OsmGpsMapPoint, OsmGpsMapPoint> &bounds);
#endif
void osm_gps_map_redraw (OsmGpsMap *map);
osd_button_t osm_gps_map_osd_check(OsmGpsMap *map, gint x, gint y);
void osm_gps_map_repaint (OsmGpsMap *map);

G_END_DECLS

#endif /* _OSM_GPS_MAP_H_ */
