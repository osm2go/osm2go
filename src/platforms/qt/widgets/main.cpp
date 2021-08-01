/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <appdata.h>
#include <canvas.h>
#include <diff.h>
#include <gps_state.h>
#include <iconbar.h>
#include <josm_presets.h>
#include "MainUiQt.h"
#include <map.h>
#include "map_graphicsview.h"
#include <notifications.h>
#include <osm_api.h>
#include <project.h>
#include "project_widgets.h"
#include <object_dialogs.h>
#include <settings.h>
#include <style.h>
#include "style_widgets.h"
#include <track.h>
#include <wms.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <iostream>

#include <QAction>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QObject>
#include <QStringList>
#include <QToolBar>

#include "osm2go_annotations.h"
#include <osm2go_i18n.h>

#define LOCALEDIR PREFIX "/locale"

/* these size defaults are used in the non-hildonized version only */
#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480

namespace {

struct appdata_internal : public appdata_t {
  explicit appdata_internal() = default;

  QAction *btn_zoom_in = nullptr;
  QAction *btn_zoom_out = nullptr;

  inline QAction *menu_append_new_item(QMenu *menu_shell, MainUi::menu_items item);
};

} // namespace

/* disable/enable main screen control dependant on presence of open project */
void
appdata_t::main_ui_enable()
{
  bool osm_valid = (project && project->osm);

  if(unlikely(window == nullptr)) {
    qDebug() << __PRETTY_FUNCTION__ << ": main window gone";
    return;
  }

  /* cancel any action in progress */
  if(iconbar->isCancelEnabled())
    map->action_cancel();

  set_title();

  iconbar->setToolbarEnable(osm_valid);
  /* disable all menu entries related to map */
  uicontrol->setActionEnable(MainUi::SUBMENU_MAP, static_cast<bool>(project));

  // those icons that get enabled or disabled depending on OSM data being loaded
  for (auto &&item : { MainUi::MENU_ITEM_MAP_SAVE_CHANGES,
                       MainUi::MENU_ITEM_MAP_UPLOAD,
                       MainUi::MENU_ITEM_MAP_UNDO_CHANGES,
                       MainUi::MENU_ITEM_MAP_RELATIONS,
                       MainUi::SUBMENU_TRACK,
                       MainUi::SUBMENU_VIEW,
                       MainUi::SUBMENU_WMS,
                     })
    uicontrol->setActionEnable(item, osm_valid);

  static_cast<appdata_internal *>(this)->btn_zoom_in->setEnabled(osm_valid);
  static_cast<appdata_internal *>(this)->btn_zoom_out->setEnabled(osm_valid);

  if(!project)
    uicontrol->showNotification(trstring("Please load or create a project"));
}

void
appdata_t::set_title()
{
  QString str = trstring("OSM2go");

  if(project)
    str = trstring("%1 - OSM2go").arg(QString::fromStdString(project->name));

  window->setWindowTitle(str);
}

/******************** begin of menu *********************/

static void
cb_menu_project_open(appdata_t &appdata)
{
  std::unique_ptr<project_t> project = project_select(appdata);
  if(project)
    project_load(appdata, project);
  appdata.main_ui_enable();
}

static void
cb_menu_upload(appdata_t *appdata)
{
  if(!appdata->project || !appdata->project->osm)
    return;

  if(appdata->project->check_demo())
    return;

  osm_upload(*appdata);
}

static void
cb_menu_download(appdata_t *appdata)
{
  if(!appdata->project)
    return;

  if(appdata->project->check_demo())
    return;

  appdata->map->set_autosave(false);

  /* if we have valid osm data loaded: save state first */
  appdata->project->diff_save();

  // download
  const auto hasMap = static_cast<bool>(appdata->project->osm);
  if(osm_download(appdata_t::window, appdata->project.get())) {
    if(hasMap)
      /* redraw the entire map by destroying all map items and redrawing them */
      appdata->map->clear(map_t::MAP_LAYER_OBJECTS_ONLY);

    appdata->uicontrol->showNotification(trstring("Drawing"), MainUi::Busy);
    if(appdata->project->parse_osm()) {
      diff_restore(appdata->project, appdata->uicontrol.get());
      appdata->map->paint();
    }
    appdata->uicontrol->clearNotification(MainUi::Busy);
  }

  appdata->map->set_autosave(true);
  appdata->main_ui_enable();
}

/* ---------------------------------------------------------- */

