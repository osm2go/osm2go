/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifdef FREMANTLE
#define FINGER_UI
#endif

#include <osm2go_platform.h>

struct appdata_t;
class object_t;

class iconbar_t {
protected:
  iconbar_t() {}

public:
  static osm2go_platform::Widget *create(appdata_t &appdata);

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
void iconbar_register_buttons(appdata_t &appdata, osm2go_platform::Widget *ok, osm2go_platform::Widget *cancel);
#endif
