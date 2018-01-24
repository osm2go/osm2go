/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
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

#include <osm2go_platform.h>
#include <osm2go_platform_gtk.h>

#include "dbus.h"

#include <osm2go_cpp.h>

#include <hildon/hildon-picker-button.h>
#include <libosso.h>
#include <tablet-browser-interface.h>

static osso_context_t *osso_context;

bool osm2go_platform::init()
{
  g_signal_new("changed", HILDON_TYPE_PICKER_BUTTON,
               G_SIGNAL_RUN_FIRST, 0, O2G_NULLPTR, O2G_NULLPTR,
               g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  osso_context = osso_initialize("org.harbaum." PACKAGE, VERSION, TRUE, O2G_NULLPTR);

  if(G_UNLIKELY(osso_context == O2G_NULLPTR))
    return false;

  if(G_UNLIKELY(dbus_register(osso_context) != TRUE)) {
    osso_deinitialize(osso_context);
    return false;
  } else {
    return true;
  }
}

void osm2go_platform::cleanup()
{
  osso_deinitialize(osso_context);
}

void osm2go_platform::open_url(const char* url)
{
  osso_rpc_run_with_defaults(osso_context, "osso_browser",
                             OSSO_BROWSER_OPEN_NEW_WINDOW_REQ, O2G_NULLPTR,
                             DBUS_TYPE_STRING, url,
                             DBUS_TYPE_BOOLEAN, FALSE, DBUS_TYPE_INVALID);
}

GtkWidget *osm2go_platform::notebook_new(void) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  GtkWidget *notebook =  gtk_notebook_new();

  /* solution for fremantle: we use a row of ordinary buttons instead */
  /* of regular tabs */

  /* hide the regular tabs */
  gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);

  gtk_box_pack_start_defaults(GTK_BOX(vbox), notebook);

  /* store a reference to the notebook in the vbox */
  g_object_set_data(G_OBJECT(vbox), "notebook", notebook);

  /* create a hbox for the buttons */
  GtkWidget *hbox = gtk_hbox_new(TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  g_object_set_data(G_OBJECT(vbox), "hbox", hbox);

  return vbox;
}

GtkNotebook *osm2go_platform::notebook_get_gtk_notebook(GtkWidget *notebook) {
  return GTK_NOTEBOOK(g_object_get_data(G_OBJECT(notebook), "notebook"));
}

static void on_notebook_button_clicked(GtkWidget *button, gpointer data) {
  GtkNotebook *nb = GTK_NOTEBOOK(g_object_get_data(G_OBJECT(data), "notebook"));

  gint page = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "page"));
  gtk_notebook_set_current_page(nb, page);
}

void osm2go_platform::notebook_append_page(GtkWidget *notebook, GtkWidget *page, const char *label) {
  GtkNotebook *nb = notebook_get_gtk_notebook(notebook);
  gint page_num = gtk_notebook_append_page(nb, page, gtk_label_new(label));

  GtkWidget *button;

  /* select button for page 0 by default */
  if(!page_num) {
    button = gtk_radio_button_new_with_label(O2G_NULLPTR, label);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    g_object_set_data(G_OBJECT(notebook), "group_master", button);
  } else {
    gpointer master = g_object_get_data(G_OBJECT(notebook), "group_master");
    button = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(master), label);
  }

  gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
  g_object_set_data(G_OBJECT(button), "page", GINT_TO_POINTER(page_num));

  g_signal_connect(button, "clicked", G_CALLBACK(on_notebook_button_clicked), notebook);

  hildon_gtk_widget_set_theme_size(button,
                                   static_cast<HildonSizeType>(HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));

  gtk_box_pack_start_defaults(GTK_BOX(g_object_get_data(G_OBJECT(notebook), "hbox")), button);
}

GtkTreeView *osm2go_platform::tree_view_new()
{
  return GTK_TREE_VIEW(hildon_gtk_tree_view_new(HILDON_UI_MODE_EDIT));
}
