/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "fdguard.h"
#include "track.h"

#include <array>
#include <memory>
#include <string>
#include <utility>

#include <osm2go_stl.h>

#define DEFAULT_STYLE "mapnik"

class settings_t {
protected:
  settings_t();

  void load();
  void setDefaults();

public:
  ~settings_t();

  typedef std::shared_ptr<settings_t> ref;

  /* never changed */
  std::string base_path;
  fdguard base_path_fd;

  /* changed in project.cpp */
  std::string project;

  /* changed in osm_api.cpp */
  std::string server, username, password;

  /* changed in wms.cpp */
  std::vector<struct wms_server_t *> wms_server;

  /* changed in style.cpp */
  std::string style;

  /* changed in main.cpp */
  std::string track_path;
  bool enable_gps;
  bool follow_gps;
  bool imperial_units;
  TrackVisibility trackVisibility;

  /* set to true if no gconf settings were found */
  /* and the demo was loaded */
  bool first_run_demo;

  static ref instance();
  void save() const;

  typedef std::array<std::pair<const char *, std::string *>, 7> StringKeys;
  typedef std::array<std::pair<const char *, bool *>, 3> BooleanKeys;
private:
  const StringKeys store_str;
  const BooleanKeys store_bool;
};

/**
 * @brief adjust an API 0.5 url to 0.6 or http to https
 * @param rserver configured server URL
 * @returns if the server was changed
 */
bool api_adjust(std::string &rserver);
