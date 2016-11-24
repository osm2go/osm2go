/*
 * Copyright (C) 2008-2009 Till Harbaum <till@harbaum.org>.
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

#include <stdio.h>
#include <string.h>

#include <curl/curl.h>
#include <gdk/gdkkeysyms.h>

#if defined(USE_HILDON) && (MAEMO_VERSION_MAJOR == 5)
#define FREMANTLE
#include <hildon/hildon-button.h>
#include <hildon/hildon-check-button.h>
#include <hildon/hildon-window-stack.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#endif

#include "appdata.h"
#include "about.h"
#include "banner.h"
#include "diff.h"
#include "gps.h"
#include "iconbar.h"
#include "josm_presets.h"
#include "misc.h"
#include "osm_api.h"
#include "relation_edit.h"
#include "scale_popup.h"
#include "statusbar.h"
#include "style.h"
#include "track.h"
#include "wms.h"

/* disable/enable main screen control dependant on presence of open project */
void main_ui_enable(appdata_t *appdata) {
  gboolean project_valid = (appdata->project != NULL);
  gboolean osm_valid = (appdata->osm != NULL);

  if(!appdata->window) {
    printf("main_ui_enable: main window gone\n");
    return;
  }

  /* cancel any action in progress */
  g_assert(appdata->iconbar->cancel);
  if(GTK_WIDGET_FLAGS(appdata->iconbar->cancel) & GTK_SENSITIVE)
    map_action_cancel(appdata);

  /* ---- set project name as window title ----- */
#if defined(USE_HILDON) && MAEMO_VERSION_MAJOR < 5
  if(project_valid)
    gtk_window_set_title(GTK_WINDOW(appdata->window), appdata->project->name);
  else
    gtk_window_set_title(GTK_WINDOW(appdata->window), "");
#else
  char *str = NULL;
#ifdef USE_HILDON
  if(project_valid)
    str = g_markup_printf_escaped("<b>%s</b> - OSM2Go",
				  appdata->project->name);
  else
    str = g_markup_printf_escaped("OSM2Go");

  hildon_window_set_markup(HILDON_WINDOW(appdata->window), str);
#else
  if(project_valid)
    str = g_strconcat(appdata->project->name, " - OSM2Go", NULL);
  else
    str = g_strdup("OSM2Go");

  gtk_window_set_title(GTK_WINDOW(appdata->window), str);
#endif
  g_free(str);
#endif

  if(appdata->iconbar && appdata->iconbar->toolbar)
    gtk_widget_set_sensitive(appdata->iconbar->toolbar, osm_valid);
  /* disable all menu entries related to map */
  gtk_widget_set_sensitive(appdata->submenu_map, project_valid);
  gtk_widget_set_sensitive(appdata->menu_item_map_upload, osm_valid);
  if(appdata->menu_item_map_undo)
    gtk_widget_set_sensitive(appdata->menu_item_map_undo, osm_valid);
  if(appdata->menu_item_map_save_changes)
    gtk_widget_set_sensitive(appdata->menu_item_map_save_changes, osm_valid);
  gtk_widget_set_sensitive(appdata->menu_item_map_undo_changes, osm_valid);
  gtk_widget_set_sensitive(appdata->menu_item_map_relations, osm_valid);
  gtk_widget_set_sensitive(appdata->track.submenu_track, osm_valid);
  gtk_widget_set_sensitive(appdata->submenu_view, osm_valid);
  gtk_widget_set_sensitive(appdata->submenu_wms, osm_valid);

#ifdef ZOOM_BUTTONS
  gtk_widget_set_sensitive(appdata->btn_zoom_in, osm_valid);
  gtk_widget_set_sensitive(appdata->btn_zoom_out, osm_valid);
#endif

  if(!project_valid)
    statusbar_set(appdata, _("Please load or create a project"), FALSE);
}

/******************** begin of menu *********************/

static void
cb_menu_project_open(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  project_load(appdata, NULL);
  main_ui_enable(appdata);
}

void on_window_destroy (GtkWidget *widget, gpointer data);

static void
cb_menu_about(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  about_box(appdata);
}

#ifndef USE_HILDON
static void
cb_menu_quit(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  gtk_widget_destroy(GTK_WIDGET(appdata->window));
}
#endif

static void
cb_menu_upload(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  if(!appdata->osm || !appdata->project) return;

  if(project_check_demo(GTK_WIDGET(appdata->window), appdata->project))
    return;

  osm_upload(appdata, appdata->osm, appdata->project);
}

static void
cb_menu_download(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  if(!appdata->project) return;

  if(project_check_demo(GTK_WIDGET(appdata->window), appdata->project))
    return;

  /* if we have valid osm data loaded: save state first */
  if(appdata->osm) {
    /* redraw the entire map by destroying all map items and redrawing them */
    diff_save(appdata->project, appdata->osm);
    map_clear(appdata, MAP_LAYER_OBJECTS_ONLY);

    osm_free(appdata->osm);
    appdata->osm = NULL;
  }

  // download
  if(osm_download(GTK_WIDGET(appdata->window), appdata->settings,
		  appdata->project)) {

    banner_busy_start(appdata, 1, "Drawing");
    appdata->osm = osm_parse(appdata->project->path, appdata->project->osm,
                             &appdata->icon);
    diff_restore(appdata, appdata->project, appdata->osm);
    map_paint(appdata);
    banner_busy_stop(appdata); //"Redrawing"
  }

  main_ui_enable(appdata);
}

static void
cb_menu_wms_import(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  wms_import(appdata);
}

static void
cb_menu_wms_clear(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  wms_remove(appdata);
}

static void
cb_menu_wms_adjust(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  map_action_set(appdata, MAP_ACTION_BG_ADJUST);
}

/* ----------- hide objects for performance reasons ----------- */

static void
cb_menu_map_hide_sel(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  map_hide_selected(appdata);
}

static void
cb_menu_map_show_all(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  map_show_all(appdata);
}

/* ---------------------------------------------------------- */

#ifdef FREMANTLE
#define MENU_CHECK_ITEM HildonCheckButton
#define MENU_CHECK_ITEM_ACTIVE(a) hildon_check_button_get_active(a)
#else
#define MENU_CHECK_ITEM GtkCheckMenuItem
#define MENU_CHECK_ITEM_ACTIVE(a) gtk_check_menu_item_get_active(a)
#endif

