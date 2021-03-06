/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * converter.h
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
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>

static inline float
deg2rad(float deg)
{
    return (deg * M_PI / 180.0);
}


float
lat2pixel(  int zoom,
            float lat);

float
lon2pixel(  int zoom,
            float lon);

float
pixel2lon(  int zoom,
            int pixel_x);

float
pixel2lat(  int zoom,
            int pixel_y);

#ifdef __cplusplus
}
#endif
