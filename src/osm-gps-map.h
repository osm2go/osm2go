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
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _OSM_GPS_MAP_H_
#define _OSM_GPS_MAP_H_

#include "config.h"

#include <osm-gps-map-point.h>

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

typedef enum {
    OSM_GPS_MAP_SOURCE_NULL,
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

#define OSM_GPS_MAP_INVALID  (0.0/0.0)

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

GType osm_gps_map_get_type (void) G_GNUC_CONST;

const char* osm_gps_map_source_get_friendly_name(OsmGpsMapSource_t source);
const char* osm_gps_map_source_get_repo_uri(OsmGpsMapSource_t source);
const char *osm_gps_map_source_get_image_format(OsmGpsMapSource_t source);
int osm_gps_map_source_get_min_zoom(OsmGpsMapSource_t source);
int osm_gps_map_source_get_max_zoom(OsmGpsMapSource_t source);

void osm_gps_map_download_maps (OsmGpsMap *map, OsmGpsMapPoint *pt1, OsmGpsMapPoint *pt2, int zoom_start, int zoom_end);
void osm_gps_map_get_bbox (OsmGpsMap *map, OsmGpsMapPoint *pt1, OsmGpsMapPoint *pt2);
void osm_gps_map_set_center_and_zoom (OsmGpsMap *map, float latitude, float longitude, int zoom);
void osm_gps_map_set_center (OsmGpsMap *map, float latitude, float longitude);
int osm_gps_map_set_zoom (OsmGpsMap *map, int zoom);
void osm_gps_map_add_track (OsmGpsMap *map, GSList *track);
void osm_gps_map_track_remove_all (OsmGpsMap *map);
void osm_gps_map_gps_add (OsmGpsMap *map, float latitude, float longitude, float heading);
void osm_gps_map_gps_clear (OsmGpsMap *map);
void osm_gps_map_convert_screen_to_geographic (OsmGpsMap *map, gint pixel_x, gint pixel_y, OsmGpsMapPoint *pt);
GtkWidget * osm_gps_map_new(void);
void osm_gps_map_scroll (OsmGpsMap *map, gint dx, gint dy);
float osm_gps_map_get_scale(OsmGpsMap *map);
#ifdef ENABLE_OSD
void osm_gps_map_register_osd(OsmGpsMap *map, osm_gps_map_osd_t *osd);
void osm_gps_map_redraw (OsmGpsMap *map);
osm_gps_map_osd_t *osm_gps_map_osd_get(OsmGpsMap *map);
void osm_gps_map_repaint (OsmGpsMap *map);
#endif


G_END_DECLS

#endif /* _OSM_GPS_MAP_H_ */
