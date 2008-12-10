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

#ifndef NAN
#define NAN (0.0/0.0)
#endif /* !NAN */

#define MAXTAGLEN    8       /* maximum length of sentence tag name */
#define MPS_TO_KNOTS 1.9438445       /* Meters per second to knots */

struct gps_fix_t {
    int    mode;	/* Mode of fix */
#define MODE_NOT_SEEN	0	/* mode update not seen yet */
#define MODE_NO_FIX	1	/* none */
#define MODE_2D  	2	/* good for latitude/longitude */
#define MODE_3D  	3	/* good for altitude/climb too */
    pos_t pos;          /* Latitude/Longitude in degrees (valid if mode >= 2) */
    double eph;  	/* Horizontal position uncertainty, meters */
};

typedef unsigned int gps_mask_t;

struct gps_data_t {
    gps_mask_t set;	/* has field been set since this was last cleared? */
#define LATLON_SET	0x00000008u
#define STATUS_SET	0x00000100u
#define MODE_SET	0x00000200u
#define SATELLITE_SET	0x00040000u

    struct gps_fix_t	fix;		/* accumulated PVT data */

    /* GPS status -- always valid */
    int    status;		/* Do we have a fix? */
#define STATUS_NO_FIX	0	/* no */
#define STATUS_FIX	1	/* yes, without DGPS */
#define STATUS_DGPS_FIX	2	/* yes, with DGPS */

};

#ifdef USE_HILDON
#include <gpsbt.h>
#include <gpsmgr.h>
#include <errno.h>
#endif

typedef struct gps_state_s {
#ifdef USE_HILDON
  gpsbt_t context;
#endif

  GThread* thread_p;
  GMutex *mutex;
  GnomeVFSInetConnection *iconn;
  GnomeVFSSocket *socket;

  struct gps_data_t gpsdata;
} gps_state_t;

void gps_init(appdata_t *appdata);
void gps_release(appdata_t *appdata);
pos_t *gps_get_pos(appdata_t *appdata);

#endif // GPS_H
