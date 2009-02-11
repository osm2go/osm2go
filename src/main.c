/*
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

#include <stdio.h>
#include <string.h>

#include <curl/curl.h>
#include <gdk/gdkkeysyms.h>

#include "appdata.h"
#include "banner.h"

/* disable/enable main screen control dependant on presence of open project */
static void main_ui_enable(appdata_t *appdata) {
  gboolean project_valid = (appdata->project != NULL);
  gboolean osm_valid = (appdata->osm != NULL);

  /* cancel any action in progress */
  if(GTK_WIDGET_FLAGS(appdata->iconbar->cancel) & GTK_SENSITIVE) 
    map_action_cancel(appdata);

  /* ---- set project name as window title ----- */
#ifndef USE_HILDON
  char *str = NULL;
  if(project_valid) 
    str = g_strdup_printf("OSM2Go - %s", appdata->project->name);
  else 
    str = g_strdup_printf("OSM2Go");
    
  gtk_window_set_title(GTK_WINDOW(appdata->window), str);
  g_free(str);
#else
  if(project_valid) 
    gtk_window_set_title(GTK_WINDOW(appdata->window), appdata->project->name);
  else
    gtk_window_set_title(GTK_WINDOW(appdata->window), "");
#endif

  if(appdata->iconbar && appdata->iconbar->toolbar)
    gtk_widget_set_sensitive(appdata->iconbar->toolbar, osm_valid);

  /* disable all menu entries related to map */
  gtk_widget_set_sensitive(appdata->menu_osm, project_valid);
  gtk_widget_set_sensitive(appdata->menu_item_osm_upload, osm_valid);
  if(appdata->menu_item_osm_undo)
    gtk_widget_set_sensitive(appdata->menu_item_osm_undo, osm_valid);
  gtk_widget_set_sensitive(appdata->menu_item_osm_save_changes, osm_valid);
  gtk_widget_set_sensitive(appdata->menu_item_osm_undo_changes, osm_valid);
  gtk_widget_set_sensitive(appdata->track.menu_track, osm_valid);
  gtk_widget_set_sensitive(appdata->menu_view, osm_valid);
  gtk_widget_set_sensitive(appdata->menu_wms, osm_valid);
  gtk_widget_set_sensitive(appdata->menu_map, osm_valid);
  gtk_widget_set_sensitive(appdata->menu_item_project_close, project_valid);

  if(!project_valid)
    statusbar_set(appdata, _("Please load or create a project"), FALSE);
}

/******************** begin of menu *********************/

static void 
cb_menu_project_open(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  project_load(appdata, NULL);
  main_ui_enable(appdata);
}

static void 
cb_menu_project_close(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  project_close(appdata);
  main_ui_enable(appdata);
}

static void 
cb_menu_about(GtkWidget *window, gpointer data) {
  GtkAboutDialog *about = GTK_ABOUT_DIALOG(gtk_about_dialog_new());

  gtk_about_dialog_set_name(about, "OSM2Go");
  gtk_about_dialog_set_version(about, VERSION);
  gtk_about_dialog_set_copyright(about, _("Copyright 2008"));

  const gchar *authors[] = {
    "Till Harbaum <till@harbaum.org>",
    "Andrew Chadwick <andrewc-osm2go@piffle.org>",
    NULL };

  gtk_about_dialog_set_authors(about, authors);

  gtk_about_dialog_set_website(about,
       _("http://www.harbaum.org/till/maemo"));
  
  gtk_about_dialog_set_comments(about, 
       _("Mobile OSM Editor"));

  gtk_widget_show_all(GTK_WIDGET(about));
  gtk_dialog_run(GTK_DIALOG(about));
  gtk_widget_destroy(GTK_WIDGET(about));
}

void on_window_destroy (GtkWidget *widget, gpointer data);

static void 
cb_menu_quit(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  gtk_widget_destroy(GTK_WIDGET(appdata->window));
}

static void 
cb_menu_upload(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  if(!appdata->osm || !appdata->project) return;

  osm_upload(appdata, appdata->osm, appdata->project);
}

