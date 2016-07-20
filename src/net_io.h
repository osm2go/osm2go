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

#include "settings.h"

#include <glib.h>
#include <gtk/gtk.h>

gboolean net_io_download_file(GtkWidget *parent, settings_t *settings,
			      char *url, char *filename, const char *title);
gboolean net_io_download_mem(GtkWidget *parent, settings_t *settings,
			     char *url, char **mem);

#include <curl/curl.h>
void net_io_set_proxy(CURL *curl, proxy_t *proxy);

#endif // NET_IO_H
