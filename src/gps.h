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

#include "appdata.h"
#include "pos.h"

#include <gtk/gtk.h>

#ifndef NAN
#define NAN (0.0/0.0)
#endif /* !NAN */

void gps_init(appdata_t *appdata);
void gps_release(appdata_t *appdata);
gboolean gps_get_pos(appdata_t *appdata, pos_t *pos, float *alt);
void gps_enable(appdata_t *appdata, gboolean enable);

/**
 * @brief register or clear the GPS callback
 * @param appdata the global information structure
 * @param cb the new callback function, set to NULL to unregister
 * @return if there was a previous handler
 *
 * Does nothing if a handler already exists.
 */
gboolean gps_register_callback(appdata_t *appdata, GtkFunction cb);

#endif // GPS_H
