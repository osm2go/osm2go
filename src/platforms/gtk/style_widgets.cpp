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
#include "misc.h"
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
  std::vector<std::string> &styles;
  combo_add_styles(const std::string &sname, int &m, std::vector<std::string> &s)
    : cnt(0), match(m), currentstyle(sname), styles(s) {}
  void operator()(const std::pair<std::string, std::string> &pair);
};

void combo_add_styles::operator()(const std::pair<std::string, std::string> &pair)
{
  if(match < 0 && style_basename(pair.second) == currentstyle)
    match = cnt;

  styles.push_back(pair.first);

  cnt++;
}

static GtkWidget *style_select_widget(const std::string &currentstyle,
                                      const std::map<std::string, std::string> &styles) {
  /* there must be at least one style, otherwise */
  /* the program wouldn't be running */
  assert(!styles.empty());

  /* fill combo box with presets */
  int match = -1;
  std::vector<std::string> stylesNames;
  std::for_each(styles.begin(), styles.end(),
                combo_add_styles(currentstyle, match, stylesNames));

  return osm2go_platform::string_select_widget(_("Style"), stylesNames, match);
}

GtkWidget *style_select_widget(const std::string &currentstyle) {
  return style_select_widget(currentstyle, style_scan());
}

static void style_change(appdata_t &appdata, const std::string &name,
                         const std::map<std::string, std::string> &styles) {
  const std::map<std::string, std::string>::const_iterator it = styles.find(name);

  assert(it != styles.end());
  const std::string &new_style = style_basename(it->second);

  /* check if style has really been changed */
  if(appdata.settings->style == new_style)
    return;

  style_t *nstyle = style_load_fname(it->second);
  if (nstyle == O2G_NULLPTR) {
    errorf(appdata.window, _("Error loading style %s"), it->second.c_str());
    return;
  }

  appdata.settings->style = new_style;

  appdata.map->clear(map_t::MAP_LAYER_OBJECTS_ONLY);
  /* let gtk clean up first */
  process_events();

  delete appdata.style;
  appdata.style = nstyle;

  /* canvas background may have changed */
  appdata.map->set_bg_color_from_style();

  appdata.map->paint();
}

#ifndef FREMANTLE
/* in fremantle this happens inside the submenu handling since this button */
/* is actually placed inside the submenu there */
void style_select(GtkWidget *parent, appdata_t &appdata) {

  g_debug("select style");

  /* ------------------ style dialog ---------------- */
  osm2go_platform::WidgetGuard dialog(gtk_dialog_new_with_buttons(_("Select style"),
                                              GTK_WINDOW(parent), GTK_DIALOG_MODAL,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              O2G_NULLPTR));

  gtk_dialog_set_default_response(GTK_DIALOG(dialog.get()), GTK_RESPONSE_ACCEPT);

  const std::map<std::string, std::string> &styles = style_scan();
  GtkWidget *cbox = style_select_widget(appdata.settings->style, styles);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 8);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), gtk_label_new(_("Style:")));

  gtk_box_pack_start_defaults(GTK_BOX(hbox), cbox);
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog.get())->vbox), hbox);

  gtk_widget_show_all(dialog.get());

  if(GTK_RESPONSE_ACCEPT != gtk_dialog_run(GTK_DIALOG(dialog.get()))) {
    g_debug("user clicked cancel");
    return;
  }

  const std::string &style = combo_box_get_active_text(cbox);
  g_debug("user clicked ok on %s", style.c_str());

  dialog.reset();

  style_change(appdata, style, styles);
}
#endif

void style_change(appdata_t &appdata, GtkWidget *widget) {
  const std::string &style = combo_box_get_active_text(widget);
  if(style.empty())
    return;

  style_change(appdata, style, style_scan());
}

//vim:et:ts=8:sw=2:sts=2:ai
