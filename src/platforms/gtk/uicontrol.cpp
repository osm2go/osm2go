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

#include <uicontrol.h>

#include <appdata.h>
#include <statusbar.h>

#include <gtk/gtk.h>

#include <osm2go_cpp.h>

class MainUiGtk : public MainUi {
public:
  MainUiGtk(appdata_t &a) : MainUi(a) {}
};

MainUi *MainUi::instance(appdata_t &appdata)
{
  static MainUiGtk inst(appdata);

  return &inst;
}

void MainUi::setActionEnable(menu_items item, bool en)
{
  gtk_widget_set_sensitive(appdata.menuitems[item], en ? TRUE : FALSE);
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
