/*
 * Copyright (C) 2017 Rolf Eike Beer <eike@sf-mail.de>.
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

#ifndef OSM2GO_PLATFORM_GTK_H
#define OSM2GO_PLATFORM_GTK_H

#include <string>
#include <vector>

namespace osm2go_platform {
  bool init();

  void cleanup();

  GtkWidget *notebook_new(void);
  void notebook_append_page(GtkWidget *notebook, GtkWidget *page, const char *label);
  GtkNotebook *notebook_get_gtk_notebook(GtkWidget *notebook);

  GtkTreeView *tree_view_new();
  GtkWidget *scrollable_container(GtkWidget *view);

  /* unified widgets */
  enum EntryFlags {
    EntryFlagsDefault,
    EntryFlagsNoAutoCap
  };
  GtkWidget *entry_new(EntryFlags flags = EntryFlagsDefault);
  bool isEntryWidget(GtkWidget *widget);

  GtkWidget *button_new_with_label(const char *label);

  GtkWidget *check_button_new_with_label(const char *label);
  void check_button_set_active(GtkWidget *button, bool active);
  bool check_button_get_active(GtkWidget *button);
  bool isCheckButtonWidget(GtkWidget *widget);

  GtkWidget *combo_box_new(const char *title);
  GtkWidget *combo_box_entry_new(const char *title);
  void combo_box_append_text(GtkWidget *cbox, const char *text);
  void combo_box_set_active(GtkWidget *cbox, int index);
  int combo_box_get_active(GtkWidget *cbox);
  std::string combo_box_get_active_text(GtkWidget *cbox);
  bool isComboBoxWidget(GtkWidget *widget);
  bool isComboBoxEntryWidget(GtkWidget *widget);

  GtkWidget *string_select_widget(const char *title, const std::vector<std::string> &entries, int match);
};

#endif // OSM2GO_PLATFORM_GTK_H
