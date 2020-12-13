/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gps_p.h"

#include <osm2go_platform.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unistd.h>

#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#include <QMutexLocker>

pos_t
location_state_t::get_pos(float *alt)
{
  pos_t pos(NAN, NAN);

  if(enable) {
    QMutexLocker guard(&mutex);
    if(gpsdata.isValid()) {
      switch (gpsdata.type()) {
      case QGeoCoordinate::Coordinate3D:
        *alt = gpsdata.altitude();
        // fallthrough
      case QGeoCoordinate::Coordinate2D:
        pos.lat = gpsdata.latitude();
        pos.lon = gpsdata.longitude();
        break;
      default:
        break;
      }
    }
  }

  return pos;
}

void
location_state_t::setEnable(bool en)
{
  enable = en;
  if (en && !connection)
      connection = connect(source, &QGeoPositionInfoSource::positionUpdated, this,
                           &location_state_t::slotNewCoordinates);
  else if (!en && connection) {
    disconnect(connection);
    connection = QMetaObject::Connection();
  }
}

gps_state_t *
gps_state_t::create(GpsCallback cb, void *context)
{
  return new location_state_t(cb, context);
}

location_state_t::location_state_t(GpsCallback cb, void *context)
  : QObject()
  , gps_state_t(cb, context)
  , source(QGeoPositionInfoSource::createDefaultSource(this))
{
  source->setUpdateInterval(1000);

  connection = connect(source, &QGeoPositionInfoSource::positionUpdated, this,
                       &location_state_t::slotNewCoordinates);
}

location_state_t::~location_state_t()
{
  location_state_t::setEnable(false);
}

void
location_state_t::slotNewCoordinates(const QGeoPositionInfo &info)
{
  gpsdata = info.coordinate();
  if(callback != nullptr)
    runCallback();
}
