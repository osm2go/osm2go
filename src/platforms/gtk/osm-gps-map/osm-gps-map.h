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

#include "config.h"

#include <osm-gps-map-point.h>
#include <osm-gps-map-source.h>
#include <osm-gps-map-widget.h>

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

typedef struct {
    gint x, y, w, h;
} OsmGpsMapRect_t;

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

/* the osd structure mainly contains various callbacks */
/* required to draw and update the OSD */
typedef struct osm_gps_map_osd_s {
    GtkWidget *widget;   // the main map widget (to get its stlye info)

    void(*render)(struct osm_gps_map_osd_s *);
    void(*draw)(struct osm_gps_map_osd_s *, GdkDrawable *);
    osd_button_t(*check)(struct osm_gps_map_osd_s *,gboolean,gint, gint);       /* check if x/y lies within OSD */
    gboolean(*busy)(struct osm_gps_map_osd_s *);
    void(*free)(struct osm_gps_map_osd_s *);

    OsmGpsMapOsdCallback cb;
    gpointer data;

    gpointer priv;
} osm_gps_map_osd_t;

typedef void (*OsmGpsMapBalloonCallback)(cairo_t *, OsmGpsMapRect_t *rect,
                                         gpointer data);
#define	OSM_GPS_MAP_BALLOON_CALLBACK(f) ((OsmGpsMapBalloonCallback) (f))

void osm_gps_map_add_track (OsmGpsMap *map, GSList *track);
void osm_gps_map_add_bounds(OsmGpsMap *map, GSList *bounds);
#ifdef ENABLE_OSD
void osm_gps_map_register_osd(OsmGpsMap *map, osm_gps_map_osd_t *osd);
void osm_gps_map_redraw (OsmGpsMap *map);
osm_gps_map_osd_t *osm_gps_map_osd_get(OsmGpsMap *map);
void osm_gps_map_repaint (OsmGpsMap *map);
#endif


G_END_DECLS

#endif /* _OSM_GPS_MAP_H_ */
