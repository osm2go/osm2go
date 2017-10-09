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

#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <gtk/gtk.h>

#define STATUSBAR_DEFAULT_BRIEF_TIME 3

class statusbar_t {
protected:
  statusbar_t();
public:
  static statusbar_t *create();

  GtkWidget * const widget;

#ifndef FREMANTLE
  /**
   * @brief flash up a brief, temporary message.
   * @param msg the message to show
   * @param timeout the timeout in seconds
   *
   * Once the message disappears, drop back to any persistent message set
   * with set().
   *
   * If @msg is nullptr, clear the message and don't establish a handler.
   *
   * If timeout is negative, don't establish a handler. You'll have to clear it
   * yourself later. If it's zero, use the default.
   */
  void brief(const char *msg, int timeout);
#endif

  /**
   * @brief set the persistent message, replacing anything currently there
   */
  void set(const char *msg, bool highlight);
};

#endif // STATUSBAR_H
