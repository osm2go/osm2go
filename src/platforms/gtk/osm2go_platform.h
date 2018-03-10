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

#include <glib.h>
#include <gtk/gtk.h>
#include <memory>

#include <osm2go_cpp.h>
#include <osm2go_stl.h>

namespace osm2go_platform {
  struct gtk_widget_deleter {
    inline void operator()(GtkWidget *mem) {
      gtk_widget_destroy(mem);
    }
  };
  typedef std::unique_ptr<GtkWidget, gtk_widget_deleter> WidgetGuard;

  /**
   * @brief process all pending GUI events
   * @param tick if a '.' should be printed for every iteration
   */
  void process_events(bool tick = false);

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

  class MappedFile {
    GMappedFile *map;
  public:
    explicit inline MappedFile(const char *fname)
      : map(g_mapped_file_new(fname, FALSE, O2G_NULLPTR)) {}
    inline ~MappedFile()
    { reset(); }

    inline operator bool() const
    { return map != O2G_NULLPTR; }

    inline const char *data()
    { return g_mapped_file_get_contents(map); }

    inline size_t length()
    { return g_mapped_file_get_length(map); }

    void reset();
  };
};

#endif // OSM2GO_PLATFORM_H
