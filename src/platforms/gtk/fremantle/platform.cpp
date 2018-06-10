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
#include "misc.h"

#include <algorithm>
#include <hildon/hildon-check-button.h>
#include <hildon/hildon-entry.h>
#include <hildon/hildon-pannable-area.h>
#include <hildon/hildon-picker-button.h>
#include <hildon/hildon-touch-selector-entry.h>
#include <libosso.h>
#include <tablet-browser-interface.h>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>

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

  gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

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

  gtk_box_pack_start(GTK_BOX(g_object_get_data(G_OBJECT(notebook), "hbox")), button, TRUE, TRUE, 0);
}

GtkTreeView *osm2go_platform::tree_view_new()
{
  return GTK_TREE_VIEW(hildon_gtk_tree_view_new(HILDON_UI_MODE_EDIT));
}

GtkWidget *osm2go_platform::scrollable_container(GtkWidget *view, bool)
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
  return HILDON_IS_ENTRY(widget) == TRUE;
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
  return HILDON_IS_CHECK_BUTTON(widget) == TRUE;
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

struct combo_add_string {
  HildonTouchSelector * const selector;
  explicit combo_add_string(HildonTouchSelector *sel) : selector(sel) {}
  inline void operator()(const char *entry) {
    hildon_touch_selector_append_text(selector, entry);
  }
};

GtkWidget *osm2go_platform::combo_box_new(const char *title, const std::vector<const char *> &items, int active)
{
  GtkWidget *selector = hildon_touch_selector_new_text();
  GtkWidget *cbox = combo_box_new_with_selector(title, selector);

  /* fill combo box with entries */
  std::for_each(items.begin(), items.end(), combo_add_string(HILDON_TOUCH_SELECTOR(selector)));

  if(active >= 0)
    osm2go_platform::combo_box_set_active(cbox, active);

  return cbox;
}

/**
 * @brief extract current value of hildon_touch_selector_entry
 *
 * In contrast to the default hildon_touch_selector_entry_print_func() it will
 * just return whatever is in the edit field, so that one can clear that field
 * resulting in no value being set.
 */
static gchar *
touch_selector_entry_print_func(HildonTouchSelector *selector, gpointer)
{
  HildonEntry *entry = hildon_touch_selector_entry_get_entry(HILDON_TOUCH_SELECTOR_ENTRY(selector));

  return g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
}

GtkWidget *osm2go_platform::combo_box_entry_new(const char *title) {
  GtkWidget *selector = hildon_touch_selector_entry_new_text();
  hildon_touch_selector_set_print_func(HILDON_TOUCH_SELECTOR(selector), touch_selector_entry_print_func);
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

void osm2go_platform::combo_box_set_active_text(GtkWidget *cbox, const char *text)
{
  hildon_button_set_value(HILDON_BUTTON(cbox), text);
  // explicitely set the text in the edit, which will not happen when setting the button
  // text to something not in the model
  HildonTouchSelector *selector = hildon_picker_button_get_selector(HILDON_PICKER_BUTTON(cbox));
  HildonEntry *entry = hildon_touch_selector_entry_get_entry(HILDON_TOUCH_SELECTOR_ENTRY(selector));
  gtk_entry_set_text(GTK_ENTRY(entry), text);
  gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
}

bool osm2go_platform::isComboBoxWidget(GtkWidget *widget)
{
  return HILDON_IS_PICKER_BUTTON(widget) == TRUE;
}

bool osm2go_platform::isComboBoxEntryWidget(GtkWidget *widget)
{
  return HILDON_IS_PICKER_BUTTON(widget) == TRUE;
}

static gchar *
select_print_func(HildonTouchSelector *selector, gpointer data)
{
  GList *selected_rows = hildon_touch_selector_get_selected_rows(selector, 0);
  const char delimiter = *static_cast<const char *>(data);

  if(selected_rows == nullptr)
    return g_strdup("");

  GtkTreeModel *model = hildon_touch_selector_get_model(selector, 0);

  std::string result;
  g_string guard;

  for (GList *item = selected_rows; item != nullptr; item = g_list_next(item)) {
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, static_cast<GtkTreePath *>(item->data));

    gchar *current_string = nullptr;
    gtk_tree_model_get(model, &iter, 1, &current_string, -1);
    guard.reset(current_string);

    result += current_string;
    result += delimiter;
  }

  g_list_foreach(selected_rows, reinterpret_cast<GFunc>(gtk_tree_path_free), nullptr);
  g_list_free(selected_rows);

  result.resize(result.size() - 1);

  return g_strdup(result.c_str());
}

