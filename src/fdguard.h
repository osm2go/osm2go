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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef FDGUARD_H
#define FDGUARD_H

#include <dirent.h>
#include <string>

#include <osm2go_cpp.h>

struct fdguard {
  explicit fdguard(int f) : fd(f) {}
  /**
   * @brief open a filename as anchor point
   * @param dirname path of the directory to open
   *
   * It will use O_CLOEXEC, O_PATH, and O_DIRECTORY if present. If O_PATH is not
   * defined O_RDONLY will be used instead.
   */
  explicit fdguard(const char *dirname);
  /**
   * @brief open a filename as anchor point
   * @param pathname the path to open
   * @param flags additional flags to pass to open.
   *
   * O_CLOEXEC will always be added to flags.
   */
  explicit fdguard(const char *pathname, int flags);
  /**
   * @brief open a filename as anchor point
   * @param basefd file descriptor to use as base for pathname
   * @param pathname the path to open
   * @param flags additional flags to pass to open.
   *
   * O_CLOEXEC will always be added to flags.
   */
  explicit fdguard(int basefd, const char *pathname, int flags);
#if __cplusplus >= 201103L
  inline fdguard(fdguard &&other)
    : fd(other.fd)
  {
    const_cast<int &>(other.fd) = -1;
  }
  fdguard(const fdguard &other) = delete;
  fdguard &operator=(const fdguard &other) = delete;
#else
  fdguard(const fdguard &other);
  fdguard &operator=(const fdguard &other);
#endif
  ~fdguard();

  const int fd;
  inline operator int() const { return fd; }
  inline operator bool() const { return valid(); }
  inline bool valid() const { return fd >= 0; }
  void swap(fdguard &other);
};

class dirguard {
  const std::string p;
  DIR * const d;
public:
#if __cplusplus >= 201103L
  dirguard(const dirguard &other) = delete;
  dirguard() = delete;
  inline dirguard(dirguard &&f)
    : p(std::move(f.p)), d(f.d)
  {
    const_cast<DIR*&>(f.d) = nullptr;
  }
#else
  dirguard(const dirguard &other);
  explicit inline dirguard() : d(nullptr) {}
  dirguard &operator=(const dirguard &other);
#endif

  /**
   * @brief opens the given directory
   */
  explicit inline dirguard(const char *name) __attribute__((nonnull(2)))
    : p(name), d(opendir(name)) {}
  explicit inline dirguard(const std::string &name)
    : p(name.at(name.size() - 1) == '/' ? name : name + '/'), d(opendir(name.c_str())) {}
  explicit dirguard(int fd);
  /**
   * @brief opens the given subdirectory of a parent directory
   */
  explicit dirguard(const dirguard &parent, const char *subdir)
    : p(parent.path() + subdir + '/'), d(opendir(p.c_str())) {}
  ~dirguard();

  inline bool valid() const { return d != nullptr; }
  inline dirent *next() { return readdir(d); }
  inline int dirfd() const { return ::dirfd(d); }

  /**
   * @brief the path name of the directory
   *
   * This may be empty if the object was initialized from a filedescriptor.
   */
  inline const std::string &path() const
  { return p; }
};

#endif /* FDGUARD_H */