static void 
cb_menu_download(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  if(!appdata->project) return;

  /* if we have valid osm data loaded: save state first */
  if(appdata->osm) {
    /* redraw the entire map by destroying all map items and redrawing them */
    diff_save(appdata->project, appdata->osm);
    map_clear(appdata, MAP_LAYER_OBJECTS_ONLY);
    osm_free(&appdata->icon, appdata->osm);

    appdata->osm = NULL;
  }

  // download
  if(osm_download(GTK_WIDGET(appdata->window), appdata->project)) {
    banner_busy_start(appdata, 1, "Redrawing...");
    appdata->osm = osm_parse(appdata->project->osm);
    diff_restore(appdata, appdata->project, appdata->osm);
    map_paint(appdata);
    banner_busy_stop(appdata); //"Redrawing..."
  }

  main_ui_enable(appdata);
}

static void 
cb_menu_wms_import(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  wms_import(appdata);
}

static void 
cb_menu_wms_clear(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  wms_remove(appdata);
}

static void 
cb_menu_wms_adjust(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  map_action_set(appdata, MAP_ACTION_BG_ADJUST);
}

/* ----------- hide objects for performance reasons ----------- */

static void 
cb_menu_map_hide_sel(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  map_hide_selected(appdata);
}

static void 
cb_menu_map_show_all(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  map_show_all(appdata);
}

/* ---------------------------------------------------------- */

#if 1  // mainly for testing
static void 
cb_menu_redraw(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  /* redraw the entire map by destroying all map items and redrawing them */
  banner_busy_start(appdata, 1, "Redrawing...");
  track_save(appdata->project, appdata->track.track);
  diff_save(appdata->project, appdata->osm);
  map_clear(appdata, MAP_LAYER_ALL);
  osm_free(&appdata->icon, appdata->osm);

  appdata->osm = osm_parse(appdata->project->osm);
  diff_restore(appdata, appdata->project, appdata->osm);
  map_paint(appdata);

  appdata->track.track = track_restore(appdata, appdata->project);
  if(appdata->track.track)
    map_track_draw(appdata->map, appdata->track.track);

  wms_load(appdata);
  banner_busy_stop(appdata); //"Redrawing..."
}
#endif

static void 
cb_menu_style(GtkWidget *widget, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  style_select(GTK_WIDGET(appdata->window), appdata);
}

static void 
cb_menu_map_no_icons(GtkWidget *widget, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  banner_busy_start(appdata, 1, "Redrawing...");
  map_clear(appdata, MAP_LAYER_OBJECTS_ONLY);
  appdata->settings->no_icons = 
    gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
  map_paint(appdata);
  banner_busy_stop(appdata); //"Redrawing..."
}

static void 
cb_menu_map_no_antialias(GtkWidget *widget, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  banner_busy_start(appdata, 1, "Redrawing...");
  map_clear(appdata, MAP_LAYER_OBJECTS_ONLY);
  appdata->settings->no_antialias = 
    gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
  map_paint(appdata);
  banner_busy_stop(appdata); //"Redrawing..."
}

static void 
cb_menu_undo(GtkWidget *widget, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  undo(appdata);

  // the banner will be displayed from within undo with more details
}

static void 
cb_menu_save_changes(GtkWidget *widget, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  diff_save(appdata->project, appdata->osm);
  banner_show_info(appdata, _("Saved local changes"));
}

static void 
cb_menu_undo_changes(GtkWidget *widget, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  if(!yes_no_f(GTK_WIDGET(appdata->window), NULL, 0, 0,
	       _("Discard local changes?"), 
	       _("Throw away all the changes you've not "
		 "uploaded yet? This can't be undone.")))
    return;

  banner_busy_start(appdata, 1, _("Redrawing..."));
  map_clear(appdata, MAP_LAYER_OBJECTS_ONLY);
  osm_free(&appdata->icon, appdata->osm);
  diff_remove(appdata->project);
  appdata->osm = osm_parse(appdata->project->osm);
  map_paint(appdata);
  banner_busy_stop(appdata);  //"Redrawing..."

  banner_show_info(appdata, _("Discarded local changes"));
}

static void 
cb_menu_fullscreen(GtkWidget *widget, gpointer data) {
  appdata_t *appdata = (appdata_t *)data;

  if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
    gtk_window_fullscreen(GTK_WINDOW(appdata->window));
  else
    gtk_window_unfullscreen(GTK_WINDOW(appdata->window));
}

