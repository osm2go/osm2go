/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

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
