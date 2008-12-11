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

#include "appdata.h"

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
  gtk_widget_set_sensitive(appdata->menu_item_osm_diff, osm_valid);
  gtk_widget_set_sensitive(appdata->track.menu_track, osm_valid);
  gtk_widget_set_sensitive(appdata->menu_view, osm_valid);
  gtk_widget_set_sensitive(appdata->menu_wms, osm_valid);
  gtk_widget_set_sensitive(appdata->menu_map, osm_valid);
  gtk_widget_set_sensitive(appdata->menu_item_project_close, project_valid);

  if(!project_valid)
    statusbar_set(appdata, _("Please load or create a project"), FALSE);
}

/******************** begin of menu *********************/

#if 0 // simplify menu
static struct {
  enum { MENU_END, MENU_ITEM, MENU_SUB, MENU_SUB_END, MENU_SEP }  type;

  char *title;
  GCallback c_handler;
} menu[] = {
  { MENU_SUB, "OSM", NULL },

  { MENU_END,  NULL, NULL },
};
#endif

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
    "Andrew Chadwick",
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
    appdata->osm = osm_parse(appdata->project->osm);
    diff_restore(appdata, appdata->project, appdata->osm);
    map_paint(appdata);
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

/* ----------------------------------------------- ----------- */

#if 1  // mainly for testing
static void 
cb_menu_redraw(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  /* redraw the entire map by destroying all map items and redrawing them */
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
}
#endif

static void 
cb_menu_style(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  style_select(GTK_WIDGET(appdata->window), appdata);
}

static void 
cb_menu_save_changes(GtkWidget *window, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  diff_save(appdata->project, appdata->osm);
  statusbar_set(appdata, _("Saved all changes made to this project so far"), FALSE);
}


#ifdef USE_HILDON
static void 
cb_menu_fullscreen(GtkWidget *widget, gpointer data) {
  appdata_t *appdata = (appdata_t *)data;

  if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
    gtk_window_fullscreen(GTK_WINDOW(appdata->window));
  else
    gtk_window_unfullscreen(GTK_WINDOW(appdata->window));
}
#endif

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

void menu_create(appdata_t *appdata) { 
  GtkWidget *menu, *item, *submenu;
  menu = gtk_menu_new();


  /* -------------------- Project submenu -------------------- */

  item = gtk_menu_item_new_with_label( _("Project") );
  gtk_menu_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

  item = gtk_menu_item_new_with_label( _("Open...") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_project_open), 
		   appdata);

  appdata->menu_item_project_close = item = 
    gtk_menu_item_new_with_label( _("Close") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_project_close), 
		   appdata);

  /* --------------- view menu ------------------- */

  gtk_menu_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

  appdata->menu_view = item = 
    gtk_menu_item_new_with_label( _("View") );
  gtk_menu_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
  
#ifdef USE_HILDON
  appdata->fullscreen_menu_item = 
    item = gtk_check_menu_item_new_with_label( _("Fullscreen") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_fullscreen), 
		   appdata);