static void 
cb_menu_zoomin(GtkWidget *widget, appdata_t *appdata) {
  if(!appdata || !appdata->map) return;

  map_set_zoom(appdata->map, appdata->map->state->zoom*ZOOM_FACTOR_MENU, TRUE);
  printf("zoom is now %f\n", appdata->map->state->zoom);
}

static void 
cb_menu_zoomout(GtkWidget *widget, appdata_t *appdata) {
  if(!appdata || !appdata->map) return;

  map_set_zoom(appdata->map, appdata->map->state->zoom/ZOOM_FACTOR_MENU, TRUE);
  printf("zoom is now %f\n", appdata->map->state->zoom);
}

static void 
cb_menu_track_import(GtkWidget *window, appdata_t *appdata) {

  /* open a file selector */
  GtkWidget *dialog;
  
#ifdef USE_HILDON
  dialog = hildon_file_chooser_dialog_new(GTK_WINDOW(appdata->window), 
					  GTK_FILE_CHOOSER_ACTION_OPEN);
#else
  dialog = gtk_file_chooser_dialog_new (_("Import track file"),
			GTK_WINDOW(appdata->window),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);
#endif
  
  /* use path if one is present */
  if(appdata->track.import_path) 
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), 
					appdata->track.import_path);

  gtk_widget_show_all (GTK_WIDGET(dialog));
  if (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_FM_OK) {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

    /* load a track */
    track_do(appdata, TRACK_IMPORT, filename);
    if(appdata->track.track) {

      /* save path if gpx was successfully loaded */
      char *r = strrchr(filename, '/');

      /* there is a delimiter, use everything left of it as path */
      if(r) {
	*r = 0;
	if(appdata->track.import_path) g_free(appdata->track.import_path);
	appdata->track.import_path = g_strdup(filename);
	/* restore path ... just in case ... */
	*r = '/';
      }
    }
    g_free (filename);
  }
  
  gtk_widget_destroy (dialog);
}

static void 
cb_menu_track_gps(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  if(gtk_check_menu_item_get_active(
    GTK_CHECK_MENU_ITEM(appdata->track.menu_item_gps))) {
    track_do(appdata, TRACK_GPS, NULL);
  } else {
    track_do(appdata, TRACK_NONE, NULL);
  }
}

static void 
cb_menu_track_export(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  messagef(GTK_WIDGET(appdata->window), _("NIY"),
	   _("Track export is not yet supported."));
}

static void 
cb_menu_track_clear(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  track_do(appdata, TRACK_NONE, NULL);
}




/*
 *  Platform-specific UI tweaks.
 */


#ifndef USE_HILDON
#ifdef PORTRAIT

// Portrait mode, for openmoko-like systems
#define uispecific_main_menu_new gtk_menu_new

#else

// Regular desktop builds
#define uispecific_main_menu_new gtk_menu_bar_new
#define UISPECIFIC_MAIN_MENU_IS_MENU_BAR
#define UISPECIFIC_MENU_HAS_ICONS
#define UISPECIFIC_MENU_HAS_ACCELS

#endif //PORTRAIT
#else//USE_HILDON

// Maemo/Hildon builds
#define uispecific_main_menu_new gtk_menu_new

#endif



// Half-arsed slapdash common menu item constructor. Let's use GtkBuilder
// instead so we have some flexibility.

static GtkWidget *
menu_append_new_item(appdata_t *appdata,
                     GtkWidget *menu_shell,
                     GtkSignalFunc activate_cb,
                     char *label,
                     const gchar *stock_id, // overridden by label, accels
                     const gchar *accel_path,
                     guint accel_key,      // from gdk/gdkkeysyms.h
                     GdkModifierType accel_mods, // e.g. GDK_CONTROL_MASK
                     gboolean is_check, gboolean check_status)
{
  GtkWidget *item = NULL;
  GtkStockItem stock_item;
  gboolean stock_item_known = FALSE;
  if (stock_id != NULL) {
    stock_item_known = gtk_stock_lookup(stock_id, &stock_item);
  }

  // Icons
#ifndef UISPECIFIC_MENU_HAS_ICONS
  item = is_check ? gtk_check_menu_item_new_with_mnemonic (label)
                  : gtk_menu_item_new_with_mnemonic       (label);
#else
  if (is_check || !stock_item_known) {
    item = is_check ? gtk_check_menu_item_new_with_mnemonic (label)
                    : gtk_menu_item_new_with_mnemonic       (label);
  }
  else {
    item = gtk_image_menu_item_new_with_mnemonic(label);
    GtkWidget *stock_image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), stock_image);
  }
