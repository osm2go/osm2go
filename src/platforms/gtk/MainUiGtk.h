/*
 * SPDX-FileCopyrightText: 2017,2018 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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

  void clearNotification(NotificationFlags flags) override;

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
