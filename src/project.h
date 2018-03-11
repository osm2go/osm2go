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

#ifndef PROJECT_H
#define PROJECT_H

#include "fdguard.h"
#include "pos.h"

#include <libxml/parser.h>
#include <string>

#include <osm2go_platform.h>

struct appdata_t;
struct map_state_t;
struct osm_t;

struct project_t {
  project_t(map_state_t &ms, const std::string &n, const std::string &base_path);
  ~project_t();

  inline const std::string &server(const std::string &def) const
  { return rserver.empty() ? def : rserver; }

  /**
   * @brief set a new server value
   *
   * This will either copy nserver or clear the stored value if nserver == def
   */
  void adjustServer(const char *nserver, const std::string &def);

  struct {
    int x, y;
  } wms_offset;

  map_state_t &map_state;

  pos_area bounds;

  const std::string name;
  const std::string path;
  std::string desc;
  std::string osmFile;
  std::string rserver;

  std::string wms_server;
  std::string wms_path;

  bool data_dirty;     // needs to download new data
  bool isDemo;         // if this is the demo project
  fdguard dirfd;       // filedescriptor of path

  osm_t *parse_osm() const;

  /**
   * @brief save the current project to disk
   * @param parent parent window for dialogs
   */
  bool save(osm2go_platform::Widget *parent = O2G_NULLPTR);

  /**
   * @brief check if the current project is the demo project
   * @param parent parent window for dialogs
   */
  bool check_demo(osm2go_platform::Widget *parent = O2G_NULLPTR) const;
};

bool project_load(appdata_t &appdata, const std::string &name);
std::string project_select(appdata_t &appdata);

#endif // PROJECT_H
