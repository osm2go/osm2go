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

#include "iconbar.h"

#include "appdata.h"
#include "icon.h"
#include "info.h"
#include "map_edit.h"

#ifdef FINGER_UI
#define TOOL_ICON(a)  a "_thumb"
#define MENU_ICON(a)  a "_thumb"
#else
#define TOOL_ICON(a)  a
#define MENU_ICON(a)  a
#endif

#define MARKUP "<span size='xx-small'>%s</span>"

static void on_info_clicked(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  info_dialog(GTK_WIDGET(appdata->window), appdata, NULL);
}

static void on_node_add_clicked(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  map_action_set((appdata_t*)data, MAP_ACTION_NODE_ADD);
}

static void on_way_add_clicked(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  map_action_set((appdata_t*)data, MAP_ACTION_WAY_ADD);
}

static void on_way_node_add_clicked(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  map_action_set((appdata_t*)data, MAP_ACTION_WAY_NODE_ADD);
}

static void on_way_reverse_clicked(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  map_edit_way_reverse((appdata_t*)data);
}

static void on_way_cut_clicked(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  map_action_set((appdata_t*)data, MAP_ACTION_WAY_CUT);
}

#ifdef FINGER_UI
static GtkWidget *menu_add(GtkWidget *menu, appdata_t *appdata,
			   char *icon_str, char *menu_str,
			   void(*func)(GtkButton*, gpointer)) {

  GtkWidget *item = gtk_image_menu_item_new_with_label(menu_str);

  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		icon_widget_load(&appdata->icon, icon_str));

  if(func)
    gtk_signal_connect(GTK_OBJECT(item), "activate",
		       (GtkSignalFunc)func, appdata);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

  return item;
}

static gint on_way_button_press(GtkWidget *button, GdkEventButton *event,
				gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  if(event->type == GDK_BUTTON_PRESS) {
    //  map_action_set((appdata_t*)data, MAP_ACTION_WAY_ADD);
    printf("way clicked\n");

    /* draw a popup menu */
    gtk_menu_popup(GTK_MENU(appdata->iconbar->menu), NULL, NULL, NULL, NULL,
		   event->button, event->time);
    return TRUE;
  }
  return FALSE;
}

static GtkWidget *popup_menu_create(appdata_t *appdata) {
  GtkWidget *menu = gtk_menu_new();

  appdata->iconbar->way_add =
    menu_add(menu, appdata, MENU_ICON("way_add"),
	     _("Add new way"), on_way_add_clicked);
  appdata->iconbar->way_node_add =
    menu_add(menu, appdata, MENU_ICON("way_node_add"),
	     _("Add new node to way"), on_way_node_add_clicked);
  appdata->iconbar->way_cut =
    menu_add(menu, appdata, MENU_ICON("way_cut"),
	     _("Split way"), on_way_cut_clicked);
  appdata->iconbar->way_reverse =
    menu_add(menu, appdata, MENU_ICON("way_reverse"),
	     _("Reverse way"), on_way_reverse_clicked);

  gtk_widget_show_all(menu);

  return menu;
}
#endif

#ifdef MAIN_GUI_RELATION
static void on_relation_add_clicked(GtkButton *button, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  g_assert((appdata->map->selected.object.type == NODE) ||
	   (appdata->map->selected.object.type == WAY));

  relation_add_dialog(GTK_WIDGET(appdata->window), appdata,
		      &appdata->map->selected.object);
}
#endif

static void on_trash_clicked(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  map_delete_selected((appdata_t*)data);
}

static void on_ok_clicked(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  printf("User ok\n");
  map_action_ok((appdata_t*)data);
}

static void on_cancel_clicked(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  printf("User cancel\n");
  map_action_cancel((appdata_t*)data);
}

/* enable/disable ok and cancel button */
void icon_bar_map_cancel_ok(appdata_t *appdata,
              gboolean cancel, gboolean ok) {
  iconbar_t *iconbar = appdata->iconbar;
  gtk_widget_set_sensitive(iconbar->ok, ok);
  gtk_widget_set_sensitive(iconbar->cancel, cancel);
}

void icon_bar_map_item_selected(appdata_t *appdata,
		map_item_t *map_item, gboolean selected) {
  iconbar_t *iconbar = appdata->iconbar;

  /* one can't remove relations by clicking this while they are */
  /* selected. May change in the future */
  if(selected && (!map_item || map_item->object.type != RELATION))
    gtk_widget_set_sensitive(iconbar->trash, TRUE);
  else
    gtk_widget_set_sensitive(iconbar->trash, FALSE);

  gtk_widget_set_sensitive(iconbar->info, map_item && selected);

#ifdef MAIN_GUI_RELATION
  gtk_widget_set_sensitive(iconbar->relation_add, map_item && selected);
#endif

  gboolean way_en = (selected && map_item && map_item->object.type == WAY) ?
                    TRUE : FALSE;
  gtk_widget_set_sensitive(iconbar->way_node_add, way_en);
  gtk_widget_set_sensitive(iconbar->way_cut, way_en);
  gtk_widget_set_sensitive(iconbar->way_reverse, way_en);
}

