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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <appdata.h>
#include <canvas.h>
#include <diff.h>
#include <gps.h>
#include <icon.h>
#include <iconbar.h>
#include <josm_presets.h>
#include "MainUiGtk.h"
#include <map.h>
#include <misc.h>
#include <notifications.h>
#include <osm.h>
#include <osm_api.h>
#include <project.h>
#include <relation_edit.h>
#include <settings.h>
#include <statusbar.h>
#include <style.h>
#include <style_widgets.h>
#include <track.h>
#include <wms.h>

#ifdef FREMANTLE
#include <hildon/hildon-button.h>
#include <hildon/hildon-check-button.h>
#include <hildon/hildon-defines.h>
#include <hildon/hildon-file-chooser-dialog.h>
#include <hildon/hildon-gtk.h>
#include <hildon/hildon-program.h>
#include <hildon/hildon-window-stack.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#define GTK_FM_OK  GTK_RESPONSE_OK
#define MENU_CHECK_ITEM HildonCheckButton
#define MENU_CHECK_ITEM_ACTIVE(a) hildon_check_button_get_active(a)
#else
#define GTK_FM_OK  GTK_RESPONSE_ACCEPT
#define MENU_CHECK_ITEM GtkCheckMenuItem
#define MENU_CHECK_ITEM_ACTIVE(a) gtk_check_menu_item_get_active(a)
#endif

// Maemo/Hildon builds

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>

#include <osm2go_annotations.h>
#include "osm2go_i18n.h"
#include "osm2go_platform.h"
#include "osm2go_platform_gtk.h"

using namespace osm2go_platform;

#define LOCALEDIR PREFIX "/locale"

#ifndef FREMANTLE
/* these size defaults are used in the non-hildonized version only */
#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480
#define ACCELS_FILE "accels"
#endif

struct appdata_internal : public appdata_t {
  explicit appdata_internal(map_state_t &mstate);
  ~appdata_internal();

#ifdef FREMANTLE
  HildonProgram *program;
  /* submenues are seperate menues under fremantle */
  osm2go_platform::WidgetGuard app_menu_view, app_menu_wms, app_menu_track, app_menu_map;
#endif

  GtkWidget *btn_zoom_in, *btn_zoom_out;
};

/* disable/enable main screen control dependant on presence of open project */
void appdata_t::main_ui_enable() {
  gboolean osm_valid = (project && project->osm) ? TRUE : FALSE;

  if(unlikely(window == nullptr)) {
    g_debug("%s: main window gone\n", __PRETTY_FUNCTION__);
    return;
  }

  /* cancel any action in progress */
  if(iconbar->isCancelEnabled())
    map->action_cancel();

  /* ---- set project name as window title ----- */
  g_string str;
  const char *cstr = "OSM2go";

  if(project) {
#ifdef FREMANTLE
    str.reset(g_markup_printf_escaped(_("<b>%s</b> - OSM2Go"),
                                      project->name.c_str()));
    cstr = str.get();
  }

  hildon_window_set_markup(HILDON_WINDOW(window), cstr);
#else
    str.reset(g_strdup_printf(_("%s - OSM2Go"), project->name.c_str()));
    cstr = str.get();
  }

  gtk_window_set_title(GTK_WINDOW(window), cstr);
#endif
  str.reset();

  iconbar->setToolbarEnable(osm_valid == TRUE);
  /* disable all menu entries related to map */
  uicontrol->setActionEnable(MainUi::SUBMENU_MAP, static_cast<bool>(project));

  // those icons that get enabled or disabled depending on OSM data being loaded
#ifndef FREMANTLE
  std::array<MainUi::menu_items, 7> osm_active_items = { {
    MainUi::MENU_ITEM_MAP_SAVE_CHANGES,
#else
  std::array<MainUi::menu_items, 6> osm_active_items = { {
#endif
    MainUi::MENU_ITEM_MAP_UPLOAD,
    MainUi::MENU_ITEM_MAP_UNDO_CHANGES,
    MainUi::MENU_ITEM_MAP_RELATIONS,
    MainUi::SUBMENU_TRACK,
    MainUi::SUBMENU_VIEW,
    MainUi::SUBMENU_WMS
  } };
  for(unsigned int i = 0; i < osm_active_items.size(); i++)
    uicontrol->setActionEnable(osm_active_items[i], osm_valid == TRUE);

  gtk_widget_set_sensitive(static_cast<appdata_internal *>(this)->btn_zoom_in, osm_valid);
  gtk_widget_set_sensitive(static_cast<appdata_internal *>(this)->btn_zoom_out, osm_valid);

  if(!project)
    uicontrol->showNotification(_("Please load or create a project"));
}

/******************** begin of menu *********************/

static void
cb_menu_project_open(appdata_t *appdata) {
  const std::string &proj_name = project_select(*appdata);
  if(!proj_name.empty())
    project_load(*appdata, proj_name);
  appdata->main_ui_enable();
}

static void
cb_menu_upload(appdata_t *appdata) {
  if(!appdata->project || !appdata->project->osm)
    return;

  if(appdata->project->check_demo())
    return;

  osm_upload(*appdata);
}

static void
cb_menu_download(appdata_t *appdata) {
  if(!appdata->project) return;

  if(appdata->project->check_demo())
    return;

  appdata->map->set_autosave(false);

  /* if we have valid osm data loaded: save state first */
  appdata->project->diff_save();

  // download
  bool hasMap = static_cast<bool>(appdata->project->osm);
  if(osm_download(appdata_t::window, appdata->project.get())) {
    if(hasMap)
      /* redraw the entire map by destroying all map items and redrawing them */
      appdata->map->clear(map_t::MAP_LAYER_OBJECTS_ONLY);

    appdata->uicontrol->showNotification(_("Drawing"), MainUi::Busy);
    appdata->project->parse_osm();
    diff_restore(appdata->project.get(), appdata->uicontrol.get());
    appdata->map->paint();
    appdata->uicontrol->showNotification(nullptr, MainUi::Busy);
  }

  appdata->map->set_autosave(true);
  appdata->main_ui_enable();
}

static void
cb_menu_wms_adjust(appdata_t *appdata) {
  appdata->map->set_action(MAP_ACTION_BG_ADJUST);
}

/* ----------- hide objects for performance reasons ----------- */

static void
cb_menu_map_hide_sel(appdata_t *appdata) {
  appdata->map->hide_selected();
}

static void
cb_menu_map_show_all(appdata_t *appdata) {
  appdata->map->show_all();
}

/* ---------------------------------------------------------- */

GtkWidget *track_vis_select_widget(TrackVisibility current) {
  std::vector<const char *> labels;
  labels.push_back(_("Hide tracks"));
  labels.push_back(_("Show current position"));
  labels.push_back(_("Show current segment"));
  labels.push_back(_("Show all segments"));

  return osm2go_platform::combo_box_new(_("Track visibility"), labels, static_cast<int>(current));
}

#ifndef FREMANTLE
/* in fremantle this happens inside the submenu handling since this button */
/* is actually placed inside the submenu there */
static bool track_visibility_select(GtkWidget *parent) {
  osm2go_platform::WidgetGuard dialog(gtk_dialog_new_with_buttons(_("Select track visibility"),
                                              GTK_WINDOW(parent), GTK_DIALOG_MODAL,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              nullptr));

  gtk_dialog_set_default_response(GTK_DIALOG(dialog.get()), GTK_RESPONSE_ACCEPT);

  settings_t::ref settings = settings_t::instance();
  GtkWidget *cbox = track_vis_select_widget(settings->trackVisibility);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 8);
  gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("Track visibility:")), TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), cbox, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.get())->vbox), hbox, TRUE, TRUE, 0);

  gtk_widget_show_all(dialog.get());

  bool ret = false;
  if(GTK_RESPONSE_ACCEPT != gtk_dialog_run(GTK_DIALOG(dialog.get()))) {
    g_debug("user clicked cancel\n");
  } else {
    int index = combo_box_get_active(cbox);
    g_debug("user clicked ok on %i\n", index);

    TrackVisibility tv = static_cast<TrackVisibility>(index);
    ret = (tv != settings->trackVisibility);
    settings->trackVisibility = tv;
  }

  return ret;
}

