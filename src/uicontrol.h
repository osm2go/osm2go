/*
 * SPDX-FileCopyrightText: 2017 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

class statusbar_t;

#include <osm2go_i18n.h>

class MainUi {
protected:
  explicit inline MainUi() {}

public:
  virtual ~MainUi() {}

  enum menu_items {
    MENU_ITEM_MAP_HIDE_SEL,
    MENU_ITEM_MAP_SHOW_ALL,
    MENU_ITEM_WMS_CLEAR,
    MENU_ITEM_WMS_ADJUST,
    MENU_ITEM_TRACK_EXPORT,
    MENU_ITEM_TRACK_CLEAR,
    MENU_ITEM_TRACK_CLEAR_CURRENT,
    MENU_ITEM_TRACK_ENABLE_GPS,
    MENU_ITEM_TRACK_FOLLOW_GPS,
    SUBMENU_VIEW,
    SUBMENU_MAP,
    MENU_ITEM_MAP_RELATIONS,
    SUBMENU_WMS,
    SUBMENU_TRACK,
    MENU_ITEM_TRACK_IMPORT,
    MENU_ITEM_MAP_UPLOAD,
    MENU_ITEM_MAP_UNDO_CHANGES,
    MENU_ITEM_MAP_SHOW_CHANGES,
  #ifndef FREMANTLE
    MENU_ITEM_MAP_SAVE_CHANGES,
  #endif
    MENU_ITEMS_COUNT
  };

  enum NotificationFlags {
    NoFlags = 0,
    Brief = 1, ///< the message automatically disappears
    Highlight = 2,
    Busy = 4, ///< automatically cleared when setting any other message
    ClearNormal = 8, ///< clear non-busy messages
    ClearBoth = Busy | ClearNormal
  };

  virtual void setActionEnable(menu_items item, bool en) = 0;

  /**
   * @brief show a non-dialog notification message to the user
   * @param message the text to show, must not be empty
   * @param flags flags to control notification behavior
   */
  virtual void showNotification(trstring::arg_type message, unsigned int flags = NoFlags);

  /**
   * @brief clear the given type of messages
   */
  virtual void clearNotification(NotificationFlags flags) = 0;

  /**
   * @brief show a modal about box
   */
  void about_box();
};
