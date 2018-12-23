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

#include "pos.h"

#include <cstring>
#include <strings.h>

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

void format_float_int(int val, unsigned int decimals, char *str)
{
  unsigned int off = 0;
  // handle the sign explicitely so it does not count in the minimum
  // output length, could result in "-.42" otherwise
  if(val < 0) {
    str[0] = '-';
    off++;
    val = -val;
  }
  // make sure there are at least 3 characters in the output
  int l = sprintf(str + off, "%0*u", decimals + 1, val) + off;
  // move the last 2 digits and \0 one position to the right
  memmove(str + l + 1 - decimals, str + l - decimals, decimals + 1);
  // insert dot
  str[l - decimals] = '.';
  // remove any trailing zeroes, use the knowledge about the string length
  // to avoid needless searching
  remove_trailing_zeroes(str + l - decimals - 1);
}