#ifndef FREMANTLE
static void
cb_menu_style(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  style_select(GTK_WIDGET(appdata->window), appdata);
}
#endif

static void
cb_menu_undo(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  undo(appdata);

  // the banner will be displayed from within undo with more details
}

#ifndef USE_HILDON
static void
cb_menu_save_changes(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  diff_save(appdata->project, appdata->osm);
  banner_show_info(appdata, _("Saved local changes"));
}
#endif

static void
cb_menu_undo_changes(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  // if there is nothing to clean then don't ask
  if (!diff_present(appdata->project) && diff_is_clean(appdata->osm, TRUE))
    return;

  if(!yes_no_f(GTK_WIDGET(appdata->window), NULL, 0, 0,
	       _("Undo all changes?"),
	       _("Throw away all the changes you've not "
		 "uploaded yet? This cannot be undone.")))
    return;

  map_clear(appdata, MAP_LAYER_OBJECTS_ONLY);

  osm_free(appdata->osm);
  appdata->osm = NULL;

  diff_remove(appdata->project);
  appdata->osm = osm_parse(appdata->project->path, appdata->project->osm,
                 &appdata->icon);
  map_paint(appdata);

  banner_show_info(appdata, _("Undo all changes"));
}

static void
cb_menu_osm_relations(G_GNUC_UNUSED GtkMenuItem *item, appdata_t *appdata) {
  /* list relations of all objects */
  relation_list(GTK_WIDGET(appdata->window), appdata, NULL);
}

#if !defined(USE_HILDON) || (MAEMO_VERSION_MAJOR < 5)
static void
cb_menu_fullscreen(MENU_CHECK_ITEM *item, gpointer data) {
  appdata_t *appdata = (appdata_t *)data;

  if(MENU_CHECK_ITEM_ACTIVE(item))
    gtk_window_fullscreen(GTK_WINDOW(appdata->window));
  else
    gtk_window_unfullscreen(GTK_WINDOW(appdata->window));
}
#endif

static void
cb_menu_zoomin(G_GNUC_UNUSED GtkMenuItem *item, appdata_t *appdata) {
  if(!appdata || !appdata->map) return;

  map_set_zoom(appdata->map, appdata->map->state->zoom*ZOOM_FACTOR_MENU, TRUE);
  printf("zoom is now %f\n", appdata->map->state->zoom);
}

static void
cb_menu_zoomout(G_GNUC_UNUSED GtkMenuItem *item, appdata_t *appdata) {
  if(!appdata || !appdata->map) return;

  map_set_zoom(appdata->map, appdata->map->state->zoom/ZOOM_FACTOR_MENU, TRUE);
  printf("zoom is now %f\n", appdata->map->state->zoom);
}

#if defined(FREMANTLE) || (MAEMO_VERSION_MAJOR != 5) || !defined(DETAIL_POPUP)
static void
cb_menu_view_detail_inc(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  printf("detail level increase\n");
  map_detail_increase(appdata->map);
}

static void
cb_menu_view_detail_normal(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  printf("detail level normal\n");
  map_detail_normal(appdata->map);
}

static void
cb_menu_view_detail_dec(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  printf("detail level decrease\n");
  map_detail_decrease(appdata->map);
}
#endif

static void
cb_menu_track_import(G_GNUC_UNUSED GtkMenuItem *item, appdata_t *appdata) {
  g_assert(appdata->settings);

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

  if(appdata->settings->track_path) {
    if(!g_file_test(appdata->settings->track_path, G_FILE_TEST_EXISTS)) {
      char *last_sep = strrchr(appdata->settings->track_path, '/');
      if(last_sep) {
	*last_sep = 0;  // seperate path from file

	/* the user just created a new document */
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
				    appdata->settings->track_path);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
					  last_sep+1);

	/* restore full filename */
	*last_sep = '/';
      }
    } else
      gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog),
				    appdata->settings->track_path);
  }

  gtk_widget_show_all (GTK_WIDGET(dialog));
  if (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_FM_OK) {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

    /* load a track */
    appdata->track.track = track_import(appdata, filename);
    if(appdata->track.track) {
      g_free(appdata->settings->track_path);
      appdata->settings->track_path = g_strdup(filename);
    }
    g_free (filename);
  }

  gtk_widget_destroy (dialog);
}

static void
cb_menu_track_enable_gps(MENU_CHECK_ITEM *item, appdata_t *appdata) {
  track_enable_gps(appdata, MENU_CHECK_ITEM_ACTIVE(item));
}


static void
cb_menu_track_follow_gps(MENU_CHECK_ITEM *item, appdata_t *appdata) {
  appdata->settings->follow_gps = MENU_CHECK_ITEM_ACTIVE(item);
}


static void
cb_menu_track_export(G_GNUC_UNUSED GtkMenuItem *item, appdata_t *appdata) {
  g_assert(appdata->settings);

  /* open a file selector */
  GtkWidget *dialog;

#ifdef USE_HILDON
  dialog = hildon_file_chooser_dialog_new(GTK_WINDOW(appdata->window),
					  GTK_FILE_CHOOSER_ACTION_SAVE);
#else
  dialog = gtk_file_chooser_dialog_new(_("Export track file"),
				       GTK_WINDOW(appdata->window),
				       GTK_FILE_CHOOSER_ACTION_SAVE,
				       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				       GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				       NULL);
#endif

  printf("set filename <%s>\n", appdata->settings->track_path);

  if(appdata->settings->track_path) {
    if(!g_file_test(appdata->settings->track_path, G_FILE_TEST_EXISTS)) {
      char *last_sep = strrchr(appdata->settings->track_path, '/');
      if(last_sep) {
	*last_sep = 0;  // seperate path from file

	/* the user just created a new document */
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
					    appdata->settings->track_path);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
					  last_sep+1);

	/* restore full filename */
	*last_sep = '/';
      }
    } else
      gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog),
				    appdata->settings->track_path);
  }

  if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_FM_OK) {
    gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    if(filename) {
      printf("export to %s\n", filename);

      if(!g_file_test(filename, G_FILE_TEST_EXISTS) ||
	 yes_no_f(dialog, appdata, MISC_AGAIN_ID_EXPORT_OVERWRITE,
		  MISC_AGAIN_FLAG_DONT_SAVE_NO,
		  "Overwrite existing file",
		  "The file already exists. "
		  "Do you really want to replace it?")) {
	g_free(appdata->settings->track_path);
	appdata->settings->track_path = g_strdup(filename);

	track_export(appdata, filename);
      }
    }
  }

  gtk_widget_destroy (dialog);
}


