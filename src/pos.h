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

#ifndef POS_H
#define POS_H

/* format string used to write lat/lon coordinates, altitude and time */
#define LL_FORMAT   "%.07f"
#define ALT_FORMAT  "%.02f"
#define DATE_FORMAT "%FT%T"

#ifdef USE_FLOAT
/* use float instead of double on small machines */
typedef float pos_float_t;
#else
typedef double pos_float_t;
#endif

#ifdef __cplusplus
#include <cmath>
#include <cstddef>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>
#include <string>
/* equatorial radius in meters */
#define POS_EQ_RADIUS     (6378137.0)
#define KMPMIL   (1.609344)
#define KM2MIL(a)  ((a)/KMPMIL)
#define MIL2KM(a)  ((a)*KMPMIL)

#define DEG2RAD(a)  ((a) * M_PI / 180.0)
#define RAD2DEG(a)  ((a) * 180.0 / M_PI)
#endif

struct bounds_t;
struct lpos_t;

/* global position */
typedef struct pos_t {
  pos_float_t lat, lon;
#ifdef __cplusplus
  inline pos_t() {}
  inline pos_t(pos_float_t a, pos_float_t o) : lat(a), lon(o) {}
  bool operator==(const pos_t &other) const
  { return lat == other.lat && lon == other.lon; }
  inline bool operator!=(const pos_t &other) const
  { return !operator==(other); }
  bool valid() const;

  /**
   * @brief calculate the screen coordinates
   *
   * Use this for the map center as it is not offset by itself.
   */
  lpos_t toLpos() const;

  /**
   * @brief calculate the screen coordinates inside the given bounds
   */
  lpos_t toLpos(const bounds_t &bounds) const;

  void toXmlProperties(xmlNodePtr node) const;

  static pos_t fromXmlProperties(xmlNodePtr node,
                                 const char *latName = "lat", const char *lonName = "lon");

  static pos_t fromXmlProperties(xmlTextReaderPtr reader,
                                 const char *latName = "lat", const char *lonName = "lon");

  /**
   * @brief returns a string representation of lon + delim + lat
   */
  std::string print(char delim = ',');
#endif
} pos_t;

#ifdef __cplusplus

struct pos_area {
  explicit pos_area() {}
  pos_area(const pos_t &mi, const pos_t &ma)
    : min(mi), max(ma) {}

  pos_t min;
  pos_t max;

  bool contains(pos_t pos) const;
  inline bool valid() const
  { return min.valid() && max.valid(); }

  inline pos_float_t centerLat() const
  { return (max.lat + min.lat) / 2; }
  inline pos_float_t centerLon() const
  { return (max.lon + min.lon) / 2; }
  inline pos_t center() const
  { return pos_t(centerLat(), centerLon()); }
  inline pos_float_t latDist() const
  { return max.lat - min.lat; }
  inline pos_float_t lonDist() const
  { return max.lon - min.lon; }
  inline bool normalized() const
  { return max.lat > min.lat && max.lon > min.lon; }
  inline bool operator==(const pos_area &other) const
  { return max == other.max && min == other.min; }
  inline bool operator!=(const pos_area &other) const
  { return !operator==(other); }
};

/* local position */
typedef struct lpos_t {
  lpos_t() {}
  lpos_t(int px, int py)
    : x(px) , y(py) {}
  bool operator==(const lpos_t &other)
  { return x == other.x && y == other.y; }

  /**
   * @brief calculate the global coordinates from local position in given bounds
   */
  pos_t toPos(const bounds_t &bounds) const;
  int x, y;
} lpos_t;

struct bounds_t {
  pos_area ll;
  lpos_t min, max;
  lpos_t center;
  float scale;
  bool contains(lpos_t pos) const;
  bool init(const pos_area &area);
};

void pos_lat_str(char *str, size_t len, pos_float_t latitude);
void pos_lon_str(char *str, size_t len, pos_float_t longitude);

bool pos_lat_valid(pos_float_t lat);
bool pos_lon_valid(pos_float_t lon);

void remove_trailing_zeroes(char *str);

#endif

#endif // POS_H
