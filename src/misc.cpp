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

#include "misc.h"
#include "xml_helpers.h"

#include <cassert>
#include <cstring>
#include <strings.h>
#include <sys/stat.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>

double xml_get_prop_float(xmlNode *node, const char *prop) {
  xmlString str(xmlGetProp(node, BAD_CAST prop));
  return xml_parse_float(str);
}

bool xml_get_prop_bool(xmlNode *node, const char *prop) {
  xmlString prop_str(xmlGetProp(node, BAD_CAST prop));
  if(!prop_str)
    return false;

  return (strcasecmp(prop_str, "true") == 0);
}

std::string find_file(const std::string &n) {
  assert(!n.empty());

  struct stat st;

  if(unlikely(n[0] == '/')) {
    if(stat(n.c_str(), &st) == 0 && S_ISREG(st.st_mode))
      return n;
    return std::string();
  }

  const std::vector<osm2go_platform::datapath> &paths = osm2go_platform::base_paths();

  for(unsigned int i = 0; i < paths.size(); i++) {
    if(fstatat(paths[i].fd, n.c_str(), &st, 0) == 0 && S_ISREG(st.st_mode))
      return paths[i].pathname + n;
  }

  return std::string();
}