static void
cb_menu_track_clear(G_GNUC_UNUSED GtkMenuItem *item, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  track_clear(appdata);
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


#if !defined(USE_HILDON) || (MAEMO_VERSION_MAJOR < 5)
// Half-arsed slapdash common menu item constructor. Let's use GtkBuilder
// instead so we have some flexibility.

static GtkWidget *
menu_append_new_item(appdata_t *appdata,
                     GtkWidget *menu_shell,
                     GtkSignalFunc activate_cb,
                     char *label,
                     const gchar *icon_name, // stock id or name for icon_load
                                    // overridden by label, accels, icon_name
                     const gchar *accel_path, // must be a static string
                     guint accel_key,      // from gdk/gdkkeysyms.h
                     GdkModifierType accel_mods, // e.g. GDK_CONTROL_MASK
		     gboolean enabled,
                     gboolean is_check, gboolean check_status)
{
  GtkWidget *item = NULL;
  GtkWidget *image = NULL;

  gboolean stock_item_known = FALSE;
  GtkStockItem stock_item;
  if (icon_name != NULL) {
    stock_item_known = gtk_stock_lookup(icon_name, &stock_item);
  }

  // Icons
#ifndef UISPECIFIC_MENU_HAS_ICONS
  item = is_check ? gtk_check_menu_item_new_with_mnemonic (label)
                  : gtk_menu_item_new_with_mnemonic       (label);
#else
  if (is_check) {
    item = gtk_check_menu_item_new_with_mnemonic (label);
  }
  else if (!stock_item_known) {
    GdkPixbuf *pbuf = icon_load(&appdata->icon, icon_name);
    if (pbuf) {
      image = gtk_image_new_from_pixbuf(pbuf);
    }
    if (image) {
      item = gtk_image_menu_item_new_with_mnemonic(label);
      gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    }
    else {
      item = gtk_menu_item_new_with_mnemonic(label);
    }
  }
  else {
    item = gtk_image_menu_item_new_with_mnemonic(label);
    image = gtk_image_new_from_stock(icon_name, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
  }
#endif

#ifdef UISPECIFIC_MENU_HAS_ACCELS
  // Accelerators
  // Default
  if (accel_path != NULL) {
    accel_path = g_intern_static_string(accel_path);
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
  gtk_widget_set_sensitive(GTK_WIDGET(item), enabled);
  if (is_check)
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), check_status);

  g_signal_connect(item, "activate", GTK_SIGNAL_FUNC(activate_cb),
		   appdata);
  return item;
}

static void menu_create(appdata_t *appdata) {
  GtkWidget *menu, *item, *submenu;
  GtkWidget *about_quit_items_menu;

  if (g_module_supported()) {
    printf("*** can use GModule: consider using GtkUIManager / GtkBuilder\n");
  }

  menu = uispecific_main_menu_new();
  about_quit_items_menu = menu;

  /* -------------------- Project submenu -------------------- */

  GtkAccelGroup *accel_grp = gtk_accel_group_new();

#ifndef USE_HILDON
  item = gtk_menu_item_new_with_mnemonic( _("_Project") );
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
#ifdef UISPECIFIC_MAIN_MENU_IS_MENU_BAR
  about_quit_items_menu = submenu;
#endif

  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_project_open), _("_Open"),
    GTK_STOCK_OPEN, "<OSM2Go-Main>/Project/Open",
    0, 0, TRUE, FALSE, FALSE
  );
#else
  menu_append_new_item(
    appdata, menu, GTK_SIGNAL_FUNC(cb_menu_project_open), _("_Project"),
    GTK_STOCK_OPEN, "<OSM2Go-Main>/Project",
    0, 0, TRUE, FALSE, FALSE
  );
#endif

  /* --------------- view menu ------------------- */

#ifndef UISPECIFIC_MAIN_MENU_IS_MENU_BAR
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
#endif

  appdata->submenu_view = item = gtk_menu_item_new_with_mnemonic( _("_View") );
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

#if !defined(USE_HILDON) || (MAEMO_VERSION_MAJOR < 5)
  appdata->menu_item_view_fullscreen = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_fullscreen), _("_Fullscreen"),
    GTK_STOCK_FULLSCREEN, "<OSM2Go-Main>/View/Fullscreen",
    0, 0, TRUE, TRUE, FALSE
  );
#endif

#if !defined(ZOOM_BUTTONS) || !defined(USE_HILDON)
  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_zoomin), _("Zoom _in"),
    GTK_STOCK_ZOOM_IN, "<OSM2Go-Main>/View/ZoomIn",
    GDK_comma, GDK_CONTROL_MASK, TRUE, FALSE, FALSE
  );

  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_zoomout), _("Zoom _out"),
    GTK_STOCK_ZOOM_OUT, "<OSM2Go-Main>/View/ZoomOut",
    GDK_period, GDK_CONTROL_MASK, TRUE, FALSE, FALSE
  );
