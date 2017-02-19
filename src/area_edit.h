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

#ifndef AREA_EDIT_H
#define AREA_EDIT_H

#include "appdata.h"
#include "pos.h"

struct area_edit_t {
  explicit area_edit_t(appdata_t *a, pos_t *mi, pos_t *ma);
  appdata_t *appdata;
  GtkWidget *parent;   /* parent widget to be placed upon */
  pos_t *min, *max;    /* positions to work on */
};

bool area_edit(area_edit_t &area);

#endif // AREA_EDIT_H
