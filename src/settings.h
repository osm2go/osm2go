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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <track.h>

#include <string>
#include <vector>
#include <utility>

#define DEFAULT_STYLE "mapnik"

class settings_t {
  settings_t();
public:
  ~settings_t();

  /* never changed */
  std::string base_path;

  /* changed in project.c */
  std::string project;

  /* changed in osm_api.c */
  std::string server, username, password;

  /* changed in wms.c */
  std::vector<struct wms_server_t *> wms_server;

  /* changed in style.c */
  std::string style;

  /* changed in main.c */
  std::string track_path;
  bool enable_gps;
  bool follow_gps;
  TrackVisibility trackVisibility;

  /* set to true if no gconf settings were found */
  /* and the demo was loaded */
  bool first_run_demo;

  static settings_t *load();
  void save() const;

private:
  typedef std::vector<std::pair<const char *, std::string *> > StringKeys;
  StringKeys store_str;
  typedef std::vector<std::pair<const char *, bool *> > BooleanKeys;
  BooleanKeys store_bool;
};

/**
 * @brief adjust an API 0.5 url to 0.6 or http to https
 * @param rserver configured server URL
 * @returns if the server was changed
 */
bool api_adjust(std::string &rserver);

#endif // SETTINGS_H