#endif

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_view_detail_inc), _("More details"),
    NULL, "<OSM2Go-Main>/View/DetailInc",
    GDK_period, GDK_MOD1_MASK, TRUE, FALSE, FALSE
  );

  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_view_detail_normal), _("Normal details"),
    NULL, "<OSM2Go-Main>/View/DetailNormal",
    0, 0, TRUE, FALSE, FALSE
  );

  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_view_detail_dec), _("Less details"),
    NULL, "<OSM2Go-Main>/View/DetailDec",
    GDK_comma, GDK_MOD1_MASK, TRUE, FALSE, FALSE
  );

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  appdata->menu_item_map_hide_sel = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_map_hide_sel), _("_Hide selected"),
    GTK_STOCK_REMOVE, "<OSM2Go-Main>/View/HideSelected",
    0, 0, TRUE, FALSE, FALSE
  );
  gtk_widget_set_sensitive(appdata->menu_item_map_hide_sel, FALSE);

  appdata->menu_item_map_show_all = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_map_show_all), _("_Show all"),
    GTK_STOCK_ADD, "<OSM2Go-Main>/View/ShowAll",
    0, 0, TRUE, FALSE, FALSE
  );
  gtk_widget_set_sensitive(appdata->menu_item_map_show_all, FALSE);

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_style), _("St_yle"),
    GTK_STOCK_SELECT_COLOR, "<OSM2Go-Main>/View/Style",
    0, 0, TRUE, FALSE, FALSE
  );

  /* -------------------- map submenu -------------------- */

  appdata->submenu_map = item = gtk_menu_item_new_with_mnemonic( _("_Map") );
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

  appdata->menu_item_map_upload = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_upload), _("_Upload"),
    "upload.16", "<OSM2Go-Main>/Map/Upload",
    GDK_u, GDK_SHIFT_MASK|GDK_CONTROL_MASK, TRUE, FALSE, FALSE
  );

  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_download), _("_Download"),
    "download.16", "<OSM2Go-Main>/Map/Download",
    GDK_d, GDK_SHIFT_MASK|GDK_CONTROL_MASK, TRUE, FALSE, FALSE
  );

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  if(getenv("OSM2GO_UNDO_TEST")) {
    appdata->menu_item_map_undo = menu_append_new_item(
	       appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_undo), _("_Undo"),
	       GTK_STOCK_UNDO, "<OSM2Go-Main>/Map/Undo",
	       GDK_z, GDK_CONTROL_MASK, TRUE, FALSE, FALSE
	       );
  } else
    printf("set environment variable OSM2GO_UNDO_TEST to enable undo framework tests\n");

#ifndef USE_HILDON
  appdata->menu_item_map_save_changes = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_save_changes), _("_Save local changes"),
    GTK_STOCK_SAVE, "<OSM2Go-Main>/Map/SaveChanges",
    GDK_s, GDK_SHIFT_MASK|GDK_CONTROL_MASK, TRUE, FALSE, FALSE
  );
#endif

  appdata->menu_item_map_undo_changes = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_undo_changes), _("Undo _all"),
    GTK_STOCK_DELETE, "<OSM2Go-Main>/Map/UndoAll",
    0, 0, TRUE, FALSE, FALSE
  );

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());
  appdata->menu_item_map_relations = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_osm_relations), _("_Relations"),
    NULL, "<OSM2Go-Main>/Map/Relations",
    GDK_r, GDK_SHIFT_MASK|GDK_CONTROL_MASK, TRUE, FALSE, FALSE
  );

  /* -------------------- wms submenu -------------------- */

  appdata->submenu_wms = item = gtk_menu_item_new_with_mnemonic( _("_WMS") );
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

  menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_wms_import), _("_Import"),
    GTK_STOCK_INDEX, "<OSM2Go-Main>/WMS/Import",
    0, 0, TRUE, FALSE, FALSE
  );

  appdata->menu_item_wms_clear = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_wms_clear), _("_Clear"),
    GTK_STOCK_CLEAR, "<OSM2Go-Main>/WMS/Clear",
    0, 0, TRUE, FALSE, FALSE
  );
  gtk_widget_set_sensitive(appdata->menu_item_wms_clear, FALSE);

  appdata->menu_item_wms_adjust = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_wms_adjust), _("_Adjust"),
    NULL, "<OSM2Go-Main>/WMS/Adjust",
    0, 0, TRUE, FALSE, FALSE
  );
  gtk_widget_set_sensitive(appdata->menu_item_wms_adjust, FALSE);

  /* -------------------- track submenu -------------------- */

  appdata->track.submenu_track = item =
    gtk_menu_item_new_with_mnemonic(_("_Track"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  submenu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

  appdata->track.menu_item_track_import = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_track_import), _("_Import"),
    NULL, "<OSM2Go-Main>/Track/Import",
    0, 0, TRUE, FALSE, FALSE
  );

  appdata->track.menu_item_track_export = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_track_export), _("_Export"),
    NULL, "<OSM2Go-Main>/Track/Export",
    0, 0, FALSE, FALSE, FALSE
  );

  appdata->track.menu_item_track_clear = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_track_clear), _("_Clear"),
    GTK_STOCK_CLEAR, "<OSM2Go-Main>/Track/Clear",
    0, 0, FALSE, FALSE, FALSE
  );


  appdata->track.menu_item_track_enable_gps = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_track_enable_gps),_("_GPS enable"),
    NULL, "<OSM2Go-Main>/Track/GPS",
    GDK_g, GDK_CONTROL_MASK|GDK_SHIFT_MASK, TRUE, TRUE,
    appdata->settings->enable_gps
  );

  appdata->track.menu_item_track_follow_gps = menu_append_new_item(
    appdata, submenu, GTK_SIGNAL_FUNC(cb_menu_track_follow_gps), _("GPS follow"),
    NULL, "<OSM2Go-Main>/Track/Follow",
    0, 0, appdata->settings->enable_gps, TRUE,
    appdata->settings->follow_gps
  );

  /* ------------------------------------------------------- */

  gtk_menu_shell_append(GTK_MENU_SHELL(about_quit_items_menu),
                        gtk_separator_menu_item_new());

  menu_append_new_item(
    appdata, about_quit_items_menu, GTK_SIGNAL_FUNC(cb_menu_about), _("_About"),
    GTK_STOCK_ABOUT, "<OSM2Go-Main>/About",
    0, 0, TRUE, FALSE, FALSE
  );

#ifndef USE_HILDON
  menu_append_new_item(
    appdata, about_quit_items_menu, GTK_SIGNAL_FUNC(cb_menu_quit), _("_Quit"),
    GTK_STOCK_QUIT, "<OSM2Go-Main>/Quit",
    0, 0, TRUE, FALSE, FALSE
  );
#endif

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

void menu_cleanup(G_GNUC_UNUSED appdata_t *appdata) { }

#else // !defined(USE_HILDON) || (MAEMO_VERSION_MAJOR < 5)

void submenu_entry(appdata_t *appdata, HildonAppMenu *menu,
		   const char *label, const char *value,
		   GtkSignalFunc activate_cb) {

}

typedef struct {
  const char *label, *value;
  gboolean enabled;
  gboolean (*toggle)(appdata_t *appdata);
  gulong offset;
  GtkSignalFunc activate_cb;
} menu_entry_t;

typedef struct {
  const char *title;
  const menu_entry_t *menu;
  int len;
} submenu_t;

