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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef GPS_H
#define GPS_H

#include "pos.h"

#include <osm2go_cpp.h>

typedef int (*GpsCallback)(void *context);

class gps_state_t {
protected:
  const GpsCallback callback;
  void * const cb_context;

  gps_state_t(GpsCallback cb, void *context)
    : callback(cb)
    , cb_context(context)
  {
  }
public:
  virtual ~gps_state_t() {}

  /**
   * @brief return the last position from GPS
   * @param alt optional storage for altitude information
   * @return the GPS position
   *
   * In case no valid position was received an invalid pos_t will be returned.
   *
   * If @alt was given but no altitude was received the value is set to NAN.
   *
   * This returns an invalid position if GPS tracking is disabled.
   */
  virtual pos_t get_pos(float *alt = nullptr) = 0;

  virtual void setEnable(bool en) = 0;

  /**
   * @brief create a GPS instance
   * @param cb the callback function called on position updates
   * @param context a context pointer passed to cb
   *
   * The callback state can be changed with enableCallback()
   */
  static gps_state_t *create(GpsCallback cb, void *context);
};

#endif // GPS_H
