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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NET_IO_H
#define NET_IO_H

#include <glib.h>
#include <gtk/gtk.h>
#include <string>

bool net_io_download_file(GtkWidget *parent,
                          const std::string &url, const std::string &filename, const char *title);
bool net_io_download_mem(GtkWidget *parent,
                         const std::string &url, char **mem, size_t &len);

/**
 * @brief translate HTTP status code to string
 * @param id the HTTP status code
 */
const char *http_message(int id);

#endif // NET_IO_H