static gboolean enable_gps_get_toggle(appdata_t *appdata) {
  if(!appdata)           return FALSE;
  if(!appdata->settings) return FALSE;
  return appdata->settings->enable_gps;
}

static gboolean follow_gps_get_toggle(appdata_t *appdata) {
  if(!appdata)           return FALSE;
  if(!appdata->settings) return FALSE;
  return appdata->settings->follow_gps;
}

/* create a HildonAppMenu */
static GtkWidget *app_menu_create(appdata_t *appdata,
				  const menu_entry_t *menu_entries) {
  HildonAppMenu *menu = HILDON_APP_MENU(hildon_app_menu_new());

  while(menu_entries->label) {
    GtkWidget *button = NULL;

    if(!menu_entries->toggle) {
      button = hildon_button_new_with_text(
	    HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH,
	    HILDON_BUTTON_ARRANGEMENT_VERTICAL,
	    _(menu_entries->label), _(menu_entries->value));
      g_signal_connect_after(button, "clicked",
			     menu_entries->activate_cb, appdata);
    } else {
      button = hildon_check_button_new(HILDON_SIZE_AUTO);
      gtk_button_set_label(GTK_BUTTON(button), _(menu_entries->label));
      printf("requesting check for %s: %p\n", menu_entries->label,
	     menu_entries->toggle);
      hildon_check_button_set_active(HILDON_CHECK_BUTTON(button),
				     menu_entries->toggle(appdata));
      g_signal_connect_after(button, "toggled",
			     menu_entries->activate_cb, appdata);
    }

    hildon_button_set_title_alignment(HILDON_BUTTON(button), 0.5, 0.5);
    hildon_button_set_value_alignment(HILDON_BUTTON(button), 0.5, 0.5);

    /* offset to GtkWidget pointer was given -> store pointer */
    if(menu_entries->offset)
      *(GtkWidget**)(((void*)appdata)+menu_entries->offset) = button;

    gtk_widget_set_sensitive(button, menu_entries->enabled);

    hildon_app_menu_append(menu, GTK_BUTTON(button));
    menu_entries++;
  }

  gtk_widget_show_all(GTK_WIDGET(menu));
  return GTK_WIDGET(menu);
}

#define COLUMNS  2

void on_submenu_entry_clicked(GtkButton *button, GtkWidget *menu) {

  /* force closing of submenu dialog */
  gtk_dialog_response(GTK_DIALOG(menu), GTK_RESPONSE_NONE);
  gtk_widget_hide(menu);

  /* let gtk clean up */
  while(gtk_events_pending())
    gtk_main_iteration();
}

/* use standard dialog boxes for fremantle submenues */
static GtkWidget *app_submenu_create(appdata_t *appdata,
				     const submenu_t *submenu) {

  /* create a oridinary dialog box */
  GtkWidget *dialog = misc_dialog_new(MISC_DIALOG_SMALL, _(submenu->title),
				      GTK_WINDOW(appdata->window), NULL);

  gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

  GtkWidget *table = gtk_table_new(submenu->len/COLUMNS, COLUMNS, TRUE);
  int x = 0, y = 0;

  const menu_entry_t *menu_entries = submenu->menu;
  while(menu_entries->label) {
    GtkWidget *button = NULL;

    /* the "Style" menu entry is very special */
    /* and is being handled seperately */
    if(!strcmp("Style", menu_entries->label)) {
      button = style_select_widget(appdata);
      g_object_set_data(G_OBJECT(dialog), "style_widget", button);
    } else {
      if(!menu_entries->toggle) {
	button = hildon_button_new_with_text(
	     HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH,
	     HILDON_BUTTON_ARRANGEMENT_VERTICAL,
	     _(menu_entries->label), _(menu_entries->value));

	g_signal_connect(button, "clicked",
			 G_CALLBACK(on_submenu_entry_clicked), dialog);

	g_signal_connect(button, "clicked",
			 menu_entries->activate_cb, appdata);

	hildon_button_set_title_alignment(HILDON_BUTTON(button), 0.5, 0.5);
	hildon_button_set_value_alignment(HILDON_BUTTON(button), 0.5, 0.5);
      } else {
	button = hildon_check_button_new(HILDON_SIZE_AUTO);
	gtk_button_set_label(GTK_BUTTON(button), _(menu_entries->label));
	printf("requesting check for %s: %p\n", menu_entries->label,
	       menu_entries->toggle);
	hildon_check_button_set_active(HILDON_CHECK_BUTTON(button),
				       menu_entries->toggle(appdata));

	g_signal_connect(button, "clicked",
			 G_CALLBACK(on_submenu_entry_clicked), dialog);

	g_signal_connect(button, "toggled",
			 menu_entries->activate_cb, appdata);

	gtk_button_set_alignment(GTK_BUTTON(button), 0.5, 0.5);
      }
    }

    /* offset to GtkWidget pointer was given -> store pointer */
    if(menu_entries->offset)
      *(GtkWidget**)(((void*)appdata)+menu_entries->offset) = button;

    gtk_widget_set_sensitive(button, menu_entries->enabled);

    gtk_table_attach_defaults(GTK_TABLE(table),  button, x, x+1, y, y+1);

    x++;
    if(x == COLUMNS) { x = 0; y++; }

    menu_entries++;
  }


  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), table);

  return dialog;
}

/* popup the dialog shaped submenu */
static void submenu_popup(appdata_t *appdata, GtkWidget *menu) {
  gtk_widget_show_all(menu);
  gtk_dialog_run(GTK_DIALOG(menu));
  gtk_widget_hide(menu);

  /* check if the style menu was in here */
  GtkWidget *style_widget = GTK_WIDGET(g_object_get_data(G_OBJECT(menu), "style_widget"));
  if(style_widget) {
    const char *ptr = combo_box_get_active_text(style_widget);
    if(ptr) style_change(appdata, ptr);
  }
}

static void submenu_cleanup(GtkWidget *menu) {
  gtk_widget_destroy(menu);
}

/* the view submenu */
void on_submenu_view_clicked(GtkButton *button, appdata_t *appdata) {
  submenu_popup(appdata, appdata->app_menu_view);
}

void on_submenu_map_clicked(GtkButton *button, appdata_t *appdata) {
  submenu_popup(appdata, appdata->app_menu_map);
}

