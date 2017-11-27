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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ICONBAR_H
#define ICONBAR_H

#include <gtk/gtk.h>

#ifdef FREMANTLE
#define FINGER_UI
#endif

struct appdata_t;
struct object_t;

class iconbar_t {
protected:
  iconbar_t() {}

public:
  static GtkWidget *create(appdata_t &appdata);

  void map_item_selected(const object_t &item);
  void map_cancel_ok(bool cancel, bool ok);

  /**
   * @brief set enable state of edit buttons
   * @param idle if an operation is currently active
   * @param selected if the operations affecting ways should be enabled
   *
   * If a user action is in progress, then disable all buttons that
   * cause an action to take place or interfere with the action.
   */
  void map_action_idle(bool idle, const object_t &selected);

  void setToolbarEnable(bool en);

  bool isCancelEnabled() const;
  bool isInfoEnabled() const;
  bool isOkEnabled() const;
  bool isTrashEnabled() const;
};

#ifdef FINGER_UI
void iconbar_register_buttons(appdata_t &appdata, GtkWidget *ok, GtkWidget *cancel);
#endif

#endif // ICONBAR_H
