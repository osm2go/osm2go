/*
 * Copyright (C) 2008-2009 Till Harbaum <till@harbaum.org>.
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

typedef struct {
  GtkWidget *view;
  GtkMenu *menu;

  struct {
    gboolean(*func)(GtkTreeSelection *, GtkTreeModel *,
		    GtkTreePath *, gboolean,
		    gpointer);
    gpointer data;
  } sel;

  struct {
    void(*func)(GtkTreeSelection *, gpointer);
    gpointer data;
  } change;

  GtkWidget *table;

  struct {
    gpointer data;
    GtkWidget *widget[6];
    int flags;
  } button;

} list_priv_t;

#ifdef FREMANTLE_USE_POPUP

static void cmenu_init(GtkWidget *list) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);

  /* Create needed handles. */
  priv->menu = GTK_MENU(gtk_menu_new());

  gtk_widget_tap_and_hold_setup(priv->view, GTK_WIDGET(priv->menu), NULL, 0);
}

static GtkWidget *cmenu_append(GtkWidget *list, char *label, 
			       GCallback cb, gpointer data) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);

  GtkWidget *menu_item;

  /* Setup the map context menu. */
  gtk_menu_append(priv->menu, menu_item
		  = gtk_menu_item_new_with_label(label));

  hildon_gtk_widget_set_theme_size(menu_item, 
	   (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));

  gtk_signal_connect(GTK_OBJECT(menu_item), "activate", 
		     GTK_SIGNAL_FUNC(cb), data);

  gtk_widget_show_all(GTK_WIDGET(priv->menu));

  return menu_item;
}

#endif

GtkWidget *list_get_view(GtkWidget *list) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);
  return priv->view;
}

/* a list supports up to three user defined buttons besides */
/* add, edit and remove */
void list_set_user_buttons(GtkWidget *list, ...) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);

  va_list ap;

  /* make space for user buttons */
  if(!(priv->button.flags & LIST_BTN_WIDE))
    gtk_table_resize(GTK_TABLE(priv->table), 2, 3);
  else
    gtk_table_resize(GTK_TABLE(priv->table), 1, 5);

  va_start(ap, list);
  list_button_t id = va_arg(ap, list_button_t);
  while(id) {
    char *label = va_arg(ap, char*);
    GCallback cb = va_arg(ap, GCallback);

    priv->button.widget[id] = button_new_with_label(label);
    if(!(priv->button.flags & LIST_BTN_WIDE))
      gtk_table_attach_defaults(GTK_TABLE(priv->table), priv->button.widget[id], 
		id-LIST_BUTTON_USER0, id-LIST_BUTTON_USER0+1, 1, 2);
    else
      gtk_table_attach_defaults(GTK_TABLE(priv->table), priv->button.widget[id], 
		3+id-LIST_BUTTON_USER0, 3+id-LIST_BUTTON_USER0+1, 0, 1);

    gtk_signal_connect(GTK_OBJECT(priv->button.widget[id]), "clicked", 
		       GTK_SIGNAL_FUNC(cb), priv->button.data);

    id = va_arg(ap, list_button_t);
  }

  va_end(ap);
}

void list_set_columns(GtkWidget *list, ...) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);
  va_list ap;

  va_start(ap, list);
  char *name = va_arg(ap, char*);
  while(name) {
    int hlkey = -1, key = va_arg(ap, int);
    int flags = va_arg(ap, int);

    if(flags & LIST_FLAG_CAN_HIGHLIGHT)
      hlkey = va_arg(ap, int);

    GtkTreeViewColumn *column = NULL;

    if(flags & LIST_FLAG_TOGGLE) {
      GCallback cb = va_arg(ap, GCallback);
      gpointer data = va_arg(ap, gpointer);

      GtkCellRenderer *renderer = gtk_cell_renderer_toggle_new();
      column = gtk_tree_view_column_new_with_attributes(
			   name, renderer, "active", key, NULL);
      g_signal_connect(renderer, "toggled", cb, data);
    
    } else if(flags & LIST_FLAG_STOCK_ICON) {
      GtkCellRenderer *pixbuf_renderer = gtk_cell_renderer_pixbuf_new();
      column = gtk_tree_view_column_new_with_attributes(name,
	          pixbuf_renderer, "stock_id", key, NULL);
    } else {
      GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

      if(flags & LIST_FLAG_CAN_HIGHLIGHT)
	g_object_set(renderer, "background", "red", NULL );

      if(flags & LIST_FLAG_ELLIPSIZE)
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

      column = gtk_tree_view_column_new_with_attributes(name, renderer, 
	"text", key, 
	 (flags & LIST_FLAG_CAN_HIGHLIGHT)?"background-set":NULL, hlkey,
	NULL);

      gtk_tree_view_column_set_expand(column, 
		      flags & (LIST_FLAG_EXPAND | LIST_FLAG_ELLIPSIZE));
    }
    
    gtk_tree_view_column_set_sort_column_id(column, key);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(priv->view), column, -1);
    
    name = va_arg(ap, char*);
  }

  va_end(ap);
}

