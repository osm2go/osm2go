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

typedef enum {
  LIST_BUTTON_NEW = 0,
  LIST_BUTTON_EDIT,
  LIST_BUTTON_REMOVE,
  LIST_BUTTON_USER0,
  LIST_BUTTON_USER1,
  LIST_BUTTON_USER2
} list_button_t;

#define LIST_BTN_NEW  (1<<0)   // use "new" instead of "add" button
#define LIST_BTN_BIG  (1<<1)   // all buttons finger sized

/* list item flags */
#define LIST_FLAG_EXPAND         (1<<0)   /* column expands with dialog size */
#define LIST_FLAG_ELLIPSIZE      (1<<1)   /* column expands and text is ellipsized */
#define LIST_FLAG_CAN_HIGHLIGHT  (1<<2)   /* column can be highlighted */
#define LIST_FLAG_STOCK_ICON     (1<<3)   /* column contains stock icons */
#define LIST_FLAG_TOGGLE         (1<<4)   /* column contains a toggle item */

#ifdef USE_HILDON

/* on hildon a list may be system default (LIST_HILDON_WITHOUT_HEADERS), */
/* forced to have headers (LIST_HILDON_WITH_HEADERS) or only be forced */
/* on MAEMO5 (LIST_HILDON_WITH_HEADERS_ON_MAEMO5) */

#define LIST_HILDON_WITH_HEADERS     TRUE
#define LIST_HILDON_WITHOUT_HEADERS  FALSE

#if MAEMO_VERSION_MAJOR <= 4
#define LIST_HILDON_WITH_HEADERS_ON_MAEMO5  FALSE
#else
#define LIST_HILDON_WITH_HEADERS_ON_MAEMO5  TRUE
#endif

GtkWidget *list_new(gboolean show_headers);
#else
#define LIST_HILDON_WITH_HEADERS
#define LIST_HILDON_WITHOUT_HEADERS
#define LIST_HILDON_WITH_HEADERS_ON_MAEMO5
GtkWidget *list_new(void);
#endif

GtkWidget *list_get_view(GtkWidget *list);
void list_set_user_buttons(GtkWidget *list, ...);
void list_set_columns(GtkWidget *list, ...);
void list_button_connect(GtkWidget *list, list_button_t id, 
			 GCallback cb, gpointer data);
void list_set_custom_user_button(GtkWidget *list, list_button_t id, 
				 GtkWidget *widget);
GtkTreeSelection *list_get_selection(GtkWidget *list);
void list_button_enable(GtkWidget *list, list_button_t id, gboolean enable);
void list_set_store(GtkWidget *list, GtkListStore *store);
void list_set_selection_function(GtkWidget *list, GtkTreeSelectionFunc func,
				 gpointer data);
void list_set_static_buttons(GtkWidget *list, int flags, 
GCallback cb_new, GCallback cb_edit, GCallback cb_remove, 
	     gpointer data);
GtkTreeModel *list_get_model(GtkWidget *list);
void list_pre_inplace_edit_tweak (GtkTreeModel *model);
void list_focus_on(GtkWidget *list, GtkTreeIter *iter, gboolean highlight);
gboolean list_get_selected(GtkWidget *list, GtkTreeModel **model,
			   GtkTreeIter *iter);

#endif // LIST_H
