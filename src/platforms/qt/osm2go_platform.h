/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <color.h>

#include <QDialog>
#include <QFile>
#include <QPixmap>
#include <QPointer>
#include <QPointF>
#include <QTimer>
#include <QWidget>

class icon_item;
class presets_items;
class QGeoRectangle;
class QMenu;
class QString;
class QSvgRenderer;
struct pos_area;
class tag_context_t;

namespace osm2go_platform {
  typedef QWidget Widget;
  typedef QPointF screenpos;

  class MappedFile {
    QFile map;
    char *mem;
    size_t len;
  public:
    MappedFile(const std::string &fname);
    ~MappedFile();

    operator bool() const { return mem != nullptr; }

    const char *data() { return mem; }
    size_t length() { return len; }

    void reset();
  };

  QPixmap icon_pixmap(icon_item *icon);
  QSvgRenderer *icon_renderer(const icon_item *icon);
};

#include "../osm2go_platform_common.h"
