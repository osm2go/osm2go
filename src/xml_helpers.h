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

#ifndef XML_HELPERS_H
#define XML_HELPERS_H

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
};

struct xmlDocDelete {
  inline void operator()(xmlDocPtr doc) {
    xmlFreeDoc(doc);
  }
};

double xml_get_prop_float(xmlNode *node, const char *prop);
bool xml_get_prop_bool(xmlNode *node, const char *prop);

static inline double xml_parse_float(const xmlChar *str)
{
  return osm2go_platform::string_to_double(reinterpret_cast<const char *>(str));
}
inline double xml_parse_float(const xmlString &str)
{ return xml_parse_float(str.get()); }

#endif /* XML_HELPERS_H */
