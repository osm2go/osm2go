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

#include <glib.h>
#include <map>
#include <string>
#include <vector>

/* define this for a vertical UI layout */
#undef PORTRAIT

/* these size defaults are used in the non-hildonized version only */
#ifndef PORTRAIT
#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480
#else
/* about the size of the openmoko screen */
#define DEFAULT_WIDTH 480
#define DEFAULT_HEIGHT 620
#endif

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
  gboolean enable_gps;
  gboolean follow_gps;

  /* set to true if no gconf settings were found */
  /* and the demo was loaded */
  gboolean first_run_demo;

  static settings_t *load();
  void save() const;

private:
  std::map<const char *, std::string *> store_str;
  std::map<const char *, gboolean *> store_bool;
};

#endif // SETTINGS_H
