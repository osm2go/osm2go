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

#ifndef PROJECT_P_H
#define PROJECT_P_H

#include <string>
#include <vector>

struct map_state_t;
struct project_t;

std::string project_filename(const project_t *project);
bool project_read(const std::string &project_file, project_t *project,
                  const std::string &defaultserver, int basefd);
void project_close(appdata_t &appdata);
std::vector<project_t *> project_scan(map_state_t &ms, const std::string &base_path,
                                      int base_path_fd, const std::string &server);
std::string project_exists(int base_path, const char *name);
void project_delete(project_t *project);

struct projects_to_bounds {
  std::vector<pos_area> &pbounds;
  explicit projects_to_bounds(std::vector<pos_area> &b) : pbounds(b) {}
  void operator()(const project_t *project);
};

#endif // PROJECT_P_H