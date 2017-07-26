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

#include <glib.h>
#include <gtk/gtk.h>

#if defined(USE_HILDON) && (MAEMO_VERSION_MAJOR == 5)
#define FINGER_UI
#endif

struct appdata_t;
struct map_item_t;

struct iconbar_t {
  iconbar_t(appdata_t &appdata);

  GtkWidget * const toolbar;

  GtkWidget *info;
  GtkWidget *trash;
  GtkWidget *node_add;

#ifdef FINGER_UI
  GtkWidget * const menu;
#endif

  GtkWidget *way_add;
  GtkWidget *way_node_add;
  GtkWidget *way_cut;
  GtkWidget *way_reverse;

  GtkWidget *cancel;
  GtkWidget *ok;

  void map_item_selected(map_item_t *map_item);
  void map_cancel_ok(gboolean cancelv, gboolean okv);

  /**
   * @brief set enable state of edit buttons
   * @param idle if an operation is currently active
   * @param way_en if the operations affecting ways should be enabled
   *
   * If a user action is in progress, then disable all buttons that
   * cause an action to take place or interfere with the action.
   */
  void map_action_idle(gboolean idle, gboolean way_en);
};

GtkWidget *iconbar_new(appdata_t &appdata);

#ifdef FINGER_UI
void iconbar_register_buttons(appdata_t &appdata, GtkWidget *ok, GtkWidget *cancel);
#endif

#endif // ICONBAR_H
