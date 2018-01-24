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
  gtk_show_uri(O2G_NULLPTR, url, GDK_CURRENT_TIME, O2G_NULLPTR);
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
