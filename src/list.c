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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * list.c - generic implementation of a list style widget:
 *          
 * +---------+-----------+
 * | Key     | Key       |
 * +---------+-----------+
 * | Test1    Test2     ^|
 * | Test3    Test4     #|
 * |                    ||
 * |                    v|
 * +---------------------+
 * ( Add )( Edit )(Remove)
 */

#include "appdata.h"

#include <stdarg.h>

static const char *list_button_id_to_name(list_button_t id) {
  const char *names[] = { "btn_new", "btn_edit", "btn_remove",
			  "btn_user0", "btn_user1", "btn_user2" };
  g_assert(id < 6);
  return names[id];
}

GtkWidget *list_get_view(GtkWidget *list) {
  return g_object_get_data(G_OBJECT(list), "view");
}

/* a list supports up to three user defined buttons besides */
/* add, edit and remove */
void list_set_user_buttons(GtkWidget *list, ...) {
  va_list ap;
  GtkWidget *table = g_object_get_data(G_OBJECT(list), "table");
  void *data = g_object_get_data(G_OBJECT(list), "userdata");

  /* make space for user buttons */
  gtk_table_resize(GTK_TABLE(table), 2, 3);

  va_start(ap, list);
  list_button_t id = va_arg(ap, list_button_t);
  while(id) {
    char *label = va_arg(ap, char*);
    GCallback cb = va_arg(ap, GCallback);

    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_table_attach_defaults(GTK_TABLE(table), button, 
	      id-LIST_BUTTON_USER0, id-LIST_BUTTON_USER0+1, 1, 2);
    g_object_set_data(G_OBJECT(list), list_button_id_to_name(id), button);
    gtk_signal_connect(GTK_OBJECT(button), "clicked", 
		       GTK_SIGNAL_FUNC(cb), data);

    id = va_arg(ap, list_button_t);
  }

  va_end(ap);
}

void list_set_columns(GtkWidget *list, ...) {
  va_list ap;
  GtkWidget *view = g_object_get_data(G_OBJECT(list), "view");

  va_start(ap, list);
  char *name = va_arg(ap, char*);
  while(name) {
    int hlkey = -1, key = va_arg(ap, int);
    int flags = va_arg(ap, int);

    if(flags & LIST_FLAG_CAN_HIGHLIGHT)
      hlkey = va_arg(ap, int);

    GtkTreeViewColumn *column = NULL;
    if(!(flags & LIST_FLAG_STOCK_ICON)) {
      GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
      g_object_set(renderer, 
		   "ellipsize", PANGO_ELLIPSIZE_END, 
		   (flags & LIST_FLAG_CAN_HIGHLIGHT)?"background":NULL, "red", 
		   NULL );
      column = gtk_tree_view_column_new_with_attributes(name, renderer, 
	 "text", key, 
	 (flags & LIST_FLAG_CAN_HIGHLIGHT)?"background-set":NULL, hlkey,
	NULL);
      gtk_tree_view_column_set_expand(column, flags & LIST_FLAG_EXPAND);
    } else {
      GtkCellRenderer *pixbuf_renderer = gtk_cell_renderer_pixbuf_new();
      column = gtk_tree_view_column_new_with_attributes(name,
	          pixbuf_renderer, "stock_id", key, NULL);
    }

    gtk_tree_view_insert_column(GTK_TREE_VIEW(view), column, -1);

    name = va_arg(ap, char*);
  }

  va_end(ap);
}

static GtkWidget *list_button_get(GtkWidget *list, list_button_t id) {
  return g_object_get_data(G_OBJECT(list), list_button_id_to_name(id));
}

void list_button_connect(GtkWidget *list, list_button_t id, 
			 GCallback cb, gpointer data) {
  GtkWidget *but = list_button_get(list, id);
  gtk_signal_connect(GTK_OBJECT(but), "clicked", GTK_SIGNAL_FUNC(cb), data);
}

/* put a custom widget into one of the button slots */
void list_set_custom_user_button(GtkWidget *list, list_button_t id, 
				 GtkWidget *widget) {
  GtkWidget *table = g_object_get_data(G_OBJECT(list), "table");
  g_assert((id >= 3) && (id < 6));

  /* make space for user buttons */
  gtk_table_resize(GTK_TABLE(table), 2, 3);
  
  gtk_table_attach_defaults(GTK_TABLE(table), widget,
		    id-LIST_BUTTON_USER0, id-LIST_BUTTON_USER0+1, 1, 2);
  g_object_set_data(G_OBJECT(list), list_button_id_to_name(id), widget);
}

