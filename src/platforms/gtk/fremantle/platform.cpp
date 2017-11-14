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
 * along with OSM2Go. If not, see <http://www.gnu.org/licenses/>.
 */

#include <osm2go_platform.h>

#include "dbus.h"

#include <osm2go_cpp.h>

#include <hildon/hildon-picker-button.h>

int osm2go_platform::init(int &argc, char **argv)
{
#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init(O2G_NULLPTR);
#endif

  gtk_init(&argc, &argv);

  g_signal_new("changed", HILDON_TYPE_PICKER_BUTTON,
               G_SIGNAL_RUN_FIRST, 0, O2G_NULLPTR, O2G_NULLPTR,
               g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  return dbus_register() == TRUE ? 0 : 1;
}
