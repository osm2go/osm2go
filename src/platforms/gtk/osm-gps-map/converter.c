/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * converter.c
 * SPDX-FileCopyrightText: Marcus Bauer 2008 <marcus.bauer@gmail.com>
 * SPDX-FileCopyrightText: John Stowers 2008 <john.stowers@gmail.com>
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

#include "converter.h"

#include "osm-gps-map-types.h"

#include <math.h>
#include <stdio.h>

static float
arc2pixel(int zoom, float arc)
{
    /* the formula is
     *
     * pixel_y = -(2^zoom * TILESIZE * lat_m) / 2PI + (2^zoom * TILESIZE) / 2
     */
    return ( (arc * TILESIZE * (1 << zoom) ) / (2*M_PI)) +
        ((1 << zoom) * (TILESIZE/2) );
}

float
lat2pixel(  int zoom,
            float lat)
{
    return arc2pixel(zoom, -atanhf(sinf(lat)));
}


float
lon2pixel(  int zoom,
            float lon)
{
    return arc2pixel(zoom, lon);
}

float
pixel2lon(  int zoom,
            int pixel_x)
{
    return ((pixel_x - ( (1 << zoom) * (TILESIZE/2) ) ) *2*M_PI) /
        (TILESIZE * (1 << zoom) );
}

float
pixel2lat(  int zoom,
            int pixel_y)
{
    float lat_m = (-( pixel_y - ( (1 << zoom) * (TILESIZE/2) ) ) * (2*M_PI)) /
        (TILESIZE * (1 << zoom));

    return asinf(tanhf(lat_m));
}
