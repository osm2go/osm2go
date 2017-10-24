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
#include "osm.h"

#if __cplusplus < 201103L
#include <tr1/array>
namespace std {
  using namespace tr1;
};
#else
#include <array>
#endif
#include <cassert>

#include <osm2go_cpp.h>

#if !GTK_CHECK_VERSION(2, 18, 0)
#define gtk_widget_is_sensitive(w) (GTK_WIDGET_FLAGS(w) & GTK_SENSITIVE)
#endif

#ifdef FINGER_UI
#define TOOL_ICON(a)  a "_thumb"
#define MENU_ICON(a)  a "_thumb"
#else
#define TOOL_ICON(a)  a
#define MENU_ICON(a)  a
#endif

#define MARKUP "<span size='xx-small'>%s</span>"

class iconbar_gtk : public iconbar_t {
public:
  explicit iconbar_gtk(appdata_t &appdata);

  GtkToolbar * const toolbar;

  GtkWidget * const info;
  GtkWidget * const trash;

  GtkWidget * const node_add;

#ifdef FINGER_UI
  GtkWidget * const menu;
#endif

  GtkWidget * const way_add;
  GtkWidget * const way_node_add;
  GtkWidget * const way_cut;
  GtkWidget * const way_reverse;

  GtkWidget *cancel;
  GtkWidget *ok;

  inline void map_action_idle(bool idle, const object_t &selected);
  inline void map_cancel_ok(bool cancelv, bool okv);
};

static void on_info_clicked(appdata_t *appdata) {
  info_dialog(appdata->window, *appdata);
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

static gint on_way_button_press(GtkMenu *menu, GdkEventButton *event) {
  if(event->type == GDK_BUTTON_PRESS) {
    printf("way clicked\n");

    /* draw a popup menu */
    gtk_menu_popup(menu, O2G_NULLPTR, O2G_NULLPTR, O2G_NULLPTR, O2G_NULLPTR,
		   event->button, event->time);
    return TRUE;
  }
  return FALSE;
}
#endif

/* enable/disable ok and cancel button */
void iconbar_gtk::map_cancel_ok(bool cancelv, bool okv) {
  gtk_widget_set_sensitive(ok, okv ? TRUE : FALSE);
  gtk_widget_set_sensitive(cancel, cancelv ? TRUE : FALSE);
}

void iconbar_t::map_cancel_ok(bool cancel, bool ok) {
  static_cast<iconbar_gtk *>(this)->map_cancel_ok(cancel, ok);
}

static void iconbar_toggle_sel_widgets(iconbar_gtk *iconbar, gboolean value) {
  const std::array<GtkWidget *, 2> sel_widgets = { {
    iconbar->trash,
    iconbar->info
  } };

  for(int i = sel_widgets.size() - 1; i >= 0; i--)
    gtk_widget_set_sensitive(sel_widgets[i], value);
}

static void iconbar_toggle_way_widgets(iconbar_gtk *iconbar, bool value, const object_t &selected) {
  const std::array<GtkWidget *, 2> way_widgets = { {
    iconbar->way_node_add,
    iconbar->way_reverse
  } };

  for(int i = way_widgets.size() - 1; i >= 0; i--)
    gtk_widget_set_sensitive(way_widgets[i], value ? TRUE : FALSE);

  if(value)
    assert(selected.type != ILLEGAL);

  gtk_widget_set_sensitive(iconbar->way_cut,
                           (value && selected.way->node_chain.size() > 2) ? TRUE : FALSE);
}

void iconbar_t::map_item_selected(const object_t &item) {
  bool selected = item.type != ILLEGAL;
  iconbar_toggle_sel_widgets(static_cast<iconbar_gtk *>(this), selected ? TRUE : FALSE);

  bool way_en = selected && item.type == WAY;
  iconbar_toggle_way_widgets(static_cast<iconbar_gtk *>(this), way_en, item);
}

void iconbar_gtk::map_action_idle(bool idle, const object_t &selected) {
  /* icons that are enabled in idle mode */
  std::array<GtkWidget *, 2> action_idle_widgets = { {
    node_add,
    way_add,
  } };

  for(int i = action_idle_widgets.size() - 1; i >= 0; i--)
    gtk_widget_set_sensitive(action_idle_widgets[i], idle ? TRUE : FALSE);

  bool way_en = idle && selected.type == WAY;

  iconbar_toggle_sel_widgets(this, FALSE);
  iconbar_toggle_way_widgets(this, way_en, selected);
}

void iconbar_t::map_action_idle(bool idle, const object_t &selected) {
  static_cast<iconbar_gtk *>(this)->map_action_idle(idle, selected);
}

void iconbar_t::setToolbarEnable(bool en)
{
  gtk_widget_set_sensitive(GTK_WIDGET(static_cast<iconbar_gtk *>(this)->toolbar), en);
}

bool iconbar_t::isCancelEnabled() const
{
  return gtk_widget_is_sensitive(static_cast<const iconbar_gtk *>(this)->cancel) == TRUE;
}

bool iconbar_t::isInfoEnabled() const
{
  return gtk_widget_is_sensitive(static_cast<const iconbar_gtk *>(this)->info) == TRUE;
}

bool iconbar_t::isOkEnabled() const
{
  return gtk_widget_is_sensitive(static_cast<const iconbar_gtk *>(this)->ok) == TRUE;
}

bool iconbar_t::isTrashEnabled() const
{
  return gtk_widget_is_sensitive(static_cast<const iconbar_gtk *>(this)->trash) == TRUE;
}

#ifndef FINGER_UI
static GtkWidget *icon_add(GtkWidget *vbox, appdata_t &appdata,
                           const char *icon_str,
                           void(*func)(map_t *)) {
  GtkWidget *but = gtk_button_new();
  GtkWidget *icon = appdata.icons.widget_load(icon_str);
  gtk_button_set_image(GTK_BUTTON(but), icon);
  g_signal_connect_swapped(GTK_OBJECT(but), "clicked", G_CALLBACK(func), appdata.map);

  gtk_box_pack_start(GTK_BOX(vbox), but, FALSE, FALSE, 0);
  return but;
}
#endif

static GtkWidget *tool_button_label(appdata_t &appdata, GtkToolbar *toolbar,
                                    const char *label_str, const char *icon_str) {
  PangoAttrList *attrs = pango_attr_list_new();
  pango_attr_list_change(attrs, pango_attr_scale_new(PANGO_SCALE_XX_SMALL));
  GtkWidget *label = gtk_label_new(label_str);
  gtk_label_set_attributes(GTK_LABEL(label), attrs);
  pango_attr_list_unref(attrs);

  GtkToolItem *item = gtk_tool_button_new(
               appdata.icons.widget_load(icon_str), O2G_NULLPTR);

  gtk_tool_button_set_label_widget(GTK_TOOL_BUTTON(item), label);

#ifndef FREMANTLE
  gtk_widget_set_tooltip_text(GTK_WIDGET(item), label_str);
#endif

  gtk_toolbar_insert(toolbar, item, -1);

  return GTK_WIDGET(item);
}

static GtkWidget *  __attribute__((nonnull(1,3,4,5)))
                  tool_add(GtkToolbar *toolbar, appdata_t &appdata,
                           const char *icon_str, char *tooltip_str,
                           GCallback func, gpointer context,
                           bool separator = false) {
  GtkWidget *item = tool_button_label(appdata, toolbar, tooltip_str, icon_str);

  g_signal_connect_swapped(GTK_OBJECT(item), "clicked", func, context);

  if(separator)
    gtk_toolbar_insert(toolbar, gtk_separator_tool_item_new(), -1);

  return GTK_WIDGET(item);
}

iconbar_gtk::iconbar_gtk(appdata_t& appdata)
  : iconbar_t()
  , toolbar(GTK_TOOLBAR(gtk_toolbar_new()))
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
#if !GTK_CHECK_VERSION(2, 16, 0)
  gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar), GTK_ORIENTATION_VERTICAL);
