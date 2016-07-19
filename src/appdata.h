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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <locale.h>
#include <libintl.h>

#define LOCALEDIR PREFIX "/locale"

#define _(String) gettext(String)
#define N_(String) (String)

#ifdef USE_HILDON
#if (MAEMO_VERSION_MAJOR >= 5)
#define FREMANTLE
#define FREMANTLE_PANNABLE_AREA
#define USE_PANNABLE_AREA
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

#define ZOOM_BUTTONS
#define DETAIL_POPUP

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-inet-connection.h>

#ifdef ENABLE_BROWSER_INTERFACE
#ifndef USE_HILDON
#include <libgnome/gnome-url.h>
#else
#include <tablet-browser-interface.h>
#endif
#endif

#include "pos.h"
#include "osm.h"

#include "canvas.h"
#include "undo.h"

#ifdef USE_HILDON
#include "dbus.h"
#endif

typedef struct appdata_s {
#ifdef USE_HILDON
  HildonProgram *program;
  HildonWindow *window;
  osso_context_t *osso_context;

#else
  GtkWidget *window;
#endif

  GtkWidget *vbox;
  struct map_s *map;
  osm_t *osm;

#ifdef ZOOM_BUTTONS
  GtkWidget *btn_zoom_in, *btn_zoom_out, *btn_detail_popup;
#endif

  struct statusbar_s *statusbar;
  struct settings_s *settings;
  struct project_s *project;
  struct iconbar_s *iconbar;
  struct icon_s *icon;
  struct presets_item_s *presets;

  /* menu items to be enabled and disabled every now and then */
  struct gps_state_s *gps_state;

#ifdef USE_HILDON
  dbus_mm_pos_t mmpos;
  GtkWidget *banner;
#endif
  gboolean banner_is_grabby;

  /* flags used to prevent re-appearence of dialogs */
  struct {
    gulong not;     /* bit is set if dialog is not to be displayed again */
    gulong reply;   /* reply to be assumed if "not" bit is set */
  } dialog_again;

  struct {
    GtkWidget *submenu_track;
    GtkWidget *menu_item_track_import;
    GtkWidget *menu_item_track_export;
    GtkWidget *menu_item_track_clear;
    GtkWidget *menu_item_track_enable_gps;
    GtkWidget *menu_item_track_follow_gps;
    struct track_s *track;
#ifndef ENABLE_LIBLOCATION
    /* when using liblocation, events are generated on position change */
    /* and no seperate timer is required */
    guint handler_id;
#endif
    canvas_item_t *gps_item; // the purple circle
    int warn_cnt;;
  } track;

#if !defined(USE_HILDON) || (MAEMO_VERSION_MAJOR < 5)
  GtkWidget *menu_item_view_fullscreen;
#endif

  GtkWidget *submenu_view;

  GtkWidget *submenu_map;
  GtkWidget *menu_item_map_upload;
  GtkWidget *menu_item_map_undo;
  GtkWidget *menu_item_map_save_changes;
  GtkWidget *menu_item_map_undo_changes;
  GtkWidget *menu_item_map_relations;

  GtkWidget *submenu_wms;
  GtkWidget *menu_item_wms_clear;
  GtkWidget *menu_item_wms_adjust;

  GtkWidget *menu_item_map_hide_sel;
  GtkWidget *menu_item_map_show_all;

#if defined(USE_HILDON) && (MAEMO_VERSION_MAJOR == 5)
  /* submenues are seperate menues under fremantle */
  GtkWidget *app_menu_view, *app_menu_wms, *app_menu_track;
  GtkWidget *app_menu_map;
#endif

  undo_t undo;

} appdata_t;

#include "settings.h"
#include "map.h"
#include "map_hl.h"
#include "osm_api.h"
#include "statusbar.h"
#include "area_edit.h"
#include "project.h"
#include "diff.h"
#include "iconbar.h"
#include "icon.h"
#include "info.h"
#include "gps.h"
#include "wms.h"
#include "relation_edit.h"
#include "misc.h"
#include "map_edit.h"
#include "net_io.h"
#include "banner.h"
#include "list.h"
#include "scale_popup.h"
#include "about.h"

void main_ui_enable(appdata_t *appdata);

#endif // APPDATA_H
