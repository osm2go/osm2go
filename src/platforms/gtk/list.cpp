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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
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

#include "list.h"

#include "misc.h"

#include <array>
#include <cassert>
#include <cstring>
#ifdef FREMANTLE
#include <hildon/hildon-gtk.h>
#include <hildon/hildon-pannable-area.h>
#endif

#include "osm2go_annotations.h"
#include "osm2go_i18n.h"

struct list_priv_t {
  GtkWidget *view;
  GtkMenu *menu;

  void *callback_context;

  void(*change)(GtkTreeSelection *, gpointer);

  GtkWidget *table;

  struct {
    std::array<GtkWidget *, 6> widget;
    int flags;
  } button;
};

/* a list supports up to three user defined buttons besides */
/* add, edit and remove */
static void list_set_user_buttons(list_priv_t *priv, const std::vector<list_button> &buttons) {
  for(unsigned int id = LIST_BUTTON_USER0; id < buttons.size(); id++) {
    const char *label = buttons[id].first;
    if(!label)
      continue;
    GCallback cb = buttons[id].second;

    priv->button.widget[id] = button_new_with_label(label);
    if(priv->button.flags & LIST_BTN_2ROW)
      gtk_table_attach_defaults(GTK_TABLE(priv->table), priv->button.widget[id],
		id-LIST_BUTTON_USER0, id-LIST_BUTTON_USER0+1, 1, 2);
    else
      gtk_table_attach_defaults(GTK_TABLE(priv->table), priv->button.widget[id],
                                id, id + 1, 0, 1);

    g_signal_connect_swapped(GTK_OBJECT(priv->button.widget[id]), "clicked",
                             G_CALLBACK(cb), priv->callback_context);
  }
}

static void list_set_columns(list_priv_t *priv, const std::vector<list_view_column> &columns) {
  for(unsigned int key = 0; key < columns.size(); key++) {
    const char *name = columns[key].name;
    int hlkey = columns[key].hlkey;
    int flags = columns[key].flags;

    GtkTreeViewColumn *column = O2G_NULLPTR;

    if(flags & LIST_FLAG_STOCK_ICON) {
      GtkCellRenderer *pixbuf_renderer = gtk_cell_renderer_pixbuf_new();
      column = gtk_tree_view_column_new_with_attributes(name,
	          pixbuf_renderer, "stock_id", key, O2G_NULLPTR);
    } else {
      GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

      if(flags & LIST_FLAG_CAN_HIGHLIGHT)
	g_object_set(renderer, "background", "red", O2G_NULLPTR );

      if(flags & LIST_FLAG_ELLIPSIZE)
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, O2G_NULLPTR);

      column = gtk_tree_view_column_new_with_attributes(name, renderer,
	"text", key,
	 (flags & LIST_FLAG_CAN_HIGHLIGHT)?"background-set":O2G_NULLPTR, hlkey,
	O2G_NULLPTR);

      gtk_tree_view_column_set_expand(column,
		      flags & (LIST_FLAG_EXPAND | LIST_FLAG_ELLIPSIZE));
    }

    gtk_tree_view_column_set_sort_column_id(column, key);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(priv->view), column, -1);
  }
}

/* put a custom widget into one of the button slots */
void list_set_custom_user_button(GtkWidget *list, list_button_t id,
				 GtkWidget *widget) {
  list_priv_t *priv = static_cast<list_priv_t *>(g_object_get_data(G_OBJECT(list), "priv"));
  assert(priv != O2G_NULLPTR);
  assert_cmpnum_op(static_cast<int>(id), >=, 3);
  assert_cmpnum_op(static_cast<int>(id), <,  6);

  /* make space for user buttons */
  gtk_table_resize(GTK_TABLE(priv->table), 2, 3);

  if(priv->button.flags & LIST_BTN_2ROW)
    gtk_table_attach_defaults(GTK_TABLE(priv->table), widget,
	      id-LIST_BUTTON_USER0, id-LIST_BUTTON_USER0+1, 1, 2);
  else
    gtk_table_attach_defaults(GTK_TABLE(priv->table), widget,
                              id, id + 1, 0, 1);

  priv->button.widget[id] = widget;
}

GtkTreeSelection *list_get_selection(GtkWidget *list) {
  list_priv_t *priv = static_cast<list_priv_t *>(g_object_get_data(G_OBJECT(list), "priv"));
  assert(priv != O2G_NULLPTR);

  GtkTreeSelection *sel =
    gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->view));

  return sel;
}

