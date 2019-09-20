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


#include <array>
#include <cassert>
#include <cstring>

#include "osm2go_annotations.h"
#include "osm2go_i18n.h"
#include <osm2go_platform_gtk.h>

namespace {

struct list_priv_t {
  typedef void (*change_cb)(GtkTreeSelection *, void *);
  list_priv_t(change_cb cb, void *cb_ctx, GtkWidget *tw, unsigned int btnflags);

  GtkTreeView * const view;

  const change_cb change;
  void * const callback_context;

  GtkWidget * const table;

  struct {
    std::array<GtkWidget *, 6> widget;
    int flags;
  } button;
};

list_priv_t::list_priv_t(change_cb cb, void *cb_ctx, GtkWidget *tw, unsigned int btnflags)
  : view(osm2go_platform::tree_view_new())
  , change(cb)
  , callback_context(cb_ctx)
  , table(tw)
{
  button.flags = btnflags;
}

struct tree_path_deleter {
  inline void operator()(GtkTreePath *path)
  { gtk_tree_path_free(path); }
};
typedef std::unique_ptr<GtkTreePath, tree_path_deleter> tree_path_guard;

/* a list supports up to three user defined buttons besides */
/* add, edit and remove */
void
list_set_user_buttons(list_priv_t *priv, const std::vector<list_button> &buttons)
{
  for(unsigned int id = LIST_BUTTON_USER0; id < buttons.size(); id++) {
    const char *label = buttons[id].first;
    if(label == nullptr)
      continue;
    GCallback cb = buttons[id].second;

    priv->button.widget[id] = osm2go_platform::button_new_with_label(label);
    if(priv->button.flags & LIST_BTN_2ROW)
      gtk_table_attach_defaults(GTK_TABLE(priv->table), priv->button.widget[id],
		id-LIST_BUTTON_USER0, id-LIST_BUTTON_USER0+1, 1, 2);
    else
      gtk_table_attach_defaults(GTK_TABLE(priv->table), priv->button.widget[id],
                                id, id + 1, 0, 1);

    g_signal_connect_swapped(priv->button.widget[id], "clicked", cb, priv->callback_context);
  }
}

void
list_set_columns(GtkTreeView *view, const std::vector<list_view_column> &columns)
{
  for(unsigned int key = 0; key < columns.size(); key++) {
    const char *name = columns[key].name;
    int hlkey = columns[key].hlkey;
    int flags = columns[key].flags;

    GtkTreeViewColumn *column;

    if(flags & LIST_FLAG_STOCK_ICON) {
      GtkCellRenderer *pixbuf_renderer = gtk_cell_renderer_pixbuf_new();
      column = gtk_tree_view_column_new_with_attributes(name, pixbuf_renderer,
                                                        "stock_id", key, nullptr);
    } else {
      GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

      if(flags & LIST_FLAG_CAN_HIGHLIGHT)
        g_object_set(renderer, "background", "red", nullptr );

      if(flags & LIST_FLAG_ELLIPSIZE)
        g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, nullptr);

      // if LIST_FLAG_CAN_HIGHLIGHT is not set this will be nullptr, so the function
      // will ignore the following int attribute anyway
      const char *hlattr = (flags & LIST_FLAG_CAN_HIGHLIGHT) ? "background-set" : nullptr;
      column = gtk_tree_view_column_new_with_attributes(name, renderer, "text", key,
                                                        hlattr, hlkey, nullptr);

      gtk_tree_view_column_set_expand(column,
                                      (flags & (LIST_FLAG_EXPAND | LIST_FLAG_ELLIPSIZE)) ? TRUE : FALSE);
    }

    gtk_tree_view_column_set_sort_column_id(column, key);
    gtk_tree_view_insert_column(view, column, -1);
  }
}

}

/* put a custom widget into one of the button slots */
void list_set_custom_user_button(GtkWidget *list, list_button_t id,
				 GtkWidget *widget) {
  list_priv_t *priv = static_cast<list_priv_t *>(g_object_get_data(G_OBJECT(list), "priv"));
  assert(priv != nullptr);
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
  assert(priv != nullptr);

  return gtk_tree_view_get_selection(priv->view);
}

/* returns true if something is selected. on in mode multiple returns */
/* true if exactly one item is selected */
bool list_get_selected(GtkWidget *list, GtkTreeModel **model, GtkTreeIter *iter) {
  GtkTreeSelection *sel = list_get_selection(list);

#if 1
  // this copes with multiple selections ...
  bool retval = false;

  GList *slist = gtk_tree_selection_get_selected_rows(sel, model);

  if(g_list_length(slist) == 1)
    retval = gtk_tree_model_get_iter(*model, iter, static_cast<GtkTreePath *>(slist->data)) == TRUE;

#if GLIB_CHECK_VERSION(2,28,0)
  g_list_free_full(slist, reinterpret_cast<GDestroyNotify>(gtk_tree_path_free));
#else
  g_list_foreach(slist, reinterpret_cast<GFunc>(gtk_tree_path_free), nullptr);
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
  assert(priv != nullptr);

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

  if(gtk_tree_model_get_iter(model, &iter, path) == TRUE) {
    assert(g_object_get_data(G_OBJECT(userdata), "priv") != nullptr);

    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(treeview));

    /* emit a "response accept" signal so we might close the */
    /* dialog */
    gtk_dialog_response(GTK_DIALOG(toplevel), GTK_RESPONSE_ACCEPT);
  }
}

