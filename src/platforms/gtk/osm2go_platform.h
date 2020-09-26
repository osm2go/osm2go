/*
 * SPDX-FileCopyrightText: 2017-2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>

#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
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
    GtkBox *vbox() __attribute__((warn_unused_result));
  };

  class MappedFile {
    GMappedFile *map;
  public:
    explicit MappedFile(const std::string &fname);
    inline ~MappedFile()
    { reset(); }

    inline operator bool() const noexcept
    { return map != nullptr; }

    const char *data() __attribute__((warn_unused_result));

    size_t length() __attribute__((warn_unused_result));

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