/* returns true if something is selected. on in mode multiple returns */
/* true if exactly one item is selected */
bool list_get_selected(GtkWidget *list, GtkTreeModel **model, GtkTreeIter *iter) {
  bool retval = false;
  list_priv_t *priv = static_cast<list_priv_t *>(g_object_get_data(G_OBJECT(list), "priv"));
  assert(priv != O2G_NULLPTR);

#if 1
  // this copes with multiple selections ...
  GtkTreeSelection *sel =
    gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->view));

  GList *slist = gtk_tree_selection_get_selected_rows(sel, model);

  if(g_list_length(slist) == 1)
    retval = gtk_tree_model_get_iter(*model, iter, static_cast<GtkTreePath *>(slist->data)) == TRUE;

#if GLIB_CHECK_VERSION(2,28,0)
  g_list_free_full(slist, (GDestroyNotify)gtk_tree_path_free);
#else
  g_list_foreach(slist, (GFunc)gtk_tree_path_free, O2G_NULLPTR);
  g_list_free(slist);
#endif

  return retval;
#else
  // ... this doesn't
  return gtk_tree_selection_get_selected(sel, model, iter) == TRUE;
#endif
}

void list_button_enable(GtkWidget *list, list_button_t id, bool enable) {
  list_priv_t *priv = static_cast<list_priv_t *>(g_object_get_data(G_OBJECT(list), "priv"));
  assert(priv != O2G_NULLPTR);

  GtkWidget *but = priv->button.widget[id];

  if(likely(but))
    gtk_widget_set_sensitive(but, enable ? TRUE : FALSE);
}

static void on_row_activated(GtkTreeView *treeview,
			     GtkTreePath        *path,
                             GtkTreeViewColumn  *,
			     gpointer            userdata) {
  GtkTreeIter   iter;
  GtkTreeModel *model = gtk_tree_view_get_model(treeview);

  g_debug("row activated");

  if(gtk_tree_model_get_iter(model, &iter, path)) {
    assert(g_object_get_data(G_OBJECT(userdata), "priv") != O2G_NULLPTR);

    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(treeview));
    assert(GTK_IS_DIALOG(toplevel) == TRUE);

    /* emit a "response accept" signal so we might close the */
    /* dialog */
    gtk_dialog_response(GTK_DIALOG(toplevel), GTK_RESPONSE_ACCEPT);
  }
}

GtkTreeModel *list_get_model(GtkWidget *list) {
  list_priv_t *priv = static_cast<list_priv_t *>(g_object_get_data(G_OBJECT(list), "priv"));
  assert(priv != O2G_NULLPTR);

  return gtk_tree_view_get_model(GTK_TREE_VIEW(priv->view));
}

/* Refocus a GtkTreeView an item specified by iter, unselecting the current
   selection and optionally highlighting the new one. Typically called after
   making an edit to an item with a covering sub-dialog. */

void list_focus_on(GtkWidget *list, GtkTreeIter *iter) {
  list_priv_t *priv = static_cast<list_priv_t *>(g_object_get_data(G_OBJECT(list), "priv"));
  assert(priv != O2G_NULLPTR);
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(priv->view));

  // Handle de/reselection
  GtkTreeSelection *sel =
    gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->view));
  gtk_tree_selection_unselect_all(sel);

  // Scroll to it, since it might now be out of view.
  GtkTreePath *path = gtk_tree_model_get_path(model, iter);
  gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(priv->view), path,
			       O2G_NULLPTR, FALSE, 0, 0);
  gtk_tree_path_free(path);

  // reselect
  gtk_tree_selection_select_iter(sel, iter);
}

static void changed(GtkTreeSelection *treeselection, gpointer user_data) {
  GtkWidget *list = static_cast<GtkWidget *>(user_data);
  list_priv_t *priv = static_cast<list_priv_t *>(g_object_get_data(G_OBJECT(list), "priv"));

  GtkTreeModel *model;
  GtkTreeIter iter;
  bool selected = list_get_selected(list, &model, &iter);

  /* scroll to selected entry if exactly one is selected */
  if(selected) {
    /* check if the entry isn't already visible */
    GtkTreePath *start = O2G_NULLPTR, *end = O2G_NULLPTR;
    GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

    gtk_tree_view_get_visible_range(GTK_TREE_VIEW(priv->view), &start, &end);

    /* check if path is before start of visible area or behin end of it */
    if((start && (gtk_tree_path_compare(path, start)) < 0) ||
       (end && (gtk_tree_path_compare(path, end) > 0)))
      gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(priv->view),
				   path, O2G_NULLPTR, TRUE, 0.5, 0.5);

    if(start) gtk_tree_path_free(start);
    if(end)   gtk_tree_path_free(end);
    gtk_tree_path_free(path);
  }

  /* the change event handler is overridden */
  priv->change(treeselection, priv->callback_context);
}

