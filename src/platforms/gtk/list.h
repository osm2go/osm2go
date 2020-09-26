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
#include "osm2go_i18n.h"

enum list_button_t {
  LIST_BUTTON_NEW = 0,
  LIST_BUTTON_EDIT,
  LIST_BUTTON_REMOVE,
  LIST_BUTTON_USER0,
  LIST_BUTTON_USER1,
  LIST_BUTTON_USER2
};

enum flag_values {
  LIST_HILDON_WITH_HEADERS = 0, // this is the default: force headers
  LIST_HILDON_WITHOUT_HEADERS =
#ifdef FREMANTLE
/* on hildon a list may be system default (LIST_HILDON_WITHOUT_HEADERS), */
                                (1<<0),
#else
                                0, // headers are always shown on desktop platform
#endif
  LIST_BTN_2ROW = (1<<4)   // use 2 rows for the buttons
};

/* list item flags */
#define LIST_FLAG_EXPAND         (1<<0)   /* column expands with dialog size */
#define LIST_FLAG_ELLIPSIZE      (1<<1)   /* column expands and text is ellipsized */
#define LIST_FLAG_CAN_HIGHLIGHT  (1<<2)   /* column can be highlighted */
#define LIST_FLAG_STOCK_ICON     (1<<3)   /* column contains stock icons */

struct list_view_column {
  explicit list_view_column(trstring::native_type_arg n, unsigned int fl, int hk = -1)
    : name(n), flags(fl), hlkey(hk) {}
  trstring::native_type name;
  unsigned int flags;
  int hlkey; ///< highlight key in case LIST_FLAG_CAN_HIGHLIGHT is set
};

class list_button {
public:
  inline list_button(trstring::native_type_arg lb, GCallback c)
    : label(lb), cb(c) {}

  // with C++11 the relevant places use move operations which have no
  // problem with the const members
#if __cplusplus < 201103L
  inline list_button &operator=(const list_button &other)
  {
    memcpy(this, &other, sizeof(this));
    return *this;
  }
#endif

  const trstring::native_type label;
  const GCallback cb;
};

typedef void(*list_changed_callback)(GtkTreeSelection*, void*);

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
GtkWidget *list_new(unsigned int flags, void *context,
                    list_changed_callback cb_changed,
                    const std::vector<list_button> &buttons,
                    const std::vector<list_view_column> &columns,
                    GtkTreeModel *store);

/**
 * @brief get the first custom button, i.e. the one without callback
 *
 * Assumes it is LIST_BUTTON_USER1.
 */
GtkWidget *list_get_custom_button(GtkWidget *list);
GtkTreeSelection *list_get_selection(GtkWidget *list);
void list_button_enable(GtkWidget *list, list_button_t id, bool enable);

void list_focus_on(GtkWidget *list, GtkTreeIter *iter);
bool list_get_selected(GtkWidget *list, GtkTreeModel **model, GtkTreeIter *iter);
void list_scroll(GtkWidget *list, GtkTreeIter *iter);
void list_view_scroll(GtkTreeView *view, GtkTreeSelection *sel, GtkTreeIter* iter);