static void
cb_menu_track_vis(appdata_t *appdata) {
  if(track_visibility_select(appdata_t::window) && appdata->track.track)
    appdata->map->track_draw(settings_t::instance()->trackVisibility, *appdata->track.track);
}

static void
cb_menu_save_changes(appdata_t *appdata) {
  if(likely(appdata->project))
    appdata->project->diff_save();
  appdata->uicontrol->showNotification(_("Saved local changes"), MainUi::Brief);
}
#endif

static void
cb_menu_undo_changes(appdata_t *appdata) {
  project_t::ref project = appdata->project;
  // if there is nothing to clean then don't ask
  if (!project->diff_file_present() && project->osm->is_clean(true))
    return;

  if(!yes_no_f(nullptr, 0, _("Undo all changes?"),
               _("Throw away all the changes you've not uploaded yet? This cannot be undone.")))
    return;

  appdata->map->clear(map_t::MAP_LAYER_OBJECTS_ONLY);

  project->diff_remove_file();
  project->parse_osm();
  appdata->map->paint();

  appdata->uicontrol->showNotification(_("Undo all changes"), MainUi::Brief);
}

static void
cb_menu_osm_relations(appdata_t *appdata) {
  /* list relations of all objects */
  relation_list(appdata_t::window, appdata->map, appdata->project->osm, appdata->presets.get());
}

#ifndef FREMANTLE
static void
cb_menu_fullscreen(appdata_t *, GtkCheckMenuItem *item) {
  if(MENU_CHECK_ITEM_ACTIVE(item))
    gtk_window_fullscreen(GTK_WINDOW(appdata_t::window));
  else
    gtk_window_unfullscreen(GTK_WINDOW(appdata_t::window));
}
#endif

static void
cb_menu_zoomin(map_t *map) {
  map->set_zoom(map->state.zoom * ZOOM_FACTOR_MENU, true);
  g_debug("zoom is now %f\n", map->state.zoom);
}

static void
cb_menu_zoomout(map_t *map) {
  map->set_zoom(map->state.zoom / ZOOM_FACTOR_MENU, true);
  g_debug("zoom is now %f\n", map->state.zoom);
}

static void
cb_menu_view_detail_inc(map_t *map) {
  g_debug("detail level increase\n");
  map->detail_increase();
}

#ifndef FREMANTLE
static void
cb_menu_view_detail_normal(map_t *map) {
  g_debug("detail level normal\n");
  map->detail_normal();
}
#endif

static void
cb_menu_view_detail_dec(map_t *map) {
  g_debug("detail level decrease\n");
  map->detail_decrease();
}

static void
cb_menu_track_import(appdata_t *appdata) {
  /* open a file selector */
  osm2go_platform::WidgetGuard dialog(
#ifdef FREMANTLE
                  hildon_file_chooser_dialog_new(GTK_WINDOW(appdata_t::window),
                                                 GTK_FILE_CHOOSER_ACTION_OPEN)
#else
                  gtk_file_chooser_dialog_new (_("Import track file"),
                                               GTK_WINDOW(appdata_t::window),
                                               GTK_FILE_CHOOSER_ACTION_OPEN,
                                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                               GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                               nullptr)
#endif
           );

  settings_t::ref settings = settings_t::instance();
  if(!settings->track_path.empty()) {
    if(g_file_test(settings->track_path.c_str(), G_FILE_TEST_EXISTS) != TRUE) {
      std::string::size_type slashpos = settings->track_path.rfind('/');
      if(slashpos != std::string::npos) {
        settings->track_path[slashpos] = '\0';  // seperate path from file

        /* the user just created a new document */
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog.get()),
                                            settings->track_path.c_str());
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog.get()),
                                          settings->track_path.c_str() + slashpos + 1);

        /* restore full filename */
        settings->track_path[slashpos] = '/';
      }
    } else
      gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog.get()),
                                    settings->track_path.c_str());
  }

  gtk_widget_show_all(GTK_WIDGET(dialog.get()));
  if (gtk_dialog_run(GTK_DIALOG(dialog.get())) == GTK_FM_OK) {
    g_string filename(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog.get())));

    /* remove any existing track */
    appdata->track_clear();

    /* load a track */
    appdata->track.track.reset(track_import(filename.get()));
    if(appdata->track.track) {
      appdata->map->track_draw(settings->trackVisibility, *appdata->track.track);

      settings->track_path = filename.get();
    }
    track_menu_set(*appdata);
  }
}

