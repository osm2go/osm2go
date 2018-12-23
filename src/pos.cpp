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
  size_t offs;
  if(std::isnan(latitude)) {
    strncpy(str, "---", len);
    offs = 3;
  } else {
    snprintf(str, len-1, "%.5f", latitude);
    remove_trailing_zeroes(str);
    offs = strlen(str);
  }
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

pos_t lpos_t::toPos(const bounds_t &bounds) const {
  lpos_t lpos = *this;
  lpos.x = ( lpos.x / bounds.scale) + bounds.center.x;
  lpos.y = (-lpos.y / bounds.scale) + bounds.center.y;

  pos_t pos;
  pos.lon = RAD2DEG(lpos.x / POS_EQ_RADIUS);
  pos.lat = RAD2DEG(2 * atan(exp(lpos.y / POS_EQ_RADIUS)) - M_PI/2);
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
  pos_t c = ll.center();

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
  std::array<pos_float_t, 4> pos = { { min.lon, min.lat, max.lon, max.lat } };
  char buf[16 * pos.size() + pos.size()];

  size_t p = 0;
  for(unsigned int i = 0; i < pos.size(); i++) {
    format_float(pos.at(i), 7, buf + p);
    p += strlen(buf + p);
    buf[p++] = ',';
  }

  return std::string(buf, p - 1);
}

void remove_trailing_zeroes(char *str) {
  char *delim = str;
  while(*delim >= '0' && *delim <= '9')
    delim++;
  if(*delim == '\0')
    return;
  char *p = delim + strlen(delim) - 1;
  while(*p == '0')
    *p-- = '\0';
  if(p == delim)
    *p = '\0';
}