static bool
track_visibility_select()
{
  const QStringList items = {
    trstring("Hide tracks"),
    trstring("Show current position"),
    trstring("Show current segment"),
    trstring("Show all segments")
  };

  auto settings = settings_t::instance();

  bool ok;
  const QString item = QInputDialog::getItem(appdata_t::window, trstring("Select track visibility"),
                                             trstring("Track visibility:"), items,
                                             static_cast<int>(settings->trackVisibility),
                                             false, &ok);

  if(ok && !item.isEmpty()) {
    int index = items.indexOf(item);
    assert_cmpnum_op(index, >=, 0);

    auto tv = static_cast<TrackVisibility>(index);
    ok = (tv != settings->trackVisibility);
    settings->trackVisibility = tv;
  }

  return ok;
}

static void
cb_menu_save_changes(appdata_t *appdata)
{
  if(likely(appdata->project))
    appdata->project->diff_save();
  appdata->uicontrol->showNotification(trstring("Saved local changes"), MainUi::Brief);
}

static void
cb_menu_undo_changes(appdata_t *appdata)
{
  project_t::ref project = appdata->project;
  // if there is nothing to clean then don't ask
  if (!project->diff_file_present() && project->osm->is_clean(true))
    return;

  if(!osm2go_platform::yes_no(trstring("Undo all changes?"),
                              trstring("Throw away all the changes you've not uploaded yet? This cannot be undone.")))
    return;

  appdata->map->clear(map_t::MAP_LAYER_OBJECTS_ONLY);

  project->diff_remove_file();
  project->parse_osm();
  appdata->map->paint();

  appdata->uicontrol->showNotification(trstring("Undo all changes"), MainUi::Brief);
}

static void
cb_menu_track_import(appdata_t &appdata)
{
  settings_t::ref settings = settings_t::instance();

  QString dir;
  if(!settings->track_path.empty()) {
    QFileInfo track(QString::fromStdString(settings->track_path));
    if(track.isDir())
      dir = track.filePath();
    else
      dir = track.path();
  }

  const QString filename = QFileDialog::getOpenFileName(appdata.window, trstring("Import track file"), dir);
  if (!filename.isEmpty()) {
    /* remove any existing track */
    appdata.track_clear();

    /* load a track */
    appdata.track.track.reset(track_import(filename.toUtf8().constData()));
    if(appdata.track.track) {
      appdata.map->track_draw(settings->trackVisibility, *appdata.track.track);

      settings->track_path = filename.toUtf8().constData();
    }
    track_menu_set(appdata);
  }
}

static void
cb_menu_track_export(appdata_t &appdata)
{
  settings_t::ref settings = settings_t::instance();

  QString dir;
  if(!settings->track_path.empty()) {
    QFileInfo track(QString::fromStdString(settings->track_path));
    if(track.isDir())
      dir = track.filePath();
    else
      dir = track.path();
  }

  const QString filename = QFileDialog::getSaveFileName(appdata_t::window, trstring("Export track file"), dir);
  if (!filename.isEmpty()) {
    qDebug() << "export to " << filename;

    settings->track_path = filename.toUtf8().constData();

    assert(appdata.track.track);
    track_export(appdata.track.track.get(), filename.toUtf8().constData());
  }
}

/*
 *  Platform-specific UI tweaks.
 */

/**
 * @brief create a new submenu entry
 * @param menu_shell the menu to attach to
 * @param label the label to show (may be nullptr in case of item being set)
 * @param icon_name name for icon_load (may be nullptr)
 * @param keys the key sequence to trigger this action
 * @param item pre-created menu item (icon_name is ignored in this case)
 */
static QAction * __attribute__((nonnull(1))) __attribute__((warn_unused_result))
menu_append_new_item(QMenu *menu_shell, const QString &label, const char *icon_name = nullptr,
                     const QKeySequence &shortcut = QKeySequence())
{
  QIcon icon;
  if (icon_name != nullptr) {
    icon = QIcon::fromTheme(QLatin1String(icon_name));

    if (icon.isNull()) {
      icon_item *icitem = icon_t::instance().load(icon_name);
      if (icitem != nullptr)
        icon = osm2go_platform::icon_pixmap(icitem);
    }
  }

  auto *action = menu_shell->addAction(icon, label);

  if (!shortcut.isEmpty())
    action->setShortcut(shortcut);

  return action;
}

/**
 * @brief create a new submenu entry
 * @param appdata the appdata object
 * @param menu_shell the menu to attach to
 * @param item the menu item to use
 * @param keys the key sequence to trigger this action
 */