static void
cb_menu_track_enable_gps(appdata_t *appdata, MENU_CHECK_ITEM *item) {
  track_enable_gps(*appdata, MENU_CHECK_ITEM_ACTIVE(item));
}


static void
cb_menu_track_follow_gps(appdata_t *, MENU_CHECK_ITEM *item) {
  settings_t::instance()->follow_gps = MENU_CHECK_ITEM_ACTIVE(item);
}


static void
cb_menu_track_export(appdata_t *appdata) {
  /* open a file selector */
  osm2go_platform::WidgetGuard dialog(
#ifdef FREMANTLE
                  hildon_file_chooser_dialog_new(GTK_WINDOW(appdata_t::window),
                                                 GTK_FILE_CHOOSER_ACTION_SAVE)
#else
                  gtk_file_chooser_dialog_new(_("Export track file"),
                                              GTK_WINDOW(appdata_t::window),
                                              GTK_FILE_CHOOSER_ACTION_SAVE,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                              GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                              nullptr)
#endif
           );

  settings_t::ref settings = settings_t::instance();
  g_debug("set filename <%s>\n", settings->track_path.c_str());

  if(!settings->track_path.empty()) {
    if(g_file_test(settings->track_path.c_str(), G_FILE_TEST_EXISTS) != TRUE) {
      std::string::size_type slashpos = settings->track_path.rfind('/');
      if(slashpos != std::string::npos) {
        settings->track_path[slashpos] = '\0';  // seperate path from file

        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog.get()),
                                            settings->track_path.c_str());
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog.get()),
                                          settings->track_path.c_str() + slashpos + 1);

        /* restore full filename */
        settings->track_path[slashpos] = '/';
      }
    } else
      gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog.get()),
                                    settings->track_path.c_str());
  }

  if(gtk_dialog_run(GTK_DIALOG(dialog.get())) == GTK_FM_OK) {
    g_string filename(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog.get())));
    if(filename) {
      g_debug("export to %s\n", filename.get());

      if(g_file_test(filename.get(), G_FILE_TEST_EXISTS) != TRUE ||
         yes_no_f(dialog.get(), MISC_AGAIN_ID_EXPORT_OVERWRITE | MISC_AGAIN_FLAG_DONT_SAVE_NO,
                  _("Overwrite existing file"),
                  _("The file already exists. "
                    "Do you really want to replace it?"))) {
        settings->track_path = filename.get();

        assert(appdata->track.track);
        track_export(appdata->track.track.get(), filename.get());
      }
    }
  }
}

/*
 *  Platform-specific UI tweaks.
 */

static void track_clear_cb(appdata_t *appdata) {
  appdata->track_clear();
}

static void about_box(MainUi *uicontrol)
{
  uicontrol->about_box();
}

#ifndef FREMANTLE
// Half-arsed slapdash common menu item constructor. Let's use GtkBuilder
// instead so we have some flexibility.

struct KeySequence {
  explicit inline KeySequence(guint k = 0, GdkModifierType m1 = static_cast<GdkModifierType>(0), GdkModifierType m2 = static_cast<GdkModifierType>(0))
    : key(k), mods(static_cast<GdkModifierType>(m1 | m2)) {}
  explicit inline KeySequence(GtkStockItem &s)
    : key(s.keyval), mods(s.modifier) {}
  inline bool isEmpty()
  { return key == 0; }
  const guint key;
  const GdkModifierType mods;
};

/**
 * @brief create a new submenu entry
 * @param context the callback reference
 * @param menu_shell the menu to attach to
 * @param activate_cb the function to be called on selection
 * @param label the label to show (may be nullptr in case of item being set)
 * @param icon_name stock id or name for icon_load (may be nullptr)
 * @param accel_path accel database key (must be a static string)
 * @param keys the key sequence to trigger this action
 * @param item pre-created menu item (icon_name is ignored in this case)
 */
static GtkWidget * __attribute__((nonnull(2,6)))
menu_append_new_item(void *context, GtkWidget *menu_shell,
                     GCallback activate_cb, const char *label,
                     const gchar *icon_name,
                     const gchar *accel_path,
                     KeySequence keys = KeySequence(),
                     GtkWidget *item = nullptr)
{
  GtkStockItem stock_item;
  const bool stock_item_known = icon_name != nullptr &&
                                gtk_stock_lookup(icon_name, &stock_item) == TRUE;

  // Icons
  if (item == nullptr) {
    if(stock_item_known) {
      item = gtk_image_menu_item_new_with_mnemonic(label);
      gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
                                    gtk_image_new_from_stock(icon_name, GTK_ICON_SIZE_MENU));
    } else {
      item = MainUiGtk::createMenuItem(label, icon_name);
    }
  }

  // Accelerators
  // Default
  accel_path = g_intern_static_string(accel_path);
  gtk_menu_item_set_accel_path(GTK_MENU_ITEM(item), accel_path);
  if (!keys.isEmpty())
    gtk_accel_map_add_entry(accel_path, keys.key, keys.mods);
  else if (stock_item_known)
    gtk_accel_map_add_entry(accel_path, stock_item.keyval,
                              stock_item.modifier);

  gtk_menu_shell_append(GTK_MENU_SHELL(menu_shell), GTK_WIDGET(item));

  g_signal_connect_swapped(item, "activate", activate_cb, context);
  return item;
}

static GtkWidget *  __attribute__((nonnull(2,5)))
menu_append_new_item(appdata_t &appdata, GtkWidget *menu_shell,
                     GCallback activate_cb, MainUi::menu_items item,
                     const gchar *accel_path,
                     KeySequence keys = KeySequence())
{
  return menu_append_new_item(&appdata, menu_shell, activate_cb, nullptr, nullptr,
                              accel_path, keys,
                              static_cast<GtkWidget *>(static_cast<MainUiGtk *>(appdata.uicontrol.get())->menu_item(item)));
}

