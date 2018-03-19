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

#include <hildon/hildon-check-button.h>
#include <hildon/hildon-entry.h>
#include <hildon/hildon-pannable-area.h>
#include <hildon/hildon-picker-button.h>
#include <hildon/hildon-touch-selector-entry.h>
#include <libosso.h>
#include <tablet-browser-interface.h>

static osso_context_t *osso_context;

bool osm2go_platform::init()
{
  g_signal_new("changed", HILDON_TYPE_PICKER_BUTTON,
               G_SIGNAL_RUN_FIRST, 0, nullptr, nullptr,
               g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  osso_context = osso_initialize("org.harbaum." PACKAGE, VERSION, TRUE, nullptr);

  if(G_UNLIKELY(osso_context == nullptr))
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
                             OSSO_BROWSER_OPEN_NEW_WINDOW_REQ, nullptr,
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
    button = gtk_radio_button_new_with_label(nullptr, label);
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

GtkWidget *osm2go_platform::scrollable_container(GtkWidget *view)
{
  /* put view into a pannable area */
  GtkWidget *container = hildon_pannable_area_new();
  gtk_container_add(GTK_CONTAINER(container), view);
  return container;
}

GtkWidget *osm2go_platform::entry_new(osm2go_platform::EntryFlags flags) {
  GtkWidget *ret = hildon_entry_new(HILDON_SIZE_AUTO);
  if(flags & EntryFlagsNoAutoCap)
    hildon_gtk_entry_set_input_mode(GTK_ENTRY(ret), HILDON_GTK_INPUT_MODE_FULL);
  return ret;
}

bool osm2go_platform::isEntryWidget(GtkWidget *widget)
{
  return G_OBJECT_TYPE(widget) == HILDON_TYPE_ENTRY;
}

GtkWidget *osm2go_platform::button_new_with_label(const char *label) {
  GtkWidget *button = gtk_button_new_with_label(label);
  hildon_gtk_widget_set_theme_size(button,
           static_cast<HildonSizeType>(HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));
  return button;
}

GtkWidget *osm2go_platform::check_button_new_with_label(const char *label) {
  GtkWidget *cbut =
    hildon_check_button_new(static_cast<HildonSizeType>(HILDON_SIZE_FINGER_HEIGHT |
                                                        HILDON_SIZE_AUTO_WIDTH));
  gtk_button_set_label(GTK_BUTTON(cbut), label);
  return cbut;
}

bool osm2go_platform::isCheckButtonWidget(GtkWidget *widget)
{
  return G_OBJECT_TYPE(widget) == HILDON_TYPE_CHECK_BUTTON;
}

void osm2go_platform::check_button_set_active(GtkWidget *button, bool active) {
  gboolean state = active ? TRUE : FALSE;
  hildon_check_button_set_active(HILDON_CHECK_BUTTON(button), state);
}

bool osm2go_platform::check_button_get_active(GtkWidget *button) {
  return hildon_check_button_get_active(HILDON_CHECK_BUTTON(button)) == TRUE;
}

static void on_value_changed(HildonPickerButton *widget) {
  g_signal_emit_by_name(widget, "changed");
}

static GtkWidget *combo_box_new_with_selector(const gchar *title, GtkWidget *selector) {
  GtkWidget *button =
    hildon_picker_button_new(static_cast<HildonSizeType>(HILDON_SIZE_FINGER_HEIGHT |
                                                         HILDON_SIZE_AUTO_WIDTH),
			     HILDON_BUTTON_ARRANGEMENT_VERTICAL);

  hildon_button_set_title_alignment(HILDON_BUTTON(button), 0.5, 0.5);
  hildon_button_set_value_alignment(HILDON_BUTTON(button), 0.5, 0.5);

  /* allow button to emit "changed" signal */
  g_signal_connect(button, "value-changed", G_CALLBACK(on_value_changed), nullptr);

  hildon_button_set_title(HILDON_BUTTON (button), title);

  hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(button),
				    HILDON_TOUCH_SELECTOR(selector));

  return button;
}

/* the title is only used on fremantle with the picker widget */
GtkWidget *osm2go_platform::combo_box_new(const char *title) {
  GtkWidget *selector = hildon_touch_selector_new_text();
  return combo_box_new_with_selector(title, selector);
}

GtkWidget *osm2go_platform::combo_box_entry_new(const char *title) {
  GtkWidget *selector = hildon_touch_selector_entry_new_text();
  return combo_box_new_with_selector(title, selector);
}

void osm2go_platform::combo_box_append_text(GtkWidget *cbox, const char *text) {
  HildonTouchSelector *selector = hildon_picker_button_get_selector(HILDON_PICKER_BUTTON(cbox));

  hildon_touch_selector_append_text(selector, text);
}

void osm2go_platform::combo_box_set_active(GtkWidget *cbox, int index) {
  hildon_picker_button_set_active(HILDON_PICKER_BUTTON(cbox), index);
}

int osm2go_platform::combo_box_get_active(GtkWidget *cbox) {
  return hildon_picker_button_get_active(HILDON_PICKER_BUTTON(cbox));
}

std::string osm2go_platform::combo_box_get_active_text(GtkWidget *cbox) {
  return hildon_button_get_value(HILDON_BUTTON(cbox));
}

bool osm2go_platform::isComboBoxWidget(GtkWidget *widget)
{
  return G_OBJECT_TYPE(widget) == HILDON_TYPE_PICKER_BUTTON;
}

bool osm2go_platform::isComboBoxEntryWidget(GtkWidget *widget)
{
  return G_OBJECT_TYPE(widget) == HILDON_TYPE_PICKER_BUTTON;
}

void osm2go_platform::setEntryText(GtkEntry *entry, const char *text, const char *placeholder)
{
  hildon_gtk_entry_set_placeholder_text(entry, placeholder);
  gtk_entry_set_text(entry, text);
}
