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

#ifndef STATUSBAR_H
#define STATUSBAR_H

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

#endif // STATUSBAR_H
