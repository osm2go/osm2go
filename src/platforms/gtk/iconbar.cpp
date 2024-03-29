/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iconbar.h>

#include <appdata.h>
#include <icon.h>
#include <map.h>
#include <object_dialogs.h>
#include <osm.h>
#include <project.h>

#include <array>
#include <cassert>
#include <gtk/gtk.h>

#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include "osm2go_platform_gtk.h"
#include "osm2go_platform_gtk_icon.h"

namespace {

#if !GTK_CHECK_VERSION(2, 18, 0)
inline gboolean
gtk_widget_is_sensitive(const GtkWidget *w)
{
  return (GTK_WIDGET_FLAGS(w) & GTK_SENSITIVE) ? TRUE : FALSE;
}
#endif

#ifdef FINGER_UI
#define TOOL_ICON(a)  a "_thumb"
#define MENU_ICON(a)  a "_thumb"
#else
#define TOOL_ICON(a)  a
#define MENU_ICON(a)  a
#endif

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

void
on_info_clicked(map_t *map)
{
  map->info_selected();
}

void
on_node_add_clicked(map_t *map)
{
  map->set_action(MAP_ACTION_NODE_ADD);
}

void
on_way_add_clicked(map_t *map)
{
  map->set_action(MAP_ACTION_WAY_ADD);
}

void
on_way_node_add_clicked(map_t *map)
{
  map->set_action(MAP_ACTION_WAY_NODE_ADD);
}

void
on_way_cut_clicked(map_t *map)
{
  map->set_action(MAP_ACTION_WAY_CUT);
}

#ifdef FINGER_UI
GtkWidget * __attribute__((nonnull(1,3,4,5)))
menu_add(GtkWidget *menu, appdata_t &appdata, const char *icon_str, const char *menu_str, GCallback func)
{
  GtkWidget *item = gtk_image_menu_item_new_with_label(menu_str);

  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
                                static_cast<gtk_platform_icon_t &>(appdata.icons).widget_load(icon_str));

  g_signal_connect_swapped(item, "activate", func, appdata.map);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

  return item;
}

gint
on_way_button_press(GtkMenu *menu, GdkEventButton *event)
{
  if(event->type == GDK_BUTTON_PRESS) {
    g_debug("way clicked");

    /* draw a popup menu */
    gtk_menu_popup(menu, nullptr, nullptr, nullptr, nullptr,
		   event->button, event->time);
    return TRUE;
  }
  return FALSE;
}
#endif

void
iconbar_toggle_sel_widgets(iconbar_gtk *iconbar, gboolean value)
{
  const std::array<GtkWidget *, 2> sel_widgets = { {
    iconbar->trash,
    iconbar->info
  } };

  for(int i = sel_widgets.size() - 1; i >= 0; i--)
    gtk_widget_set_sensitive(sel_widgets[i], value);
}

void
iconbar_toggle_way_widgets(iconbar_gtk *iconbar, bool value, const object_t &selected)
{
  const std::array<GtkWidget *, 2> way_widgets = { {
    iconbar->way_node_add,
    iconbar->way_reverse
  } };

  for(int i = way_widgets.size() - 1; i >= 0; i--)
    gtk_widget_set_sensitive(way_widgets[i], value ? TRUE : FALSE);

  if(value)
    assert(selected.type != object_t::ILLEGAL);

  gtk_widget_set_sensitive(iconbar->way_cut,
                           (value && static_cast<way_t *>(selected)->node_chain.size() > 2) ? TRUE : FALSE);
}

} // namespace

void iconbar_t::map_item_selected(const object_t &item) {
  bool selected = item.type != object_t::ILLEGAL;
  iconbar_toggle_sel_widgets(static_cast<iconbar_gtk *>(this), selected ? TRUE : FALSE);

  bool way_en = item.type == object_t::WAY;
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

  bool way_en = idle && selected.type == object_t::WAY;

  iconbar_toggle_sel_widgets(this, FALSE);
  iconbar_toggle_way_widgets(this, way_en, selected);
}

void iconbar_t::map_action_idle(bool idle, const object_t &selected) {
  static_cast<iconbar_gtk *>(this)->map_action_idle(idle, selected);
}

