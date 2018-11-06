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

#pragma once

#include <memory>

#include <osm2go_cpp.h>
#include <osm2go_stl.h>

typedef struct _GMappedFile GMappedFile;
typedef struct _GtkBox GtkBox;
typedef struct _GtkDialog GtkDialog;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

namespace osm2go_platform {
  typedef GtkWidget Widget;

  struct gtk_widget_deleter {
    void operator()(GtkWidget *mem);
  };
  typedef std::unique_ptr<GtkWidget, gtk_widget_deleter> WidgetGuard;
  class DialogGuard : public WidgetGuard {
  public:
    explicit inline DialogGuard() : WidgetGuard() {};
    explicit DialogGuard(GtkWidget *dlg);
#if __cplusplus >= 201103L
    DialogGuard(std::nullptr_t) = delete;
    void reset(std::nullptr_t) = delete;
#endif
    void reset(GtkWidget *dlg);
    inline void reset() { WidgetGuard::reset(); }

    inline operator GtkWindow *() const
    { return reinterpret_cast<GtkWindow *>(get()); }
    inline operator GtkDialog *() const
    { return reinterpret_cast<GtkDialog *>(get()); }
    GtkBox *vbox();
  };

  class MappedFile {
    GMappedFile *map;
  public:
    explicit MappedFile(const char *fname);
    inline ~MappedFile()
    { reset(); }

    inline operator bool() const noexcept
    { return map != nullptr; }

    const char *data();

    size_t length();

    void reset();
  };

  class screenpos {
  public:
    typedef double value_type;
    inline screenpos(value_type px, value_type py) noexcept : m_x(px), m_y(py) {}
    inline value_type x() const noexcept { return m_x; }
    inline value_type y() const noexcept { return m_y; }

    screenpos operator-(const screenpos &other) const noexcept
    { return screenpos(x() - other.x(), y() - other.y()); }

    screenpos &operator+=(const screenpos &other) noexcept
    { m_x += other.x(); m_y +=other.y(); return *this; }
  private:
    value_type m_x, m_y;

  };
};

#include "../osm2go_platform_common.h"
