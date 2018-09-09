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

#include "style_widgets.h"
#include "style_p.h"

#include "appdata.h"
#include "map.h"
#include <notifications.h>
#include "osm2go_platform.h"
#include "settings.h"
#include "style.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <vector>

#include <osm2go_cpp.h>
#include "osm2go_i18n.h"
#include "osm2go_platform.h"
#include "osm2go_platform_gtk.h"

using namespace osm2go_platform;

struct combo_add_styles {
  int cnt;
  int &match;
  const std::string &currentstyle;
  std::vector<const char *> &styles;
  combo_add_styles(const std::string &sname, int &m, std::vector<const char *> &s)
    : cnt(0), match(m), currentstyle(sname), styles(s) {}
  void operator()(const std::pair<std::string, std::string> &pair);
};

void combo_add_styles::operator()(const std::pair<std::string, std::string> &pair)
{
  if(match < 0 && style_basename(pair.second) == currentstyle)
    match = cnt;

  styles.push_back(pair.first.c_str());

  cnt++;
}

struct selector_model_functor {
  GtkListStore * const store;
  int cnt;
  int &match;
  const std::string &currentstyle;
  selector_model_functor(GtkListStore *s, int &m, const std::string &current)
    : store(s), cnt(0), match(m), currentstyle(current) { }
  inline void operator()(const std::pair<std::string, std::string> &pair) {
    gtk_list_store_insert_with_values(store, nullptr, -1, 0, pair.first.c_str(), 1, pair.second.c_str(), -1);
    if(match < 0 && style_basename(pair.second) == currentstyle)
      match = cnt;
    cnt++;
  }
};

static GtkWidget *style_select_widget(const std::string &currentstyle,
                                      const std::map<std::string, std::string> &styles) {
  /* there must be at least one style, otherwise */
  /* the program wouldn't be running */
  assert(!styles.empty());

  /* fill combo box with presets */
  int match = -1;
  GtkListStore *store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

  std::for_each(styles.begin(), styles.end(), selector_model_functor(store, match, currentstyle));

  GtkWidget *ret = osm2go_platform::select_widget_wrapped(_("Style"), GTK_TREE_MODEL(store));
  g_object_unref(store);
  osm2go_platform::combo_box_set_active(ret, match);
  return ret;
}

GtkWidget *style_select_widget(const std::string &currentstyle) {
  return style_select_widget(currentstyle, style_scan());
}

static void style_change(appdata_t &appdata, const std::string &style_path) {
  const std::string &new_style = style_basename(style_path);
  /* check if style has really been changed */
  if(settings_t::instance()->style == new_style)
    return;

  style_t *nstyle = style_load_fname(style_path);
  if (nstyle == nullptr) {
    error_dlg(trstring("Error loading style %1").arg(style_path));
    return;
  }

  settings_t::instance()->style = new_style;

  appdata.map->clear(map_t::MAP_LAYER_OBJECTS_ONLY);
  /* let gtk clean up first */
  process_events();

  appdata.style.reset(nstyle);

  /* canvas background may have changed */
  appdata.map->set_bg_color_from_style();

  appdata.map->paint();
}

#ifndef FREMANTLE
/* in fremantle this happens inside the submenu handling since this button */
/* is actually placed inside the submenu there */
void style_select(appdata_t *appdata) {

  g_debug("select style");

  /* ------------------ style dialog ---------------- */
  osm2go_platform::WidgetGuard dialog(gtk_dialog_new_with_buttons(_("Select style"),
                                              GTK_WINDOW(appdata_t::window), GTK_DIALOG_MODAL,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              nullptr));

  gtk_dialog_set_default_response(GTK_DIALOG(dialog.get()), GTK_RESPONSE_ACCEPT);

  GtkWidget *cbox = style_select_widget(settings_t::instance()->style, style_scan());

  GtkWidget *hbox = gtk_hbox_new(FALSE, 8);
  gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("Style:")), TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), cbox, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.get())->vbox), hbox, TRUE, TRUE, 0);

  gtk_widget_show_all(dialog.get());

  if(GTK_RESPONSE_ACCEPT != gtk_dialog_run(GTK_DIALOG(dialog.get()))) {
    g_debug("user clicked cancel");
    return;
  }

  const std::string &style = osm2go_platform::select_widget_value(cbox);
  g_debug("user clicked ok on %s", style.c_str());

  dialog.reset();

  style_change(*appdata, style);
}
#endif

void style_change(appdata_t &appdata, GtkWidget *widget) {
  const std::string &style = osm2go_platform::select_widget_value(widget);
  if(style.empty())
    return;

  style_change(appdata, style);
}

//vim:et:ts=8:sw=2:sts=2:ai
