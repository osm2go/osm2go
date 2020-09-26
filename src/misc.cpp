/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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

void remove_trailing_zeroes(char *str)
{
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