static void menu_create(appdata_internal &appdata, GtkBox *mainvbox) {
  GtkWidget *item, *submenu;

  MainUiGtk * const mainui = static_cast<MainUiGtk *>(appdata.uicontrol.get());

  /* -------------------- Project submenu -------------------- */

  GtkAccelGroup *accel_grp = gtk_accel_group_new();

  submenu = mainui->addMenu(_("_Project"));
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);

  menu_append_new_item(
    &appdata, submenu, G_CALLBACK(cb_menu_project_open), _("_Open"),
    GTK_STOCK_OPEN, "<OSM2Go-Main>/Project/Open");

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu),
                        gtk_separator_menu_item_new());

  menu_append_new_item(
    appdata.uicontrol.get(), submenu, G_CALLBACK(about_box), _("_About"),
    GTK_STOCK_ABOUT, "<OSM2Go-Main>/About");

  menu_append_new_item(
    appdata_t::window, submenu, G_CALLBACK(gtk_widget_destroy), _("_Quit"),
    GTK_STOCK_QUIT, "<OSM2Go-Main>/Quit");

  /* --------------- view menu ------------------- */

  submenu = mainui->addMenu(MainUi::SUBMENU_VIEW);
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);

  item = gtk_check_menu_item_new_with_mnemonic(_("_Fullscreen"));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
  menu_append_new_item(
    &appdata, submenu, G_CALLBACK(cb_menu_fullscreen), nullptr,
    nullptr, "<OSM2Go-Main>/View/Fullscreen",
    KeySequence(GDK_F11), item);

  menu_append_new_item(
    appdata.map, submenu, G_CALLBACK(cb_menu_zoomin), _("Zoom _in"),
    "zoom-in", "<OSM2Go-Main>/View/ZoomIn",
    KeySequence(GDK_comma, GDK_CONTROL_MASK));

  menu_append_new_item(
    appdata.map, submenu, G_CALLBACK(cb_menu_zoomout), _("Zoom _out"),
    "zoom-out", "<OSM2Go-Main>/View/ZoomOut",
    KeySequence(GDK_period, GDK_CONTROL_MASK));

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  menu_append_new_item(
    appdata.map, submenu, G_CALLBACK(cb_menu_view_detail_inc), _("More details"),
    nullptr, "<OSM2Go-Main>/View/DetailInc", KeySequence(GDK_period, GDK_MOD1_MASK));

  menu_append_new_item(
    appdata.map, submenu, G_CALLBACK(cb_menu_view_detail_normal), _("Normal details"),
    nullptr, "<OSM2Go-Main>/View/DetailNormal");

  menu_append_new_item(
    appdata.map, submenu, G_CALLBACK(cb_menu_view_detail_dec), _("Less details"),
    nullptr, "<OSM2Go-Main>/View/DetailDec", KeySequence(GDK_comma, GDK_MOD1_MASK));

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  item = menu_append_new_item(
    appdata, submenu, G_CALLBACK(cb_menu_map_hide_sel), MainUi::MENU_ITEM_MAP_HIDE_SEL,
    "<OSM2Go-Main>/View/HideSelected");
  gtk_widget_set_sensitive(item, FALSE);

  item = menu_append_new_item(
    appdata, submenu, G_CALLBACK(cb_menu_map_show_all), MainUi::MENU_ITEM_MAP_SHOW_ALL,
    "<OSM2Go-Main>/View/ShowAll");
  gtk_widget_set_sensitive(item, FALSE);

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  menu_append_new_item(
    &appdata, submenu, G_CALLBACK(style_select), _("St_yle"),
    GTK_STOCK_SELECT_COLOR, "<OSM2Go-Main>/View/Style");

  /* -------------------- map submenu -------------------- */

  submenu = mainui->addMenu(MainUi::SUBMENU_MAP);
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);

  menu_append_new_item(
    appdata, submenu, G_CALLBACK(cb_menu_upload), MainUi::MENU_ITEM_MAP_UPLOAD,
    "<OSM2Go-Main>/Map/Upload",
    KeySequence(GDK_u, GDK_SHIFT_MASK, GDK_CONTROL_MASK));

  menu_append_new_item(
    &appdata, submenu, G_CALLBACK(cb_menu_download), _("_Download"),
    "download.16", "<OSM2Go-Main>/Map/Download",
    KeySequence(GDK_d, GDK_SHIFT_MASK, GDK_CONTROL_MASK));

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  GtkStockItem stock_item;
  gboolean b = gtk_stock_lookup(GTK_STOCK_SAVE, &stock_item);
  assert(b == TRUE);
  menu_append_new_item(
    appdata, submenu, G_CALLBACK(cb_menu_save_changes), MainUi::MENU_ITEM_MAP_SAVE_CHANGES,
    "<OSM2Go-Main>/Map/SaveChanges", KeySequence(stock_item));

  menu_append_new_item(
    appdata, submenu, G_CALLBACK(cb_menu_undo_changes), MainUi::MENU_ITEM_MAP_UNDO_CHANGES,
    "<OSM2Go-Main>/Map/UndoAll");

  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

  menu_append_new_item(
    appdata, submenu, G_CALLBACK(cb_menu_osm_relations), MainUi::MENU_ITEM_MAP_RELATIONS,
    "<OSM2Go-Main>/Map/Relations",
    KeySequence(GDK_r, GDK_SHIFT_MASK, GDK_CONTROL_MASK));

  /* -------------------- wms submenu -------------------- */

  submenu = mainui->addMenu(MainUi::SUBMENU_WMS);
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);

  menu_append_new_item(
    &appdata, submenu, G_CALLBACK(wms_import), _("_Import"),
    GTK_STOCK_INDEX, "<OSM2Go-Main>/WMS/Import");

  menu_append_new_item(
    appdata, submenu, G_CALLBACK(wms_remove), MainUi::MENU_ITEM_WMS_CLEAR,
    "<OSM2Go-Main>/WMS/Clear");

  menu_append_new_item(
    appdata, submenu, G_CALLBACK(cb_menu_wms_adjust), MainUi::MENU_ITEM_WMS_ADJUST,
    "<OSM2Go-Main>/WMS/Adjust");

  /* -------------------- track submenu -------------------- */

  submenu = mainui->addMenu(MainUi::SUBMENU_TRACK);
  gtk_menu_set_accel_group(GTK_MENU(submenu), accel_grp);

  menu_append_new_item(
    appdata, submenu, G_CALLBACK(cb_menu_track_import), MainUi::MENU_ITEM_TRACK_IMPORT,
    "<OSM2Go-Main>/Track/Import");

  menu_append_new_item(
    appdata, submenu, G_CALLBACK(cb_menu_track_export), MainUi::MENU_ITEM_TRACK_EXPORT,
    "<OSM2Go-Main>/Track/Export");

  menu_append_new_item(
    appdata, submenu, G_CALLBACK(track_clear_cb), MainUi::MENU_ITEM_TRACK_CLEAR,
    "<OSM2Go-Main>/Track/Clear");

  const settings_t::ref settings = settings_t::instance();
  item = menu_append_new_item(
    appdata, submenu, G_CALLBACK(cb_menu_track_enable_gps), MainUi::MENU_ITEM_TRACK_ENABLE_GPS,
    "<OSM2Go-Main>/Track/GPS",
    KeySequence(GDK_g, GDK_CONTROL_MASK, GDK_SHIFT_MASK));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), settings->enable_gps ? TRUE : FALSE);

  item = menu_append_new_item(
    appdata, submenu, G_CALLBACK(cb_menu_track_follow_gps), MainUi::MENU_ITEM_TRACK_FOLLOW_GPS,
    "<OSM2Go-Main>/Track/Follow");
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), settings->follow_gps ? TRUE : FALSE);

  menu_append_new_item(
    &appdata, submenu, G_CALLBACK(cb_menu_track_vis), _("Track _visibility"),
    nullptr, "<OSM2Go-Main>/Track/Visibility",
    KeySequence(GDK_v, GDK_CONTROL_MASK, GDK_SHIFT_MASK));

  /* ------------------------------------------------------- */

  gtk_window_add_accel_group(GTK_WINDOW(appdata_t::window), accel_grp);

  gtk_box_pack_start(mainvbox, GTK_WIDGET(mainui->menuBar()), 0, 0, 0);
}

