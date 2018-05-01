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

#include <gtk/gtk.h>

#include <misc.h>

#include <osm2go_cpp.h>

bool osm2go_platform::init()
{
  return true;
}

void osm2go_platform::cleanup()
{
}

void osm2go_platform::open_url(const char* url)
{
  gtk_show_uri(nullptr, url, GDK_CURRENT_TIME, nullptr);
}

GtkWidget *osm2go_platform::notebook_new(void) {
  return gtk_notebook_new();
}

GtkNotebook *osm2go_platform::notebook_get_gtk_notebook(GtkWidget *notebook) {
  return GTK_NOTEBOOK(notebook);
}

void osm2go_platform::notebook_append_page(GtkWidget *notebook, GtkWidget *page, const char *label) {
  GtkNotebook *nb = notebook_get_gtk_notebook(notebook);
  gtk_notebook_append_page(nb, page, gtk_label_new(label));
}

GtkTreeView *osm2go_platform::tree_view_new()
{
  return GTK_TREE_VIEW(gtk_tree_view_new());
}

GtkWidget *osm2go_platform::scrollable_container(GtkWidget *view)
{
  GtkWidget *container;
  /* put view into a scrolled window */
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr,
                                                                                   nullptr));
  gtk_scrolled_window_set_policy(scrolled_window, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(scrolled_window, GTK_SHADOW_ETCHED_IN);
  container = GTK_WIDGET(scrolled_window);
  gtk_container_add(GTK_CONTAINER(container), view);
  return container;
}

GtkWidget *osm2go_platform::entry_new(osm2go_platform::EntryFlags)
{
  return gtk_entry_new();
}

bool osm2go_platform::isEntryWidget(GtkWidget *widget)
{
  return GTK_IS_ENTRY(widget) == TRUE;
}

GtkWidget *osm2go_platform::button_new_with_label(const char *label)
{
  return gtk_button_new_with_label(label);
}

GtkWidget *osm2go_platform::check_button_new_with_label(const char *label)
{
  return gtk_check_button_new_with_label(label);
}

bool osm2go_platform::isCheckButtonWidget(GtkWidget *widget)
{
  return GTK_IS_CHECK_BUTTON(widget) == TRUE;
}

void osm2go_platform::check_button_set_active(GtkWidget *button, bool active)
{
  gboolean state = active ? TRUE : FALSE;
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), state);
}

bool osm2go_platform::check_button_get_active(GtkWidget *button)
{
  return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) == TRUE;
}

/* the title is only used on fremantle with the picker widget */
GtkWidget *osm2go_platform::combo_box_new(const char *)
{
  return gtk_combo_box_new_text();
}

GtkWidget *osm2go_platform::combo_box_entry_new(const char *)
{
  return gtk_combo_box_entry_new_text();
}

void osm2go_platform::combo_box_append_text(GtkWidget *cbox, const char *text)
{
  gtk_combo_box_append_text(GTK_COMBO_BOX(cbox), text);
}

void osm2go_platform::combo_box_set_active(GtkWidget *cbox, int index)
{
  gtk_combo_box_set_active(GTK_COMBO_BOX(cbox), index);
}

int osm2go_platform::combo_box_get_active(GtkWidget *cbox)
{
  return gtk_combo_box_get_active(GTK_COMBO_BOX(cbox));
}

std::string osm2go_platform::combo_box_get_active_text(GtkWidget *cbox)
{
  g_string ptr(gtk_combo_box_get_active_text(GTK_COMBO_BOX(cbox)));
  std::string ret = ptr.get();
  return ret;
}

bool osm2go_platform::isComboBoxWidget(GtkWidget *widget)
{
  return GTK_IS_COMBO_BOX(widget) == TRUE;
}

bool osm2go_platform::isComboBoxEntryWidget(GtkWidget *widget)
{
  return GTK_IS_COMBO_BOX_ENTRY(widget) == TRUE;
}

void osm2go_platform::setEntryText(GtkEntry *entry, const char *text, const char *placeholder)
{
  if(text == nullptr || *text == '\0')
    gtk_entry_set_text(entry, placeholder);
  else
    gtk_entry_set_text(entry, text);
}
