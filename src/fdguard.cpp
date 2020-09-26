/*
 * SPDX-FileCopyrightText: 2017 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "fdguard.h"

#include "osm2go_annotations.h"

#include <fcntl.h>
#include <unistd.h>

#ifndef O_PATH
#define O_PATH O_RDONLY
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

fdguard::fdguard(const char *dirname)
  : fd(open(dirname, O_DIRECTORY | O_PATH | O_CLOEXEC))
{
}

fdguard::fdguard(const char *pathname, int flags)
  : fd(open(pathname, flags | O_CLOEXEC))
{
}

fdguard::fdguard(int basefd, const char *pathname, int flags)
  : fd(openat(basefd, pathname, flags | O_CLOEXEC))
{
}

fdguard::~fdguard() {
  if(likely(valid()))
    close(fd);
}

#if __cplusplus < 201103L
fdguard::fdguard(const fdguard &other)
  : fd(dup(other.fd))
{
}

fdguard &fdguard::operator=(const fdguard& other)
{
  const_cast<int &>(fd) = dup(other.fd);
  return *this;
}
#endif

void fdguard::swap(fdguard &other) noexcept
{
  int f = fd;
  const_cast<int &>(fd) = other.fd;
  const_cast<int &>(other.fd) = f;
}

dirguard::dirguard(int fd)
  : d(fdopendir(dup(fd)))
{
  // ignore the position of fd
  if(d != nullptr)
    rewinddir(d);
}

#if __cplusplus < 201103L
dirguard::dirguard(const dirguard &other)
  : p(other.p), d(fdopendir(dup(other.dirfd())))
{
}

dirguard& dirguard::operator=(const dirguard &other)
{
  assert(d == nullptr);
  assert(p.empty());
  const_cast<DIR*&>(d) = fdopendir(dup(other.dirfd()));
  const_cast<std::string&>(p) = other.p;

  return *this;
}
#endif

dirguard::~dirguard()
{
  if(likely(valid()))
    closedir(d);
}