void on_submenu_wms_clicked(GtkButton *button, appdata_t *appdata) {
  submenu_popup(appdata, appdata->app_menu_wms);
}

void on_submenu_track_clicked(GtkButton *button, appdata_t *appdata) {
  submenu_popup(appdata, appdata->app_menu_track);
}

#define APP_OFFSET(a)  offsetof(appdata_t, a)
#define SIMPLE_ENTRY(a,b)     { a, NULL, TRUE,   NULL, 0, G_CALLBACK(b) }
#define ENABLED_ENTRY(a,b,c)  { a, NULL, TRUE,  NULL, APP_OFFSET(c), G_CALLBACK(b) }
#define DISABLED_ENTRY(a,b,c) { a, NULL, FALSE,  NULL, APP_OFFSET(c), G_CALLBACK(b) }
#define TOGGLE_ENTRY(a,b,c)   { a, NULL, TRUE, c, 0, G_CALLBACK(b) }
#define DISABLED_TOGGLE_ENTRY(a,b,c,d)  \
                              { a, NULL, FALSE, c, APP_OFFSET(d), G_CALLBACK(b) }
#define ENABLED_TOGGLE_ENTRY(a,b,c,d) \
                              { a, NULL, TRUE, c, APP_OFFSET(d), G_CALLBACK(b) }
#define LAST_ENTRY            { NULL, NULL, FALSE, NULL, 0, NULL }

/* -- the view submenu -- */
static const menu_entry_t submenu_view_entries[] = {
#ifndef ZOOM_BUTTONS
  SIMPLE_ENTRY("Zoom in",         cb_menu_zoomin),
  SIMPLE_ENTRY("Zoom out",        cb_menu_zoomout),
#endif
  /* --- */
  SIMPLE_ENTRY("Style",           NULL),
  /* --- */
#ifndef DETAIL_POPUP
  SIMPLE_ENTRY("Normal details",  cb_menu_view_detail_normal),
  SIMPLE_ENTRY("More details",    cb_menu_view_detail_inc),
  SIMPLE_ENTRY("Less details",    cb_menu_view_detail_dec),
#endif
  /* --- */
  DISABLED_ENTRY("Hide selected", cb_menu_map_hide_sel, menu_item_map_hide_sel),
  DISABLED_ENTRY("Show all",      cb_menu_map_show_all, menu_item_map_show_all),

  LAST_ENTRY
};

static const submenu_t submenu_view = {
  "View", submenu_view_entries,
  sizeof(submenu_view_entries)/sizeof(menu_entry_t)-1
};

/* -- the map submenu -- */
static const menu_entry_t submenu_map_entries[] = {
  ENABLED_ENTRY("Upload",                cb_menu_upload, menu_item_map_upload),
  SIMPLE_ENTRY("Download",               cb_menu_download),
  ENABLED_ENTRY("Undo all",              cb_menu_undo_changes,
		menu_item_map_undo_changes),

  LAST_ENTRY
};

static const submenu_t submenu_map = {
  "OSM", submenu_map_entries,
  sizeof(submenu_map_entries)/sizeof(menu_entry_t)-1
};

/* -- the wms submenu -- */
static const menu_entry_t submenu_wms_entries[] = {
  SIMPLE_ENTRY("Import",   cb_menu_wms_import),
  DISABLED_ENTRY("Clear",  cb_menu_wms_clear, menu_item_wms_clear),
  DISABLED_ENTRY("Adjust", cb_menu_wms_adjust, menu_item_wms_adjust),

  LAST_ENTRY
};

static const submenu_t submenu_wms = {
  "WMS", submenu_wms_entries,
  sizeof(submenu_wms_entries)/sizeof(menu_entry_t)-1
};

/* -- the track submenu -- */
static const menu_entry_t submenu_track_entries[] = {
  ENABLED_ENTRY("Import",  cb_menu_track_import, track.menu_item_track_import),
  DISABLED_ENTRY("Export", cb_menu_track_export, track.menu_item_track_export),
  DISABLED_ENTRY("Clear",  cb_menu_track_clear, track.menu_item_track_clear),
  ENABLED_TOGGLE_ENTRY("GPS enable", cb_menu_track_enable_gps,
		enable_gps_get_toggle, track.menu_item_track_enable_gps),
  DISABLED_TOGGLE_ENTRY("GPS follow", cb_menu_track_follow_gps,
		follow_gps_get_toggle, track.menu_item_track_follow_gps),

  LAST_ENTRY
};

static const submenu_t submenu_track = {
  "Track", submenu_track_entries,
  sizeof(submenu_track_entries)/sizeof(menu_entry_t)-1
};


/* -- the applications main menu -- */
static const menu_entry_t main_menu[] = {
  SIMPLE_ENTRY("About",   cb_menu_about),
  SIMPLE_ENTRY("Project", cb_menu_project_open),
  ENABLED_ENTRY("View",   on_submenu_view_clicked,  submenu_view),
  ENABLED_ENTRY("OSM",    on_submenu_map_clicked,   submenu_map),
  ENABLED_ENTRY("Relations", cb_menu_osm_relations, menu_item_map_relations),
  ENABLED_ENTRY("WMS",    on_submenu_wms_clicked,   submenu_wms),
  ENABLED_ENTRY("Track",  on_submenu_track_clicked, track.submenu_track),

  LAST_ENTRY
};

static void menu_create(appdata_t *appdata) {
  HildonAppMenu *menu = HILDON_APP_MENU(hildon_app_menu_new());

  /* build menu/submenus */
  menu = HILDON_APP_MENU(app_menu_create(appdata, main_menu));
  appdata->app_menu_wms   = app_submenu_create(appdata, &submenu_wms);
  g_object_ref(appdata->app_menu_wms);
  appdata->app_menu_map   = app_submenu_create(appdata, &submenu_map);
  g_object_ref(appdata->app_menu_map);
  appdata->app_menu_view  = app_submenu_create(appdata, &submenu_view);
  g_object_ref(appdata->app_menu_view);
  appdata->app_menu_track = app_submenu_create(appdata, &submenu_track);
  g_object_ref(appdata->app_menu_track);

  /* enable/disable some entries according to settings */
  if(appdata && appdata->settings)
    gtk_widget_set_sensitive(appdata->track.menu_item_track_follow_gps,
			     appdata->settings->enable_gps);

  hildon_window_set_app_menu(HILDON_WINDOW(appdata->window), menu);
}

