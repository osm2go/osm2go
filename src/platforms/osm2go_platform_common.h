/*
 * Copyright (C) 2017-2018 Rolf Eike Beer <eike@sf-mail.de>.
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

#ifndef OSM2GO_PLATFORM_COMMON_H
#define OSM2GO_PLATFORM_COMMON_H

class color_t;
class trstring;

// not in the namespace to keep the idenfiers short
enum {
  MISC_AGAIN_ID_DELETE           = (1<<0),
  MISC_AGAIN_ID_JOIN_NODES       = (1<<1),
  MISC_AGAIN_ID_JOIN_WAYS        = (1<<2),
  MISC_AGAIN_ID_OVERWRITE_TAGS   = (1<<3),
  MISC_AGAIN_ID_EXTEND_WAY       = (1<<4),
  MISC_AGAIN_ID_EXTEND_WAY_END   = (1<<5),
  MISC_AGAIN_ID_EXPORT_OVERWRITE = (1<<6),
  MISC_AGAIN_ID_AREA_TOO_BIG     = (1<<7),

  /* these flags prevent you from leaving the dialog with no (or yes) */
  /* if the "dont show me this dialog again" checkbox is selected. This */
  /* makes sure, that you can't permanently switch certain things in, but */
  /* only on. e.g. it doesn't make sense to answer a "do you really want to */
  /* delete this" dialog with "no and don't ask me again". You'd never be */
  /* able to delete anything again */
  MISC_AGAIN_FLAG_DONT_SAVE_NO   = (1<<30),
  MISC_AGAIN_FLAG_DONT_SAVE_YES  = (1<<31)
};

#include <fdguard.h>

#include <string>
#include <vector>

namespace osm2go_platform {
  /**
   * @brief process all pending GUI events
   */
  void process_events();

  /**
   * @brief simple interface to the systems web browser
   */
  void open_url(const char *url);

  /**
   * @brief parses a string representation of a color value using
   * @param str the string to parse
   * @param color the color object to update
   * @returns if the given string is a valid color
   *
   * The string is expected to begin with a '#'.
   */
  bool parse_color_string(const char *str, color_t &color) __attribute__((nonnull(1)));

  /**
   * @brief converts a character string to a double in local-unaware fashion
   * @param str the string to parse
   * @returns the parsed value or NAN if str == nullptr
   */
  double string_to_double(const char *str);

  /**
   * @brief a dialog asking for yes or no
   * @retval true the user clicked yes
   */
  bool yes_no(const char *title, const char *msg,
              unsigned int again_flags = 0,Widget *parent = nullptr);
  bool yes_no(const char *title, const trstring &msg,
              unsigned int again_flags = 0, Widget *parent = nullptr);
  bool yes_no(const trstring &title, const trstring &msg,
              unsigned int again_flags = 0, Widget *parent = nullptr);

  struct datapath {
#if __cplusplus >= 201103L
    explicit inline datapath(fdguard &&f)  : fd(std::move(f)) {}
#else
    explicit inline datapath(fdguard &f)  : fd(f) {}
#endif
    fdguard fd;
    std::string pathname;
  };

  const std::vector<datapath> &base_paths();

  bool create_directories(const std::string &path);
};

#endif // OSM2GO_PLATFORM_COMMON_H
