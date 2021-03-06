/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * osm-gps-map-widget.h
 * SPDX-FileCopyrightText: Marcus Bauer 2008 <marcus.bauer@gmail.com>
 * SPDX-FileCopyrightText: John Stowers 2009 <john.stowers@gmail.com>
 *
 * Contributions by
 * Everaldo Canuto 2009 <everaldo.canuto@gmail.com>
 *
 * osm-gps-map.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * osm-gps-map.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _OSM_GPS_MAP_WIDGET_H_
#define _OSM_GPS_MAP_WIDGET_H_

#include "osm-gps-map-point.h"

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define OSM_TYPE_GPS_MAP             (osm_gps_map_get_type ())
#define OSM_GPS_MAP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), OSM_TYPE_GPS_MAP, OsmGpsMap))
#define OSM_GPS_MAP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), OSM_TYPE_GPS_MAP, OsmGpsMapClass))
#define OSM_IS_GPS_MAP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OSM_TYPE_GPS_MAP))
#define OSM_IS_GPS_MAP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), OSM_TYPE_GPS_MAP))
#define OSM_GPS_MAP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), OSM_TYPE_GPS_MAP, OsmGpsMapClass))

typedef struct _OsmGpsMapClass OsmGpsMapClass;
typedef struct _OsmGpsMap OsmGpsMap;
typedef struct _OsmGpsMapPrivate OsmGpsMapPrivate;

struct _OsmGpsMapClass
{
    GtkDrawingAreaClass parent_class;
};

struct _OsmGpsMap
{
    GtkDrawingArea parent_instance;
    OsmGpsMapPrivate *priv;
};

GType osm_gps_map_get_type (void) G_GNUC_CONST;

void osm_gps_map_set_center_and_zoom (OsmGpsMap *map, float latitude, float longitude, int zoom);
void osm_gps_map_gps_add (OsmGpsMap *map, float latitude, float longitude, float heading);
void osm_gps_map_gps_clear (OsmGpsMap *map);
OsmGpsMapPoint osm_gps_map_convert_screen_to_geographic(OsmGpsMap *map, gint pixel_x, gint pixel_y);

G_END_DECLS

#endif /* _OSM_GPS_MAP_WIDGET_H_ */
