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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <gps_state.h>

#include <settings.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <glib.h>
#include <location/location-gps-device.h>
#include <location/location-gpsd-control.h>

#include <osm2go_cpp.h>

class gps_liblocation_state_t : public gps_state_t {
public:
  gps_liblocation_state_t(GpsCallback cb, void *context);
  virtual ~gps_liblocation_state_t();

  virtual pos_t get_pos(float *alt = nullptr) override;
  virtual void setEnable(bool) override;

  void updateCallback();

  LocationGPSDevice * const device;
  LocationGPSDControl * const control;
  bool gps_is_on;
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

  if(!callback(cb_context))
    setEnable(false);
}

gps_state_t *gps_state_t::create(GpsCallback cb, void *context) {
  return new gps_liblocation_state_t(cb, context);
}

gps_liblocation_state_t::gps_liblocation_state_t(GpsCallback cb, void *context)
  : gps_state_t(cb, context)
  , device(static_cast<LocationGPSDevice *>(g_object_new(LOCATION_TYPE_GPS_DEVICE, nullptr)))
  , control(location_gpsd_control_get_default())
  , gps_is_on(false)
  , fix(false)
  , enabled(false)
{
  g_debug("GPS init: Using liblocation");

  if(!device) {
    g_warning("Unable to connect to liblocation");
    return;
  }

  idd_changed = g_signal_connect_swapped(device, "changed",
                                         G_CALLBACK(location_changed), this);
}

gps_liblocation_state_t::~gps_liblocation_state_t()
{
  if(!device)
    return;

  if(control) {
    g_debug("Having control over GPSD and its running, stopping it");
    if(gps_is_on)
      location_gpsd_control_stop(control);
  }

  /* Disconnect signal */
  g_signal_handler_disconnect(device, idd_changed);
}

void gps_liblocation_state_t::setEnable(bool en)
{
  if(en != gps_is_on) {
    if(device && control) {
      if(en) {
        g_debug("starting gpsd");
        location_gpsd_control_start(control);
      } else {
        g_debug("stopping gpsd");
        location_gpsd_control_stop(control);
      }
      gps_is_on = en;
    }
  }
  enabled = en;
}