#else // !FREMANTLE

struct menu_entry_t {
  typedef gboolean (*toggle_cb)();
  explicit menu_entry_t(const char *l, GCallback cb = nullptr,
                        gboolean en = TRUE)
    : label(l), enabled(en), toggle(nullptr), menuindex(-1), activate_cb(cb) {}
  explicit menu_entry_t(MainUi::menu_items idx, GCallback cb = nullptr,
                        toggle_cb tg = nullptr)
    : label(nullptr), enabled(TRUE), toggle(tg), menuindex(idx), activate_cb(cb) {}
  const char *label;
  gboolean enabled;
  toggle_cb toggle;
  int menuindex;
  GCallback activate_cb;
};

static gboolean enable_gps_get_toggle() {
  return settings_t::instance()->enable_gps;
}

static gboolean follow_gps_get_toggle() {
  return settings_t::instance()->follow_gps;
}

#define COLUMNS  2

static void on_submenu_entry_clicked(GtkWidget *menu)
{
  /* force closing of submenu dialog */
  gtk_dialog_response(GTK_DIALOG(menu), GTK_RESPONSE_NONE);
  gtk_widget_hide(menu);

  /* let gtk clean up */
  osm2go_platform::process_events();
}

/* use standard dialog boxes for fremantle submenues */
static GtkWidget *app_submenu_create(appdata_t &appdata, MainUi::menu_items submenu,
                                     const menu_entry_t *menu, const unsigned int rows) {
  MainUiGtk * const mainui = static_cast<MainUiGtk *>(appdata.uicontrol.get());
  const char *title = hildon_button_get_title(HILDON_BUTTON(mainui->menu_item(submenu)));
  /* create a oridinary dialog box */
  GtkWidget *dialog = gtk_dialog_new_with_buttons(title, GTK_WINDOW(appdata_t::window),
                                                  GTK_DIALOG_MODAL, nullptr);

  osm2go_platform::dialog_size_hint(GTK_WINDOW(dialog), MISC_DIALOG_SMALL);
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

  GtkWidget *table = gtk_table_new(rows / COLUMNS, COLUMNS, TRUE);

  for(unsigned int idx = 0; idx < rows; idx++) {
    const menu_entry_t *menu_entries = menu + idx;
    GtkWidget *button;

    /* the "Style" menu entry is very special */
    /* and is being handled seperately */
    if(menu_entries->label != nullptr && strcmp(_("Style"), menu_entries->label) == 0) {
      button = style_select_widget(settings_t::instance()->style);
      g_object_set_data(G_OBJECT(dialog), "style_widget", button);
    } else if(menu_entries->label != nullptr && strcmp(_("Track visibility"), menu_entries->label) == 0) {
      button = track_vis_select_widget(settings_t::instance()->trackVisibility);
      g_object_set_data(G_OBJECT(dialog), "track_widget", button);
    } else if(!menu_entries->toggle) {
      if (menu_entries->menuindex >= 0)
        button = mainui->menu_item(static_cast<MainUi::menu_items>(menu_entries->menuindex));
      else
        button = MainUiGtk::createMenuItem(menu_entries->label);

      g_signal_connect_swapped(button, "clicked",
                               G_CALLBACK(on_submenu_entry_clicked), dialog);

      g_signal_connect_swapped(button, "clicked",
                               menu_entries->activate_cb, &appdata);

      hildon_button_set_title_alignment(HILDON_BUTTON(button), 0.5, 0.5);
      hildon_button_set_value_alignment(HILDON_BUTTON(button), 0.5, 0.5);

      if (unlikely(menu_entries->enabled == FALSE))
        gtk_widget_set_sensitive(button, FALSE);
    } else {
      button = mainui->menu_item(static_cast<MainUi::menu_items>(menu_entries->menuindex));
      hildon_check_button_set_active(HILDON_CHECK_BUTTON(button), menu_entries->toggle());

      g_signal_connect_swapped(button, "clicked",
                               G_CALLBACK(on_submenu_entry_clicked), dialog);

      g_signal_connect_swapped(button, "toggled",
                               menu_entries->activate_cb, &appdata);

      gtk_button_set_alignment(GTK_BUTTON(button), 0.5, 0.5);
    }

    const guint x = idx % COLUMNS, y = idx / COLUMNS;
    gtk_table_attach_defaults(GTK_TABLE(table),  button, x, x+1, y, y+1);
  }

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

  g_object_ref(dialog);
  return dialog;
}

