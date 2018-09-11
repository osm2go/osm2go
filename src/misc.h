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

#ifndef MISC_H
#define MISC_H

#include "fdguard.h"

#include <string>
#include <vector>

#include <osm2go_cpp.h>
#include <osm2go_stl.h>

struct datapath {
#if __cplusplus >= 201103L
  explicit inline datapath(fdguard &&f)  : fd(std::move(f)) {}
#else
  explicit inline datapath(fdguard &f)  : fd(f) {}
#endif
  fdguard fd;
  std::string pathname;
};

const std::vector<datapath> &base_paths();

std::string find_file(const std::string &n);

#endif // MISC_H
