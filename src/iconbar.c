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

#include "appdata.h"

static void on_info_clicked(GtkButton *button, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  info_dialog(GTK_WIDGET(appdata->window), appdata, NULL);
}

static void on_node_add_clicked(GtkButton *button, gpointer data) {
  map_action_set((appdata_t*)data, MAP_ACTION_NODE_ADD);
}

static void on_way_add_clicked(GtkButton *button, gpointer data) {
  map_action_set((appdata_t*)data, MAP_ACTION_WAY_ADD);
}

static void on_way_node_add_clicked(GtkButton *button, gpointer data) {
  map_action_set((appdata_t*)data, MAP_ACTION_WAY_NODE_ADD);
}

static void on_way_reverse_clicked(GtkButton *button, gpointer data) {
  map_edit_way_reverse((appdata_t*)data);
}

static void on_way_cut_clicked(GtkButton *button, gpointer data) {
  map_action_set((appdata_t*)data, MAP_ACTION_WAY_CUT);
}

static void on_relation_add_clicked(GtkButton *button, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  g_assert((appdata->map->selected.object.type == NODE) ||
	   (appdata->map->selected.object.type == WAY));

  relation_add_dialog(appdata, &appdata->map->selected.object);
}

static void on_trash_clicked(GtkButton *button, gpointer data) {
  map_delete_selected((appdata_t*)data);
}

static void on_ok_clicked(GtkButton *button, gpointer data) {
  printf("User ok\n");
  map_action_ok((appdata_t*)data);
}

static void on_cancel_clicked(GtkButton *button, gpointer data) {
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

  gtk_widget_set_sensitive(iconbar->relation_add, map_item && selected);

  if(selected && map_item && map_item->object.type == WAY) {
    gtk_widget_set_sensitive(iconbar->way_node_add, TRUE);
    gtk_widget_set_sensitive(iconbar->way_cut, TRUE);
    gtk_widget_set_sensitive(iconbar->way_reverse, TRUE);
  } else {
    gtk_widget_set_sensitive(iconbar->way_node_add, FALSE);
    gtk_widget_set_sensitive(iconbar->way_cut, FALSE);
    gtk_widget_set_sensitive(iconbar->way_reverse, FALSE);
  }
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
    appdata->iconbar->relation_add,
    NULL
  };

  for(i=0;action_idle_widgets[i];i++) 
    gtk_widget_set_sensitive(action_idle_widgets[i], idle);

  for(i=0;action_disable_widgets[i];i++) 
    gtk_widget_set_sensitive(action_disable_widgets[i], FALSE);

  /* special handling for icons that depend on further state */
  if(!idle) {
    gtk_widget_set_sensitive(appdata->iconbar->way_node_add, FALSE);
    gtk_widget_set_sensitive(appdata->iconbar->way_cut, FALSE);
    gtk_widget_set_sensitive(appdata->iconbar->way_reverse, FALSE);
  } else {
    if(appdata->map->selected.object.type == WAY) {
      gtk_widget_set_sensitive(appdata->iconbar->way_node_add, TRUE);
      gtk_widget_set_sensitive(appdata->iconbar->way_cut, TRUE);
      gtk_widget_set_sensitive(appdata->iconbar->way_reverse, TRUE);
    } else {
      gtk_widget_set_sensitive(appdata->iconbar->way_node_add, FALSE);
      gtk_widget_set_sensitive(appdata->iconbar->way_cut, FALSE);
      gtk_widget_set_sensitive(appdata->iconbar->way_reverse, FALSE);
    }
  }
}

GtkWidget *icon_add(GtkWidget *vbox, appdata_t *appdata, 
		    char *icon_str, 
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
		    char *icon_str,
		    char *tooltip_str,
		    void(*func)(GtkButton*, gpointer)) {
  GtkWidget *item = 
    GTK_WIDGET(gtk_tool_button_new(
	   icon_widget_load(&appdata->icon, icon_str), NULL));

#ifndef USE_HILDON
  gtk_widget_set_tooltip_text(item, tooltip_str);
#endif

  if(func)
    gtk_signal_connect(GTK_OBJECT(item), "clicked",
		       (GtkSignalFunc)func, appdata);

  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);

#ifdef USE_HILDON
#if MAEMO_VERSION_MAJOR == 5
  gtk_widget_set_size_request(GTK_WIDGET(item), -1, 40);
#endif
#endif

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

  gtk_toolbar_set_style(GTK_TOOLBAR(iconbar->toolbar), GTK_TOOLBAR_ICONS);

  /* -------------------------------------------------------- */
  iconbar->trash = tool_add(iconbar->toolbar, appdata,
		    "trash", _("Delete item"), on_trash_clicked);

  /* -------------------------------------------------------- */
  gtk_toolbar_insert(GTK_TOOLBAR(iconbar->toolbar), 
		     gtk_separator_tool_item_new(),-1);
  iconbar->info = tool_add(iconbar->toolbar, appdata, 
                     "info", _("Properties"), on_info_clicked);

  /* -------------------------------------------------------- */
  gtk_toolbar_insert(GTK_TOOLBAR(iconbar->toolbar), 
		     gtk_separator_tool_item_new(),-1);

  iconbar->node_add = tool_add(iconbar->toolbar, appdata, "node_add", 
		       _("Add node"), on_node_add_clicked);

  /* -------------------------------------------------------- */
  gtk_toolbar_insert(GTK_TOOLBAR(iconbar->toolbar), 
		     gtk_separator_tool_item_new(),-1);

  iconbar->way_add = tool_add(iconbar->toolbar, appdata, "way_add", 
			      _("Add way"), on_way_add_clicked);
  iconbar->way_node_add = tool_add(iconbar->toolbar, appdata, 
	"way_node_add", _("Add a node to a way"), on_way_node_add_clicked);
  iconbar->way_cut = tool_add(iconbar->toolbar, appdata, 
        "way_cut", _("Split a way"), on_way_cut_clicked);
  iconbar->way_reverse = tool_add(iconbar->toolbar, appdata, 
        "way_reverse", _("Reverse way"), on_way_reverse_clicked);

  /* -------------------------------------------------------- */
  gtk_toolbar_insert(GTK_TOOLBAR(iconbar->toolbar), 
		     gtk_separator_tool_item_new(),-1);

  iconbar->relation_add = tool_add(iconbar->toolbar, appdata, 
      "relation_add", _("Edit item's relations"), on_relation_add_clicked);

  gtk_box_pack_start(GTK_BOX(box), iconbar->toolbar, TRUE, TRUE, 0);

  /* -------------------------------------------------------- */

  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);

#ifdef USE_HILDON
  gtk_widget_set_size_request(GTK_WIDGET(hbox), -1, 32);
#endif

  iconbar->ok = icon_add(hbox, appdata, "ok", on_ok_clicked);
  iconbar->cancel = icon_add(hbox, appdata, "cancel", on_cancel_clicked);
  gtk_box_pack_end(GTK_BOX(box), hbox, FALSE, FALSE, 0);
  
  /* --------------------------------------------------------- */  

  icon_bar_map_item_selected(appdata, NULL, FALSE);
  icon_bar_map_cancel_ok(appdata, FALSE, FALSE);

  return box;
}

void iconbar_free(iconbar_t *iconbar) {
  if(iconbar)
    g_free(iconbar);
}
