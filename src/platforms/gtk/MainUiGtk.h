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

#pragma once

#include <uicontrol.h>

#include <array>
#include <memory>
#ifdef FREMANTLE
#include <hildon/hildon-app-menu.h>
typedef HildonAppMenu MenuBar;
#else
typedef struct _GtkMenuShell GtkMenuShell;
typedef GtkMenuShell MenuBar;
#endif

#include <osm2go_cpp.h>
#include "osm2go_i18n.h"
#include <osm2go_stl.h>

typedef struct _GtkWidget GtkWidget;

class MainUiGtk : public MainUi {
  std::array<GtkWidget *, MainUi::MENU_ITEMS_COUNT> menuitems;

  const std::unique_ptr<statusbar_t> statusbar;
  MenuBar * const menubar;

  GtkWidget *addMenu(GtkWidget *item);
public:
  MainUiGtk();
  virtual ~MainUiGtk() {}

  inline GtkWidget *menu_item(menu_items item)
  { return menuitems[item]; }

  inline MenuBar *menuBar()
  { return menubar; }

  void setActionEnable(menu_items item, bool en) override;

  /**
   * @brief create a new submenu entry in the global menu bar
   */
  GtkWidget *addMenu(trstring::native_type_arg label);

  /**
   * @brief add one of the predefined entries to the global menu bar
   */
  GtkWidget *addMenu(menu_items item);

  static GtkWidget *createMenuItem(trstring::native_type_arg label, const char *icon_name = nullptr);

  inline statusbar_t *statusBar() const
  { return statusbar.get(); }
};
