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

#include "net_io.h"

#include <unordered_map>

#include <osm2go_annotations.h>

typedef std::unordered_map<int, const char *> HttpCodeMap;

static HttpCodeMap http_msg_init() {
  HttpCodeMap http_messages;

  http_messages[200] = "Ok";
  http_messages[203] = "No Content";
  http_messages[301] = "Moved Permenently";
  http_messages[302] = "Moved Temporarily";
  http_messages[400] = "Bad Request";
  http_messages[401] = "Unauthorized";
  http_messages[403] = "Forbidden";
  http_messages[404] = "Not Found";
  http_messages[405] = "Method Not Allowed";
  http_messages[409] = "Conflict";
  http_messages[410] = "Gone";
  http_messages[412] = "Precondition Failed";
  http_messages[417] = "(Expect rejected)";
  http_messages[500] = "Internal Server Error";
  http_messages[503] = "Service Unavailable";
  http_messages[509] = "Bandwidth Limit Exceeded";

  return http_messages;
}

const char *http_message(int id) {
  static const HttpCodeMap http_messages = http_msg_init();

  const HttpCodeMap::const_iterator it = http_messages.find(id);
  if(likely(it != http_messages.end()))
    return it->second;

  return "(unknown HTTP response code)";
}

bool check_gzip(const char* mem, const size_t len)
{
  return len > 2 && mem[0] == 0x1f && static_cast<unsigned char>(mem[1]) == 0x8b;
}