/* if a user action is on progress, then disable all buttons that */
/* cause an action to take place or interfere with the action */
void icon_bar_map_action_idle(appdata_t *appdata, gboolean idle) {
  gint i;

  /* icons that are enabled in idle mode */
  GtkWidget *action_idle_widgets[] = {
    appdata->iconbar->node_add,
    appdata->iconbar->way_add,
    NULL
  };

  /* icons that are disabled in idle mode */
  GtkWidget *action_disable_widgets[] = {
    appdata->iconbar->trash,
    appdata->iconbar->info,
#ifdef MAIN_GUI_RELATION
    appdata->iconbar->relation_add,
#endif
    NULL
  };

  for(i=0;action_idle_widgets[i];i++)
    gtk_widget_set_sensitive(action_idle_widgets[i], idle);

  for(i=0;action_disable_widgets[i];i++)
    gtk_widget_set_sensitive(action_disable_widgets[i], FALSE);

  /* special handling for icons that depend on further state */
  gboolean way_en = (idle && (appdata->map->selected.object.type == WAY)) ?
                    TRUE : FALSE;
  gtk_widget_set_sensitive(appdata->iconbar->way_node_add, way_en);
  gtk_widget_set_sensitive(appdata->iconbar->way_cut, way_en);
  gtk_widget_set_sensitive(appdata->iconbar->way_reverse, way_en);
}

static GtkWidget *icon_add(GtkWidget *vbox, appdata_t *appdata,
                           const char *icon_str,
                           void(*func)(GtkButton*, gpointer)) {
  GtkWidget *but = gtk_button_new();
  GtkWidget *icon = gtk_image_new_from_pixbuf(
		      icon_load(&appdata->icon, icon_str));
  gtk_button_set_image(GTK_BUTTON(but), icon);
  gtk_signal_connect(GTK_OBJECT(but), "clicked",
		     (GtkSignalFunc)func, appdata);

  gtk_box_pack_start(GTK_BOX(vbox), but, FALSE, FALSE, 0);
  return but;
}

static GtkWidget *tool_add(GtkWidget *toolbar, appdata_t *appdata,
			   char *icon_str, char *tooltip_str,
			   void(*func)(GtkButton*, gpointer)) {
  GtkWidget *item =
    GTK_WIDGET(gtk_tool_button_new(
	   icon_widget_load(&appdata->icon, icon_str), NULL));

  GtkWidget *label = gtk_label_new(NULL);
  char *markup = g_markup_printf_escaped(MARKUP, tooltip_str);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  gtk_tool_button_set_label_widget(GTK_TOOL_BUTTON(item), label);

#ifndef USE_HILDON
  gtk_widget_set_tooltip_text(item, tooltip_str);
#endif

  if(func)
    gtk_signal_connect(GTK_OBJECT(item), "clicked",
		       (GtkSignalFunc)func, appdata);

  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);

  return item;
}

