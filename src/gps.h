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

#include <osm2go_cpp.h>

typedef int (*GpsCallback)(void *context);

class gps_state_t {
protected:
  GpsCallback callback;
  void *cb_context;

  gps_state_t()
    : callback(O2G_NULLPTR)
    , cb_context(O2G_NULLPTR)
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
  virtual pos_t get_pos(float *alt = O2G_NULLPTR) = 0;

  virtual void setEnable(bool en) = 0;

  /**
   * @brief register or clear the GPS callback
   * @param cb the new callback function, set to NULL to unregister
   * @param context a context pointer passed to cb
   * @return if there was a previous handler
   *
   * Does nothing if a handler already exists.
   */
  virtual bool registerCallback(GpsCallback cb, void *context) = 0;

  static gps_state_t *create();
};

#endif // GPS_H