#endif

#ifdef UISPECIFIC_MENU_HAS_ACCELS
  // Accelerators
  // Default
  if (accel_path != NULL) {
    gtk_menu_item_set_accel_path(GTK_MENU_ITEM(item), accel_path);
    if (accel_key != 0) {
      gtk_accel_map_add_entry( accel_path, accel_key, accel_mods );
    }
    else if (stock_item_known) {
      gtk_accel_map_add_entry( accel_path, stock_item.keyval,
                               stock_item.modifier );
    }
  }
#endif
 
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_shell), GTK_WIDGET(item));
  if (is_check) {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), check_status);
  }
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(activate_cb), 
		   appdata);
  return item;
}


void menu_create(appdata_t *appdata) { 
  GtkWidget *menu, *item, *submenu;
  GtkWidget *about_quit_items_menu;

  if (g_module_supported()) {
    printf("*** can use GModule: consider using GtkUIManager / GtkBuilder\n");
  }

  menu = uispecific_main_menu_new();
  about_quit_items_menu = menu;

  /* -------------------- Project submenu -------------------- */

  GtkAccelGroup *accel_grp = gtk_accel_group_new();
  item = gtk_menu_item_new_with_mnemonic( _("_Project") );
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
#ifdef UISPECIFIC_MAIN_MENU_IS_MENU_BAR
  about_quit_items_menu = submenu;
#endif

  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_project_open), _("_Open..."),
    GTK_STOCK_OPEN, "<OSM2Go-Main>/Project/Open",
    0, 0, FALSE, FALSE
  );

  appdata->menu_item_project_close = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_project_close), _("_Close"),
    GTK_STOCK_CLOSE, "<OSM2Go-Main>/Project/Close",
    0, 0, FALSE, FALSE
  );

  /* --------------- view menu ------------------- */