QAction * __attribute__((warn_unused_result))
appdata_internal::menu_append_new_item(QMenu *menu_shell, MainUi::menu_items item)
{
  auto *action = static_cast<QAction *>(static_cast<MainUiQt *>(uicontrol.get())->menu_item(item));

  menu_shell->addAction(action);

  return action;
}

static void
menu_create(appdata_internal &appdata, QMenuBar *menu)
{
  auto * const mainui = static_cast<MainUiQt *>(appdata.uicontrol.get());

  /* -------------------- Project submenu -------------------- */

  QMenu *submenu = menu->addMenu(trstring("&Project"));
  map_t * const map = appdata.map;

  QAction *item = menu_append_new_item(submenu, trstring("&Open"),
                              "document-open", QKeySequence::Open);
  QObject::connect(item, &QAction::triggered, [&appdata](){ cb_menu_project_open(appdata); });

  submenu->addSeparator();

  item = menu_append_new_item(submenu, trstring("&About"), "help-about");
  QObject::connect(item, &QAction::triggered, appdata.window, [mainui]() { mainui->about_box(); });

  item = menu_append_new_item(submenu, trstring("&Quit"),
                              "application-exit", QKeySequence::Quit);
  QObject::connect(item, &QAction::triggered, QApplication::instance(), &QCoreApplication::quit);

  /* --------------- view menu ------------------- */

  submenu = static_cast<QMenu *>(mainui->menu_item(MainUi::SUBMENU_VIEW));
  menu->addMenu(submenu);

  item = menu_append_new_item(submenu, trstring("&Fullscreen"),
                              "view-fullscreen", QKeySequence::FullScreen);
  item->setCheckable(true);
  QObject::connect(item, &QAction::triggered, appdata.window,
    [&appdata](bool on) {
      if(on)
        appdata.window->setWindowState(appdata.window->windowState() | Qt::WindowFullScreen);
      else
        appdata.window->setWindowState(appdata.window->windowState() & ~Qt::WindowFullScreen);
    }
  );

  appdata.btn_zoom_in = menu_append_new_item(submenu, trstring("Zoom &in"),
                                             "zoom-in", QKeySequence::ZoomIn);
  QObject::connect(appdata.btn_zoom_in, &QAction::triggered, map->canvas->widget,
    [map, &appdata](){map->set_zoom(appdata.project->map_state.zoom * ZOOM_FACTOR_MENU, true);});

  appdata.btn_zoom_out = menu_append_new_item(submenu, trstring("Zoom &out"),
                                              "zoom-out", QKeySequence::ZoomOut);
  QObject::connect(appdata.btn_zoom_out, &QAction::triggered, map->canvas->widget,
    [map, &appdata](){map->set_zoom(appdata.project->map_state.zoom / ZOOM_FACTOR_MENU, true);});

  submenu->addSeparator();

  item = menu_append_new_item(submenu, trstring("More details"),
                              nullptr, QKeySequence(Qt::ALT + Qt::Key_Period));
  QObject::connect(item, &QAction::triggered, appdata.window, [map]() { map->detail_increase(); });

  item = menu_append_new_item(submenu, trstring("Normal details"));
  QObject::connect(item, &QAction::triggered, appdata.window, [map]() { map->detail_normal(); });

  item = menu_append_new_item(submenu, trstring("Less details"),
                              nullptr, QKeySequence(Qt::ALT + Qt::Key_Comma));
  QObject::connect(item, &QAction::triggered, appdata.window, [map]() { map->detail_decrease(); });

  submenu->addSeparator();

  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_MAP_HIDE_SEL);
  QObject::connect(item, &QAction::triggered, appdata.window, [map]() { map->hide_selected(); });
  item->setEnabled(false);

  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_MAP_SHOW_ALL);
  QObject::connect(item, &QAction::triggered, appdata.window, [map]() { map->show_all(); });
  item->setEnabled(false);

  submenu->addSeparator();

  item = menu_append_new_item(submenu, trstring("St&yle"), "color-picker");
  QObject::connect(item, &QAction::triggered, [&appdata]() { style_select(appdata); });

  /* -------------------- map submenu -------------------- */
  submenu = static_cast<QMenu *>(mainui->menu_item(MainUi::SUBMENU_MAP));
  menu->addMenu(submenu);

  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_MAP_UPLOAD);
  QObject::connect(item, &QAction::triggered, [&appdata]() { cb_menu_upload(&appdata); });

  item = menu_append_new_item(submenu, trstring("&Download"),
                              "download.16", QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D));
  QObject::connect(item, &QAction::triggered, [&appdata]() { cb_menu_download(&appdata); });

  submenu->addSeparator();

  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_MAP_SAVE_CHANGES);
  QObject::connect(item, &QAction::triggered, [&appdata]() { cb_menu_save_changes(&appdata); });

  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_MAP_UNDO_CHANGES);
  QObject::connect(item, &QAction::triggered, [&appdata]() { cb_menu_undo_changes(&appdata); });

  submenu->addSeparator();

  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_MAP_RELATIONS);
  QObject::connect(item, &QAction::triggered, [&appdata]() {
    relation_list(appdata_t::window, appdata.map, appdata.project->osm, appdata.presets.get());
  });

  /* -------------------- wms submenu -------------------- */
  submenu = static_cast<QMenu *>(mainui->menu_item(MainUi::SUBMENU_WMS));
  menu->addMenu(submenu);

  item = menu_append_new_item(submenu, trstring("&Import"), "document-import");
  QObject::connect(item, &QAction::triggered, [&appdata]() {
    std::string fn = wms_import(appdata_t::window, appdata.project);
    if (!fn.empty())
      appdata.map->set_bg_image(fn, osm2go_platform::screenpos(0, 0));
  });

  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_WMS_CLEAR);
  QObject::connect(item, &QAction::triggered, [&appdata]() {
    appdata.map->remove_bg_image();
    wms_remove_file(*appdata.project);
  });

  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_WMS_ADJUST);
  QObject::connect(item, &QAction::triggered, [map]() { map->set_action(MAP_ACTION_BG_ADJUST); });

  /* -------------------- track submenu -------------------- */
  submenu = static_cast<QMenu *>(mainui->menu_item(MainUi::SUBMENU_TRACK));
  menu->addMenu(submenu);

  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_TRACK_IMPORT);
  QObject::connect(item, &QAction::triggered, [&appdata]() { cb_menu_track_import(appdata); });

  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_TRACK_EXPORT);
  QObject::connect(item, &QAction::triggered, [&appdata]() { cb_menu_track_export(appdata); });

  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_TRACK_CLEAR);
  QObject::connect(item, &QAction::triggered, [&appdata]() { appdata.track_clear(); });

  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_TRACK_CLEAR_CURRENT);
  QObject::connect(item, &QAction::triggered, [&appdata]() { appdata.track_clear_current(); });

  const settings_t::ref settings = settings_t::instance();
  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_TRACK_ENABLE_GPS);
  item->setChecked(settings->enable_gps);
  // connect afterwards to not trigger track enabling here, will later be done explicitely
  QObject::connect(item, &QAction::toggled, [&appdata](bool en) { track_enable_gps(appdata, en); });

  item = appdata.menu_append_new_item(submenu, MainUi::MENU_ITEM_TRACK_FOLLOW_GPS);
  item->setChecked(settings->follow_gps);
  QObject::connect(item, &QAction::toggled, [](bool en) { settings_t::instance()->follow_gps = en; });

  item = menu_append_new_item(submenu, trstring("Track &visibility"), nullptr,
                              QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_V));
  QObject::connect(item, &QAction::triggered, [&appdata]() {
    if(track_visibility_select() && appdata.track.track)
      appdata.map->track_draw(settings_t::instance()->trackVisibility, *appdata.track.track);
  });
}

