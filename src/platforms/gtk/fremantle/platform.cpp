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
 * along with OSM2Go. If not, see <https://www.gnu.org/licenses/>.
 */

#include <osm2go_platform.h>

#include "dbus.h"

#include <osm2go_cpp.h>

#include <hildon/hildon-picker-button.h>
#include <libosso.h>
#include <tablet-browser-interface.h>

static osso_context_t *osso_context;

int osm2go_platform::init(int &argc, char **argv)
{
#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init(O2G_NULLPTR);
#endif

  gtk_init(&argc, &argv);

  g_signal_new("changed", HILDON_TYPE_PICKER_BUTTON,
               G_SIGNAL_RUN_FIRST, 0, O2G_NULLPTR, O2G_NULLPTR,
               g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  osso_context = osso_initialize("org.harbaum." PACKAGE, VERSION, TRUE, O2G_NULLPTR);

  if(G_UNLIKELY(osso_context == O2G_NULLPTR))
    return 1;

  if(G_UNLIKELY(dbus_register(osso_context) != TRUE)) {
    osso_deinitialize(osso_context);
    return 1;
  } else {
    return 0;
  }
}

void osm2go_platform::cleanup()
{
  osso_deinitialize(osso_context);
}

void osm2go_platform::open_url(const char* url)
{
  osso_rpc_run_with_defaults(osso_context, "osso_browser",
                             OSSO_BROWSER_OPEN_NEW_WINDOW_REQ, O2G_NULLPTR,
                             DBUS_TYPE_STRING, url,
                             DBUS_TYPE_BOOLEAN, FALSE, DBUS_TYPE_INVALID);
}
