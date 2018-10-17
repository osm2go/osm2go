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

#pragma once

#include <gtk/gtk.h>
#include <vector>
#include <utility>

#include <osm2go_cpp.h>

enum list_button_t {
  LIST_BUTTON_NEW = 0,
  LIST_BUTTON_EDIT,
  LIST_BUTTON_REMOVE,
  LIST_BUTTON_USER0,
  LIST_BUTTON_USER1,
  LIST_BUTTON_USER2
};

#define LIST_BTN_2ROW  (1<<4)   // use 2 rows for the buttons

/* list item flags */
#define LIST_FLAG_EXPAND         (1<<0)   /* column expands with dialog size */
#define LIST_FLAG_ELLIPSIZE      (1<<1)   /* column expands and text is ellipsized */
#define LIST_FLAG_CAN_HIGHLIGHT  (1<<2)   /* column can be highlighted */
#define LIST_FLAG_STOCK_ICON     (1<<3)   /* column contains stock icons */

#ifdef FREMANTLE

/* on hildon a list may be system default (LIST_HILDON_WITHOUT_HEADERS), */
/* or forced to have headers (LIST_HILDON_WITH_HEADERS) */

#define LIST_HILDON_WITH_HEADERS     true
#define LIST_HILDON_WITHOUT_HEADERS  false

#else

/* there is more space on the PC, so always show headers there */
#define LIST_HILDON_WITH_HEADERS true
#define LIST_HILDON_WITHOUT_HEADERS true
#endif

struct list_view_column {
  explicit list_view_column(const char *n, unsigned int fl, int hk = -1)
    : name(n), flags(fl), hlkey(hk) {}
  const char *name;
  unsigned int flags;
  int hlkey; ///< highlight key in case LIST_FLAG_CAN_HIGHLIGHT is set
};

typedef std::pair<const char *, GCallback> list_button;

/**
 * @brief create a new list widget
 * @param show_headers if the table headers should be shown
 * @param btn_flags list button flags
 * @param context the context passed to all callbacks
 * @param buttons list of button texts and their callbacks
 * @param columns definition of the columns that should be shown
 * @param store the data store
 *
 * WARNING: all callbacks have swapped arguments
 */
GtkWidget *list_new(bool show_headers, unsigned int btn_flags, void *context,
                    void(*cb_changed)(GtkTreeSelection*,void*),
                    const std::vector<list_button> &buttons,
                    const std::vector<list_view_column> &columns,
                    GtkListStore *store);

void list_set_custom_user_button(GtkWidget *list, list_button_t id,
				 GtkWidget *widget);
GtkTreeSelection *list_get_selection(GtkWidget *list);
void list_button_enable(GtkWidget *list, list_button_t id, bool enable);

void list_focus_on(GtkWidget *list, GtkTreeIter *iter);
bool list_get_selected(GtkWidget *list, GtkTreeModel **model, GtkTreeIter *iter);
void list_scroll(GtkWidget *list, GtkTreeIter *iter);
void list_view_scroll(GtkTreeView *view, GtkTreeSelection *sel, GtkTreeIter* iter);
