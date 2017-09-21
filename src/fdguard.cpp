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

#include "fdguard.h"

#include "misc.h"

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

fdguard::~fdguard() {
  if(G_LIKELY(valid()))
    close(fd);
}

void fdguard::swap(fdguard &other)
{
  int f = fd;
  const_cast<int&>(fd) = other.fd;
  const_cast<int&>(other.fd) = f;
}