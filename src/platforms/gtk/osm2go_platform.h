/*
 * Copyright (C) 2017 Rolf Eike Beer <eike@sf-mail.de>.
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

#ifndef OSM2GO_PLATFORM_H
#define OSM2GO_PLATFORM_H

#include <gtk/gtk.h>

namespace osm2go_platform {
  /**
   * @brief process all pending GUI events
   * @param tick if a '.' should be printed for every iteration
   */
  void process_events(bool tick = false);

  int init(int &argc, char **argv);

  void cleanup();

  /**
   * @brief simple interface to the systems web browser
   */
  void open_url(const char *url);

  class Timer {
    guint id;
  public:
    explicit inline Timer()
      : id(0) {}
    inline ~Timer()
    { stop(); }

    void restart(unsigned int seconds, GSourceFunc callback, void *data);
    void stop();

    inline bool isActive() const
    { return id != 0; }
  };
};

#endif // OSM2GO_PLATFORM_H
