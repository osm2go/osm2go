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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef UICONTROL_H
#define UICONTROL_H

class statusbar_t;
class trstring;

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

  virtual void setActionEnable(menu_items item, bool en) = 0;

  /**
   * @brief show a non-dialog notification message to the user
   * @param message the text to show
   * @param flags flags to control notification behavior
   *
   * message may be nullptr to clear the current message.
   */
  virtual void showNotification(const char *message, unsigned int flags = NoFlags);

  /**
   * @overload
   */
  void showNotification(const trstring &message, unsigned int flags = NoFlags);

  /**
   * @brief show a modal about box
   */
  void about_box();
};

#endif /* UICONTROL_H */
