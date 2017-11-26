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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef XML_HELPERS_H
#define XML_HELPERS_H

#include <libxml/tree.h>
#if __cplusplus >= 201103L
#include <memory>
#else
#include <osm2go_stl.h>
#endif

double xml_get_prop_float(xmlNode *node, const char *prop);
bool xml_get_prop_bool(xmlNode *node, const char *prop);

struct xmlDelete {
  inline void operator()(xmlChar *s) {
    xmlFree(s);
  }
};

typedef std::unique_ptr<xmlChar, xmlDelete> xmlString;

#endif /* XML_HELPERS_H */
