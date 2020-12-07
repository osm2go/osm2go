/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

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

#include <osm2go_cpp.h>

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
struct pos_t {
  pos_float_t lat, lon;
#ifdef __cplusplus
  inline pos_t() noexcept {}
  inline pos_t(pos_float_t a, pos_float_t o) noexcept : lat(a), lon(o) {}
  bool operator==(const pos_t &other) const noexcept
  { return lat == other.lat && lon == other.lon; }
  inline bool operator!=(const pos_t &other) const noexcept
  { return !operator==(other); }
  bool valid() const noexcept;

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
#endif
};

#ifdef __cplusplus

struct pos_area {
  explicit pos_area() noexcept {}
  pos_area(const pos_t &mi, const pos_t &ma) noexcept
    : min(mi), max(ma) {}

  pos_t min;
  pos_t max;

  bool contains(pos_t pos) const noexcept;
  bool valid() const noexcept;

  inline pos_float_t centerLat() const noexcept
  { return (max.lat + min.lat) / 2; }
  inline pos_float_t centerLon() const noexcept
  { return (max.lon + min.lon) / 2; }
  inline pos_t center() const noexcept
  { return pos_t(centerLat(), centerLon()); }
  inline pos_float_t latDist() const noexcept
  { return max.lat - min.lat; }
  inline pos_float_t lonDist() const noexcept
  { return max.lon - min.lon; }
  inline bool normalized() const noexcept
  { return max.lat > min.lat && max.lon > min.lon; }
  inline bool operator==(const pos_area &other) const noexcept
  { return max == other.max && min == other.min; }
  inline bool operator!=(const pos_area &other) const noexcept
  { return !operator==(other); }

  static inline pos_area normalized(const pos_t &mi, const pos_t &ma)
  {
    pos_area ret(mi, ma);
    if (ret.min.lat > ret.max.lat)
      std::swap(ret.max.lat, ret.min.lat);
    if (ret.min.lon > ret.max.lon)
      std::swap(ret.max.lon, ret.min.lon);
    return ret;
  }

  /**
   * @brief returns a string representation of min.lon + ',' + min.lat + ',' + max.lon + ',' + max.lat
   */
  std::string print() const;
};

/* local position */
struct lpos_t {
  lpos_t() noexcept {}
  lpos_t(int px, int py) noexcept
    : x(px) , y(py) {}
  bool operator==(const lpos_t &other) noexcept
  { return x == other.x && y == other.y; }

  /**
   * @brief calculate the global coordinates from local position in given bounds
   */
  pos_t toPos(const bounds_t &bounds) const;
  int x, y;
};

struct bounds_t {
  pos_area ll;
  lpos_t min, max;
  lpos_t center;
  float scale;
  bool contains(lpos_t pos) const noexcept;
  bool init(const pos_area &area);
};

void pos_lat_str(char *str, size_t len, pos_float_t latitude);
static inline void pos_lon_str(char *str, size_t len, pos_float_t longitude)
{
  pos_lat_str(str, len, longitude);
}

void pos_lat_str_deg(char *str, size_t len, pos_float_t latitude);
static inline void pos_lon_str_deg(char *str, size_t len, pos_float_t longitude)
{
  pos_lat_str_deg(str, len, longitude);
}

bool pos_lat_valid(pos_float_t lat) noexcept;
bool pos_lon_valid(pos_float_t lon) noexcept;

#endif
