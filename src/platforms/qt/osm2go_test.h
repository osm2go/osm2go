/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <canvas.h>

#include <QApplication>
#include <QTimer>

extern canvas_t *canvas_t_create();

#define OSM2GO_TEST_INIT(argc, argv) \
  QApplication app(argc, argv); \
  QCoreApplication::setApplicationName("osm2go")

#define OSM2GO_TEST_CODE(x) \
  QTimer::singleShot(0, [&](){ \
    x; \
    QCoreApplication::exit(0); \
  }); \
  app.exec();

class canvas_holder {
  canvas_t * const c;
public:
  explicit canvas_holder()
    : c(canvas_t_create())
  {
    assert(c != nullptr);
  }

  ~canvas_holder()
  {
    delete c->widget;
  }

  inline canvas_t *operator->() { return c; }
  inline canvas_t *operator*() { return c; }
};