#endif

  item = gtk_menu_item_new_with_label( _("Zoom +" ));
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_zoomin), appdata);

  item = gtk_menu_item_new_with_label( _("Zoom -") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", 
		   GTK_SIGNAL_FUNC(cb_menu_zoomout), appdata);

  /* -------------------- OSM submenu -------------------- */

  appdata->menu_osm = item = gtk_menu_item_new_with_label( _("OSM") );
  gtk_menu_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

  appdata->menu_item_osm_upload = item = 
    gtk_menu_item_new_with_label( _("Upload...") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_upload), appdata);

  item = gtk_menu_item_new_with_label( _("Download...") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", 
		   GTK_SIGNAL_FUNC(cb_menu_download), appdata);

  gtk_menu_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  appdata->menu_item_osm_diff = item = 
    gtk_menu_item_new_with_label( _("Save diff file") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_save_changes), 
		   appdata);

  /* -------------------- wms submenu -------------------- */

  appdata->menu_wms = item = gtk_menu_item_new_with_label( _("WMS") );
  gtk_menu_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
  
  item = gtk_menu_item_new_with_label( _("Import...") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_wms_import), 
		   appdata);

  appdata->menu_item_wms_clear = item = 
    gtk_menu_item_new_with_label( _("Clear") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  gtk_widget_set_sensitive(item, FALSE);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_wms_clear), 
		   appdata);

  appdata->menu_item_wms_adjust = item = 
    gtk_menu_item_new_with_label( _("Adjust") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  gtk_widget_set_sensitive(item, FALSE);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_wms_adjust), 
		   appdata);

  /* -------------------- map submenu -------------------- */

  appdata->menu_map = item = gtk_menu_item_new_with_label( _("Map") );
  gtk_menu_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
  
  appdata->menu_item_map_hide_sel = item = 
    gtk_menu_item_new_with_label( _("Hide selected") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  gtk_widget_set_sensitive(item, FALSE);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_map_hide_sel), 
		   appdata);

  appdata->menu_item_map_show_all = item = 
    gtk_menu_item_new_with_label( _("Show all") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  gtk_widget_set_sensitive(item, FALSE);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_map_show_all), 
		   appdata);

  gtk_menu_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  item = gtk_menu_item_new_with_label( _("Redraw") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_redraw), appdata);

  gtk_menu_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  item = gtk_menu_item_new_with_label( _("Style...") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_style), appdata);

  /* -------------------- track submenu -------------------- */

  appdata->track.menu_track = item = gtk_menu_item_new_with_label(_("Track"));
  gtk_menu_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
  
  appdata->track.menu_item_import =
    item = gtk_menu_item_new_with_label( _("Import...") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_track_import), 
		   appdata);

  appdata->track.menu_item_export =
    item = gtk_menu_item_new_with_label( _("Export...") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_track_export), 
		   appdata);

  appdata->track.menu_item_clear =
    item = gtk_menu_item_new_with_label( _("Clear") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_track_clear), 
		   appdata);

  appdata->track.menu_item_gps =
    item = gtk_check_menu_item_new_with_label( _("GPS") );
  gtk_menu_append(GTK_MENU_SHELL(submenu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_track_gps), 
		   appdata);
  
  /* ------------------------------------------------------- */

  gtk_menu_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());


  item = gtk_menu_item_new_with_label( _("About...") );
  gtk_menu_append(GTK_MENU_SHELL(menu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_about), appdata);


  item = gtk_menu_item_new_with_label( _("Quit") );
  gtk_menu_append(GTK_MENU_SHELL(menu), item);
  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(cb_menu_quit), appdata);

#ifdef USE_HILDON
  hildon_window_set_menu(appdata->window, GTK_MENU(menu));
#else
  /* attach ordinary gtk menu */
  GtkWidget *menu_bar = gtk_menu_bar_new();

  GtkWidget *root_menu = gtk_menu_item_new_with_label (_("Menu"));
  gtk_widget_show(root_menu);

  gtk_menu_bar_append(menu_bar, root_menu); 
  gtk_menu_item_set_submenu(GTK_MENU_ITEM (root_menu), menu);

  gtk_widget_show(menu_bar);
  gtk_box_pack_start(GTK_BOX(appdata->vbox), menu_bar, 0, 0, 0);
#endif
}

/********************* end of menu **********************/


void cleanup(appdata_t *appdata) {
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

  //  printf("key event %d\n", event->keyval);

  // the map handles some keys on its own ...
  switch(event->keyval) {
#ifdef USE_HILDON

#if 0
  case HILDON_HARDKEY_SELECT:
    handled = TRUE;
    break;
#endif
    
  case HILDON_HARDKEY_FULLSCREEN:
    {
      gboolean fullscreen = !gtk_check_menu_item_get_active(
	       GTK_CHECK_MENU_ITEM(appdata->fullscreen_menu_item));
      gtk_check_menu_item_set_active(
	       GTK_CHECK_MENU_ITEM(appdata->fullscreen_menu_item), fullscreen);

      if(fullscreen)
	gtk_window_fullscreen(GTK_WINDOW(appdata->window));
      else
	gtk_window_unfullscreen(GTK_WINDOW(appdata->window));

      handled = TRUE;
    }
    break;
#endif
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

  appdata.vbox = gtk_vbox_new(FALSE,0);
  menu_create(&appdata);

  /* user specific init */
  appdata.settings = settings_load();  

  /* ----------------------- setup main window ---------------- */

  GtkWidget *hbox = gtk_hbox_new(FALSE,0);
  GtkWidget *vbox = gtk_vbox_new(FALSE,0);

#ifdef PORTRAIT
  gtk_box_pack_start(GTK_BOX(vbox), iconbar_new(&appdata), FALSE, FALSE, 0);
#endif
  gtk_box_pack_start(GTK_BOX(vbox), map_new(&appdata), TRUE, TRUE, 0);
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