static GtkWidget *list_button_get(GtkWidget *list, list_button_t id) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);

  return priv->button.widget[id];
}

void list_button_connect(GtkWidget *list, list_button_t id, 
			 GCallback cb, gpointer data) {
  GtkWidget *but = list_button_get(list, id);
  gtk_signal_connect(GTK_OBJECT(but), "clicked", GTK_SIGNAL_FUNC(cb), data);
}

/* put a custom widget into one of the button slots */
void list_set_custom_user_button(GtkWidget *list, list_button_t id, 
				 GtkWidget *widget) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);
  g_assert((id >= 3) && (id < 6));

  /* make space for user buttons */
  gtk_table_resize(GTK_TABLE(priv->table), 2, 3);
  
  if(!(priv->button.flags & LIST_BTN_WIDE))
    gtk_table_attach_defaults(GTK_TABLE(priv->table), widget,
	      id-LIST_BUTTON_USER0, id-LIST_BUTTON_USER0+1, 1, 2);
  else
    gtk_table_attach_defaults(GTK_TABLE(priv->table), widget,
	      3+id-LIST_BUTTON_USER0, 3+id-LIST_BUTTON_USER0+1, 0, 1);

  priv->button.widget[id] = widget;
}

GtkTreeSelection *list_get_selection(GtkWidget *list) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);

  GtkTreeSelection *sel = 
    gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->view));

  return sel;
}

/* returns true if something is selected. on in mode multiple returns */
/* true if exactly one item is selected */
gboolean list_get_selected(GtkWidget *list, GtkTreeModel **model,
			   GtkTreeIter *iter) {
  gboolean retval = FALSE;
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);

#if 1  
  // this copes with multiple selections ...
  GtkTreeSelection *sel =
    gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->view));

  GList *slist = gtk_tree_selection_get_selected_rows(sel, model);
  
  if(g_list_length(slist) == 1) 
    retval = gtk_tree_model_get_iter(*model, iter, slist->data);

  g_list_foreach(slist, (GFunc)gtk_tree_path_free, NULL);
  g_list_free(slist);

  return retval;
#else
  // ... this doesn't
  return gtk_tree_selection_get_selected(sel, model, iter);
#endif
}

void list_button_enable(GtkWidget *list, list_button_t id, gboolean enable) {
  GtkWidget *but = list_button_get(list, id);
  if(but) gtk_widget_set_sensitive(but, enable);
}

void list_set_store(GtkWidget *list, GtkListStore *store) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);

  gtk_tree_view_set_model(GTK_TREE_VIEW(priv->view), GTK_TREE_MODEL(store));
}

void list_set_selection_function(GtkWidget *list, GtkTreeSelectionFunc func,
				 gpointer data) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);

  priv->sel.func = func;
  priv->sel.data = data;
}

/* default selection function enables edit and remove if a row is being */
/* selected */
static gboolean
list_selection_function(GtkTreeSelection *selection, GtkTreeModel *model,
			GtkTreePath *path, gboolean path_currently_selected,
			gpointer list) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);

  GtkTreeIter iter;

  if(gtk_tree_model_get_iter(model, &iter, path)) {
    g_assert(gtk_tree_path_get_depth(path) == 1);

    /* this is now handled by the "changed" event handler */
    //    list_button_enable(GTK_WIDGET(list), LIST_BUTTON_REMOVE, TRUE);
    //    list_button_enable(GTK_WIDGET(list), LIST_BUTTON_EDIT, TRUE);
  }

  if(priv->sel.func)
    return priv->sel.func(selection, model, path, path_currently_selected, 
			  priv->sel.data);

  return TRUE; /* allow selection state to change */
}

