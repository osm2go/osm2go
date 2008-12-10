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

#include "version.h"

#define LOCALEDIR "/usr/share/locale"

#define _(String) gettext(String)
#define N_(String) (String)

#ifdef USE_HILDON
#include <hildon/hildon-program.h>
#include <hildon/hildon-file-chooser-dialog.h>
#include <hildon/hildon-file-system-model.h>
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

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-inet-connection.h>

#include "pos.h"
#include "osm.h"

#include "canvas.h"

#ifdef USE_HILDON
#include "dbus.h"
#endif

typedef struct appdata_s {
#ifdef USE_HILDON
  HildonProgram *program;
  HildonWindow *window;
  osso_context_t *osso_context;

  /* and the ability to zoom */
  GtkWidget *fullscreen_menu_item;
#else
  GtkWidget *window;
#endif

  GtkWidget *vbox;
  struct map_s *map;
  osm_t *osm;

  struct statusbar_s *statusbar;
  struct settings_s *settings;
  struct project_s *project;
  struct iconbar_s *iconbar;
  struct icon_s *icon;
  struct presets_item_s *presets;

  /* menu items to be enabled and disabled every now and then */
  gboolean gps_enabled;
  struct gps_state_s *gps_state;

#ifdef USE_HILDON
  dbus_mm_pos_t mmpos;
#endif

  /* flags used to prevent re-appearence of dialogs */
  struct {
    gulong not;     /* bit is set if dialog is not to be displayed again */
    gulong reply;   /* reply to be assumed if "not" bit is set */
  } dialog_again;

  struct {
    char *import_path;
    GtkWidget *menu_track;
    GtkWidget *menu_item_import;
    GtkWidget *menu_item_export;
    GtkWidget *menu_item_clear;
    GtkWidget *menu_item_gps;
    struct track_s *track;
    guint handler_id;
    canvas_item_t *gps_item;      // the purple curcle
  } track;

  GtkWidget *menu_item_project_close;

  GtkWidget *menu_view;

  GtkWidget *menu_osm;
  GtkWidget *menu_item_osm_upload;
  GtkWidget *menu_item_osm_diff;

  GtkWidget *menu_wms;
  GtkWidget *menu_item_wms_clear;
  GtkWidget *menu_item_wms_adjust;

  GtkWidget *menu_map;
  GtkWidget *menu_item_map_hide_sel;
  GtkWidget *menu_item_map_show_all;

} appdata_t;

#include "map.h"
#include "map_hl.h"
#include "osm_api.h"
#include "statusbar.h"
#include "area_edit.h"
#include "project.h"
#include "settings.h"
#include "diff.h"
#include "iconbar.h"
#include "icon.h"
#include "info.h"
#include "track.h"
#include "gps.h"
#include "wms.h"
#include "josm_presets.h"
#include "relation_edit.h"
#include "misc.h"
#include "map_edit.h"
#include "josm_elemstyles.h"
#include "style.h"
#include "net_io.h"

#endif // APPDATA_H