#ifndef UISPECIFIC_MENU_IS_MENU_BAR
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
#endif

  appdata->menu_view = item = gtk_menu_item_new_with_mnemonic( _("_View") );
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
  
  appdata->menu_item_view_fullscreen = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_fullscreen), _("_Fullscreen"),
    GTK_STOCK_FULLSCREEN, "<OSM2Go-Main>/View/Fullscreen",
    0, 0, TRUE, FALSE
  );

  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_zoomin), _("Zoom _in"),
    GTK_STOCK_ZOOM_IN, "<OSM2Go-Main>/View/ZoomIn",
    GDK_comma, GDK_CONTROL_MASK, FALSE, FALSE
  );

  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_zoomout), _("Zoom _out"),
    GTK_STOCK_ZOOM_OUT, "<OSM2Go-Main>/View/ZoomOut",
    GDK_period, GDK_CONTROL_MASK, FALSE, FALSE
  );

  /* -------------------- OSM submenu -------------------- */

  appdata->menu_osm = item = gtk_menu_item_new_with_mnemonic( _("_OSM") );
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

  appdata->menu_item_osm_upload = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_upload), _("_Upload..."),
    NULL, "<OSM2Go-Main>/OSM/Upload",
    GDK_u, GDK_SHIFT_MASK|GDK_CONTROL_MASK, FALSE, FALSE
  );

  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_download), _("_Download..."),
    NULL, "<OSM2Go-Main>/OSM/Download",
    GDK_d, GDK_SHIFT_MASK|GDK_CONTROL_MASK, FALSE, FALSE
  );

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  if(getenv("OSM2GO_UNDO_TEST")) {
    appdata->menu_item_osm_undo = menu_append_new_item(
	       appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_undo), _("_Undo"),
	       GTK_STOCK_UNDO, "<OSM2Go-Main>/OSM/Undo",
	       GDK_z, GDK_CONTROL_MASK, FALSE, FALSE
	       );
  } else
    printf("set environment variable OSM2GO_UNDO_TEST to enable undo framework tests\n");

  appdata->menu_item_osm_save_changes = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_save_changes), _("_Save local changes"),
    GTK_STOCK_SAVE, "<OSM2Go-Main>/OSM/SaveChanges",
    GDK_s, GDK_SHIFT_MASK|GDK_CONTROL_MASK, FALSE, FALSE
  );

  appdata->menu_item_osm_undo_changes = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_undo_changes), _("Disca_rd local changes..."),
    GTK_STOCK_DELETE, "<OSM2Go-Main>/OSM/DiscardChanges",
    0, 0, FALSE, FALSE
  );

  /* -------------------- wms submenu -------------------- */

  appdata->menu_wms = item = gtk_menu_item_new_with_mnemonic( _("_WMS") );
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
  
  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_wms_import), _("_Import..."),
    GTK_STOCK_INDEX, "<OSM2Go-Main>/WMS/Import",
    0, 0, FALSE, FALSE
  );

  appdata->menu_item_wms_clear = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_wms_clear), _("_Clear"),
    GTK_STOCK_CLEAR, "<OSM2Go-Main>/WMS/Clear",
    0, 0, FALSE, FALSE
  );
  gtk_widget_set_sensitive(appdata->menu_item_wms_clear, FALSE);

  appdata->menu_item_wms_adjust = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_wms_adjust), _("_Adjust"),
    NULL, "<OSM2Go-Main>/WMS/Adjust",
    0, 0, FALSE, FALSE
  );
  gtk_widget_set_sensitive(appdata->menu_item_wms_adjust, FALSE);

  /* -------------------- map submenu -------------------- */

  appdata->menu_map = item = gtk_menu_item_new_with_mnemonic( _("_Map") );
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
  
  appdata->menu_item_map_hide_sel = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_map_hide_sel), _("_Hide selected"),
    GTK_STOCK_REMOVE, "<OSM2Go-Main>/Map/HideSelected",
    0, 0, FALSE, FALSE
  );
  gtk_widget_set_sensitive(appdata->menu_item_map_hide_sel, FALSE);

  appdata->menu_item_map_show_all = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_map_show_all), _("_Show all"),
    GTK_STOCK_ADD, "<OSM2Go-Main>/Map/ShowAll",
    0, 0, FALSE, FALSE
  );
  gtk_widget_set_sensitive(appdata->menu_item_map_show_all, FALSE);

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_style), _("St_yle..."),
    GTK_STOCK_SELECT_COLOR, "<OSM2Go-Main>/Map/Style",
    0, 0, FALSE, FALSE
  );

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  /* switches mainly intended for testing/debugging */
  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_redraw), _("_Redraw"),
    NULL, "<OSM2Go-Main>/Map/Redraw",
    GDK_r, GDK_CONTROL_MASK, FALSE, FALSE
  );

  appdata->menu_item_map_no_icons = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_map_no_icons), _("No _icons"),
    NULL, "<OSM2Go-Main>/Map/NoIcons",
    0, 0, TRUE, appdata->settings->no_icons
  );

  appdata->menu_item_map_no_antialias = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_map_no_antialias),
    _("No _antialias"),
    NULL, "<OSM2Go-Main>/Map/NoAntialias",
    0, 0, TRUE, appdata->settings->no_antialias
  );

  /* -------------------- track submenu -------------------- */

  appdata->track.menu_track = item = gtk_menu_item_new_with_mnemonic(_("_Track"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
  
  appdata->track.menu_item_import = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_track_import), _("_Import..."),
    NULL, "<OSM2Go-Main>/Track/Import",
    0, 0, FALSE, FALSE
  );

  appdata->track.menu_item_export = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_track_export), _("_Export..."),
    NULL, "<OSM2Go-Main>/Track/Export",
    0, 0, FALSE, FALSE
  );

  appdata->track.menu_item_clear = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_track_clear), _("_Clear"),
    GTK_STOCK_CLEAR, "<OSM2Go-Main>/Track/Clear",
    0, 0, FALSE, FALSE
  );


  appdata->track.menu_item_gps = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_track_gps), _("_GPS"),
    NULL, "<OSM2Go-Main>/Track/GPS",
    GDK_g, GDK_CONTROL_MASK|GDK_SHIFT_MASK, TRUE, FALSE
  );
  
  /* ------------------------------------------------------- */

  gtk_menu_shell_append(GTK_MENU_SHELL(about_quit_items_menu),
                        gtk_separator_menu_item_new());

  menu_append_new_item(
    appdata, about_quit_items_menu, GTK_SIGNAL_FUNC(cb_menu_about), _("_About..."),
    GTK_STOCK_ABOUT, "<OSM2Go-Main>/About",
    0, 0, FALSE, FALSE
  );

  menu_append_new_item(
    appdata, about_quit_items_menu, GTK_SIGNAL_FUNC(cb_menu_quit), _("_Quit"),
    GTK_STOCK_QUIT, "<OSM2Go-Main>/Quit",
    0, 0, FALSE, FALSE
  );

  gtk_window_add_accel_group(GTK_WINDOW(appdata->window), accel_grp);

