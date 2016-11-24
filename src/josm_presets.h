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

#ifndef JOSM_PRESETS_H
#define JOSM_PRESETS_H

#include "info.h"

/* the presets type specifies which item type it is */
/* appropriate for */
#define PRESETS_TYPE_WAY       (1<<0)
#define PRESETS_TYPE_NODE      (1<<1)
#define PRESETS_TYPE_RELATION  (1<<2)
#define PRESETS_TYPE_CLOSEDWAY (1<<3)
#define PRESETS_TYPE_ALL       (0xffff)

struct presets_items;

#ifdef __cplusplus
extern "C" {
#endif

struct presets_items *josm_presets_load(void);
GtkWidget *josm_build_presets_button(appdata_t *appdata, tag_context_t *tag_context);
void josm_presets_free(struct presets_items *presets);
char *josm_icon_name_adjust(char *name);

#ifdef __cplusplus
}
#endif

#endif // JOSM_PRESETS_H
