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
#include "misc.h"

#include <osm2go_cpp.h>

#ifdef FINGER_UI
#define TOOL_ICON(a)  a "_thumb"
#define MENU_ICON(a)  a "_thumb"
#else
#define TOOL_ICON(a)  a
#define MENU_ICON(a)  a
#endif

#define MARKUP "<span size='xx-small'>%s</span>"
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

static void on_info_clicked(appdata_t *appdata) {
  info_dialog(GTK_WIDGET(appdata->window), *appdata);
}

static void on_node_add_clicked(map_t *map) {
  map->set_action(MAP_ACTION_NODE_ADD);
}

static void on_way_add_clicked(map_t *map) {
  map->set_action(MAP_ACTION_WAY_ADD);
}

static void on_way_node_add_clicked(map_t *map) {
  map->set_action(MAP_ACTION_WAY_NODE_ADD);
}

static void on_way_cut_clicked(map_t *map) {
  map->set_action(MAP_ACTION_WAY_CUT);
}

#ifdef FINGER_UI
static GtkWidget * __attribute__((nonnull(1,3,4,5)))
                  menu_add(GtkWidget *menu, appdata_t &appdata,
                           const char *icon_str, const char *menu_str,
                           GCallback func) {

  GtkWidget *item = gtk_image_menu_item_new_with_label(menu_str);

  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
                                appdata.icons.widget_load(icon_str));

  g_signal_connect_swapped(GTK_OBJECT(item), "activate",
                           func, appdata.map);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

  return item;
}

static gint on_way_button_press(GtkWidget *,
                                GdkEventButton *event, iconbar_t *iconbar) {
  if(event->type == GDK_BUTTON_PRESS) {
    printf("way clicked\n");

    /* draw a popup menu */
    gtk_menu_popup(GTK_MENU(iconbar->menu), O2G_NULLPTR, O2G_NULLPTR, O2G_NULLPTR, O2G_NULLPTR,
		   event->button, event->time);
    return TRUE;
  }
  return FALSE;
}
#endif

/* enable/disable ok and cancel button */
void iconbar_t::map_cancel_ok(gboolean cancelv, gboolean okv) {
  gtk_widget_set_sensitive(ok, okv);
  gtk_widget_set_sensitive(cancel, cancelv);
}

static void iconbar_toggle_sel_widgets(iconbar_t *iconbar, gboolean value) {
  GtkWidget *sel_widgets[] = {
    iconbar->trash,
    iconbar->info
  };

  for(int i = ARRAY_SIZE(sel_widgets) - 1; i >= 0; i--)
    gtk_widget_set_sensitive(sel_widgets[i], value);
}

static void iconbar_toggle_way_widgets(iconbar_t *iconbar, gboolean value, const map_item_t *selected) {
  GtkWidget *way_widgets[] = {
    iconbar->way_node_add,
    iconbar->way_reverse
  };

  for(int i = ARRAY_SIZE(way_widgets) - 1; i >= 0; i--)
    gtk_widget_set_sensitive(way_widgets[i], value);

  if(value)
    g_assert_nonnull(selected);

  gtk_widget_set_sensitive(iconbar->way_cut,
                           value && selected->object.way->node_chain.size() > 2);
}

void iconbar_t::map_item_selected(const map_item_t *map_item) {
  gboolean selected = map_item ? TRUE : FALSE;
  iconbar_toggle_sel_widgets(this, selected);

  gboolean way_en = (selected && map_item->object.type == WAY) ?
                    TRUE : FALSE;
  iconbar_toggle_way_widgets(this, way_en, map_item);
}

void iconbar_t::map_action_idle(gboolean idle, const map_item_t &selected) {
  gint i;

  /* icons that are enabled in idle mode */
  GtkWidget *action_idle_widgets[] = {
    node_add,
    way_add,
  };

  for(i = sizeof(action_idle_widgets) / sizeof(*action_idle_widgets) - 1; i >= 0; i--)
    gtk_widget_set_sensitive(action_idle_widgets[i], idle);

  gboolean way_en = (idle && selected.object.type == WAY) ? TRUE : FALSE;

  iconbar_toggle_sel_widgets(this, FALSE);
  iconbar_toggle_way_widgets(this, way_en, &selected);
}

#ifndef FINGER_UI
static GtkWidget *icon_add(GtkWidget *vbox, appdata_t &appdata,
                           const char *icon_str,
                           void(*func)(map_t *)) {
  GtkWidget *but = gtk_button_new();
  GtkWidget *icon = gtk_image_new_from_pixbuf(appdata.icons.load(icon_str));
  gtk_button_set_image(GTK_BUTTON(but), icon);
  g_signal_connect_swapped(GTK_OBJECT(but), "clicked", G_CALLBACK(func), appdata.map);

  gtk_box_pack_start(GTK_BOX(vbox), but, FALSE, FALSE, 0);
  return but;
}
#endif

static GtkWidget *  __attribute__((nonnull(1,3,4,5)))
                  tool_add(GtkWidget *toolbar, appdata_t &appdata,
                           const char *icon_str, char *tooltip_str,
                           GCallback func, gpointer context,
                           bool separator = false) {
  GtkWidget *item =
    GTK_WIDGET(gtk_tool_button_new(
               appdata.icons.widget_load(icon_str), O2G_NULLPTR));

  GtkWidget *label = gtk_label_new(O2G_NULLPTR);
  char *markup = g_markup_printf_escaped(MARKUP, tooltip_str);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  gtk_tool_button_set_label_widget(GTK_TOOL_BUTTON(item), label);

#ifndef USE_HILDON
  gtk_widget_set_tooltip_text(item, tooltip_str);
#endif

  g_signal_connect_swapped(GTK_OBJECT(item), "clicked", func, context);

  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);

  if(separator)
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

  return item;
}

