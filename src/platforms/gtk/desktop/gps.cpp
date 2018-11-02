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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gps.h>
#include <gtk/gtk.h>
#include <mutex>
#include <unistd.h>

#include <osm2go_cpp.h>
#include <osm2go_platform.h>
#include <osm2go_platform_gtk.h>

/* setup for direct gpsd based communication */
class gpsd_state_t : public gps_state_t {
public:
  gpsd_state_t(GpsCallback cb, void *context);
  ~gpsd_state_t() override;

  virtual pos_t get_pos(float *alt) override;
  virtual void setEnable(bool en) override;

  bool runCallback()
  {
    return callback(cb_context);
  }

  osm2go_platform::Timer timer;

  GThread * const thread_p;
  std::mutex mutex;

  bool enable;
  bool terminate;

  gps_data_t gpsdata;
};

pos_t gpsd_state_t::get_pos(float* alt)
{
  pos_t pos(NAN, NAN);

  if(enable) {
    std::lock_guard<std::mutex> lock(mutex);
    if(gpsdata.set & STATUS_SET) {
      if(gpsdata.status != STATUS_NO_FIX) {
        if(gpsdata.set & LATLON_SET) {
          pos.lat = gpsdata.fix.latitude;
          pos.lon = gpsdata.fix.longitude;
        }
        if(alt != nullptr && gpsdata.set & ALTITUDE_SET)
          *alt = gpsdata.fix.altitude;
      }
    }
  }

  return pos;
}

static bool gps_connect(gps_data_t &gps)
{
  memset(&gps, 0, sizeof(gps));
  if(gps_open("localhost", DEFAULT_GPSD_PORT, &gps) != 0)
    return false;

  if(gps_stream(&gps, WATCH_ENABLE | WATCH_JSON, nullptr) < 0)
    return false;

  return true;
}


static gboolean gps_callback(gpointer data) {
  gpsd_state_t * const state = static_cast<gpsd_state_t *>(data);

  return state->runCallback() ? TRUE : FALSE;
}

void gpsd_state_t::setEnable(bool en)
{
  if(!en && timer.isActive())
    timer.stop();
  else if(en && !timer.isActive())
    timer.restart(1, gps_callback, this);
}

gpointer gps_thread(gpointer data) {
  gpsd_state_t * const gps_state = static_cast<gpsd_state_t *>(data);
  gps_state->gpsdata.set = 0;

  gps_data_t gps;
  bool connected = false;

  while(!gps_state->terminate) {
    if(gps_state->enable) {
      if(!connected) {
        g_debug("trying to connect\n");

        connected = gps_connect(gps);
        if(!connected)
          sleep(10);
      } else {
        /* update every second, wait here to make sure a complete */
        /* reply is received */
        if(gps_waiting(&gps, 1000000)) {
          int r = gps_read(&gps);

          std::lock_guard<std::mutex> lock(gps_state->mutex);

          if(G_LIKELY(r > 0))
            gps_state->gpsdata = gps;
          else
            gps_clear_fix(&gps_state->gpsdata.fix);
        }
      }
    } else {
      if(connected) {
        g_debug("stopping GPS connection due to user request\n");
        gps_stream(&gps, WATCH_DISABLE, nullptr);
        connected = false;
      } else
        sleep(1);
    }
  }

  g_debug("GPS thread ended\n");
  return nullptr;
}

gps_state_t *gps_state_t::create(GpsCallback cb, void *context) {
  return new gpsd_state_t(cb, context);
}

gpsd_state_t::gpsd_state_t(GpsCallback cb, void *context)
  : gps_state_t(cb, context)
  , thread_p(g_thread_try_new("gps", gps_thread, this, nullptr))
  , enable(false)
  , terminate(false)
{
  g_debug("GPS init: Using gpsd\n");

  /* start a new thread to listen to gpsd */
}

gpsd_state_t::~gpsd_state_t()
{
  terminate = true;
  if(thread_p != nullptr)
    g_thread_join(thread_p);
}
