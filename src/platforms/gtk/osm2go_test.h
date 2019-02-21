#pragma once

#include <canvas.h>
#include "osm2go_platform_gtk.h"

#include <glib.h>
#include <glib-object.h>
#include <memory>

class canvas_t;
extern canvas_t *canvas_t_create();

#if !GLIB_CHECK_VERSION(2,36,0)
#define OSM2GO_TEST_INIT(argc, argv) g_type_init();(void)argc;(void)argv
#else
#define OSM2GO_TEST_INIT(argc, argv) (void)argc;(void)argv
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
};