GtkTreeModel *list_get_model(GtkWidget *list) {
  list_priv_t *priv = static_cast<list_priv_t *>(g_object_get_data(G_OBJECT(list), "priv"));
  assert(priv != nullptr);

  return gtk_tree_view_get_model(priv->view);
}

/* Refocus a GtkTreeView an item specified by iter, unselecting the current
   selection and optionally highlighting the new one. Typically called after
   making an edit to an item with a covering sub-dialog. */

void list_focus_on(GtkWidget *list, GtkTreeIter *iter) {
  list_priv_t *priv = static_cast<list_priv_t *>(g_object_get_data(G_OBJECT(list), "priv"));
  assert(priv != nullptr);
  GtkTreeModel *model = gtk_tree_view_get_model(priv->view);

  // Handle de/reselection
  GtkTreeSelection *sel =
    gtk_tree_view_get_selection(priv->view);
  gtk_tree_selection_unselect_all(sel);

  // Scroll to it, since it might now be out of view.
  tree_path_guard path(gtk_tree_model_get_path(model, iter));
  gtk_tree_view_scroll_to_cell(priv->view, path.get(), nullptr, FALSE, 0, 0);

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
    GtkTreePath *start = nullptr, *end = nullptr;
    tree_path_guard path(gtk_tree_model_get_path(model, &iter));

    gtk_tree_view_get_visible_range(priv->view, &start, &end);
    tree_path_guard sguard(start), eguard(end);

    /* check if path is before start of visible area or behin end of it */
    if((sguard && (gtk_tree_path_compare(path.get(), sguard.get())) < 0) ||
       (eguard && (gtk_tree_path_compare(path.get(), eguard.get()) > 0)))
      gtk_tree_view_scroll_to_cell(priv->view, path.get(), nullptr, TRUE, 0.5, 0.5);
  }

  /* the change event handler is overridden */
  priv->change(treeselection, priv->callback_context);
}

static void del_priv(gpointer p)
{
  delete static_cast<list_priv_t *>(p);
}

/* a generic list widget with "add", "edit" and "remove" buttons as used */
/* for all kinds of lists in osm2go */
GtkWidget *list_new(bool show_headers, unsigned int btn_flags, void *context,
                    void(*cb_changed)(GtkTreeSelection*,void*),
                    const std::vector<list_button> &buttons,
                    const std::vector<list_view_column> &columns,
                    GtkTreeModel *store)
{
  assert(cb_changed != nullptr);

  guint rows = 1;
  guint cols = 3;
  /* make space for user buttons */
  if(btn_flags & LIST_BTN_2ROW)
    rows = 2;
  else
    cols = buttons.size();

  GtkWidget *vbox = gtk_vbox_new(FALSE,3);
  list_priv_t *priv = new list_priv_t(cb_changed, context, gtk_table_new(rows, cols, TRUE), btn_flags);

  g_object_set_data(G_OBJECT(vbox), "priv", priv);
  g_signal_connect_swapped(vbox, "destroy", G_CALLBACK(del_priv), priv);

  /* hildon hides these by default */
  gtk_tree_view_set_headers_visible(priv->view, show_headers ? TRUE : FALSE);

  GtkTreeSelection *sel = gtk_tree_view_get_selection(priv->view);

  gtk_box_pack_start(GTK_BOX(vbox), osm2go_platform::scrollable_container(GTK_WIDGET(priv->view)),
                     TRUE, TRUE, 0);

  /* make list react on clicks */
  g_signal_connect_after(priv->view, "row-activated", G_CALLBACK(on_row_activated), vbox);

  /* add button box */
  gtk_box_pack_start(GTK_BOX(vbox), priv->table, FALSE, FALSE, 0);

  assert_cmpnum_op(buttons.size(), >=, cols);
  assert_cmpnum_op(buttons.size(), <=, cols * rows);

  /* add the three default buttons, but keep all but the first disabled for now */
  for(unsigned int i = 0; i < 3; i++) {
    if(strchr(buttons[i].first, '_') != nullptr)
      priv->button.widget[i] = gtk_button_new_with_mnemonic(buttons[i].first);
    else
      priv->button.widget[i] = osm2go_platform::button_new_with_label(buttons[i].first);
    gtk_table_attach_defaults(GTK_TABLE(priv->table),
                              priv->button.widget[i], i, i + 1, 0, 1);
    g_signal_connect_swapped(priv->button.widget[i], "clicked",
                             buttons[i].second, priv->callback_context);
    gtk_widget_set_sensitive(priv->button.widget[i], i == 0 ? TRUE : FALSE);
  }

  list_set_columns(priv->view, columns);

  if(buttons.size() > 3)
    list_set_user_buttons(priv, buttons);

  gtk_tree_view_set_model(priv->view, store);

  // set this up last so it will not be called with an incompletely set up
  // context pointer
  g_signal_connect(sel, "changed", G_CALLBACK(changed), vbox);

  return vbox;
}

void list_scroll(GtkWidget* list, GtkTreeIter* iter) {
  list_priv_t *priv = static_cast<list_priv_t *>(g_object_get_data(G_OBJECT(list), "priv"));
  assert(priv != nullptr);

  list_view_scroll(priv->view, list_get_selection(list), iter);
}

void list_view_scroll(GtkTreeView *view, GtkTreeSelection *sel, GtkTreeIter* iter) {
  GtkTreeModel *model = gtk_tree_view_get_model(view);

  gtk_tree_selection_select_iter(sel, iter);

  tree_path_guard mpath(gtk_tree_model_get_path(model, iter));
  gtk_tree_view_scroll_to_cell(view, mpath.get(), nullptr, FALSE, 0.0f, 0.0f);
}
