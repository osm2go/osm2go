#pragma once

#include <glib.h>
#include <glib-object.h>

#if !GLIB_CHECK_VERSION(2,36,0)
#define OSM2GO_TEST_INIT(argc, argv) g_type_init();(void)argc;(void)argv
#else
#define OSM2GO_TEST_INIT(argc, argv) (void)argc;(void)argv
#endif
