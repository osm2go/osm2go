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
  std::unique_ptr<GtkListStore, g_object_deleter> store(gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING));

  std::for_each(styles.begin(), styles.end(), selector_model_functor(store.get(), match, currentstyle));

  GtkWidget *ret = osm2go_platform::select_widget_wrapped(_("Style"), GTK_TREE_MODEL(store.get()));
  osm2go_platform::combo_box_set_active(ret, match);
  return ret;
}

#ifndef FREMANTLE

/* in fremantle this happens inside the submenu handling since this button */
/* is actually placed inside the submenu there */
void style_select(appdata_t *appdata) {

  g_debug("select style");

  /* ------------------ style dialog ---------------- */
  osm2go_platform::DialogGuard dialog(gtk_dialog_new_with_buttons(_("Select style"),
                                              GTK_WINDOW(appdata_t::window), GTK_DIALOG_MODAL,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              nullptr));

  gtk_dialog_set_default_response(dialog, GTK_RESPONSE_ACCEPT);

  GtkWidget *cbox = style_select_widget(settings_t::instance()->style, style_scan());

  GtkWidget *hbox = gtk_hbox_new(FALSE, 8);
  gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("Style:")), TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), cbox, TRUE, TRUE, 0);
  gtk_box_pack_start(dialog.vbox(), hbox, TRUE, TRUE, 0);

  gtk_widget_show_all(dialog.get());

  if(GTK_RESPONSE_ACCEPT != gtk_dialog_run(dialog)) {
    g_debug("user clicked cancel");
    return;
  }

  const std::string &style = osm2go_platform::select_widget_value(cbox);
  g_debug("user clicked ok on %s", style.c_str());

  dialog.reset();

  style_change(*appdata, style);
}

#else

GtkWidget *style_select_widget(const std::string &currentstyle) {
  return style_select_widget(currentstyle, style_scan());
}

void style_change(appdata_t &appdata, GtkWidget *widget) {
  const std::string &style = osm2go_platform::select_widget_value(widget);
  if(style.empty())
    return;

  style_change(appdata, style);
}

#endif