GtkWidget *osm2go_platform::select_widget(const char *title, GtkTreeModel *model, unsigned int flags, const char *delimiter)
{
  HildonTouchSelector *selector;

  switch (flags) {
  case NoSelectionFlags:
    selector = HILDON_TOUCH_SELECTOR(hildon_touch_selector_new_text());
    break;
  case AllowEditing:
    selector = HILDON_TOUCH_SELECTOR(hildon_touch_selector_entry_new_text());
    hildon_touch_selector_set_print_func(selector, touch_selector_entry_print_func);
    hildon_touch_selector_entry_set_text_column(HILDON_TOUCH_SELECTOR_ENTRY(selector), 1);
    break;
  case AllowMultiSelection:
    selector = HILDON_TOUCH_SELECTOR(hildon_touch_selector_new_text());
    hildon_touch_selector_set_print_func_full(selector, select_print_func, const_cast<char *>(delimiter), nullptr);
    hildon_touch_selector_set_column_selection_mode(selector, HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE);
    break;
  default:
    assert_unreachable();
  }

  hildon_touch_selector_set_model(selector, 0, model);

  return combo_box_new_with_selector(title, GTK_WIDGET(selector));
}

std::string osm2go_platform::select_widget_value(GtkWidget *widget)
{
  HildonTouchSelector *selector = hildon_picker_button_get_selector(HILDON_PICKER_BUTTON(widget));

  if(HILDON_IS_TOUCH_SELECTOR_ENTRY(selector)) {
    return combo_box_get_active_text(widget);
  } else {
    GtkTreeModel *model = hildon_touch_selector_get_model(selector, 0);
    std::string ret;

    if(hildon_touch_selector_get_column_selection_mode(selector) ==
       HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE) {
      // the button has already the properly formatted result
      ret = hildon_button_get_value(HILDON_BUTTON(widget));
    } else {
      int row = hildon_picker_button_get_active(HILDON_PICKER_BUTTON(widget));
      GtkTreeIter iter;
      gboolean b = gtk_tree_model_iter_nth_child(model, &iter, nullptr, row);
      g_assert(b == TRUE);
      gchar *s;
      gtk_tree_model_get(model, &iter, 1, &s, -1);
      g_string guard(s);
      ret = s;
    }

    return ret;
  }
}

void osm2go_platform::select_widget_select(GtkWidget *widget, const std::vector<unsigned int> &indexes)
{
  HildonTouchSelector *selector = hildon_picker_button_get_selector(HILDON_PICKER_BUTTON(widget));

  if(HILDON_IS_TOUCH_SELECTOR_ENTRY(selector) ||
     hildon_touch_selector_get_column_selection_mode(selector) !=
         HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE) {
    assert_cmpnum(indexes.size(), 1);
    hildon_picker_button_set_active(HILDON_PICKER_BUTTON(widget), indexes.front());
  } else {
    GtkTreeIter iter;
    GtkTreeModel *model = hildon_touch_selector_get_model(selector, 0);

    for(size_t i = 0; i < indexes.size(); i++) {
      gboolean b = gtk_tree_model_iter_nth_child(model, &iter, nullptr, indexes[i]);
      g_assert(b == TRUE);
      hildon_touch_selector_select_iter(selector, 0, &iter, FALSE);
    }
  }
}

void osm2go_platform::setEntryText(GtkEntry *entry, const char *text, const char *placeholder)
{
  hildon_gtk_entry_set_placeholder_text(entry, placeholder);
  gtk_entry_set_text(entry, text);
}
