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

#ifndef ICON_H
#define ICON_H

#include <algorithm>
#include <gtk/gtk.h>
#include <map>
#include <string>

#include <osm2go_cpp.h>

class icon_t {
public:
  class icon_item {
    friend class icon_t;

    GdkPixbuf *buf;
    int use;
    explicit icon_item(GdkPixbuf *nbuf = O2G_NULLPTR);
  public:
    ~icon_item();

    inline bool operator==(const GdkPixbuf *b) const { return buf == b; }
    inline bool operator==(const icon_item &other) const { return buf == other.buf; }

    inline GdkPixbuf *buffer() {
      return buf;
    }

    int width() const;
    int height() const;
    inline int maxDimension() const {
      return std::max(width(), height());
    }
  };

  ~icon_t();

  /**
   * @brief load an icon from disk, limited to given dimensions
   * @param sname the name of the icon
   * @param limit the maximum dimensions of the image
   * @return the scaled pixbuf
   *
   * The image is only scaled down to the given dimensions, not enlarged.
   * The limit is only applied if the icon is not already cached.
   */
  icon_item *load(const std::string &sname, int limit = -1);

  GtkWidget *widget_load(const std::string &name, int limit = -1);

  void icon_free(icon_item *buf);

private:
  std::map<std::string, icon_item *> entries;
};

#endif // ICON_H
