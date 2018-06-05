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

#include <misc.h>

#include <algorithm>
#include <cstdint>
#include <gtk/gtk.h>

#include <osm2go_annotations.h>
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

struct combo_add_string {
  GtkComboBoxText * const cbox;
  explicit combo_add_string(GtkWidget *w) : cbox(GTK_COMBO_BOX_TEXT(w)) {}
  inline void operator()(const char *entry) {
    gtk_combo_box_text_append_text(cbox, entry);
  }
};

/* the title is only used on fremantle with the picker widget */
GtkWidget *osm2go_platform::combo_box_new(const char *, const std::vector<const char *> &items, int active)
{
  GtkWidget *cbox = gtk_combo_box_text_new();

  /* fill combo box with entries */
  std::for_each(items.begin(), items.end(), combo_add_string(cbox));

  if(active >= 0)
    osm2go_platform::combo_box_set_active(cbox, active);

  return cbox;
}

GtkWidget *osm2go_platform::combo_box_entry_new(const char *)
{
  return gtk_combo_box_text_new_with_entry();
}

void osm2go_platform::combo_box_append_text(GtkWidget *cbox, const char *text)
{
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cbox), text);
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
  g_string ptr(gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(cbox)));
  std::string ret = ptr.get();
  return ret;
}

void osm2go_platform::combo_box_set_active_text(GtkWidget *cbox, const char *text)
{
  gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(cbox))), text);
}

static bool isCombo(GtkWidget *widget, bool entry)
{
  if(!GTK_IS_COMBO_BOX_TEXT(widget))
    return false;
  gboolean b;
  g_object_get(widget, "has-entry", &b, nullptr);
  return b == (entry ? TRUE : FALSE);
}

bool osm2go_platform::isComboBoxWidget(GtkWidget *widget)
{
  return isCombo(widget, FALSE);
}

bool osm2go_platform::isComboBoxEntryWidget(GtkWidget *widget)
{
  return isCombo(widget, TRUE);
}

GtkWidget *osm2go_platform::select_widget(const char *, GtkTreeModel *model, unsigned int flags, const char *delimiter)
{
  GtkWidget *ret;
  GtkCellRenderer *rnd = gtk_cell_renderer_text_new();

  switch (flags) {
  case NoSelectionFlags:
    ret = gtk_combo_box_new_with_model(model);
    break;
  case AllowEditing:
    ret = gtk_combo_box_new_with_model_and_entry(model);
    gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(ret), 1);
    break;
  case AllowMultiSelection: {
    GtkTreeView *tree = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(tree), GTK_SELECTION_MULTIPLE);
    gtk_tree_view_set_headers_visible(tree, FALSE);
    uintptr_t ch = *delimiter;
    g_object_set_data(G_OBJECT(tree), "user delimiter", reinterpret_cast<gpointer>(ch));
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(nullptr, rnd,
                                                                         "text", 0,
                                                                         nullptr);
    gtk_tree_view_append_column(tree, column);

    return scrollable_container(GTK_WIDGET(tree));
  }
  default:
    assert_unreachable();
  }

  GtkCellLayout *cell = GTK_CELL_LAYOUT(ret);
  gtk_cell_layout_clear(cell);
  gtk_cell_layout_pack_start(cell, rnd, TRUE);
  gtk_cell_layout_add_attribute(cell, rnd, "text", 0);

  return ret;
}

std::string osm2go_platform::select_widget_value(GtkWidget *widget)
{
  gboolean b;
  std::string ret;
  g_string guard;

  if(GTK_IS_COMBO_BOX(widget)) {
    g_object_get(widget, "has-entry", &b, nullptr);
    if(b == TRUE) {
      ret = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(widget))));
    } else {
      GtkComboBox *cbox = GTK_COMBO_BOX(widget);
      int row = gtk_combo_box_get_active(cbox);
      g_assert_cmpint(row, >=, 0);
      GtkTreeModel *model = gtk_combo_box_get_model(cbox);
      g_assert_nonnull(model);
      GtkTreeIter iter;
      b = gtk_tree_model_iter_nth_child(model, &iter, nullptr, row);
      g_assert_true(b);
      gchar *s;
      gtk_tree_model_get(model, &iter, 1, &s, -1);
      guard.reset(s);
      ret = s;
    }
  } else {
    GtkTreeView *tree = GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(widget)));
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree);
    assert(selection != nullptr);
    GtkTreeModel *model;
    GList *selected_rows = gtk_tree_selection_get_selected_rows(selection, &model);

    gpointer p = g_object_get_data(G_OBJECT(tree), "user delimiter");
    char delimiter = reinterpret_cast<uintptr_t>(p);

    for (GList *item = selected_rows; item != nullptr; item = g_list_next(item)) {
      GtkTreeIter iter;
      gtk_tree_model_get_iter(model, &iter, static_cast<GtkTreePath *>(item->data));

      gchar *current_string;
      gtk_tree_model_get(model, &iter, 1, &current_string, -1);
      guard.reset(current_string);

      if(!ret.empty())
        ret += delimiter;
      ret += current_string;
    }

    g_list_free_full(selected_rows, reinterpret_cast<GDestroyNotify>(gtk_tree_path_free));
  }

  return ret;
}

void osm2go_platform::select_widget_select(GtkWidget *widget, const std::vector<unsigned int> &indexes)
{
  if(GTK_IS_COMBO_BOX(widget)) {
    assert_cmpnum(indexes.size(), 1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), indexes.front());
  } else {
    GtkTreeIter iter;
    GtkTreeView *tree = GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(widget)));
    GtkTreeModel *model = gtk_tree_view_get_model(tree);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree);

    for(size_t i = 0; i < indexes.size(); i++) {
      gboolean b = gtk_tree_model_iter_nth_child(model, &iter, nullptr, indexes[i]);
      g_assert(b == TRUE);
      gtk_tree_selection_select_iter(selection, &iter);
    }
  }
}

void osm2go_platform::setEntryText(GtkEntry *entry, const char *text, const char *placeholder)
{
  if(text == nullptr || *text == '\0')
    gtk_entry_set_text(entry, placeholder);
  else
    gtk_entry_set_text(entry, text);
}
