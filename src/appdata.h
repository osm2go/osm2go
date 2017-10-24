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

#ifndef APPDATA_H
#define APPDATA_H

#include <locale.h>
#include <libintl.h>

#define _(String) gettext(String)

#ifdef FREMANTLE
#include <hildon/hildon-gtk.h>
#include <hildon/hildon-pannable-area.h>
#include <hildon/hildon-program.h>
#include <hildon/hildon-file-chooser-dialog.h>
#include <hildon/hildon-file-system-model.h>
#include <hildon/hildon-defines.h>
#include <libosso.h>      /* required for screen saver timeout */
#define GTK_FM_OK  GTK_RESPONSE_OK
#define HILDON_ENTRY_NO_AUTOCAP(a) \
  hildon_gtk_entry_set_input_mode(GTK_ENTRY(a),HILDON_GTK_INPUT_MODE_FULL)
#else
#define GTK_FM_OK  GTK_RESPONSE_ACCEPT
#define HILDON_ENTRY_NO_AUTOCAP(a)
#endif

#include <gtk/gtk.h>

#if __cplusplus < 201103L
#include <tr1/array>
namespace std {
  using namespace tr1;
};
#else
#include <array>
#endif

#include "icon.h"
#include "map.h"
#include "osm.h"
#include "uicontrol.h"

struct appdata_t {
  appdata_t();
  ~appdata_t();

  MainUi * const uicontrol;

#ifdef FREMANTLE
  HildonProgram *program;
  HildonWindow *window;
  osso_context_t * const osso_context;
  /* submenues are seperate menues under fremantle */
  GtkWidget *app_menu_view, *app_menu_wms, *app_menu_track;
  GtkWidget *app_menu_map;
#else
  GtkWidget *window;
  GtkWidget *menu_item_view_fullscreen;
#endif

  GtkWidget *btn_zoom_in, *btn_zoom_out;

  class statusbar_t * const statusbar;
  struct project_t *project;
  class iconbar_t *iconbar;
  struct presets_items *presets;

  /* flags used to prevent re-appearence of dialogs */
  struct {
    unsigned int not_again;     /* bit is set if dialog is not to be displayed again */
    unsigned int reply;         /* reply to be assumed if "not_again" bit is set */
  } dialog_again;

  std::array<GtkWidget *, MainUi::MENU_ITEMS_COUNT> menuitems;

  struct {
    struct track_t *track;
    canvas_item_t *gps_item; // the purple circle
    int warn_cnt;
  } track;

  map_state_t map_state;
  map_t *map;
  struct osm_t *osm;
  class settings_t * const settings;
  struct style_t *style;
  class gps_state_t * const gps_state;
  icon_t icons;
};

void main_ui_enable(appdata_t &appdata);

#endif // APPDATA_H
