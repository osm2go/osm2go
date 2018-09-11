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

#ifndef OSM2GO_I18N_H
#define OSM2GO_I18N_H

#include <locale.h>
#include <libintl.h>
#include <string>

#include <osm2go_stl.h>

#define _(String) gettext(String)

class trstring : public std::string {
  explicit inline trstring(const std::string &s) : std::string(s) {}
  std::string argn(const std::string &spattern, const std::string &a, std::string::size_type pos) const;
public:
  explicit inline trstring() : std::string() {}
  explicit inline trstring(const char *s) : std::string(gettext(s)) {}
#if __cplusplus >= 201103L
  // catch if one passes a constant nullptr as argument
  trstring(std::nullptr_t) = delete;
  trstring(std::nullptr_t, const char *, int) = delete;
#endif
  trstring(const char *msg, const char *, int n) __attribute__((nonnull(2)))
    : std::string(trstring(msg).argn("%n", std::to_string(n), std::string(msg).find("%n"))) { }

  trstring arg(const std::string &a) const;
  inline trstring arg(const char *a) const
  { return arg(std::string(a)); }
  inline trstring arg(char *a) const
  { return arg(std::string(a)); }
  inline trstring arg(const trstring &a) const
  { return arg(static_cast<std::string>(a)); }
  template<typename T> inline trstring arg(T l) const
  { return arg(std::to_string(l)); }
};

#endif // OSM2GO_I18N_H
