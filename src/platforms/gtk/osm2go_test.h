/*
 * SPDX-FileCopyrightText: 2018-2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <canvas.h>
#include "osm2go_platform_gtk.h"

#include <glib.h>
#include <glib-object.h>
#include <memory>

class canvas_t;
extern canvas_t *canvas_t_create();

extern bool use_test_paths_only;

#if !GLIB_CHECK_VERSION(2,36,0)
#define OSM2GO_TEST_INIT(argc, argv) g_type_init();(void)argc;(void)argv;use_test_paths_only = true
#else
#define OSM2GO_TEST_INIT(argc, argv) (void)argc;(void)argv;use_test_paths_only = true
#endif

class canvas_holder {
  canvas_t * const c;
  std::unique_ptr<GtkWidget, g_object_deleter> w;
public:
  explicit canvas_holder()
    : c(canvas_t_create())
    , w(static_cast<GtkWidget *>(g_object_ref_sink(c->widget)))
  {
  }

  inline canvas_t *operator->() { return c; }
  inline canvas_t *operator*() { return c; }
};