void menu_cleanup(appdata_t *appdata) {
  submenu_cleanup(appdata->app_menu_view);
  submenu_cleanup(appdata->app_menu_map);
  submenu_cleanup(appdata->app_menu_wms);
  submenu_cleanup(appdata->app_menu_track);
}
#endif

/********************* end of menu **********************/

#ifdef UISPECIFIC_MENU_HAS_ACCELS
#define ACCELS_FILE "accels"

static void menu_accels_load(appdata_t *appdata) {
  char *accels_file = g_strconcat(appdata->settings->base_path, ACCELS_FILE,
                                      NULL);
  gtk_accel_map_load(accels_file);
  g_free(accels_file);
}

static void menu_accels_save(appdata_t *appdata) {
  char *accels_file = g_strconcat(appdata->settings->base_path, ACCELS_FILE,
                                      NULL);
  gtk_accel_map_save(accels_file);
  g_free(accels_file);
}

#endif


void cleanup(appdata_t *appdata) {
  printf("cleaning up ...\n");

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

  osm_free(appdata->osm);
  appdata->osm = NULL;

  xmlCleanupParser();

  curl_global_cleanup();

  josm_presets_free(appdata->presets);

  icon_free_all(&appdata->icon);

  gps_release(appdata);

  settings_free(appdata->settings);

  statusbar_free(appdata->statusbar);

  iconbar_free(appdata->iconbar);

  project_free(appdata->project);

  if(appdata->menu_item_map_undo)
    undo_free(appdata->osm, &appdata->undo);

  menu_cleanup(appdata);

  puts("everything is gone");
}

void on_window_destroy(G_GNUC_UNUSED GtkWidget *widget, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  puts("main window destroy");

  gtk_main_quit();
  appdata->window = NULL;
}

gboolean on_window_key_press(G_GNUC_UNUSED GtkWidget *widget,
			 GdkEventKey *event, gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  int handled = FALSE;

  //  printf("key event with keyval %x\n", event->keyval);

  // the map handles some keys on its own ...
  switch(event->keyval) {

#if !defined(USE_HILDON) || (MAEMO_VERSION_MAJOR < 5)
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
#endif
  }

  /* forward unprocessed key presses to map */
  if(!handled && appdata->project && appdata->osm)
    handled = map_key_press_event(appdata, event);

  return handled;
}

#if (MAEMO_VERSION_MAJOR == 5) && !defined(__i386__)
/* get access to zoom buttons */
static void
on_window_realize(GtkWidget *widget, appdata_t *appdata) {
  if (widget->window) {
    unsigned char value = 1;
    Atom hildon_zoom_key_atom =
      gdk_x11_get_xatom_by_name("_HILDON_ZOOM_KEY_ATOM"),
      integer_atom = gdk_x11_get_xatom_by_name("INTEGER");
    Display *dpy =
      GDK_DISPLAY_XDISPLAY(gdk_drawable_get_display(widget->window));
    Window w = GDK_WINDOW_XID(widget->window);

    XChangeProperty(dpy, w, hildon_zoom_key_atom,
		    integer_atom, 8, PropModeReplace, &value, 1);
  }
}
#endif

#ifdef FREMANTLE
static GtkWidget *icon_button(appdata_t *appdata, char *icon, GCallback cb,
			      GtkWidget *box) {
  /* add zoom-in button */
  GtkWidget *but = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(but),
		       icon_widget_load(&appdata->icon, icon));
  //  gtk_button_set_relief(GTK_BUTTON(but), GTK_RELIEF_NONE);
  hildon_gtk_widget_set_theme_size(but,
  		(HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));

  if(cb)
    g_signal_connect(but, "clicked", G_CALLBACK(cb), appdata);

  gtk_box_pack_start(GTK_BOX(box), but, FALSE, FALSE, 0);
  return but;
}
#endif

/* handle pending gtk events, but don't let the user actually do something */
static void gtk_process_blocking(G_GNUC_UNUSED appdata_t *appdata) {
  while(gtk_events_pending())
    gtk_main_iteration();
}


int main(int argc, char *argv[]) {
  appdata_t appdata;

  LIBXML_TEST_VERSION;

  /* init appdata */
  memset(&appdata, 0, sizeof(appdata));

  printf("Using locale for %s in %s\n", PACKAGE, LOCALEDIR);

  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(PACKAGE, "UTF-8");
  textdomain(PACKAGE);

  /* Must initialize libcurl before any threads are started */
  curl_global_init(CURL_GLOBAL_ALL);

  /* Same for libxml2 */
  xmlInitParser();

  /* whitespace between tags has no meaning in any of the XML files used here */
  xmlKeepBlanksDefault(0);

#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init(NULL);
#endif

  gtk_init (&argc, &argv);

  misc_init();

  gps_init(&appdata);

#ifdef USE_HILDON
  printf("Installing osso context for \"org.harbaum." PACKAGE "\"\n");
  appdata.osso_context = osso_initialize("org.harbaum."PACKAGE,
					 VERSION, TRUE, NULL);
  if(appdata.osso_context == NULL)
    fprintf(stderr, "error initiating osso context\n");

  dbus_register(&appdata.mmpos);

  /* Create the hildon program and setup the title */
  appdata.program = HILDON_PROGRAM(hildon_program_get_instance());
  g_set_application_name("OSM2Go");

  /* Create HildonWindow and set it to HildonProgram */
#if MAEMO_VERSION_MAJOR < 5
  appdata.window = HILDON_WINDOW(hildon_window_new());
#else
  appdata.window = HILDON_WINDOW(hildon_stackable_window_new());
#endif
  hildon_program_add_window(appdata.program, appdata.window);

  /* try to enable the zoom buttons. don't do this on x86 as it breaks */
  /* at runtime with cygwin x */
#if (MAEMO_VERSION_MAJOR == 5) && !defined(__i386__)
  g_signal_connect(G_OBJECT(appdata.window), "realize",
		   G_CALLBACK(on_window_realize), &appdata);
#endif // MAEMO_VERSION_MAJOR

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

  g_signal_connect(G_OBJECT(appdata.window), "key_press_event",
 		   G_CALLBACK(on_window_key_press), &appdata);
  g_signal_connect(G_OBJECT(appdata.window), "destroy",
		   G_CALLBACK(on_window_destroy), &appdata);

  /* user specific init */
  appdata.settings = settings_load();

  appdata.vbox = gtk_vbox_new(FALSE,0);

  /* unconditionally enable the GPS */
  appdata.settings->enable_gps = TRUE;
  menu_create(&appdata);

  /* if tracking is enable, start it now */
  track_enable_gps(&appdata, appdata.settings->enable_gps);

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

  /* fremantle has seperate zoom/details buttons on the right screen side */
#if !defined(FREMANTLE) && (defined(ZOOM_BUTTONS) || defined(DETAIL_POPUP))
  GtkWidget *zhbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(zhbox), statusbar_new(&appdata));
