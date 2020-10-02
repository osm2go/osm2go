/*
 * SPDX-FileCopyrightText: 2017,2018,2020 Rolf Eike Beer <eike@sf-mail.de>
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

namespace {

DIR *openByFd(int fd, bool rewind)
{
  int nfd = dup(fd);
  if (unlikely(nfd < 0))
    return nullptr;

  DIR *ret = fdopendir(nfd);
  if (unlikely(ret == nullptr)) {
    close(nfd);
    return nullptr;
  }

  // ignore the position of fd
  if (rewind)
    rewinddir(ret);

  return ret;
}

} // namespace

dirguard::dirguard(int fd)
  : d(openByFd(fd, true))
{
}

#if __cplusplus < 201103L
dirguard::dirguard(const dirguard &other)
  : p(other.p)
  // do not rewind to behave the same like a move constructor
  , d(openByFd(other.dirfd(), false))
{
}

dirguard& dirguard::operator=(const dirguard &other)
{
  assert(d == nullptr);
  assert(p.empty());
  const_cast<DIR*&>(d) = openByFd(other.dirfd(), false);
  const_cast<std::string&>(p) = other.p;

  return *this;
}
#endif

dirguard::~dirguard()
{
  if(likely(valid()))
    closedir(d);
}
