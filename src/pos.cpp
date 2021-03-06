/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pos.h"

#include "misc.h"

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>

#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

bool pos_t::valid() const noexcept
{
  return pos_lat_valid(lat) && pos_lon_valid(lon);
}

void pos_lat_str(char *str, size_t len, pos_float_t latitude) {
  if(std::isnan(latitude)) {
    strncpy(str, "---", len);
  } else {
    snprintf(str, len, "%.5f", latitude);
    remove_trailing_zeroes(str);
  }
}

void pos_lat_str_deg(char *str, size_t len, pos_float_t latitude)
{
  pos_lat_str(str, len, latitude);
  size_t offs = strlen(str);
  strncat(str + offs, "°", len - offs);
}

bool pos_lat_valid(pos_float_t lat) noexcept
{
  return(!std::isnan(lat) && (lat >= -90.0) && (lat <= 90.0));
}

bool pos_lon_valid(pos_float_t lon) noexcept
{
  return(!std::isnan(lon) && (lon >= -180.0) && (lon <= 180.0));
}

lpos_t pos_t::toLpos(const bounds_t &bounds) const {
  lpos_t lpos = toLpos();
  lpos.x = ( lpos.x - bounds.center.x) * bounds.scale;
  lpos.y = (-lpos.y + bounds.center.y) * bounds.scale;
  return lpos;
}

static void xml_add_prop_coord(xmlNodePtr node, const char *key, pos_float_t val)
{
  char str[16];
  format_float(val, 7, str);
  xmlNewProp(node, BAD_CAST key, BAD_CAST str);
}

void pos_t::toXmlProperties(xmlNodePtr node) const {
  xml_add_prop_coord(node, "lat", lat);
  xml_add_prop_coord(node, "lon", lon);
}

pos_t pos_t::fromXmlProperties(xmlNodePtr node, const char *latName, const char *lonName)
{
  return pos_t(xml_get_prop_float(node, latName),
               xml_get_prop_float(node, lonName));
}

static pos_float_t xml_reader_attr_float(xmlTextReaderPtr reader, const char *name) {
  xmlString prop(xmlTextReaderGetAttribute(reader, BAD_CAST name));
  return xml_parse_float(prop);
}

pos_t pos_t::fromXmlProperties(xmlTextReaderPtr reader, const char *latName, const char *lonName)
{
  return pos_t(xml_reader_attr_float(reader, latName),
               xml_reader_attr_float(reader, lonName));
}

lpos_t pos_t::toLpos() const {
  lpos_t lpos;
  lpos.x = POS_EQ_RADIUS * DEG2RAD(lon);
  lpos.y = POS_EQ_RADIUS * log(tan(M_PI / 4 + DEG2RAD(lat) / 2));
  return lpos;
}

pos_t lpos_t::toPos(const bounds_t &bounds) const
{
  float fx = ( x / bounds.scale) + bounds.center.x;
  float fy = (-y / bounds.scale) + bounds.center.y;

  pos_t pos;
  pos.lon = RAD2DEG(fx / POS_EQ_RADIUS);
  pos.lat = RAD2DEG(2 * atan(exp(fy / POS_EQ_RADIUS)) - M_PI/2);
  return pos;
}

bool bounds_t::contains(lpos_t pos) const noexcept {
  if((pos.x < min.x) || (pos.x > max.x))
    return false;
  if((pos.y < min.y) || (pos.y > max.y))
    return false;
  return true;
}

bool bounds_t::init(const pos_area &area)
{
  ll = area;

  if(unlikely(!ll.valid()))
    return false;

  // calculate map zone which will be used as a reference for all drawing/projection later on
  const pos_t c = ll.center();

  center = c.toLpos();

  // the scale is needed to accomodate for "streching" by the mercartor projection
  scale = cos(DEG2RAD(c.lat));

  return true;
}

bool pos_area::contains(pos_t pos) const noexcept
{
  if((pos.lat < min.lat) || (pos.lat > max.lat))
    return false;
  if((pos.lon < min.lon) || (pos.lon > max.lon))
    return false;
  return true;
}

bool pos_area::valid() const noexcept
{
  return min.valid() &&
         max.valid() &&
         min.lat < max.lat &&
         min.lon < max.lon;
}

std::string pos_area::print() const
{
  const std::array<pos_float_t, 4> pos = { { min.lon, min.lat, max.lon, max.lat } };
  char buf[16 * pos.size() + pos.size()];

  size_t p = 0;
  for(unsigned int i = 0; i < pos.size(); i++) {
    format_float(pos.at(i), 7, buf + p);
    p += strlen(buf + p);
    buf[p++] = ',';
  }

  return std::string(buf, p - 1);
}