/* a generic list widget with "add", "edit" and "remove" buttons as used */
/* for all kinds of lists in osm2go */
GtkWidget *list_new(bool show_headers, unsigned int btn_flags, void *context,
                    void(*cb_changed)(GtkTreeSelection*,void*),
                    const std::vector<list_button> &buttons,
                    const std::vector<list_view_column> &columns,
                    GtkListStore *store)
{
  list_priv_t *priv = g_new0(list_priv_t, 1);

  assert(cb_changed != O2G_NULLPTR);

  GtkWidget *vbox = gtk_vbox_new(FALSE,3);
  g_object_set_data(G_OBJECT(vbox), "priv", priv);
  g_signal_connect_swapped(G_OBJECT(vbox), "destroy",
                           G_CALLBACK(g_free), priv);

  priv->callback_context = context;
  priv->change = cb_changed;

#ifndef FREMANTLE
  priv->view = gtk_tree_view_new();
#else
  priv->view = hildon_gtk_tree_view_new(HILDON_UI_MODE_EDIT);
#endif

  /* hildon hides these by default */
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(priv->view), show_headers ? TRUE : FALSE);

  GtkTreeSelection *sel =
    gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->view));

#ifndef FREMANTLE
  /* put view into a scrolled window */
  GtkWidget *scrolled_window = gtk_scrolled_window_new(O2G_NULLPTR, O2G_NULLPTR);
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
#endif

  /* make list react on clicks (double clicks on pre-fremantle) */
  g_signal_connect_after(GTK_OBJECT(priv->view), "row-activated",
                         G_CALLBACK(on_row_activated), vbox);

  guint rows = 1;
  guint cols = 3;
  /* make space for user buttons */
  if(btn_flags & LIST_BTN_2ROW)
    rows = 2;
  else
    cols = buttons.size();

  /* add button box */
  priv->table = gtk_table_new(rows, cols, TRUE);

  gtk_box_pack_start(GTK_BOX(vbox), priv->table, FALSE, FALSE, 0);

  priv->button.flags = btn_flags;

  assert_cmpnum_op(buttons.size(), >=, cols);
  assert_cmpnum_op(buttons.size(), <=, cols * rows);

  /* add the three default buttons, but keep all but the first disabled for now */
  for(unsigned int i = 0; i < 3; i++) {
    if(strchr(buttons[i].first, '_') != O2G_NULLPTR)
      priv->button.widget[i] = gtk_button_new_with_mnemonic(buttons[i].first);
    else
      priv->button.widget[i] = button_new_with_label(buttons[i].first);
    gtk_table_attach_defaults(GTK_TABLE(priv->table),
                              priv->button.widget[i], i, i + 1, 0, 1);
    g_signal_connect_swapped(GTK_OBJECT(priv->button.widget[i]), "clicked",
                             buttons[i].second, priv->callback_context);
    gtk_widget_set_sensitive(priv->button.widget[i], i == 0 ? TRUE : FALSE);
  }

  list_set_columns(priv, columns);

  if(buttons.size() > 3)
    list_set_user_buttons(priv, buttons);

  gtk_tree_view_set_model(GTK_TREE_VIEW(priv->view), GTK_TREE_MODEL(store));

  // set this up last so it will not be called with an incompletely set up
  // context pointer
  g_signal_connect(G_OBJECT(sel), "changed", G_CALLBACK(changed), vbox);

  return vbox;
}

void list_scroll(GtkWidget* list, GtkTreeIter* iter) {
  list_priv_t *priv = static_cast<list_priv_t *>(g_object_get_data(G_OBJECT(list), "priv"));
  assert(priv != O2G_NULLPTR);

  list_view_scroll(GTK_TREE_VIEW(priv->view), list_get_selection(list), iter);
}

void list_view_scroll(GtkTreeView *view, GtkTreeSelection *sel, GtkTreeIter* iter) {
  GtkTreeModel *model = gtk_tree_view_get_model(view);

  gtk_tree_selection_select_iter(sel, iter);

  GtkTreePath *mpath = gtk_tree_model_get_path(model, iter);
  gtk_tree_view_scroll_to_cell(view, mpath, O2G_NULLPTR, FALSE, 0.0f, 0.0f);
  gtk_tree_path_free(mpath);
}
