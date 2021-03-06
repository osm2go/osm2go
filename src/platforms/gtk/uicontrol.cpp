/*
 * SPDX-FileCopyrightText: 2017 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "MainUiGtk.h"

#include <appdata.h>
#include <icon.h>
#include <statusbar.h>

#include <array>
#include <gtk/gtk.h>
#ifdef FREMANTLE
#include <hildon/hildon-button.h>
#include <hildon/hildon-check-button.h>
#include <string>
#endif

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include "osm2go_platform_gtk_icon.h"

// declared here so it is available for all lib users (i.e. testcases)
osm2go_platform::Widget *appdata_t::window;

namespace {

#ifdef FREMANTLE
std::string
strip_mnemonic(trstring::native_type_arg label)
{
  // remove mnemonic marker
  assert(!label.isEmpty());
  std::string hlabel = label.toStdString();
  std::string::size_type _pos = hlabel.find('_');
  if(likely(_pos != std::string::npos))
    hlabel.erase(_pos, 1);
  return hlabel;
}
#endif

GtkWidget *
create_submenu_item(trstring::native_type_arg label)
{
#ifdef FREMANTLE
  return hildon_button_new_with_text(
                static_cast<HildonSizeType>(HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH),
                HILDON_BUTTON_ARRANGEMENT_VERTICAL, strip_mnemonic(label).c_str(), nullptr);
#else
  return gtk_menu_item_new_with_mnemonic(static_cast<const gchar *>(label));
#endif
}

GtkWidget *
create_checkbox_item(trstring::native_type_arg label)
{
#ifdef FREMANTLE
  GtkWidget *button = hildon_check_button_new(HILDON_SIZE_AUTO);
  gtk_button_set_label(GTK_BUTTON(button), strip_mnemonic(label).c_str());
  return button;
#else
  return gtk_check_menu_item_new_with_mnemonic(static_cast<const gchar *>(label));
#endif
}

} // namespace

GtkWidget *
MainUiGtk::createMenuItem(trstring::native_type_arg label, const char *icon_name
#ifdef FREMANTLE
                          __attribute__((unused))
#endif
                          )
{
  assert(!label.isEmpty());
#ifndef FREMANTLE
  // Icons
  if(icon_name != nullptr) {
    GtkWidget *image = gtk_platform_icon_t::instance().widget_load(icon_name);
    if (image == nullptr)
      image = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
    assert(image != nullptr);
    GtkWidget *item = gtk_image_menu_item_new_with_mnemonic(static_cast<const gchar *>(label));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    return item;
  }
#endif
  return create_submenu_item(label);
}

MainUiGtk::MainUiGtk()
  : MainUi()
  , statusbar(statusbar_t::create())
#ifdef FREMANTLE
  , menubar(HILDON_APP_MENU(hildon_app_menu_new()))
#else
  , menubar(GTK_MENU_SHELL(gtk_menu_bar_new()))
#endif
{
  menuitems[MENU_ITEM_MAP_HIDE_SEL] = createMenuItem(_("_Hide selected"), "list-remove");
  menuitems[MENU_ITEM_MAP_SHOW_ALL] = createMenuItem(_("_Show all"), "list-add");
  menuitems[MENU_ITEM_WMS_CLEAR] = createMenuItem(_("_Clear"), "edit-clear");
  menuitems[MENU_ITEM_WMS_ADJUST] = createMenuItem(_("_Adjust"));
  menuitems[SUBMENU_VIEW] = create_submenu_item(_("_View"));
  menuitems[MENU_ITEM_TRACK_EXPORT] = createMenuItem(_("_Export"));
  menuitems[MENU_ITEM_TRACK_CLEAR] = createMenuItem(_("_Clear"), "edit-clear");
  menuitems[MENU_ITEM_TRACK_CLEAR_CURRENT] = createMenuItem(_("Clear c_urrent segment"), "edit-clear");
  menuitems[MENU_ITEM_TRACK_ENABLE_GPS] = create_checkbox_item(_("_GPS enable"));
  menuitems[MENU_ITEM_TRACK_FOLLOW_GPS] = create_checkbox_item(_("GPS follow"));
#ifdef FREMANTLE
  menuitems[SUBMENU_MAP] = create_submenu_item(_("OSM"));
#else
  menuitems[SUBMENU_MAP] = create_submenu_item(_("_Map"));
#endif
  menuitems[MENU_ITEM_MAP_RELATIONS] = createMenuItem(_("_Relations"));
  menuitems[SUBMENU_WMS] = create_submenu_item(_("_WMS"));
  menuitems[SUBMENU_TRACK] = create_submenu_item(_("_Track"));
  menuitems[MENU_ITEM_TRACK_IMPORT] = createMenuItem(_("_Import"));
  menuitems[MENU_ITEM_MAP_UPLOAD] = createMenuItem(_("_Upload"), "upload.16");
  menuitems[MENU_ITEM_MAP_UNDO_CHANGES] = createMenuItem(_("Undo _all"), "edit-delete");
  menuitems[MENU_ITEM_MAP_SHOW_CHANGES] = createMenuItem(_("Show _changes"));
#ifndef FREMANTLE
  menuitems[MENU_ITEM_MAP_SAVE_CHANGES] = createMenuItem(_("_Save local changes"), "document-save");
#endif
}

void MainUiGtk::setActionEnable(menu_items item, bool en)
{
  gtk_widget_set_sensitive(menu_item(item), en ? TRUE : FALSE);
}

void MainUi::showNotification(trstring::arg_type message, unsigned int flags)
{
  statusbar_t *statusbar = static_cast<MainUiGtk *>(this)->statusBar();
  assert(!message.isEmpty());
  trstring::native_type nativeMsg = static_cast<trstring::native_type>(message);
  if (flags & Brief)
    statusbar->banner_show_info(static_cast<const char *>(nativeMsg));
  else if (flags & Busy)
    statusbar->banner_busy_start(static_cast<const char *>(nativeMsg));
  else
    statusbar->set(static_cast<const char *>(nativeMsg), flags & Highlight);
}

void MainUiGtk::clearNotification(NotificationFlags flags)
{
  statusbar_t *sbar = statusBar();
  if (flags & Busy)
    sbar->banner_busy_stop();
  if (flags & ClearNormal)
    sbar->set(nullptr, false);
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

GtkWidget *MainUiGtk::addMenu(trstring::native_type_arg label)
{
  return addMenu(create_submenu_item(label));
}

GtkWidget *MainUiGtk::addMenu(menu_items item)
{
  GtkWidget *widget = menu_item(item);
  assert(widget != nullptr);
  return addMenu(widget);
}
