/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
