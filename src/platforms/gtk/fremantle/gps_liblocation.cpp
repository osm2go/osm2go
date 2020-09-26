/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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

namespace {

class gps_liblocation_state_t : public gps_state_t {
public:
  gps_liblocation_state_t(GpsCallback cb, void *context);
  virtual ~gps_liblocation_state_t();

  pos_t get_pos(float *alt = nullptr) override;
  void setEnable(bool) override;

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

}

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