#else
  gtk_orientable_set_orientation(GTK_ORIENTABLE(toolbar), GTK_ORIENTATION_VERTICAL);
#endif

  gtk_toolbar_set_style(toolbar,
#ifndef FREMANTLE
                                              GTK_TOOLBAR_ICONS);
#else
                                              GTK_TOOLBAR_BOTH);
#endif
}

GtkWidget *iconbar_t::create(appdata_t &appdata) {
  iconbar_gtk * const iconbar = new iconbar_gtk(appdata);
  appdata.iconbar = iconbar;

  GtkBox * const box = GTK_BOX(gtk_vbox_new(FALSE, 0));

#ifdef FINGER_UI
  gtk_widget_show_all(iconbar->menu);

  /* the way button is special as it pops up a menu for */
  /* further too selection */
  GtkWidget *way = tool_button_label(appdata, iconbar->toolbar, _("Way"), TOOL_ICON("way"));

  gtk_widget_set_size_request(way, -1, 40);

  gtk_widget_set_events(way, GDK_EXPOSURE_MASK);
  gtk_widget_add_events(way, GDK_BUTTON_PRESS_MASK);
  g_signal_connect_swapped(GTK_OBJECT(gtk_bin_get_child(GTK_BIN(way))),
                           "button-press-event",
                           G_CALLBACK(on_way_button_press), iconbar->menu);
#endif

  gtk_box_pack_start(box, GTK_WIDGET(iconbar->toolbar), TRUE, TRUE, 0);

  /* -------------------------------------------------------- */

  /* fremantle has these buttons on the right screen size */
#ifndef FINGER_UI
  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);

#ifdef FREMANTLE
  /* make buttons smaller for non-finger maemo */
  gtk_widget_set_size_request(GTK_WIDGET(hbox), -1, 32);
#endif

  iconbar->ok = icon_add(hbox, appdata, TOOL_ICON("ok"), map_action_ok);
  iconbar->cancel = icon_add(hbox, appdata, TOOL_ICON("cancel"), map_action_cancel);
  gtk_box_pack_end(box, hbox, FALSE, FALSE, 0);
  iconbar->map_cancel_ok(false, false);
#endif

  /* --------------------------------------------------------- */

  iconbar->map_item_selected(object_t());

  return GTK_WIDGET(box);
}

#if defined(FINGER_UI)
/* the ok and cancel buttons are moved to the right screen side on */
/* fremantle. technically they are still part of the iconbar and thus */
/* are registered there */
void iconbar_register_buttons(appdata_t &appdata, GtkWidget *ok, GtkWidget *cancel) {
  assert(appdata.iconbar != O2G_NULLPTR);
  iconbar_gtk * const iconbar = static_cast<iconbar_gtk *>(appdata.iconbar);

  iconbar->ok = ok;
  g_signal_connect_swapped(GTK_OBJECT(ok), "clicked",
                           G_CALLBACK(map_action_ok), appdata.map);
  iconbar->cancel = cancel;
  g_signal_connect_swapped(GTK_OBJECT(cancel), "clicked",
                           G_CALLBACK(map_action_cancel), appdata.map);

  iconbar->map_cancel_ok(false, false);
}
#endif