GtkTreeSelection *list_get_selection(GtkWidget *list) {
  GtkWidget *view = g_object_get_data(G_OBJECT(list), "view");
  return gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
}

void list_button_enable(GtkWidget *list, list_button_t id, gboolean enable) {
  GtkWidget *but = list_button_get(list, id);
  gtk_widget_set_sensitive(but, enable);
}

void list_set_store(GtkWidget *list, GtkListStore *store) {
  gtk_tree_view_set_model(
	  GTK_TREE_VIEW(g_object_get_data(G_OBJECT(list), "view")), 
	  GTK_TREE_MODEL(store));
}

void list_set_selection_function(GtkWidget *list, GtkTreeSelectionFunc func,
				 gpointer data) {
  GtkWidget *view = g_object_get_data(G_OBJECT(list), "view");
  gtk_tree_selection_set_select_function(
	 gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), 
	 func, data, NULL);
}

/* default selection function enables edit and remove if a row is being */
/* selected */
static gboolean
list_selection_function(GtkTreeSelection *selection, GtkTreeModel *model,
			GtkTreePath *path, gboolean path_currently_selected,
			gpointer list) {
  GtkTreeIter iter;

  if(gtk_tree_model_get_iter(model, &iter, path)) {
    g_assert(gtk_tree_path_get_depth(path) == 1);

    list_button_enable(GTK_WIDGET(list), LIST_BUTTON_REMOVE, TRUE);
    list_button_enable(GTK_WIDGET(list), LIST_BUTTON_EDIT, TRUE);
  }
  
  return TRUE; /* allow selection state to change */
}

void list_set_static_buttons(GtkWidget *list, GCallback cb_new, 
	     GCallback cb_edit, GCallback cb_remove, gpointer data) {
  GtkWidget *table = g_object_get_data(G_OBJECT(list), "table");
  g_object_set_data(G_OBJECT(list), "userdata", data);

  /* add the three default buttons, but keep the disabled for now */
  GtkWidget *button = NULL;
  if(cb_new) {
    button = gtk_button_new_with_label(_("Add..."));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 1, 0, 1);
    gtk_widget_set_sensitive(button, TRUE);
    g_object_set_data(G_OBJECT(list), "btn_new", button);
    gtk_signal_connect(GTK_OBJECT(button), "clicked", 
		       GTK_SIGNAL_FUNC(cb_new), data);
  }

  if(cb_edit) {
    button = gtk_button_new_with_label(_("Edit..."));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 1, 2, 0, 1);
    gtk_widget_set_sensitive(button, FALSE);
    g_object_set_data(G_OBJECT(list), "btn_edit", button);
    gtk_signal_connect(GTK_OBJECT(button), "clicked", 
		       GTK_SIGNAL_FUNC(cb_edit), data);
  }

  if(cb_remove) {
    button = gtk_button_new_with_label(_("Remove"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 2, 3, 0, 1);
    gtk_widget_set_sensitive(button, FALSE);
    g_object_set_data(G_OBJECT(list), "btn_remove", button);
    gtk_signal_connect(GTK_OBJECT(button), "clicked", 
		       GTK_SIGNAL_FUNC(cb_remove), data);
  }
}

GtkTreeModel *list_get_model(GtkWidget *list) {
  return gtk_tree_view_get_model(GTK_TREE_VIEW(
       g_object_get_data(G_OBJECT(list), "view")));
}

/* a generic list widget with "add", "edit" and "remove" buttons as used */
/* for all kinds of lists in osm2go */
GtkWidget *list_new(void) {

  GtkWidget *vbox = gtk_vbox_new(FALSE,3);
  GtkWidget *view = gtk_tree_view_new();
  g_object_set_data(G_OBJECT(vbox), "view", view);
  
  gtk_tree_selection_set_select_function(
	 gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), 
	 list_selection_function, vbox, NULL);

  /* put view into a scrolled window */
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), 
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), 
				      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scrolled_window), view);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), scrolled_window);

  /* add button box */
  GtkWidget *table = gtk_table_new(1, 3, TRUE);
  g_object_set_data(G_OBJECT(vbox), "table", table);

  gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

  return vbox;
}

