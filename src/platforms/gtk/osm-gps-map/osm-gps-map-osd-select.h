/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * Copyright (C) Till Harbaum 2009 <till@harbaum.org>
 *
 * osm-gps-map is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * osm-gps-map is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _OSM_GPS_MAP_OSD_SELECT_H_
#define _OSM_GPS_MAP_OSD_SELECT_H_

#include "osm-gps-map.h"

#define OSD_SELECT   OSD_CUSTOM
#define OSD_DRAG     OSD_CUSTOM+1

#ifdef __cplusplus
extern "C" {
#endif

/* the osd structure mainly contains various callbacks */
/* required to draw and update the OSD */
typedef struct osm_gps_map_osd_s {
    void(*render)(struct osm_gps_map_osd_s *);
    void(*draw)(struct osm_gps_map_osd_s *, GtkWidget *, GdkDrawable *);
    osd_button_t(*check)(struct osm_gps_map_osd_s *, OsmGpsMap *, gint, gint);       /* check if x/y lies within OSD */
    void(*free)(struct osm_gps_map_osd_s *);

    gpointer priv;
} osm_gps_map_osd_t;

void osm_gps_map_osd_select_init(OsmGpsMap *map);
gboolean osm_gps_map_osd_get_state(OsmGpsMap *map);
void osm_gps_map_register_osd(OsmGpsMap *map, osm_gps_map_osd_t *osd);
osm_gps_map_osd_t *osm_gps_map_osd_get(OsmGpsMap *map);

#ifdef __cplusplus
}
#endif

#endif
