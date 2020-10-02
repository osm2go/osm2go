/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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

namespace {

/* setup for direct gpsd based communication */
class gpsd_state_t : public gps_state_t {
public:
  gpsd_state_t(GpsCallback cb, void *context);
  gpsd_state_t() O2G_DELETED_FUNCTION;
  gpsd_state_t(const gpsd_state_t &) O2G_DELETED_FUNCTION;
  gpsd_state_t &operator=(const gpsd_state_t &) O2G_DELETED_FUNCTION;
#if __cplusplus >= 201103L
  gpsd_state_t(gpsd_state_t &&) = delete;
  gpsd_state_t &operator=(gpsd_state_t &&) = delete;
#endif
  ~gpsd_state_t() override;

  pos_t get_pos(float *alt) override;
  void setEnable(bool en) override;

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

bool
gps_connect(gps_data_t &gps)
{
  memset(&gps, 0, sizeof(gps));
  if(gps_open("localhost", DEFAULT_GPSD_PORT, &gps) != 0)
    return false;

  if(gps_stream(&gps, WATCH_ENABLE | WATCH_JSON, nullptr) < 0)
    return false;

  return true;
}

gboolean
gps_callback(gpointer data)
{
  return static_cast<gpsd_state_t *>(data)->runCallback() ? TRUE : FALSE;
}

inline bool
hasGpsFix(gps_data_t &gpsdata)
{
#if GPSD_API_MAJOR_VERSION < 10
  return (gpsdata.status != STATUS_NO_FIX);
#else
  return (gpsdata.fix.status != STATUS_NO_FIX);
#endif
}

#if GPSD_API_MAJOR_VERSION >= 7
inline int
gps_read(gps_data_t *gps)
{
  return gps_read(gps, nullptr, 0);
}
#endif

} // namespace

pos_t gpsd_state_t::get_pos(float* alt)
{
  pos_t pos(NAN, NAN);

  if(enable) {
    std::lock_guard<std::mutex> lock(mutex);
    if(gpsdata.set & STATUS_SET) {
      if(hasGpsFix(gpsdata)) {
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

void gpsd_state_t::setEnable(bool en)
{
  if(!en && timer.isActive())
    timer.stop();
  else if(en && !timer.isActive())
    timer.restart(1, gps_callback, this);
  enable = en;
}

gpointer gps_thread(gpointer data) {
  gpsd_state_t * const gps_state = static_cast<gpsd_state_t *>(data);
  gps_state->gpsdata.set = 0;

  gps_data_t gps;
  bool connected = false;

  while(!gps_state->terminate) {
    if(gps_state->enable) {
      if(!connected) {
        g_debug("trying to connect");

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
        g_debug("stopping GPS connection due to user request");
        gps_stream(&gps, WATCH_DISABLE, nullptr);
        connected = false;
      } else
        sleep(1);
    }
  }

  g_debug("GPS thread ended");
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
  g_debug("GPS init: Using gpsd");

  /* start a new thread to listen to gpsd */
}

gpsd_state_t::~gpsd_state_t()
{
  terminate = true;
  if(thread_p != nullptr)
    g_thread_join(thread_p);
}
