/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
 *
 * This file is based upon parts of gpsd/libgps
 *
 * This file is part of OSM2Go.
 *
 * OSM2Go is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OSM2Go is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef GPS_H
#define GPS_H

#include "pos.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gps_state_t gps_state_t;
struct appdata_t;

gps_state_t *gps_init(struct appdata_t *appdata);
void gps_release(gps_state_t *gps_state);
int gps_get_pos(gps_state_t *gps_state, pos_t *pos, float *alt) __attribute__((nonnull(1,2)));
void gps_enable(gps_state_t *gps_state, gboolean enable);

typedef int (*GpsCallback)(void *context);

/**
 * @brief register or clear the GPS callback
 * @param gps_state the GPS context struct as returned by gps_init;
 * @param cb the new callback function, set to NULL to unregister
 * @param context a context pointer passed to cb
 * @return if there was a previous handler
 *
 * Does nothing if a handler already exists.
 */
int gps_register_callback(struct gps_state_t *gps_state, GpsCallback cb, void *context);

#ifdef __cplusplus
}
#endif

#endif // GPS_H
