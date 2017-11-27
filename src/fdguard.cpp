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

void fdguard::swap(fdguard &other)
{
  int f = fd;
  const_cast<int &>(fd) = other.fd;
  const_cast<int &>(other.fd) = f;
}

dirguard::dirguard(const char *name)
  : d(opendir(name))
{
}

dirguard::dirguard(int fd)
  : d(fdopendir(dup(fd)))
{
  // ignore the position of fd
  rewinddir(d);
}

dirguard::~dirguard()
{
  if(likely(valid()))
    closedir(d);
}