/* popup the dialog shaped submenu */
static void submenu_popup(GtkWidget *menu) {
  gtk_widget_show_all(menu);
  gtk_dialog_run(GTK_DIALOG(menu));
  gtk_widget_hide(menu);
}

/* the view submenu */
static void on_submenu_view_clicked(appdata_internal *appdata)
{
  GtkWidget *menu = appdata->app_menu_view.get();
  submenu_popup(menu);
  GtkWidget *combo_widget = GTK_WIDGET(g_object_get_data(G_OBJECT(menu), "style_widget"));
  if(combo_widget != nullptr)
    style_change(*appdata, combo_widget);
}

static void on_submenu_track_clicked(appdata_internal *appdata)
{
  GtkWidget *menu = appdata->app_menu_track.get();
  submenu_popup(menu);

  GtkWidget *combo_widget = GTK_WIDGET(g_object_get_data(G_OBJECT(menu), "track_widget"));
  if(combo_widget != nullptr) {
    TrackVisibility tv = static_cast<TrackVisibility>(combo_box_get_active(combo_widget));
    settings_t::ref settings = settings_t::instance();
    if(tv != settings->trackVisibility && appdata->track.track) {
      appdata->map->track_draw(tv, *(appdata->track.track.get()));
      settings->trackVisibility = tv;
    }
  }
}

struct main_menu_entry_t {
  explicit main_menu_entry_t(const char *l, GCallback cb, void *cb_context)
    : label(l), menuindex(-1), activate_cb(cb), activate_context(cb_context) {}
  explicit main_menu_entry_t(MainUi::menu_items idx, GCallback cb, void *cb_context)
    : label(nullptr), menuindex(idx), activate_cb(cb), activate_context(cb_context) {}
  const char *label;
  int menuindex;
  GCallback activate_cb;
  void * const activate_context;
};

/* create a HildonAppMenu */
static HildonAppMenu *app_menu_create(appdata_internal &appdata) {
  /* -- the applications main menu -- */
  std::array<main_menu_entry_t, 7> main_menu = { {
    main_menu_entry_t(_("About"),                      G_CALLBACK(about_box), appdata.uicontrol.get()),
    main_menu_entry_t(_("Project"),                    G_CALLBACK(cb_menu_project_open), &appdata),
    main_menu_entry_t(MainUi::SUBMENU_VIEW,            G_CALLBACK(on_submenu_view_clicked), &appdata),
    main_menu_entry_t(MainUi::SUBMENU_MAP,             G_CALLBACK(submenu_popup), appdata.app_menu_map.get()),
    main_menu_entry_t(MainUi::MENU_ITEM_MAP_RELATIONS, G_CALLBACK(cb_menu_osm_relations), &appdata),
    main_menu_entry_t(MainUi::SUBMENU_WMS,             G_CALLBACK(submenu_popup), appdata.app_menu_wms.get()),
    main_menu_entry_t(MainUi::SUBMENU_TRACK,           G_CALLBACK(on_submenu_track_clicked), &appdata)
  } };

  MainUiGtk * const mainui = static_cast<MainUiGtk *>(appdata.uicontrol.get());
  HildonAppMenu * const menu = mainui->menuBar();
  for(unsigned int i = 0; i < main_menu.size(); i++) {
    const main_menu_entry_t &entry = main_menu[i];
    GtkWidget *button;

    if (entry.label == nullptr)
      button = mainui->addMenu(static_cast<MainUi::menu_items>(entry.menuindex));
    else
      button = mainui->addMenu(entry.label);

    g_signal_connect_data(button, "clicked",
                          entry.activate_cb, entry.activate_context, nullptr,
                          static_cast<GConnectFlags>(G_CONNECT_AFTER | G_CONNECT_SWAPPED));
  }

  gtk_widget_show_all(GTK_WIDGET(menu));
  return menu;
}

static void menu_create(appdata_internal &appdata, GtkBox *) {
  /* -- the view submenu -- */
  const std::array<menu_entry_t, 3> sm_view_entries = { {
    /* --- */
    menu_entry_t(_("Style")),
    /* --- */
    menu_entry_t(MainUi::MENU_ITEM_MAP_HIDE_SEL, G_CALLBACK(cb_menu_map_hide_sel), FALSE),
    menu_entry_t(MainUi::MENU_ITEM_MAP_SHOW_ALL, G_CALLBACK(cb_menu_map_show_all), FALSE),
  } };

  /* -- the map submenu -- */
  const std::array<menu_entry_t, 3> sm_map_entries = { {
    menu_entry_t(MainUi::MENU_ITEM_MAP_UPLOAD,       G_CALLBACK(cb_menu_upload)),
    menu_entry_t(_("Download"),                      G_CALLBACK(cb_menu_download)),
    menu_entry_t(MainUi::MENU_ITEM_MAP_UNDO_CHANGES, G_CALLBACK(cb_menu_undo_changes)),
  } };

  /* -- the wms submenu -- */
  const std::array<menu_entry_t, 3> sm_wms_entries = { {
    menu_entry_t(_("Import"),                  G_CALLBACK(wms_import)),
    menu_entry_t(MainUi::MENU_ITEM_WMS_CLEAR,  G_CALLBACK(wms_remove)),
    menu_entry_t(MainUi::MENU_ITEM_WMS_ADJUST, G_CALLBACK(cb_menu_wms_adjust)),
  } };

  /* -- the track submenu -- */
  const std::array<menu_entry_t, 6> sm_track_entries = { {
    menu_entry_t(MainUi::MENU_ITEM_TRACK_IMPORT,     G_CALLBACK(cb_menu_track_import)),
    menu_entry_t(MainUi::MENU_ITEM_TRACK_EXPORT,     G_CALLBACK(cb_menu_track_export)),
    menu_entry_t(MainUi::MENU_ITEM_TRACK_CLEAR,      G_CALLBACK(track_clear_cb)),
    menu_entry_t(MainUi::MENU_ITEM_TRACK_ENABLE_GPS, G_CALLBACK(cb_menu_track_enable_gps),
                 enable_gps_get_toggle),
    menu_entry_t(MainUi::MENU_ITEM_TRACK_FOLLOW_GPS, G_CALLBACK(cb_menu_track_follow_gps),
                 follow_gps_get_toggle),
    menu_entry_t(_("Track visibility")),
  } };

  /* build menu/submenus */
  appdata.app_menu_wms.reset(app_submenu_create(appdata, MainUi::SUBMENU_WMS,
                                                sm_wms_entries.data(), sm_wms_entries.size()));
  appdata.app_menu_map.reset(app_submenu_create(appdata, MainUi::SUBMENU_MAP,
                                                sm_map_entries.data(), sm_map_entries.size()));
  appdata.app_menu_view.reset(app_submenu_create(appdata, MainUi::SUBMENU_VIEW,
                                                 sm_view_entries.data(), sm_view_entries.size()));
  appdata.app_menu_track.reset(app_submenu_create(appdata, MainUi::SUBMENU_TRACK,
                                                  sm_track_entries.data(), sm_track_entries.size()));

  hildon_window_set_app_menu(HILDON_WINDOW(appdata_t::window), app_menu_create(appdata));
}
#endif

