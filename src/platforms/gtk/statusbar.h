/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#define STATUSBAR_DEFAULT_BRIEF_TIME 3

class statusbar_t {
protected:
  statusbar_t() {}
public:
  virtual ~statusbar_t() {}
  static statusbar_t *create();

  /**
   * @brief set the persistent message, replacing anything currently there
   */
  virtual void set(const char *msg, bool highlight) = 0;

  // Shows a brief info splash in a suitable way for the app environment being used
  virtual void banner_show_info(const char *text) = 0;

  // Start, stop, and say "I'm still alive" to a busy message targetted at the
  // app environment in use. This can be an animation for some builds, might be
  // a static statusbar for others, a modal dialog for others.
  virtual void banner_busy_start(const char *text) = 0;
  virtual void banner_busy_stop() = 0;
};