iconbar_t::iconbar_t(appdata_t &appdata)
  : toolbar(gtk_toolbar_new())
  , info(tool_add(toolbar, appdata,
                  TOOL_ICON("info"), _("Properties"),
                  G_CALLBACK(on_info_clicked), &appdata, true))
  , trash(tool_add(toolbar, appdata,
                   TOOL_ICON("trash"), _("Delete"),
                   G_CALLBACK(map_delete_selected), appdata.map, true))
  , node_add(tool_add(toolbar, appdata,
                      TOOL_ICON("node_add"), _("New node"),
                      G_CALLBACK(on_node_add_clicked), appdata.map, true))
#ifdef FINGER_UI
  , menu(gtk_menu_new())
  , way_add(menu_add(menu, appdata, MENU_ICON("way_add"),
            _("Add new way"), G_CALLBACK(on_way_add_clicked)))
  , way_node_add(menu_add(menu, appdata, MENU_ICON("way_node_add"),
                 _("Add new node to way"), G_CALLBACK(on_way_node_add_clicked)))
  , way_cut(menu_add(menu, appdata, MENU_ICON("way_cut"),
            _("Split way"), G_CALLBACK(on_way_cut_clicked)))
  , way_reverse(menu_add(menu, appdata, MENU_ICON("way_reverse"),
                _("Reverse way"), G_CALLBACK(map_edit_way_reverse)))
#else
  , way_add(tool_add(toolbar, appdata,
                     TOOL_ICON("way_add"), _("Add way"),
                     G_CALLBACK(on_way_add_clicked), appdata.map))
  , way_node_add(tool_add(toolbar, appdata,
                          TOOL_ICON("way_node_add"), _("Add node"),
                          G_CALLBACK(on_way_node_add_clicked), appdata.map))
  , way_cut(tool_add(toolbar, appdata,
                     TOOL_ICON("way_cut"), _("Split way"),
                     G_CALLBACK(on_way_cut_clicked), appdata.map))
  , way_reverse(tool_add(toolbar, appdata,
                         TOOL_ICON("way_reverse"), _("Reverse way"),
                         G_CALLBACK(map_edit_way_reverse), appdata.map))
#endif
  , cancel(O2G_NULLPTR)
  , ok(O2G_NULLPTR)
{
#ifndef PORTRAIT
  gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar), GTK_ORIENTATION_VERTICAL);
#endif

  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar),
#if !defined(USE_HILDON) || (MAEMO_VERSION_MAJOR < 5)
                                              GTK_TOOLBAR_ICONS);
#else
                                              GTK_TOOLBAR_BOTH);
#endif
}

GtkWidget *iconbar_new(appdata_t &appdata) {
  iconbar_t * const iconbar = appdata.iconbar = new iconbar_t(appdata);

#ifndef PORTRAIT
  GtkWidget *box = gtk_vbox_new(FALSE, 0);
#else
  GtkWidget *box = gtk_hbox_new(FALSE, 0);
#endif

#ifdef FINGER_UI
  gtk_widget_show_all(iconbar->menu);

  /* the way button is special as it pops up a menu for */
  /* further too selection */
  GtkWidget *way = GTK_WIDGET(gtk_tool_button_new(
               appdata.icons.widget_load(TOOL_ICON("way")), O2G_NULLPTR));

  GtkWidget *label = gtk_label_new(O2G_NULLPTR);
  char *markup = g_markup_printf_escaped(MARKUP, _("Way"));
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  gtk_tool_button_set_label_widget(GTK_TOOL_BUTTON(way), label);

  gtk_widget_set_tooltip_text(way, "Way");

  gtk_toolbar_insert(GTK_TOOLBAR(iconbar->toolbar), GTK_TOOL_ITEM(way), -1);

  gtk_widget_set_size_request(GTK_WIDGET(way), -1, 40);

  gtk_widget_set_events(way, GDK_EXPOSURE_MASK);
  gtk_widget_add_events(way, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(GTK_OBJECT(gtk_bin_get_child(GTK_BIN(way))),
                   "button-press-event",
                   G_CALLBACK(on_way_button_press), appdata.iconbar);
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

  iconbar->ok = icon_add(hbox, appdata, TOOL_ICON("ok"), map_action_ok);
  iconbar->cancel = icon_add(hbox, appdata, TOOL_ICON("cancel"), map_action_cancel);
  gtk_box_pack_end(GTK_BOX(box), hbox, FALSE, FALSE, 0);
  iconbar->map_cancel_ok(FALSE, FALSE);
#endif

  /* --------------------------------------------------------- */

  iconbar->map_item_selected(O2G_NULLPTR);

  return box;
}

#if defined(FINGER_UI)
/* the ok and cancel buttons are moved to the right screen side on */
/* fremantle. technically they are still part of the iconbar and thus */
/* are registered there */
void iconbar_register_buttons(appdata_t &appdata, GtkWidget *ok, GtkWidget *cancel) {
  g_assert_nonnull(appdata.iconbar);

  appdata.iconbar->ok = ok;
  g_signal_connect_swapped(GTK_OBJECT(ok), "clicked",
                           G_CALLBACK(map_action_ok), appdata.map);
  appdata.iconbar->cancel = cancel;
  g_signal_connect_swapped(GTK_OBJECT(cancel), "clicked",
                           G_CALLBACK(map_action_cancel), appdata.map);

  appdata.iconbar->map_cancel_ok(FALSE, FALSE);
}
#endif