GtkWidget *iconbar_new(appdata_t *appdata) {
  appdata->iconbar = g_new0(iconbar_t, 1);
  iconbar_t *iconbar = appdata->iconbar;

  iconbar->toolbar = gtk_toolbar_new();

#ifndef PORTRAIT
  GtkWidget *box = gtk_vbox_new(FALSE, 0);
  gtk_toolbar_set_orientation(GTK_TOOLBAR(iconbar->toolbar),
			      GTK_ORIENTATION_VERTICAL);
#else
  GtkWidget *box = gtk_hbox_new(FALSE, 0);
#endif

#if !defined(USE_HILDON) || (MAEMO_VERSION_MAJOR < 5)
  gtk_toolbar_set_style(GTK_TOOLBAR(iconbar->toolbar), GTK_TOOLBAR_ICONS);
#else
  gtk_toolbar_set_style(GTK_TOOLBAR(iconbar->toolbar), GTK_TOOLBAR_BOTH);
#endif

  /* -------------------------------------------------------- */
  iconbar->trash = tool_add(iconbar->toolbar, appdata,
      TOOL_ICON("trash"), _("Delete"), on_trash_clicked);

  /* -------------------------------------------------------- */
  gtk_toolbar_insert(GTK_TOOLBAR(iconbar->toolbar),
		     gtk_separator_tool_item_new(),-1);

  iconbar->info = tool_add(iconbar->toolbar, appdata,
      TOOL_ICON("info"), _("Properties"), on_info_clicked);

  /* -------------------------------------------------------- */
  gtk_toolbar_insert(GTK_TOOLBAR(iconbar->toolbar),
		     gtk_separator_tool_item_new(),-1);

  iconbar->node_add = tool_add(iconbar->toolbar, appdata,
       TOOL_ICON("node_add"), _("New node"), on_node_add_clicked);

  /* -------------------------------------------------------- */
  gtk_toolbar_insert(GTK_TOOLBAR(iconbar->toolbar),
		     gtk_separator_tool_item_new(),-1);

#ifdef FINGER_UI
  iconbar->menu = popup_menu_create(appdata);

  /* the way button is special as it pops up a menu for */
  /* further too selection */
  GtkWidget *way =
    GTK_WIDGET(gtk_tool_button_new(
	   icon_widget_load(&appdata->icon, TOOL_ICON("way")), NULL));

  GtkWidget *label = gtk_label_new(NULL);
  char *markup = g_markup_printf_escaped(MARKUP, _("Way"));
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  gtk_tool_button_set_label_widget(GTK_TOOL_BUTTON(way), label);

  gtk_widget_set_tooltip_text(way, "Way");

  gtk_toolbar_insert(GTK_TOOLBAR(iconbar->toolbar), GTK_TOOL_ITEM(way), -1);

  gtk_widget_set_size_request(GTK_WIDGET(way), -1, 40);

  gtk_widget_set_events(way, GDK_EXPOSURE_MASK);
  gtk_widget_add_events(way, GDK_BUTTON_PRESS_MASK);
  gtk_signal_connect(GTK_OBJECT(gtk_bin_get_child(GTK_BIN(way))),
		     "button-press-event",
		     (GtkSignalFunc)on_way_button_press, appdata);

#else
  iconbar->way_add = tool_add(iconbar->toolbar, appdata,
        TOOL_ICON("way_add"), _("Add way"), on_way_add_clicked);
  iconbar->way_node_add = tool_add(iconbar->toolbar, appdata,
	TOOL_ICON("way_node_add"), _("Add node"),
				   on_way_node_add_clicked);
  iconbar->way_cut = tool_add(iconbar->toolbar, appdata,
        TOOL_ICON("way_cut"), _("Split way"), on_way_cut_clicked);
  iconbar->way_reverse = tool_add(iconbar->toolbar, appdata,
        TOOL_ICON("way_reverse"), _("Reverse way"), on_way_reverse_clicked);
#endif

#ifdef MAIN_GUI_RELATION
#ifndef FINGER_UI
  /* -------------------------------------------------------- */
  gtk_toolbar_insert(GTK_TOOLBAR(iconbar->toolbar),
		     gtk_separator_tool_item_new(),-1);
#endif

  iconbar->relation_add = tool_add(iconbar->toolbar, appdata,
      TOOL_ICON("relation_add"), _("Edit item's relations"),
      on_relation_add_clicked);
#endif

  gtk_box_pack_start(GTK_BOX(box), iconbar->toolbar, TRUE, TRUE, 0);

  /* -------------------------------------------------------- */

  /* fremantle has these buttons on the right screen size */
#ifndef FINGER_UI
  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);

#ifdef USE_HILDON
  /* make buttons smaller for non-finger maemo */
  gtk_widget_set_size_request(GTK_WIDGET(hbox), -1, 32);
#endif

  iconbar->ok = icon_add(hbox, appdata, TOOL_ICON("ok"), on_ok_clicked);
  iconbar->cancel = icon_add(hbox, appdata, TOOL_ICON("cancel"), on_cancel_clicked);
  gtk_box_pack_end(GTK_BOX(box), hbox, FALSE, FALSE, 0);
#endif

  /* --------------------------------------------------------- */

  icon_bar_map_item_selected(appdata, NULL, FALSE);

#ifndef FINGER_UI
  icon_bar_map_cancel_ok(appdata, FALSE, FALSE);
#endif

  return box;
}

void iconbar_free(iconbar_t *iconbar) {
  g_free(iconbar);
}

#if defined(FINGER_UI)
/* the ok and cancel buttons are moved to the right screen side on */
/* fremantle. technically they are still part of the iconbar and thus */
/* are registered there */
void iconbar_register_buttons(appdata_t *appdata, GtkWidget *ok, GtkWidget *cancel) {
  g_assert(appdata->iconbar);

  appdata->iconbar->ok = ok;
  gtk_signal_connect(GTK_OBJECT(ok), "clicked",
		     (GtkSignalFunc)on_ok_clicked, appdata);
  appdata->iconbar->cancel = cancel;
  gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
		     (GtkSignalFunc)on_cancel_clicked, appdata);

  icon_bar_map_cancel_ok(appdata, FALSE, FALSE);
}
#endif
