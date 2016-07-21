/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
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
 */

#include "gps.h"

#include "appdata.h"
#include "settings.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <location/location-gps-device.h>
#include <location/location-gpsd-control.h>

/* force usage of gpsd start/stop */
#define LL_CONTROL_GPSD

typedef struct gps_state_s {
  LocationGPSDevice *device;
#ifdef LL_CONTROL_GPSD
  LocationGPSDControl *control;
  gboolean gps_is_on;
#endif
  guint idd_changed;

  gboolean fix, fix3d;
  pos_t pos;
  float altitude;

  /* callback called on gps change event */
  GtkFunction cb;
  gpointer data;
} gps_state_t;

gboolean gps_get_pos(appdata_t *appdata, pos_t *pos, float *alt) {
  if(!appdata->settings || !appdata->settings->enable_gps)
    return FALSE;

  gps_state_t *gps_state = appdata->gps_state;

  if(!gps_state->fix)
    return FALSE;

  if(pos) {
    *pos = gps_state->pos;
  }

  if(alt)
    *alt = gps_state->altitude;

  return TRUE;
}

static void
location_changed(LocationGPSDevice *device, gps_state_t *gps_state) {

  gps_state->fix =
    (device->fix->fields & LOCATION_GPS_DEVICE_LATLONG_SET);

  if(gps_state->fix) {
    gps_state->pos.lat = device->fix->latitude;
    gps_state->pos.lon = device->fix->longitude;
  }

  if(device->fix->fields & LOCATION_GPS_DEVICE_ALTITUDE_SET)
    gps_state->altitude = device->fix->altitude;
  else
    gps_state->altitude = NAN;

  if(gps_state->cb)
    if(!gps_state->cb(gps_state->data))
      gps_state->cb = NULL;
}

void gps_init(appdata_t *appdata) {
  gps_state_t *gps_state = appdata->gps_state = g_new0(gps_state_t, 1);

  printf("GPS init: Using liblocation\n");

  gps_state->device = g_object_new(LOCATION_TYPE_GPS_DEVICE, NULL);
  if(!gps_state->device) {
    printf("Unable to connect to liblocation\n");
    return;
  }

  gps_state->idd_changed =
    g_signal_connect(gps_state->device, "changed",
		     G_CALLBACK(location_changed), gps_state);

#ifdef LL_CONTROL_GPSD
  gps_state->control = location_gpsd_control_get_default();
#endif
}

void gps_release(appdata_t *appdata) {
  gps_state_t *gps_state = appdata->gps_state;

  if(!gps_state->device) return;

#ifdef LL_CONTROL_GPSD
  if(gps_state->control
#if MAEMO_VERSION_MAJOR < 5
     && gps_state->control->can_control
#endif
     ) {
    printf("Having control over GPSD and its running, stopping it\n");
    if(appdata->gps_state->gps_is_on)
      location_gpsd_control_stop(gps_state->control);
  }
#endif

  /* Disconnect signal */
  g_signal_handler_disconnect(gps_state->device, gps_state->idd_changed);

  g_free(appdata->gps_state);
  appdata->gps_state = NULL;
}

void gps_enable(appdata_t *appdata, gboolean enable) {
  if(appdata->settings) {
    gps_state_t *gps_state = appdata->gps_state;

    /* for location update callbacks */
    gps_state->data = appdata;
    if(enable != appdata->gps_state->gps_is_on) {
      if(gps_state->device && gps_state->control
#if MAEMO_VERSION_MAJOR < 5
	 && gps_state->control->can_control
#endif
	 ) {
	if(enable) {
	  printf("starting gpsd\n");
	  location_gpsd_control_start(gps_state->control);
	} else {
	  printf("stopping gpsd\n");
	  location_gpsd_control_stop(gps_state->control);
	}
	appdata->gps_state->gps_is_on = enable;
      }
    }

    appdata->settings->enable_gps = enable;
  }
}

gboolean gps_register_callback(appdata_t *appdata, GtkFunction cb) {
  gboolean ret = (appdata->gps_state->cb != NULL) ? TRUE : FALSE;
  if(!ret)
    appdata->gps_state->cb = cb;
  return ret;
}
