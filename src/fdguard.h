/*
 * Copyright (C) 2017 Rolf Eike Beer <eike@sf-mail.de>.
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

#ifndef FDGUARD_H
#define FDGUARD_H

#include <dirent.h>

#include <osm2go_cpp.h>

struct fdguard {
  explicit fdguard(int f) : fd(f) {}
  /**
   * @brief open a filename as anchor point
   *
   * It will use O_CLOEXEC, O_PATH, and O_DIRECTORY if present.
   */
  explicit fdguard(const char *dirname);
  ~fdguard();

  const int fd;
  inline operator int() const { return fd; }
  inline bool valid() const { return fd >= 0; }
  void swap(fdguard &other);
};

class dirguard {
  DIR *d;
public:
  /**
   * @brief opens the given directory
   */
  dirguard(const char *name);
  ~dirguard();

  inline bool valid() const { return d != O2G_NULLPTR; }
  inline dirent *next() { return readdir(d); }
  inline int dirfd() { return ::dirfd(d); }
};

#endif /* FDGUARD_H */