static void on_row_activated(GtkTreeView *treeview,
			     GtkTreePath        *path,
			     GtkTreeViewColumn  *col,
			     gpointer            userdata) {
  GtkTreeIter   iter;
  GtkTreeModel *model = gtk_tree_view_get_model(treeview);

  printf("row activated\n");

  if(gtk_tree_model_get_iter(model, &iter, path)) {
    list_priv_t *priv = g_object_get_data(G_OBJECT(userdata), "priv");
    g_assert(priv);

    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(treeview));
    g_assert(GTK_IS_DIALOG(toplevel));

    /* emit a "response accept" signal so we might close the */
    /* dialog */
    gtk_dialog_response(GTK_DIALOG(toplevel), GTK_RESPONSE_ACCEPT);
  }
}

void list_set_static_buttons(GtkWidget *list, int flags,
			     GCallback cb_new, GCallback cb_edit, 
			     GCallback cb_remove, gpointer data) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);

  priv->button.data = data;
  priv->button.flags = flags;

  /* add the three default buttons, but keep the disabled for now */
  if(cb_new) {
    priv->button.widget[0] = 
      button_new_with_label(_((flags&LIST_BTN_NEW)?"New":"Add"));
    gtk_table_attach_defaults(GTK_TABLE(priv->table), 
			      priv->button.widget[0], 0, 1, 0, 1);
    gtk_signal_connect(GTK_OBJECT(priv->button.widget[0]), "clicked", 
		       GTK_SIGNAL_FUNC(cb_new), data);
    gtk_widget_set_sensitive(priv->button.widget[0], TRUE);
  }

  if(cb_edit) {
#ifdef FREMANTLE_USE_POPUP
    priv->button.widget[1] = cmenu_append(list, _("Edit"), 
			  GTK_SIGNAL_FUNC(cb_edit), data);
#else
    priv->button.widget[1] = button_new_with_label(_("Edit"));
    gtk_table_attach_defaults(GTK_TABLE(priv->table), 
			      priv->button.widget[1], 1, 2, 0, 1);
    gtk_signal_connect(GTK_OBJECT(priv->button.widget[1]), "clicked", 
		       GTK_SIGNAL_FUNC(cb_edit), data);
#endif
    gtk_widget_set_sensitive(priv->button.widget[1], FALSE);
  }

  if(cb_remove) {
#ifdef FREMANTLE_USE_POPUP
    priv->button.widget[2] = cmenu_append(list, _("Remove"), 
			  GTK_SIGNAL_FUNC(cb_remove), data);
#else
    priv->button.widget[2] = button_new_with_label(_("Remove"));
    gtk_table_attach_defaults(GTK_TABLE(priv->table), 
			      priv->button.widget[2], 2, 3, 0, 1);
    gtk_signal_connect(GTK_OBJECT(priv->button.widget[2]), "clicked", 
		       GTK_SIGNAL_FUNC(cb_remove), data);
#endif
    gtk_widget_set_sensitive(priv->button.widget[2], FALSE);
  }
}

GtkTreeModel *list_get_model(GtkWidget *list) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);

  return gtk_tree_view_get_model(GTK_TREE_VIEW(priv->view));
}

void list_pre_inplace_edit_tweak (GtkTreeModel *model) {
  // Remove any current sort ordering, leaving items where they are.
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
                                 GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
                                 GTK_SORT_ASCENDING);
}


/* Refocus a GtkTreeView an item specified by iter, unselecting the current
   selection and optionally highlighting the new one. Typically called after
   making an edit to an item with a covering sub-dialog. */

void list_focus_on(GtkWidget *list, GtkTreeIter *iter, gboolean highlight) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(priv->view));

  // Handle de/reselection
  GtkTreeSelection *sel = 
    gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->view));
  gtk_tree_selection_unselect_all(sel);

  // Scroll to it, since it might now be out of view.
  GtkTreePath *path = gtk_tree_model_get_path(model, iter);
  gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(priv->view), path, 
			       NULL, FALSE, 0, 0);
  gtk_tree_path_free(path);
  
  // reselect
  if (highlight)
    gtk_tree_selection_select_iter(sel, iter);
}

