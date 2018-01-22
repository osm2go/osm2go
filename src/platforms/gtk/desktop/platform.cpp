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
 * along with OSM2Go. If not, see <https://www.gnu.org/licenses/>.
 */

#include <osm2go_platform.h>

#include <gtk/gtk.h>

#include <osm2go_cpp.h>

int osm2go_platform::init()
{

  return 0;
}

void osm2go_platform::cleanup()
{
}

void osm2go_platform::open_url(const char* url)
{
  gtk_show_uri(O2G_NULLPTR, url, GDK_CURRENT_TIME, O2G_NULLPTR);
}
