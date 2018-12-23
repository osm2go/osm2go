/*
 * Copyright (C) 2017 Rolf Eike Beer <eike@sf-mail.de>.
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

#pragma once

#include <libxml/tree.h>
#include <memory>

#include <osm2go_stl.h>

#include <osm2go_platform.h>

struct xmlDelete {
  inline void operator()(xmlChar *s) {
    xmlFree(s);
  }
};

class xmlString : public std::unique_ptr<xmlChar, xmlDelete> {
public:
  xmlString(xmlChar *txt = nullptr)
    : std::unique_ptr<xmlChar, xmlDelete>(txt) {}

  operator const char *() const
  { return reinterpret_cast<const char *>(get()); }

  inline bool empty() const
  { return !operator bool() || *get() == '\0'; }
};

struct xmlDocDelete {
  inline void operator()(xmlDocPtr doc) {
    xmlFreeDoc(doc);
  }
};

typedef std::unique_ptr<xmlDoc, xmlDocDelete> xmlDocGuard;

double xml_get_prop_float(xmlNode *node, const char *prop);
bool xml_get_prop_bool(xmlNode *node, const char *prop);

static inline double xml_parse_float(const xmlChar *str)
{
  return osm2go_platform::string_to_double(reinterpret_cast<const char *>(str));
}
inline double xml_parse_float(const xmlString &str)
{ return xml_parse_float(str.get()); }

void format_float_int(int val, unsigned int decimals, char *str);

/**
 * @brief convert a floating point number to a integer representation
 * @param val the floating point value
 * @param decimals the maximum number of decimals behind the separator
 * @param str the buffer to print the number to, must be big enough
 *
 * This assumes that a "base 10 shift left by decimals" can still be
 * represented as an integer. Trailing zeroes are chopped.
 *
 * 16 as length of str is enough for every possible value: int needs at most
 * 10 digits, '-', '.', '\0' -> 13
 *
 * The whole purpose of this is to avoid using Glib, which would provide
 * g_ascii_formatd(). One can't simply use snprintf() or friends, as the
 * decimal separator is locale dependent and changing the locale is expensive
 * and not thread safe. At the end this code is twice as fast as the Glib
 * code, likely because it is much less general and uses less floating point
 * operations.
 */
template<typename T>
void format_float(T val, unsigned int decimals, char *str)
{
  for (unsigned int k = decimals; k > 0; k--)
    val *= 10;
  format_float_int(round(val), decimals, str);
}

/**
 * @brief remove trailing zeroes from a number string
 * @param str the buffer to modify
 *
 * This will remove all trailing zeroes if the buffer contains a delimiter
 * (i.e. any character outside [0..9]. If the last character would be that
 * delimiter it will also be removed.
 */
void remove_trailing_zeroes(char *str);
