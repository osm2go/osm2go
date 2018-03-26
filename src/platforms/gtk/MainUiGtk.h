/*
 * Copyright (C) 2017,2018 Rolf Eike Beer <eike@sf-mail.de>.
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

#ifndef MAINUIGTK_H
#define MAINUIGTK_H

#include <uicontrol.h>

#include <array>
#ifdef FREMANTLE
#include <hildon/hildon-app-menu.h>
typedef HildonAppMenu MenuBar;
#else
typedef struct _GtkMenuShell GtkMenuShell;
typedef GtkMenuShell MenuBar;
#endif

#include <osm2go_cpp.h>

typedef struct _GtkWidget GtkWidget;

class MainUiGtk : public MainUi {
  std::array<GtkWidget *, MainUi::MENU_ITEMS_COUNT> menuitems;

  MenuBar * const menubar;

  GtkWidget *addMenu(GtkWidget *item);
public:
  MainUiGtk(statusbar_t *s);
  virtual ~MainUiGtk() {}

  inline GtkWidget *menu_item(menu_items item)
  { return menuitems[item]; }

  inline MenuBar *menuBar()
  { return menubar; }

  virtual void setActionEnable(menu_items item, bool en) override;

  /**
   * @brief create a new submenu entry in the global menu bar
   */
  GtkWidget *addMenu(const char *label);

  /**
   * @brief add one of the predefined entries to the global menu bar
   */
  GtkWidget *addMenu(menu_items item);

  static GtkWidget *createMenuItem(const char *label, const char *icon_name = nullptr) __attribute__((nonnull(1)));
};

#endif /* MAINUIGTK_H */