#ifdef USE_HILDON
  hildon_window_set_menu(appdata->window, GTK_MENU(menu));
#else
  GtkWidget *menu_bar = menu;

#ifndef UISPECIFIC_MAIN_MENU_IS_MENU_BAR
  // we need to make one first
  menu_bar = gtk_menu_bar_new();

  GtkWidget *root_menu = gtk_menu_item_new_with_label (_("Menu"));
  gtk_widget_show(root_menu);

  gtk_menu_bar_append(menu_bar, root_menu); 
  gtk_menu_item_set_submenu(GTK_MENU_ITEM (root_menu), menu);

  gtk_widget_show(menu_bar);
#endif //UISPECIFIC_MAIN_MENU_IS_MENU_BAR

  gtk_box_pack_start(GTK_BOX(appdata->vbox), menu_bar, 0, 0, 0);

#endif //USE_HILDON
}

/********************* end of menu **********************/

#ifdef UISPECIFIC_MENU_HAS_ACCELS
#define ACCELS_FILE "accels"

static void menu_accels_load(appdata_t *appdata) {
  char *accels_file = g_strdup_printf("%s/" ACCELS_FILE,
                                      appdata->settings->base_path);
  gtk_accel_map_load(accels_file);                                      
  g_free(accels_file);
}

static void menu_accels_save(appdata_t *appdata) {
  char *accels_file = g_strdup_printf("%s" ACCELS_FILE,
                                      appdata->settings->base_path);
  gtk_accel_map_save(accels_file);                                      
  g_free(accels_file);
}

#endif


void cleanup(appdata_t *appdata) {
#ifdef UISPECIFIC_MENU_HAS_ACCELS
  menu_accels_save(appdata);
#endif

  settings_save(appdata->settings);

#ifdef USE_HILDON
  if(appdata->osso_context)
    osso_deinitialize(appdata->osso_context);

  appdata->program = NULL;
#endif

  printf("waiting for gtk to shut down ");

  /* let gtk clean up first */
  while(gtk_events_pending()) {
    putchar('.');
    gtk_main_iteration();
  }

  printf(" ok\n");

  /* save project file */
  if(appdata->project)
    project_save(GTK_WIDGET(appdata->window), appdata->project);

  map_remove_bg_image(appdata->map);

  osm_free(&appdata->icon, appdata->osm);

  curl_global_cleanup();

  josm_presets_free(appdata->presets);

  icon_free_all(&appdata->icon);

  gps_release(appdata);

  settings_free(appdata->settings);

  statusbar_free(appdata->statusbar);

  iconbar_free(appdata->iconbar);

  project_free(appdata->project);

  if(appdata->menu_item_osm_undo)
    undo_free(appdata->undo.state);

  puts("everything is gone");
}

void on_window_destroy (GtkWidget *widget, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  gtk_main_quit();
  appdata->window = NULL;
}

gboolean on_window_key_press(GtkWidget *widget, 
			 GdkEventKey *event, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  int handled = FALSE;

  // the map handles some keys on its own ...
  switch(event->keyval) {

#ifdef USE_HILDON
    /* this is in fact a mapping to GDK_F6 */
  case HILDON_HARDKEY_FULLSCREEN:
#else
  case GDK_F11:
#endif
    if(!gtk_check_menu_item_get_active(
             GTK_CHECK_MENU_ITEM(appdata->menu_item_view_fullscreen))) {
      gtk_window_fullscreen(GTK_WINDOW(appdata->window));
      gtk_check_menu_item_set_active(
	     GTK_CHECK_MENU_ITEM(appdata->menu_item_view_fullscreen), TRUE);
      } else {
	gtk_window_unfullscreen(GTK_WINDOW(appdata->window));
	gtk_check_menu_item_set_active(
	     GTK_CHECK_MENU_ITEM(appdata->menu_item_view_fullscreen), FALSE);
      }
    
    handled = TRUE;
    break;
  }

  /* forward unprocessed key presses to map */
  if(!handled && appdata->project && appdata->osm) 
    handled = map_key_press_event(appdata, event);
  
  return handled;
}

