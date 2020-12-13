/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gps_state.h>

#include <QGeoCoordinate>
#include <QObject>
#include <QMutex>

class QGeoPositionInfo;
class QGeoPositionInfoSource;

/* setup for QtLocation usage */
class location_state_t : public QObject, public gps_state_t {
  Q_OBJECT
  Q_DISABLE_COPY(location_state_t)
public:
  location_state_t(GpsCallback cb, void *context);
  ~location_state_t() override;

  pos_t get_pos(float *alt) override;
  void setEnable(bool en) override;

  bool runCallback()
  {
    return callback(cb_context);
  }

  QGeoPositionInfoSource * const source;
  QMetaObject::Connection connection;

  QMutex mutex;
  bool enable = false;

  QGeoCoordinate gpsdata;

  void slotNewCoordinates(const QGeoPositionInfo &info);
};
