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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef NET_IO_H
#define NET_IO_H

#include <curl/curl.h>
#include <string>

typedef struct _GtkWidget GtkWidget;

/**
 * @brief download from the given URL to file
 * @param parent widget for status messages
 * @param url the request URL
 * @param filename output filename
 * @param title window title string for the download window
 * @param compress if gzip compression of the data should be enabled
 *
 * @returns if the request was successful
 */
bool net_io_download_file(GtkWidget *parent,
                          const std::string &url, const std::string &filename,
                          const char *title, bool compress = false);
/**
 * @brief download from the given URL to memory
 * @param parent widget for status messages
 * @param url the request URL
 * @param mem where output will be stored
 * @param len length of the returned data
 * @returns if the request was successful
 *
 * The data is possibly gzip encoded.
 */
bool net_io_download_mem(GtkWidget *parent, const std::string &url,
                         char **mem, size_t &len, const char *title);

/**
 * @brief translate HTTP status code to string
 * @param id the HTTP status code
 */
const char *http_message(int id);

/**
 * @brief check if the given memory contains a valid gzip header
 */
bool check_gzip(const char *mem, const size_t len);

struct curl_deleter {
  inline void operator()(CURL *curl)
  { curl_easy_cleanup(curl); }
};

struct curl_slist_deleter {
  inline void operator()(curl_slist *slist)
  { curl_slist_free_all(slist); }
};

#endif // NET_IO_H