int main(int argc, char *argv[]) {
  appdata_t appdata;

  /* init appdata */
  memset(&appdata, 0, sizeof(appdata));

  printf("Using locale for %s in %s\n", PACKAGE, LOCALEDIR);

  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(PACKAGE, "UTF-8");
  textdomain(PACKAGE);

  /* Must initialize libcurl before any threads are started */
  curl_global_init(CURL_GLOBAL_ALL);

  g_thread_init(NULL);
  
  gps_init(&appdata);

  gtk_init (&argc, &argv);

#ifdef USE_HILDON
  printf("Installing osso context for \"org.harbaum." PACKAGE "\"\n");
  appdata.osso_context = osso_initialize("org.harbaum."PACKAGE, 
					 VERSION, TRUE, NULL);
  if(appdata.osso_context == NULL) 
    fprintf(stderr, "error initiating osso context\n");

  dbus_register(&appdata);
#endif

#ifdef USE_HILDON
  /* Create the hildon program and setup the title */
  appdata.program = HILDON_PROGRAM(hildon_program_get_instance());
  g_set_application_name("OSM2Go");
  
  /* Create HildonWindow and set it to HildonProgram */
  appdata.window = HILDON_WINDOW(hildon_window_new());
  hildon_program_add_window(appdata.program, appdata.window);
#else
  /* Create a Window. */
  appdata.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(appdata.window), "OSM2Go");
  /* Set a decent default size for the window. */
  gtk_window_set_default_size(GTK_WINDOW(appdata.window), 
			      DEFAULT_WIDTH, DEFAULT_HEIGHT);
  gtk_window_set_icon(GTK_WINDOW(appdata.window), 
		      icon_load(&appdata.icon, PACKAGE));
#endif

  g_signal_connect(G_OBJECT(appdata.window), "destroy", 
		   G_CALLBACK(on_window_destroy), &appdata);

  g_signal_connect(G_OBJECT(appdata.window), "key_press_event",
 		   G_CALLBACK(on_window_key_press), &appdata);

  /* user specific init */
  appdata.settings = settings_load();  

  appdata.vbox = gtk_vbox_new(FALSE,0);
  menu_create(&appdata);
#ifdef UISPECIFIC_MENU_HAS_ACCELS
  menu_accels_load(&appdata);
#endif

  /* ----------------------- setup main window ---------------- */

  GtkWidget *hbox = gtk_hbox_new(FALSE,0);
  GtkWidget *vbox = gtk_vbox_new(FALSE,0);

#ifdef PORTRAIT
  gtk_box_pack_start(GTK_BOX(vbox), iconbar_new(&appdata), FALSE, FALSE, 0);
#endif

  /* generate main map view */
  GtkWidget *map = map_new(&appdata);
  if(!map) {
    cleanup(&appdata);
    return -1;
  }

  gtk_box_pack_start(GTK_BOX(vbox), map, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), statusbar_new(&appdata), FALSE, FALSE, 0);

#ifndef PORTRAIT
  gtk_box_pack_start(GTK_BOX(hbox), iconbar_new(&appdata), FALSE, FALSE, 0);
#endif
  gtk_box_pack_start(GTK_BOX(hbox), gtk_vseparator_new(), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(appdata.vbox), hbox, TRUE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(appdata.window), appdata.vbox);

  gtk_widget_show_all(GTK_WIDGET(appdata.window));

  appdata.presets = josm_presets_load();

  /* let gtk do its thing before loading the data, */
  /* so the user sees something */
  while(gtk_events_pending()) {
    putchar('.');
    gtk_main_iteration();
  }

  /* load project if one is specified in the settings */
  if(appdata.settings->project)
    project_load(&appdata, appdata.settings->project);

  main_ui_enable(&appdata);

  /* ------------ jump into main loop ---------------- */

  gtk_main();

  puts("gtk_main() left");

  track_save(appdata.project, appdata.track.track);

  /* save a diff if there are dirty entries */
  diff_save(appdata.project, appdata.osm);

  cleanup(&appdata);

  return 0;
}

// vim:et:ts=8:sw=2:sts=2:ai