/********************* end of menu **********************/

appdata_t::appdata_t(map_state_t &mstate)
  : statusbar(statusbar_t::create())
  , uicontrol(new MainUiGtk(statusbar.get()))
  , project(nullptr)
  , iconbar(nullptr)
  , presets(nullptr)
  , map_state(mstate)
  , map(nullptr)
  , icons(icon_t::instance())
  , style(style_load(settings_t::instance()->style))
  , gps_state(gps_state_t::create(track_t::gps_position_callback, this))
{
  track.warn_cnt = 0;
}

appdata_t::~appdata_t() {
  settings_t::ref settings = settings_t::instance();
#ifdef ACCELS_FILE
  const std::string &accels_file = settings->base_path + ACCELS_FILE;
  gtk_accel_map_save(accels_file.c_str());
#endif

  settings->save();

  if(likely(map))
    map->set_autosave(false);

  g_debug("waiting for gtk to shut down ");

  /* let gtk clean up first */
  osm2go_platform::process_events();

  g_debug(" ok\n");

  /* save project file */
  if(project)
    project->save();
}

void appdata_t::track_clear()
{
  if (!track.track)
    return;

  g_debug("clearing track\n");

  if(likely(map != nullptr))
    track.track->clear();

  track.track.reset();
  track_menu_set(*this);
}

appdata_internal::appdata_internal(map_state_t &mstate)
  : appdata_t(mstate)
#ifdef FREMANTLE
  , program(nullptr)
#endif
  , btn_zoom_in(nullptr)
  , btn_zoom_out(nullptr)
{
}

appdata_internal::~appdata_internal()
{
#ifdef FREMANTLE
  program = nullptr;
#endif
}

static void on_window_destroy() {
  g_debug("main window destroy\n");

  gtk_main_quit();
  appdata_t::window = nullptr;
}

static gboolean on_window_key_press(appdata_internal *appdata, GdkEventKey *event) {
  /* forward unprocessed key presses to map */
  if(appdata->project && appdata->project->osm && event->type == GDK_KEY_PRESS)
    return appdata->map->key_press_event(event->keyval) ? TRUE : FALSE;

  return FALSE;
}

#if defined(FREMANTLE) && !defined(__i386__)
/* get access to zoom buttons */
static void
on_window_realize(GtkWidget *widget, gpointer) {
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

static GtkWidget *  __attribute__((nonnull(2,4)))
icon_button(void *context, const char *icon, GCallback cb, GtkWidget *box) {
  GtkWidget *but = gtk_button_new();
  const int icon_scale =
#ifdef FREMANTLE
    -1;
#else
    24;
#endif
  GtkWidget *iconw = icon_t::instance().widget_load(icon, icon_scale);
#ifndef FREMANTLE
  // explicitely assign image so the button does not show the action text
  if(iconw == nullptr)
    // gtk_image_new_from_icon_name() can't be used first, as it will return non-null even if nothing is found
    iconw = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_MENU);
#endif
  gtk_button_set_image(GTK_BUTTON(but), iconw);
#ifdef FREMANTLE
  //  gtk_button_set_relief(GTK_BUTTON(but), GTK_RELIEF_NONE);
  hildon_gtk_widget_set_theme_size(but,
            static_cast<HildonSizeType>(HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));

  if(cb)
#endif
    g_signal_connect_swapped(but, "clicked", cb, context);

  gtk_box_pack_start(GTK_BOX(box), but, FALSE, FALSE, 0);
  return but;
}

static int application_run(const char *proj)
{
  /* user specific init */
  map_state_t map_state;
  settings_t::ref settings = settings_t::instance();
  appdata_internal appdata(map_state);

  if(unlikely(!appdata.style)) {
    errorf(nullptr, _("Unable to load valid style %s, terminating."), settings->style.c_str());
    return -1;
  }

  assert_null(appdata_t::window);
#ifdef FREMANTLE
  /* Create the hildon program and setup the title */
  appdata.program = HILDON_PROGRAM(hildon_program_get_instance());
  g_set_application_name("OSM2Go");

  /* Create HildonWindow and set it to HildonProgram */
  HildonWindow *wnd = HILDON_WINDOW(hildon_stackable_window_new());
  appdata_t::window = GTK_WIDGET(wnd);
  hildon_program_add_window(appdata.program, wnd);

  /* try to enable the zoom buttons. don't do this on x86 as it breaks */
  /* at runtime with cygwin x */
#if !defined(__i386__)
  g_signal_connect(appdata_t::window, "realize", G_CALLBACK(on_window_realize), nullptr);
#endif // FREMANTLE

#else
  /* Create a Window. */
  appdata_t::window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(appdata_t::window), "OSM2Go");
  /* Set a decent default size for the window. */
  gtk_window_set_default_size(GTK_WINDOW(appdata_t::window), DEFAULT_WIDTH, DEFAULT_HEIGHT);
  gtk_window_set_icon(GTK_WINDOW(appdata_t::window), appdata.icons.load(PACKAGE)->buffer());
