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

#ifndef LIST_H
#define LIST_H

#include <gtk/gtk.h>

typedef enum {
  LIST_BUTTON_NEW = 0,
  LIST_BUTTON_EDIT,
  LIST_BUTTON_REMOVE,
  LIST_BUTTON_USER0,
  LIST_BUTTON_USER1,
  LIST_BUTTON_USER2
} list_button_t;

#define LIST_BTN_NEW   (1<<0)   // use "new" instead of "add" button
#define LIST_BTN_WIDE  (1<<1)   // use "wide" button layout (i.e. 5 buttons in one row)
#define LIST_BTN_WIDE4 (1<<2)   // same as LIST_BTN_WIDE, but only make room for 1 user button

/* list item flags */
#define LIST_FLAG_EXPAND         (1<<0)   /* column expands with dialog size */
#define LIST_FLAG_ELLIPSIZE      (1<<1)   /* column expands and text is ellipsized */
#define LIST_FLAG_CAN_HIGHLIGHT  (1<<2)   /* column can be highlighted */
#define LIST_FLAG_STOCK_ICON     (1<<3)   /* column contains stock icons */
#define LIST_FLAG_TOGGLE         (1<<4)   /* column contains a toggle item */

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

GtkWidget *list_new(bool show_headers);

GtkWidget *list_get_view(GtkWidget *list);
/**
 * @brief set up additional buttons
 * @param list the list widget
 * @param label0 the label for LIST_BUTTON_USER0
 * @param cb0 the callback for LIST_BUTTON_USER0
 * @param label1 the label for LIST_BUTTON_USER1
 * @param cb1 the callback for LIST_BUTTON_USER1
 * @param label2 the label for LIST_BUTTON_USER2
 * @param cb2 the callback for LIST_BUTTON_USER2
 *
 * Any unused button should have label and cb set to null pointer.s
 *
 * The context pointer passed to the callbacks is the same as set in
 * list_set_static_buttons(), which must be called before.
 */
void list_set_user_buttons(GtkWidget *list,
                           const char *label0, GCallback cb0,
                           const char *label1, GCallback cb1,
                           const char *label2, GCallback cb2);
void list_set_columns(GtkWidget *list, ...);
void list_set_custom_user_button(GtkWidget *list, list_button_t id,
				 GtkWidget *widget);
GtkTreeSelection *list_get_selection(GtkWidget *list);
void list_button_enable(GtkWidget *list, list_button_t id, bool enable);
void list_set_store(GtkWidget *list, GtkListStore *store);

/**
 * @brief register the standard buttons and their callbacks
 * @param list the list widget
 * @param flags list creation flags
 * @param cb_new the callback on the leftmost button, WARNING: swapped arguments
 * @param cb_edit callback for the middle button, WARNING: swapped arguments
 * @param cb_remove callback for the rightmost button, WARNING: swapped arguments
 * @param data context pointer passed to the callbacks
 */
void list_set_static_buttons(GtkWidget *list, int flags,
GCallback cb_new, GCallback cb_edit, GCallback cb_remove,
	     gpointer data);
GtkTreeModel *list_get_model(GtkWidget *list);
void list_focus_on(GtkWidget *list, GtkTreeIter *iter, bool highlight);
bool list_get_selected(GtkWidget *list, GtkTreeModel **model, GtkTreeIter *iter);
void list_override_changed_event(GtkWidget *list, void(*handler)(GtkTreeSelection*,gpointer), gpointer data);
void list_scroll(GtkWidget *list, GtkTreeIter *iter);
void list_view_scroll(GtkTreeView *view, GtkTreeSelection *sel, GtkTreeIter* iter);

#endif // LIST_H
