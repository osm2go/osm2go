/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <curl/curl.h>
#include <string>

#include <osm2go_i18n.h>
#include <osm2go_platform.h>

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
bool net_io_download_file(osm2go_platform::Widget *parent,
                          const std::string &url, const std::string &filename,
                          const std::string &title, bool compress = false);

/**
 * @overload
 */
bool net_io_download_file(osm2go_platform::Widget *parent,
                          const std::string &url, const std::string &filename,
                          trstring::native_type_arg title, bool compress = false);

/**
 * @brief download from the given URL to memory
 * @param parent widget for status messages
 * @param url the request URL
 * @param data where output will be stored
 * @returns if the request was successful
 *
 * The data is possibly gzip encoded.
 */
bool net_io_download_mem(osm2go_platform::Widget *parent, const std::string &url,
                         std::string &data, trstring::native_type_arg title);

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