static gint on_list_destroy(GtkWidget *list, gpointer data) {
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");
  g_assert(priv);

  g_free(priv);
  
  return FALSE;
}

static void changed(GtkTreeSelection *treeselection, gpointer user_data) {
  GtkWidget *list = (GtkWidget*)user_data;
  list_priv_t *priv = g_object_get_data(G_OBJECT(list), "priv");

  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean selected = list_get_selected(list, &model, &iter);

  /* scroll to selected entry if exactly one is selected */
  if(selected) {
    /* check if the entry isn't already visible */
    GtkTreePath *start = NULL, *end = NULL;
    GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

    gtk_tree_view_get_visible_range(GTK_TREE_VIEW(priv->view), &start, &end);
      
    /* check if path is before start of visible area or behin end of it */
    if((start && (gtk_tree_path_compare(path, start)) < 0) ||
       (end && (gtk_tree_path_compare(path, end) > 0)))
      gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(priv->view), 
				   path, NULL, TRUE, 0.5, 0.5);

    if(start) gtk_tree_path_free(start);
    if(end)   gtk_tree_path_free(end);
    gtk_tree_path_free(path);
  }

  /* the change event handler is overridden */
  if(priv->change.func) { 
    priv->change.func(treeselection, priv->change.data);
    return;
  }

  list_button_enable(GTK_WIDGET(list), LIST_BUTTON_REMOVE, selected);
  list_button_enable(GTK_WIDGET(list), LIST_BUTTON_EDIT, selected);
}

/* a generic list widget with "add", "edit" and "remove" buttons as used */
/* for all kinds of lists in osm2go */
#ifdef USE_HILDON
GtkWidget *list_new(gboolean show_headers)
#else
GtkWidget *list_new(void)
#endif
{	    
  list_priv_t *priv = g_new0(list_priv_t, 1);

  GtkWidget *vbox = gtk_vbox_new(FALSE,3);
  g_object_set_data(G_OBJECT(vbox), "priv", priv);
  g_signal_connect(G_OBJECT(vbox), "destroy",
		   G_CALLBACK(on_list_destroy), priv);

#ifndef FREMANTLE_PANNABLE_AREA
  priv->view = gtk_tree_view_new();
#else
  priv->view = hildon_gtk_tree_view_new(HILDON_UI_MODE_EDIT);
#endif

#ifdef USE_HILDON
  if(show_headers) {
    /* hildon hides these by default */
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(priv->view), TRUE); 
  }
#endif
 
  GtkTreeSelection *sel = 
    gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->view));

  gtk_tree_selection_set_select_function(sel, 
			 list_selection_function, vbox, NULL);

  g_signal_connect(G_OBJECT(sel), "changed", G_CALLBACK(changed), vbox);

#ifndef FREMANTLE_PANNABLE_AREA
  /* put view into a scrolled window */
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), 
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), 
				      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scrolled_window), priv->view);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), scrolled_window);
#else
  /* put view into a pannable area */
  GtkWidget *pannable_area = hildon_pannable_area_new();
  gtk_container_add(GTK_CONTAINER(pannable_area), priv->view);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), pannable_area);

#ifdef FREMANTLE_USE_POPUP
  cmenu_init(vbox);
#endif
#endif

  /* make list react on clicks (double clicks on pre-fremantle) */
  g_signal_connect_after(GTK_OBJECT(priv->view), "row-activated", 
			 (GCallback)on_row_activated, vbox);

  /* add button box */
  priv->table = gtk_table_new(1, 3, TRUE);

  gtk_box_pack_start(GTK_BOX(vbox), priv->table, FALSE, FALSE, 0);

  return vbox;
}

void list_override_changed_event(GtkWidget *list, 
      void(*handler)(GtkTreeSelection*,gpointer), gpointer data) {
  list_priv_t *priv = g_new0(list_priv_t, 1);
  g_assert(priv);

  priv->change.func = handler;
  priv->change.data = data;
}
