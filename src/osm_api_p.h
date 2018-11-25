/*
 * Copyright (C) 2018 Rolf Eike Beer <eike@sf-mail.de>.
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

// osm.h must come before net_io.h for Fremantle as curl would drag in inttypes.h (?),
// but not all needed definitions to get the format specifiers in C++ mode
#include "osm.h"

#include "net_io.h"
#include "project.h"

#include <curl/curl.h>
#include <memory>
#include <string>

#include <osm2go_stl.h>

struct appdata_t;
struct project_t;
class trstring;

class osm_upload_context_t {
public:
  osm_upload_context_t(appdata_t &a, project_t::ref p, const char *c, const char *s);

  appdata_t &appdata;
  osm_t::ref osm;
  project_t::ref project;
  const std::string urlbasestr; ///< API base URL, will always end in '/'

  std::string changeset;

  std::string comment;
  const std::string src;
  std::unique_ptr<CURL, curl_deleter> curl;

  void append_str(const char *msg, const char *colorname = nullptr) __attribute__((nonnull(2)));
  void append(const trstring &msg, const char *colorname = nullptr);

  void upload(const osm_t::dirty_t &dirty, osm2go_platform::Widget *parent);
};

void osm_upload_dialog(appdata_t &appdata, const osm_t::dirty_t &dirty);
