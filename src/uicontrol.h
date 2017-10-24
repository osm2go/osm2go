/*
 * Copyright (C) 2017 Rolf Eike Beer <eike@sf-mail.de>.
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

#ifndef UICONTROL_H
#define UICONTROL_H

struct appdata_t;

class MainUi {
protected:
  MainUi() {}
  ~MainUi() {}
public:
  static MainUi *instance(appdata_t &appdata);

  enum menu_items {
    MENU_ITEM_MAP_HIDE_SEL,
    MENU_ITEM_MAP_SHOW_ALL,
    MENU_ITEM_WMS_CLEAR,
    MENU_ITEM_WMS_ADJUST,
    MENU_ITEM_TRACK_EXPORT,
    MENU_ITEM_TRACK_CLEAR,
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
  #ifndef FREMANTLE
    MENU_ITEM_MAP_SAVE_CHANGES,
  #endif
    MENU_ITEMS_COUNT
  };

  enum NotificationFlags {
    NoFlags = 0,
    Brief = 1, ///< the message automatically disappears
    Highlight = 2,
    Busy = 4 ///< cleared by nullptr + Busy or when setting any other message
  };

  void setActionEnable(menu_items item, bool en);

  /**
   * @brief show a non-dialog notification message to the user
   * @param message the text to show
   * @param flags flags to control notification behavior
   *
   * message may be nullptr to clear the current message.
   */
  void showNotification(const char *message, unsigned int flags = NoFlags);
};

#endif /* UICONTROL_H */
