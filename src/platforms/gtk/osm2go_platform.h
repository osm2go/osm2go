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

#include <memory>

#include <osm2go_cpp.h>
#include <osm2go_stl.h>

typedef struct _GMappedFile GMappedFile;
typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GtkWidget GtkWidget;

namespace osm2go_platform {
  typedef GtkWidget Widget;
  typedef ::GdkPixbuf *Pixmap;

  struct gtk_widget_deleter {
    void operator()(GtkWidget *mem);
  };
  typedef std::unique_ptr<GtkWidget, gtk_widget_deleter> WidgetGuard;

  class MappedFile {
    GMappedFile *map;
  public:
    explicit MappedFile(const char *fname);
    inline ~MappedFile()
    { reset(); }

    inline operator bool() const
    { return map != nullptr; }

    const char *data();

    size_t length();

    void reset();
  };
};

#include "../osm2go_platform_common.h"

#endif // OSM2GO_PLATFORM_H