#endif

  g_signal_connect_swapped(appdata_t::window, "key_press_event",
                           G_CALLBACK(on_window_key_press), &appdata);
  g_signal_connect(appdata_t::window, "destroy", G_CALLBACK(on_window_destroy), nullptr);

  GtkBox *mainvbox = GTK_BOX(gtk_vbox_new(FALSE, 0));

  /* unconditionally enable the GPS */
  settings->enable_gps = true;
  menu_create(appdata, mainvbox);

#ifdef ACCELS_FILE
  const std::string &accels_file = settings->base_path + ACCELS_FILE;
  gtk_accel_map_load(accels_file.c_str());
#endif

  /* ----------------------- setup main window ---------------- */

  /* generate main map view */
  appdata.map = map_t::create(appdata);
  if(unlikely(!appdata.map))
    return -1;

  /* if tracking is enable, start it now */
  track_enable_gps(appdata, settings->enable_gps);

  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), iconbar_t::create(appdata), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), appdata.map->canvas->widget, TRUE, TRUE, 0);

  GtkWidget *sbar = osm2go_platform::statusBarWidget(appdata.statusbar.get());
  /* fremantle has seperate zoom/details buttons on the right screen side */
#ifndef FREMANTLE
  icon_button(appdata.map, "detailup_thumb",   G_CALLBACK(cb_menu_view_detail_inc), sbar);
  icon_button(appdata.map, "detaildown_thumb", G_CALLBACK(cb_menu_view_detail_dec), sbar);
  appdata.btn_zoom_out = icon_button(appdata.map, "zoom-in", G_CALLBACK(cb_menu_zoomout), sbar);
  appdata.btn_zoom_in = icon_button(appdata.map, "zoom-out", G_CALLBACK(cb_menu_zoomin), sbar);
#endif
  gtk_box_pack_start(GTK_BOX(vbox), sbar, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

#ifdef FREMANTLE
  /* fremantle has a set of buttons on the right screen side as well */
  vbox = gtk_vbox_new(FALSE, 0);

  GtkWidget *ivbox = gtk_vbox_new(FALSE, 0);
  appdata.btn_zoom_in =
    icon_button(appdata.map, "zoomin_thumb",   G_CALLBACK(cb_menu_zoomin), ivbox);
  appdata.btn_zoom_out =
    icon_button(appdata.map, "zoomout_thumb",  G_CALLBACK(cb_menu_zoomout), ivbox);
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, FALSE, FALSE, 0);

  ivbox = gtk_vbox_new(FALSE, 0);
  icon_button(appdata.map, "detailup_thumb",   G_CALLBACK(cb_menu_view_detail_inc), ivbox);
  icon_button(appdata.map, "detaildown_thumb", G_CALLBACK(cb_menu_view_detail_dec), ivbox);
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  ivbox = gtk_vbox_new(FALSE, 0);
  GtkWidget *ok = icon_button(nullptr, "ok_thumb", nullptr, ivbox);
  GtkWidget *cancel = icon_button(nullptr, "cancel_thumb", nullptr, ivbox);
  iconbar_register_buttons(appdata, ok, cancel);
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
#endif // FREMANTLE

  gtk_box_pack_start(mainvbox, hbox, TRUE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(appdata_t::window), GTK_WIDGET(mainvbox));

  gtk_widget_show_all(appdata_t::window);

  appdata.presets.reset(presets_items::load());

  /* let gtk do its thing before loading the data, */
  /* so the user sees something */
  osm2go_platform::process_events();
  if(unlikely(appdata_t::window == nullptr)) {
    g_debug("shutdown while starting up (1)\n");
    return -1;
  }

  if(proj != nullptr) {
    if(strcmp(proj, "-p") == 0) {
      cb_menu_project_open(&appdata);
    } else if(!project_load(appdata, proj)) {
      warningf(_("You passed '%s' on the command line, but it was neither"
                 "recognized as option nor could it be loaded as project."), proj);
    }
  }
  /* load project if one is specified in the settings */
  if(!appdata.project && !settings->project.empty())
    project_load(appdata, settings->project);

  // check if map widget was already destroyed
  if(unlikely(appdata.map == nullptr)) {
    g_debug("shutdown while starting up (2)\n");
    return -1;
  }
  appdata.map->set_autosave(true);
  appdata.main_ui_enable();

  /* start GPS if enabled by config */
  if(settings->enable_gps)
    track_enable_gps(appdata, true);

  /* again let the ui do its thing */
  osm2go_platform::process_events();
  if(unlikely(appdata_t::window == nullptr)) {
    g_debug("shutdown while starting up (3)\n");
    return -1;
  }

  /* start to interact with the user now that the gui is running */
  if(unlikely(appdata.project && appdata.project->isDemo && settings->first_run_demo))
    message_dlg(_("Welcome to OSM2Go"),
                _("This is the first time you run OSM2Go. A demo project has been loaded "
                  "to get you started. You can play around with this demo as much as you "
                  "like. However, you cannot upload or download the demo project.\n\n"
                  "In order to start working on real data you'll have to setup a new "
                  "project and enter your OSM user name and password. You'll then be "
                  "able to download the latest data from OSM and upload your changes "
                  "into the OSM main database."
                  ));

  g_debug("main up\n");

  /* ------------ jump into main loop ---------------- */
  gtk_main();

  g_debug("gtk_main() left\n");

  track_save(appdata.project.get(), appdata.track.track.get());
  appdata.track_clear();

  /* save a diff if there are dirty entries */
  if(likely(appdata.project))
    appdata.project->diff_save();

  return 0;
}

int main(int argc, char *argv[]) {
  // library init
  LIBXML_TEST_VERSION;

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
  g_thread_init(nullptr);
#endif

  gtk_init(&argc, &argv);
  int ret = osm2go_platform::init() ? 0 : 1;
  if (ret == 0) {
    ret = application_run(argc > 1 ? argv[1] : nullptr);

    osm2go_platform::cleanup();
  }

  // library cleanups
  xmlCleanupParser();
  curl_global_cleanup();

  return ret;
}
