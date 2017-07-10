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

#define LOCALEDIR PREFIX "/locale"

#define _(String) gettext(String)

#ifdef USE_HILDON
#ifdef FREMANTLE
#include <hildon/hildon-gtk.h>
#include <hildon/hildon-pannable-area.h>
#endif

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
#include <glib/gstdio.h>

#include "canvas.h"

#ifdef USE_HILDON
#include "dbus.h"
#endif

#ifdef __cplusplus
#include "icon.h"
#include "osm.h"
#endif

enum menu_items {
  MENU_ITEM_MAP_HIDE_SEL,
  MENU_ITEM_MAP_SHOW_ALL,
  MENU_ITEM_WMS_CLEAR,
  MENU_ITEM_WMS_ADJUST,
  MENU_ITEM_TRACK_EXPORT,
  MENU_ITEM_TRACK_CLEAR,
  MENU_ITEM_TRACK_ENABLE_GPS,
  MENU_ITEM_TRACK_FOLLOW_GPS,
  SUBMENU_VIEW,
  SUBMENU_MAP,
  MENU_ITEM_MAP_RELATIONS,
  SUBMENU_WMS,
  SUBMENU_TRACK,
  MENU_ITEM_TRACK_IMPORT,
  MENU_ITEM_MAP_UPLOAD,
  MENU_ITEM_MAP_UNDO_CHANGES,
  MENU_ITEMS_COUNT
};

typedef struct appdata_t {
#ifdef USE_HILDON
  HildonProgram *program;
  HildonWindow *window;
  osso_context_t * const osso_context;
#else
  GtkWidget *window;
#endif

  GtkWidget *vbox;

  GtkWidget *btn_zoom_in, *btn_zoom_out, *btn_detail_popup;

  struct statusbar_t *statusbar;
  struct project_t *project;
  struct iconbar_t *iconbar;
  struct presets_items *presets;

#ifdef USE_HILDON
  dbus_mm_pos_t mmpos;
  GtkWidget *banner;
#endif

  /* flags used to prevent re-appearence of dialogs */
  struct {
    guint not_again;     /* bit is set if dialog is not to be displayed again */
    guint reply;         /* reply to be assumed if "not_again" bit is set */
  } dialog_again;

  GtkWidget *menuitems[MENU_ITEMS_COUNT];

  struct {
    struct track_t *track;
    canvas_item_t *gps_item; // the purple circle
    int warn_cnt;
  } track;

#if !defined(USE_HILDON) || (MAEMO_VERSION_MAJOR < 5)
  GtkWidget *menu_item_view_fullscreen;
#endif

  GtkWidget *menu_item_map_save_changes;

#if defined(USE_HILDON) && (MAEMO_VERSION_MAJOR == 5)
  /* submenues are seperate menues under fremantle */
  GtkWidget *app_menu_view, *app_menu_wms, *app_menu_track;
  GtkWidget *app_menu_map;
#endif

#ifdef __cplusplus
  struct map_t *map;
  struct osm_t *osm;
  class settings_t * const settings;
  class gps_state_t * const gps_state;
  icon_t icons;

  appdata_t();
  ~appdata_t();
#endif
} appdata_t;

#ifdef __cplusplus

void main_ui_enable(appdata_t *appdata);

#endif

#endif // APPDATA_H