/********************* end of menu **********************/

appdata_t::appdata_t()
  : uicontrol(new MainUiQt())
  , map(nullptr)
  , icons(icon_t::instance())
  , style(style_t::load(settings_t::instance()->style))
  , gps_state(gps_state_t::create(track_t::gps_position_callback, this))
{
  track.warn_cnt = 0;
}

appdata_t::~appdata_t()
{
  settings_t::ref settings = settings_t::instance();

  settings->save();

  if(likely(map))
    map->set_autosave(false);

  /* let gtk clean up first */
  osm2go_platform::process_events();

  /* save project file */
  if(project)
    project->save(nullptr);

  qDebug() << "everything is gone";
}

void
appdata_t::track_clear()
{
  if (!track.track)
    return;

  if(likely(map != nullptr))
    track.track->clear();

  track.track.reset();
  track_menu_set(*this);
}

void
appdata_t::track_clear_current()
{
  if (!track.track || !track.track->active)
    return;

  if(likely(map != nullptr))
    track.track->clear_current();

  track_menu_set(*this);
}

static int
application_run(bool showProjects, const QString &proj)
{
  /* Must be present before appdata, so MainUiQt can use it */
  auto mainwindow = std::make_unique<QMainWindow>();
  appdata_t::window = mainwindow.get();

  /* user specific init */
  appdata_internal appdata;

  if(unlikely(!appdata.style)) {
    error_dlg(trstring("Unable to load valid style %1, terminating.").arg(settings_t::instance()->style));
    return -1;
  }

  appdata.set_title();
  appdata_t::window->setWindowIcon(osm2go_platform::icon_pixmap(appdata.icons.load(PACKAGE)));
  mainwindow->resize(640, 480);

  /* unconditionally enable the GPS */
  settings_t::instance()->enable_gps = true;

  /* ----------------------- setup main window ---------------- */

  /* generate main map view */
  appdata.map = new map_graphicsview(appdata);

  menu_create(appdata, mainwindow->menuBar());

  /* if tracking is enable, start it now */
  track_enable_gps(appdata, settings_t::instance()->enable_gps);

  mainwindow->addToolBar(qobject_cast<QToolBar *>(iconbar_t::create(appdata)));
  mainwindow->setCentralWidget(appdata.map->canvas->widget);

  mainwindow->show();

  appdata.presets.reset(presets_items::load());

  /* let gtk do its thing before loading the data, */
  /* so the user sees something */
  osm2go_platform::process_events();
  if(unlikely(appdata_t::window == nullptr)) {
    qDebug() << "shutdown while starting up (1)";
    return -1;
  }

  if(showProjects) {
    cb_menu_project_open(appdata);
  } else if(!proj.isEmpty() && !project_load(appdata, proj.toUtf8().constData())) {
    message_dlg(trstring("Command line arguments"),
                trstring("You passed '%1' on the command line, but it was neither"
                            "recognized as option nor could it be loaded as project.").arg(proj));
  }

  /* load project if one is specified in the settings */
  if(!appdata.project && !settings_t::instance()->project.empty())
    project_load(appdata, settings_t::instance()->project);

  appdata.main_ui_enable();

  /* start GPS if enabled by config */
  if(settings_t::instance()->enable_gps)
    track_enable_gps(appdata, true);

  /* again let the ui do its thing */
  osm2go_platform::process_events();
  if(unlikely(appdata_t::window == nullptr)) {
    qDebug() << "shutdown while starting up (2)";
    return -1;
  }

  /* start to interact with the user now that the gui is running */
  if(unlikely(appdata.project && appdata.project->isDemo && settings_t::instance()->first_run_demo))
    message_dlg(trstring("Welcome to OSM2Go"),
                trstring("This is the first time you run OSM2Go. A demo project has been loaded "
                            "to get you started. You can play around with this demo as much as you "
                            "like. However, you cannot upload or download the demo project.\n\n"
                            "In order to start working on real data you'll have to setup a new "
                            "project and enter your OSM user name and password. You'll then be "
                            "able to download the latest data from OSM and upload your changes "
                            "into the OSM main database."));

  qDebug() << "main up";

  /* ------------ jump into main loop ---------------- */
  QApplication::instance()->exec();

  qDebug() << "Qt eventloop left";

  track_save(appdata.project, appdata.track.track.get());
  appdata.track_clear();

  /* save a diff if there are dirty entries */
  if(likely(appdata.project))
    appdata.project->diff_save();

  return 0;
}

int
main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  QString project;
  bool showProjects = false;

  {
//    QCoreApplication::setOrganizationName("OSM2go"); // doing this would break the standard paths
    QCoreApplication::setApplicationName("osm2go");
    QCoreApplication::setApplicationVersion(VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription(trstring("Mobile editor for OpenStreetMap.org map data"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("project", trstring("Source file to copy."));

    QCommandLineOption showProjectsOption({ QLatin1String("p"), QLatin1String("projects") },
                                          trstring("open the project selection dialog"));
    parser.addOption(showProjectsOption);

    parser.process(app);

    showProjects = parser.isSet(showProjectsOption);
    const QStringList &extraArgs = parser.positionalArguments();
    if (!extraArgs.isEmpty())
      project = extraArgs.first();
  }

  // library init
  LIBXML_TEST_VERSION;

  /* Must initialize libcurl before any threads are started */
  if (unlikely(curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK))
    return 1;

  /* Same for libxml2 */
  xmlInitParser();

  /* whitespace between tags has no meaning in any of the XML files used here */
  xmlKeepBlanksDefault(0);

  int ret = application_run(showProjects, project);

  // library cleanups
  xmlCleanupParser();
  curl_global_cleanup();

  return ret;
}
