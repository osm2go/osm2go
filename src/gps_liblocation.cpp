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

#include "settings.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <glib.h>
#include <location/location-gps-device.h>
#include <location/location-gpsd-control.h>

#include <osm2go_cpp.h>

/* force usage of gpsd start/stop */
#define LL_CONTROL_GPSD

class gps_liblocation_state_t : public gps_state_t {
public:
  gps_liblocation_state_t();
  virtual ~gps_liblocation_state_t();

  virtual pos_t get_pos(float *alt = O2G_NULLPTR) O2G_OVERRIDE;
  virtual void setEnable(bool) O2G_OVERRIDE;
  virtual bool registerCallback(GpsCallback cb, void *context) O2G_OVERRIDE;

  void updateCallback();

  LocationGPSDevice * const device;
#ifdef LL_CONTROL_GPSD
  LocationGPSDControl * const control;
  bool gps_is_on;
#endif
  guint idd_changed;

  bool fix;
  bool enabled;
  pos_t pos;
  float altitude;
};

pos_t gps_liblocation_state_t::get_pos(float* alt)
{
  if(enabled && fix) {
    if(alt)
      *alt = altitude;

    return pos;
  } else {
    return pos_t(NAN, NAN);
  }
}

static void
location_changed(gps_liblocation_state_t *gps_state) {
  gps_state->updateCallback();
}

void gps_liblocation_state_t::updateCallback()
{
  fix = (device->fix->fields & LOCATION_GPS_DEVICE_LATLONG_SET);

  if(fix) {
    pos.lat = device->fix->latitude;
    pos.lon = device->fix->longitude;
  }

  if(device->fix->fields & LOCATION_GPS_DEVICE_ALTITUDE_SET)
    altitude = device->fix->altitude;
  else
    altitude = NAN;

  if(callback)
    if(!callback(cb_context))
      callback = O2G_NULLPTR;
}

gps_state_t *gps_state_t::create() {
  return new gps_liblocation_state_t();
}

gps_liblocation_state_t::gps_liblocation_state_t()
  : device(static_cast<LocationGPSDevice *>(g_object_new(LOCATION_TYPE_GPS_DEVICE, O2G_NULLPTR)))
#ifdef LL_CONTROL_GPSD
  , control(location_gpsd_control_get_default())
  , gps_is_on(false)
#endif
  , fix(false)
  , enabled(false)
{
  printf("GPS init: Using liblocation\n");

  if(!device) {
    printf("Unable to connect to liblocation\n");
    return;
  }

  idd_changed = g_signal_connect_swapped(device, "changed",
                                         G_CALLBACK(location_changed), this);
}

gps_liblocation_state_t::~gps_liblocation_state_t()
{
  if(!device)
    return;

#ifdef LL_CONTROL_GPSD
  if(control) {
    printf("Having control over GPSD and its running, stopping it\n");
    if(gps_is_on)
      location_gpsd_control_stop(control);
  }
#endif

  /* Disconnect signal */
  g_signal_handler_disconnect(device, idd_changed);
}

void gps_liblocation_state_t::setEnable(bool en)
{
  if(en != gps_is_on) {
    if(device && control) {
      if(en) {
        printf("starting gpsd\n");
        location_gpsd_control_start(control);
      } else {
        printf("stopping gpsd\n");
        location_gpsd_control_stop(control);
      }
      gps_is_on = en;
    }
  }
  enabled = en;
}

bool gps_liblocation_state_t::registerCallback(GpsCallback cb, void* context)
{
  bool ret = (callback != O2G_NULLPTR);
  if(!ret) {
    cb_context = context;
    callback = cb;
  }
  return ret;
}
