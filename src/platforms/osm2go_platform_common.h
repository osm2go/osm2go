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
};

#endif // OSM2GO_PLATFORM_COMMON_H
