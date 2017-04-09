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

#include "dbus.h"

#include <libosso.h>
#include <dbus/dbus-glib-lowlevel.h>

#define MM_DBUS_SERVICE "com.gnuite.maemo_mapper"
#define MM_DBUS_PATH "/com/gnuite/maemo_mapper"
#define MM_DBUS_INTERFACE "com.gnuite.maemo_mapper"

#include <glib.h>
#include <dbus/dbus-glib.h>

static DBusHandlerResult
signal_filter(G_GNUC_UNUSED DBusConnection *connection, DBusMessage *message, void *user_data) {
  /* User data is the place to store the received position */
  dbus_mm_pos_t *mmpos = user_data;

  if(dbus_message_is_signal(message, MM_DBUS_SERVICE, "view_position_changed")) {
    DBusError error;
    double lat, lon;
    long zoom;
    dbus_error_init(&error);

    if(dbus_message_get_args(message, &error,
			     DBUS_TYPE_DOUBLE, &lat,
			     DBUS_TYPE_DOUBLE, &lon,
			     DBUS_TYPE_INT32,  &zoom,
			     DBUS_TYPE_INVALID)) {

      g_print("MM: position received: %f/%f, zoom = %ld\n",
	      (float)lat, (float)lon, zoom);

      /* store position for further processing */
      mmpos->pos.lat = lat;
      mmpos->pos.lon = lon;
      mmpos->zoom = zoom;
      mmpos->valid = TRUE;

    } else {
      g_print("  Error getting message: %s\n", error.message);
      dbus_error_free (&error);
    }
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* only the screen is refreshed, useful if e.g. the poi database changed */
gboolean dbus_mm_set_position(osso_context_t *osso_context) {
  osso_rpc_t retval;
  osso_return_t ret;

  ret = osso_rpc_run(osso_context,
		     MM_DBUS_SERVICE,
		     MM_DBUS_PATH,
		     MM_DBUS_INTERFACE,
		     "set_view_center",
		     &retval,
		     DBUS_TYPE_INVALID);

  osso_rpc_free_val(&retval);

  return(ret == OSSO_OK);
}

void dbus_register(dbus_mm_pos_t *mmpos) {
  DBusConnection *bus;
  DBusError error;

  dbus_error_init (&error);
  bus = dbus_bus_get(DBUS_BUS_SESSION, &error);
  if(!bus) {
    g_warning("Failed to connect to the D-BUS daemon: %s", error.message);
    dbus_error_free(&error);
    return;
  }
  dbus_connection_setup_with_g_main(bus, NULL);

  /* listening to messages from all objects as no path is specified */
  dbus_bus_add_match(bus, "type='signal',interface='"MM_DBUS_INTERFACE"'", &error);
  dbus_connection_add_filter(bus, signal_filter, mmpos, NULL);
}
