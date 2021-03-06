/*
 * SPDX-FileCopyrightText: 2017-2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

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

#include <optional>
#include <string>
#include <vector>

#include <osm2go_i18n.h>

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
  std::optional<color_t> parse_color_string(const char *str) __attribute__((nonnull(1)));

  /**
   * @brief converts a character string to a double in local-unaware fashion
   * @param str the string to parse
   * @returns the parsed value or NAN if str == nullptr
   */
  double string_to_double(const char *str) __attribute__((warn_unused_result));

  /**
   * @brief a dialog asking for yes or no
   * @retval true the user clicked yes
   */
  bool yes_no(trstring::arg_type title, trstring::arg_type msg,
              unsigned int again_flags = 0, Widget *parent = nullptr) __attribute__((warn_unused_result));

  const std::vector<dirguard> &base_paths() __attribute__((warn_unused_result));

  /**
   * @brief return the path where the user may store custom presets
   */
  dirguard userdatapath() __attribute__((warn_unused_result));

  /**
   * @brief create the given directory and all missing intermediate directories
   */
  bool create_directories(const std::string &path) __attribute__((warn_unused_result));
};