#endif

#if !defined(FREMANTLE) && defined(DETAIL_POPUP)
  /* ---- detail popup ---- */
  appdata.btn_detail_popup = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(appdata.btn_detail_popup),
	gtk_image_new_from_stock(GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU));
  g_signal_connect(appdata.btn_detail_popup, "clicked",
		   G_CALLBACK(scale_popup), &appdata);
  gtk_box_pack_start(GTK_BOX(zhbox), appdata.btn_detail_popup, FALSE, FALSE, 0);
#endif

#if !defined(FREMANTLE) && defined(ZOOM_BUTTONS)
  /* ---- add zoom out button right of statusbar ---- */
  appdata.btn_zoom_out = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(appdata.btn_zoom_out),
	gtk_image_new_from_stock(GTK_STOCK_ZOOM_OUT, GTK_ICON_SIZE_MENU));
  g_signal_connect(appdata.btn_zoom_out, "clicked",
		   G_CALLBACK(cb_menu_zoomout), &appdata);
  gtk_box_pack_start(GTK_BOX(zhbox), appdata.btn_zoom_out, FALSE, FALSE, 0);

  /* ---- add zoom in button right of statusbar ---- */
  appdata.btn_zoom_in = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(appdata.btn_zoom_in),
	gtk_image_new_from_stock(GTK_STOCK_ZOOM_IN, GTK_ICON_SIZE_MENU));
  g_signal_connect(appdata.btn_zoom_in, "clicked",
		   G_CALLBACK(cb_menu_zoomin), &appdata);
  gtk_box_pack_start(GTK_BOX(zhbox), appdata.btn_zoom_in, FALSE, FALSE, 0);
#endif

#if !defined(FREMANTLE) && (defined(ZOOM_BUTTONS) || defined(DETAIL_POPUP))
  gtk_box_pack_start(GTK_BOX(vbox), zhbox, FALSE, FALSE, 0);
#else
  gtk_box_pack_start(GTK_BOX(vbox), statusbar_new(&appdata), FALSE, FALSE, 0);
#endif

#ifndef PORTRAIT
  gtk_box_pack_start(GTK_BOX(hbox), iconbar_new(&appdata), FALSE, FALSE, 0);
#endif
  gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

#ifdef FREMANTLE
  /* fremantle has a set of buttons on the right screen side as well */
  vbox = gtk_vbox_new(FALSE, 0);

  GtkWidget *ivbox = gtk_vbox_new(FALSE, 0);
  appdata.btn_zoom_in =
    icon_button(&appdata, "zoomin_thumb",   G_CALLBACK(cb_menu_zoomin), ivbox);
  appdata.btn_zoom_out =
    icon_button(&appdata, "zoomout_thumb",  G_CALLBACK(cb_menu_zoomout), ivbox);
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, FALSE, FALSE, 0);

  ivbox = gtk_vbox_new(FALSE, 0);
  icon_button(&appdata, "detailup_thumb",   G_CALLBACK(cb_menu_view_detail_inc), ivbox);
  icon_button(&appdata, "detaildown_thumb", G_CALLBACK(cb_menu_view_detail_dec), ivbox);
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  ivbox = gtk_vbox_new(FALSE, 0);
  GtkWidget *ok = icon_button(&appdata, "ok_thumb", NULL, ivbox);
  GtkWidget *cancel = icon_button(&appdata, "cancel_thumb", NULL, ivbox);
  iconbar_register_buttons(&appdata, ok, cancel);
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
#endif // FREMANTLE

  gtk_box_pack_start(GTK_BOX(appdata.vbox), hbox, TRUE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(appdata.window), appdata.vbox);

  gtk_widget_show_all(GTK_WIDGET(appdata.window));

  appdata.presets = josm_presets_load();

  /* let gtk do its thing before loading the data, */
  /* so the user sees something */
  gtk_process_blocking(&appdata);
  if(!appdata.window) {
    printf("shutdown while starting up (1)\n");
    cleanup(&appdata);
    return -1;
  }

  /* load project if one is specified in the settings */
  if(appdata.settings->project)
    project_load(&appdata, appdata.settings->project);

  main_ui_enable(&appdata);

  /* start GPS if enabled by config */
  if(appdata.settings && appdata.settings->enable_gps)
    track_enable_gps(&appdata, TRUE);

  /* again let the ui do its thing */
  gtk_process_blocking(&appdata);
  if(!appdata.window) {
    printf("shutdown while starting up (2)\n");
    cleanup(&appdata);
    return -1;
  }

  /* start to interact with the user now that the gui is running */
  if(appdata.settings->first_run_demo) {
    messagef(GTK_WIDGET(appdata.window), _("Welcome to OSM2Go"),
	     _("This is the first time you run OSM2Go. "
	       "A demo project has been loaded to get you "
	       "started. You can play around with this demo as much "
	       "as you like. However, you cannot upload or download "
	       "the demo project.\n\n"
	       "In order to start working on real data you'll have "
	       "to setup a new project and enter your OSM user name "
	       "and password. You'll then be able to download the "
	       "latest data from OSM and upload your changes into "
	       "the OSM main database."
	       ));
  }

  puts("main up");

  /* ------------ jump into main loop ---------------- */
  gtk_main();

  puts("gtk_main() left");

  track_save(appdata.project, appdata.track.track);
  track_clear(&appdata);

  /* save a diff if there are dirty entries */
  diff_save(appdata.project, appdata.osm);

  cleanup(&appdata);

  return 0;
}

// vim:et:ts=8:sw=2:sts=2:ai
