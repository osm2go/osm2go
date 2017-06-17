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

#ifndef ICONBAR_H
#define ICONBAR_H

#include "appdata.h"

#include <glib.h>
#include <gtk/gtk.h>

#if defined(USE_HILDON) && (MAEMO_VERSION_MAJOR == 5)
#define FINGER_UI
#endif

typedef struct iconbar_t {
  GtkWidget *toolbar;

#ifdef FINGER_UI
  GtkWidget *menu;
#endif

  GtkWidget *info;
  GtkWidget *trash;
  GtkWidget *node_add;
  GtkWidget *way_add;
  GtkWidget *way_node_add;
  GtkWidget *way_cut;
  GtkWidget *way_reverse;
  GtkWidget *relation_add;

  GtkWidget *cancel;
  GtkWidget *ok;
} iconbar_t;

#ifdef __cplusplus
extern "C" {
#endif

struct map_item_t;

GtkWidget *iconbar_new(appdata_t *appdata);
void icon_bar_map_item_selected(iconbar_t *iconbar, struct map_item_t *map_item);
void icon_bar_map_cancel_ok(iconbar_t *iconbar, gboolean cancel, gboolean ok);
/**
 * @brief set enable state of edit buttons
 * @param iconbar the iconbar to operate on
 * @param idle if an operation is currently active
 * @param way_en if the operations affecting ways should be enabled
 *
 * If a user action is in progress, then disable all buttons that
 * cause an action to take place or interfere with the action.
 */
void icon_bar_map_action_idle(iconbar_t *iconbar, gboolean idle, gboolean way_en);
void iconbar_free(iconbar_t *iconbar);

#ifdef FINGER_UI
void iconbar_register_buttons(appdata_t *appdata, GtkWidget *ok, GtkWidget *cancel);
#endif

#ifdef __cplusplus
}
#endif

#endif // ICONBAR_H
