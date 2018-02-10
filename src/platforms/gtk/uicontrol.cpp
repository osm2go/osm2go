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

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>

MainUiGtk::MainUiGtk(appdata_t& a)
  : MainUi(a)
{
  // the TR1 header has assign() for what is later called fill()
#if __cplusplus >= 201103L
  menuitems.fill(O2G_NULLPTR);
#else
  menuitems.assign(O2G_NULLPTR);
#endif
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