void iconbar_t::setToolbarEnable(bool en)
{
  gtk_widget_set_sensitive(GTK_WIDGET(static_cast<iconbar_gtk *>(this)->toolbar), en ? TRUE : FALSE);
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

namespace {

#ifndef FINGER_UI
GtkWidget * __attribute__((nonnull(1,3,4)))
icon_add(GtkWidget *vbox, appdata_t &appdata, const char *icon_str, void(*func)(map_t *))
{
  GtkWidget *but = gtk_button_new();
  GtkWidget *icon = static_cast<gtk_platform_icon_t &>(appdata.icons).widget_load(icon_str);
  gtk_button_set_image(GTK_BUTTON(but), icon);
  g_signal_connect_swapped(but, "clicked", G_CALLBACK(func), appdata.map);

  gtk_box_pack_start(GTK_BOX(vbox), but, FALSE, FALSE, 0);
  return but;
}
#endif

GtkWidget * __attribute__((nonnull(2,4)))
tool_button_label(icon_t &icons, GtkToolbar *toolbar, trstring::native_type_arg label_str, const char *icon_str)
{
  PangoAttrList *attrs = pango_attr_list_new();
  pango_attr_list_change(attrs, pango_attr_scale_new(PANGO_SCALE_XX_SMALL));
  GtkWidget *label = gtk_label_new(static_cast<const gchar *>(label_str));
  gtk_label_set_attributes(GTK_LABEL(label), attrs);
  pango_attr_list_unref(attrs);

  GtkToolItem *item = gtk_tool_button_new(static_cast<gtk_platform_icon_t &>(icons).widget_load(icon_str), nullptr);

  gtk_tool_button_set_label_widget(GTK_TOOL_BUTTON(item), label);

#ifndef FREMANTLE
  gtk_widget_set_tooltip_text(GTK_WIDGET(item), static_cast<const gchar *>(label_str));
#endif

  gtk_toolbar_insert(toolbar, item, -1);

  return GTK_WIDGET(item);
}

GtkWidget * __attribute__((nonnull(1,3,5)))
tool_add(GtkToolbar *toolbar, icon_t &icons, const char *icon_str, trstring::native_type_arg tooltip_str,
         GCallback func, gpointer context, bool separator = false)
{
  GtkWidget *item = tool_button_label(icons, toolbar, tooltip_str, icon_str);

  g_signal_connect_swapped(item, "clicked", func, context);

  if(separator)
    gtk_toolbar_insert(toolbar, gtk_separator_tool_item_new(), -1);

  return GTK_WIDGET(item);
}

} // namespace

iconbar_gtk::iconbar_gtk(appdata_t& appdata)
  : iconbar_t()
  , toolbar(GTK_TOOLBAR(gtk_toolbar_new()))
  , info(tool_add(toolbar, appdata.icons, TOOL_ICON("info"), _("Properties"),
                  G_CALLBACK(on_info_clicked), appdata.map, true))
  , trash(tool_add(toolbar, appdata.icons, TOOL_ICON("trash"), _("Delete"),
                   G_CALLBACK(map_t::map_delete_selected), appdata.map, true))
  , node_add(tool_add(toolbar, appdata.icons, TOOL_ICON("node_add"), _("New node"),
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
                _("Reverse way"), G_CALLBACK(map_t::edit_way_reverse)))
#else
  , way_add(tool_add(toolbar, appdata.icons, TOOL_ICON("way_add"), _("Add way"),
                     G_CALLBACK(on_way_add_clicked), appdata.map))
  , way_node_add(tool_add(toolbar, appdata.icons, TOOL_ICON("way_node_add"), _("Add node"),
                          G_CALLBACK(on_way_node_add_clicked), appdata.map))
  , way_cut(tool_add(toolbar, appdata.icons, TOOL_ICON("way_cut"), _("Split way"),
                     G_CALLBACK(on_way_cut_clicked), appdata.map))
  , way_reverse(tool_add(toolbar, appdata.icons, TOOL_ICON("way_reverse"), _("Reverse way"),
                         G_CALLBACK(map_t::edit_way_reverse), appdata.map))
#endif
  , cancel(nullptr)
  , ok(nullptr)
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
  appdata.iconbar.reset(iconbar);

#ifdef FINGER_UI
  gtk_widget_show_all(iconbar->menu);

  /* the way button is special as it pops up a menu for */
  /* further too selection */
  GtkWidget *way = tool_button_label(appdata.icons, iconbar->toolbar, _("Way"), TOOL_ICON("way"));

  gtk_widget_set_size_request(way, -1, 40);

  gtk_widget_set_events(way, GDK_EXPOSURE_MASK);
  gtk_widget_add_events(way, GDK_BUTTON_PRESS_MASK);
  g_signal_connect_swapped(gtk_bin_get_child(GTK_BIN(way)), "button-press-event",
                           G_CALLBACK(on_way_button_press), iconbar->menu);
#endif

  GtkBox * const box = GTK_BOX(gtk_vbox_new(FALSE, 0));
  gtk_box_pack_start(box, GTK_WIDGET(iconbar->toolbar), TRUE, TRUE, 0);

  /* -------------------------------------------------------- */

  /* fremantle has these buttons on the right screen size */
#ifndef FINGER_UI
  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);

#ifdef FREMANTLE
  /* make buttons smaller for non-finger maemo */
  gtk_widget_set_size_request(GTK_WIDGET(hbox), -1, 32);
#endif

  iconbar->ok = icon_add(hbox, appdata, TOOL_ICON("ok"), map_t::map_action_ok);
  iconbar->cancel = icon_add(hbox, appdata, TOOL_ICON("cancel"), map_t::map_action_cancel);
  gtk_box_pack_end(box, hbox, FALSE, FALSE, 0);
  iconbar->map_cancel_ok(false, false);
#endif

  /* --------------------------------------------------------- */

  iconbar->map_item_selected(object_t());

  return GTK_WIDGET(box);
}

/* enable/disable ok and cancel button */
void
iconbar_gtk::map_cancel_ok(bool cancelv, bool okv)
{
  gtk_widget_set_sensitive(ok, okv ? TRUE : FALSE);
  gtk_widget_set_sensitive(cancel, cancelv ? TRUE : FALSE);
}

void
iconbar_t::map_cancel_ok(bool cancel, bool ok)
{
  static_cast<iconbar_gtk *>(this)->map_cancel_ok(cancel, ok);
}

#ifdef FINGER_UI
/* the ok and cancel buttons are moved to the right screen side on */
/* fremantle. technically they are still part of the iconbar and thus */
/* are registered there */
void osm2go_platform::iconbar_register_buttons(appdata_t &appdata, GtkWidget *ok, GtkWidget *cancel)
{
  assert(appdata.iconbar);
  iconbar_gtk * const iconbar = static_cast<iconbar_gtk *>(appdata.iconbar.get());

  iconbar->ok = ok;
  g_signal_connect_swapped(ok, "clicked", G_CALLBACK(map_t::map_action_ok), appdata.map);
  iconbar->cancel = cancel;
  g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(map_t::map_action_cancel), appdata.map);

  iconbar->map_cancel_ok(false, false);
}
#endif
