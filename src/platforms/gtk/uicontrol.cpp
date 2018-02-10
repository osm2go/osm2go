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

#include "MainUiGtk.h"

#include <appdata.h>
#include <statusbar.h>

#include <array>
#include <gtk/gtk.h>
#ifdef FREMANTLE
#include <hildon/hildon-button.h>
#include <string>
#endif

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>

static GtkWidget *
create_submenu_item(const char *label)
{
#ifdef FREMANTLE
  // remove mnemonic marker
  std::string hlabel = label;
  std::string::size_type _pos = hlabel.find('_');
  if(likely(_pos != std::string::npos))
    hlabel.erase(_pos, 1);
  return hildon_button_new_with_text(
                static_cast<HildonSizeType>(HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH),
                HILDON_BUTTON_ARRANGEMENT_VERTICAL, hlabel.c_str(), O2G_NULLPTR);
#else
  return gtk_menu_item_new_with_mnemonic(label);
#endif
}

MainUiGtk::MainUiGtk(appdata_t& a)
  : MainUi(a)
#ifdef FREMANTLE
  , menubar(HILDON_APP_MENU(hildon_app_menu_new()))
#else
  , menubar(GTK_MENU_SHELL(gtk_menu_bar_new()))
#endif
{
  // the TR1 header has assign() for what is later called fill()
#if __cplusplus >= 201103L
  menuitems.fill(O2G_NULLPTR);
#else
  menuitems.assign(O2G_NULLPTR);
#endif

  menuitems[SUBMENU_VIEW] = create_submenu_item(_("_View"));
#ifdef FREMANTLE
  menuitems[SUBMENU_MAP] = create_submenu_item(_("OSM"));
  menuitems[MENU_ITEM_MAP_RELATIONS] = create_submenu_item(_("Relations"));
#else
  menuitems[SUBMENU_MAP] = create_submenu_item(_("_Map"));
#endif
  menuitems[SUBMENU_WMS] = create_submenu_item(_("_WMS"));
  menuitems[SUBMENU_TRACK] = create_submenu_item(_("_Track"));
}

MainUi *MainUi::instance(appdata_t &appdata)
{
  static MainUiGtk inst(appdata);

  return &inst;
}

void MainUi::setActionEnable(menu_items item, bool en)
{
  gtk_widget_set_sensitive(static_cast<MainUiGtk *>(this)->menu_item(item), en ? TRUE : FALSE);
}

void MainUi::showNotification(const char *message, unsigned int flags)
{
  if (flags & Brief) {
    appdata.statusbar->banner_show_info(appdata, message);
  } else if (flags & Busy) {
    if (message == O2G_NULLPTR)
      appdata.statusbar->banner_busy_stop(appdata);
    else
      appdata.statusbar->banner_busy_start(appdata, message);
  } else {
    appdata.statusbar->set(message, flags & Highlight);
  }
}

void MainUiGtk::initItem(MainUi::menu_items item, GtkWidget *widget)
{
  assert_null(menuitems[item]);

  menuitems[item] = widget;
}

GtkWidget *MainUiGtk::addMenu(GtkWidget *item)
{
#ifdef FREMANTLE
  hildon_button_set_title_alignment(HILDON_BUTTON(item), 0.5, 0.5);
  hildon_button_set_value_alignment(HILDON_BUTTON(item), 0.5, 0.5);

  hildon_app_menu_append(menubar, GTK_BUTTON(item));

  return item;
#else
  gtk_menu_shell_append(menubar, item);
  GtkWidget *submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

  return submenu;
#endif
}

GtkWidget *MainUiGtk::addMenu(const char *label)
{
  return addMenu(create_submenu_item(label));
}

GtkWidget *MainUiGtk::addMenu(menu_items item)
{
  GtkWidget *widget = menu_item(item);
  assert(widget != O2G_NULLPTR);
  return addMenu(widget);
}
