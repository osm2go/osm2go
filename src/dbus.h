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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HDR_DBUS_H
#define HDR_DBUS_H

typedef struct {
  pos_t pos;
  int zoom;
  gboolean valid;
} dbus_mm_pos_t;

struct appdata_s;
void dbus_register(struct appdata_s *appdata);
gboolean dbus_mm_set_position(osso_context_t *osso_context, pos_t *pos);

#endif // HDR_DBUS_H
