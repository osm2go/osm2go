/*
 * SPDX-FileCopyrightText: 2017 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <dirent.h>
#include <string>

#include <osm2go_cpp.h>
#include <osm2go_stl.h>

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
  inline fdguard(fdguard &&other) noexcept
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
  inline operator int() const noexcept { return fd; }
  inline operator bool() const noexcept { return valid(); }
  inline bool valid() const noexcept { return fd >= 0; }
  void swap(fdguard &other) noexcept;
};

class dirguard {
  const std::string p;
  DIR * const d;
public:
#if __cplusplus >= 201103L
  dirguard(const dirguard &other) = delete;
  dirguard() = delete;
  inline dirguard(dirguard &&f) noexcept
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
    : p(ends_with(name, '/') ? name : name + '/'), d(opendir(name.c_str())) {}
  explicit dirguard(int fd);
  /**
   * @brief opens the given subdirectory of a parent directory
   */
  explicit dirguard(const dirguard &parent, const char *subdir)
    : p(parent.path() + subdir + '/'), d(opendir(p.c_str())) {}
  ~dirguard();

  inline bool valid() const noexcept { return d != nullptr; }
  inline dirent *next() { return readdir(d); }
  inline int dirfd() const { return ::dirfd(d); }

  /**
   * @brief the path name of the directory
   *
   * This may be empty if the object was initialized from a filedescriptor.
   */
  inline const std::string &path() const noexcept
  { return p; }
};

std::string find_file(const std::string &n) __attribute__((warn_unused_result));
