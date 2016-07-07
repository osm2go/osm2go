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
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _OSM_GPS_MAP_OSD_SELECT_H_
#define _OSM_GPS_MAP_OSD_SELECT_H_

#include "osm-gps-map.h"

#define OSD_SELECT   OSD_CUSTOM
#define OSD_DRAG     OSD_CUSTOM+1

void osm_gps_map_osd_select_init(OsmGpsMap *map);
gboolean osm_gps_map_osd_get_state(OsmGpsMap *map);

#endif
