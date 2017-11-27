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

#ifndef STYLE_P_H
#define STYLE_P_H

struct appdata_t;
class icon_t;
struct style_t;

#include <map>
#include <string>

style_t *style_load_fname(icon_t &icons, const std::string &filename);
std::map<std::string, std::string> style_scan();
std::string style_basename(const std::string &name);

#endif // STYLE_P_H
