/*
 * SPDX-FileCopyrightText: 2020-2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "MainUiQt.h"

#include <appdata.h>

#include <array>
#include <cassert>
#include <cstdlib>
#include <QAction>
#include <QGuiApplication>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QStatusBar>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include "osm2go_i18n.h"

QWidget *appdata_t::window;

namespace {

QAction * __attribute__ ((nonnull(1))) __attribute__((warn_unused_result))
create_checkbox_item(QMenu *menu, const QString &label)
{
  auto ret = menu->addAction(label);
  ret->setCheckable(true);
  return ret;
}

QAction * __attribute__ ((nonnull(1))) __attribute__((warn_unused_result))
menu_entry(QMenu *menu, const QString &label, const char *icon_name, const QKeySequence &shortcut)
{
  auto ret = menu->addAction(label);
  if (icon_name != nullptr)
    ret->setIcon(QIcon::fromTheme(QLatin1String(icon_name)));
  ret->setShortcut(shortcut);
  return ret;
}

inline QAction * __attribute__ ((nonnull(1,3))) __attribute__((warn_unused_result))
menu_action(QMenu *menu, const QString &label, const char *icon_name)
{
  return menu->addAction(QIcon::fromTheme(QLatin1String(icon_name)), label);
}

} // namespace

MainUiQt::MainUiQt()
  : MainUi()
{
  // make sure the QMenu instances are owned
  assert(appdata_t::window != nullptr);

  menuitems[SUBMENU_VIEW] = new QMenu(QObject::tr("&View"), appdata_t::window);

  auto *mapMenu = new QMenu(QObject::tr("&Map"), appdata_t::window);
  menuitems[SUBMENU_MAP] = mapMenu;
  menuitems[MENU_ITEM_MAP_RELATIONS] = menu_entry(mapMenu, QObject::tr("&Relations"), nullptr, QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_R));
  menuitems[MENU_ITEM_MAP_UPLOAD] = menu_entry(mapMenu, QObject::tr("&Upload"), "upload.16", QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_U));
  menuitems[MENU_ITEM_MAP_UNDO_CHANGES] = menu_action(mapMenu, QObject::tr("Undo &all"), "edit-delete");
  menuitems[MENU_ITEM_MAP_SAVE_CHANGES] = menu_entry(mapMenu, QObject::tr("&Save local changes"), "document-save", QKeySequence::Save);
  menuitems[MENU_ITEM_MAP_HIDE_SEL] = menu_action(mapMenu, QObject::tr("&Hide selected"), "list-remove");
  menuitems[MENU_ITEM_MAP_SHOW_ALL] = menu_action(mapMenu, QObject::tr("&Show all"), "list-add");

  auto *wmsMenu = new QMenu(QObject::tr("&WMS"), appdata_t::window);
  menuitems[SUBMENU_WMS] = wmsMenu;
  menuitems[MENU_ITEM_WMS_CLEAR] = menu_action(wmsMenu, QObject::tr("&Clear"), "edit-clear");
  menuitems[MENU_ITEM_WMS_ADJUST] = wmsMenu->addAction(QObject::tr("&Adjust"));

  auto *trackMenu = new QMenu(QObject::tr("&Track"), appdata_t::window);
  menuitems[SUBMENU_TRACK] = trackMenu;
  menuitems[MENU_ITEM_TRACK_IMPORT] =  trackMenu->addAction(QObject::tr("&Import"));
  menuitems[MENU_ITEM_TRACK_EXPORT] = trackMenu->addAction(QObject::tr("&Export"));
  menuitems[MENU_ITEM_TRACK_CLEAR] = menu_action(trackMenu, QObject::tr("&Clear"), "edit-clear");
  menuitems[MENU_ITEM_TRACK_CLEAR_CURRENT] = menu_action(trackMenu, QObject::tr("Clear c&urrent segment"), "edit-clear");
  auto *mi = create_checkbox_item(trackMenu, QObject::tr("&GPS enable"));
  mi->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_G));
  menuitems[MENU_ITEM_TRACK_ENABLE_GPS] = mi;

  menuitems[MENU_ITEM_TRACK_FOLLOW_GPS] = create_checkbox_item(trackMenu, QObject::tr("GPS follow"));
}

void MainUiQt::setActionEnable(menu_items item, bool en)
{
  auto *obj = static_cast<MainUiQt *>(this)->menu_item(item);
  auto act = qobject_cast<QAction *>(obj);
  if (act != nullptr)
    act->setEnabled(en);
  else
    qobject_cast<QWidget *>(obj)->setEnabled(en);
}

void MainUiQt::clearNotification(NotificationFlags flags)
{
  QStatusBar *statusbar = static_cast<QMainWindow *>(appdata_t::window)->statusBar();

  if (flags & Busy) {
    statusbar->removeWidget(m_permanentMessage);
    delete m_permanentMessage;
    QGuiApplication::restoreOverrideCursor();
  }
  if (flags & ClearNormal) {
    statusbar->removeWidget(m_currentMessage);
    delete m_currentMessage;
  }
}

void MainUi::showNotification(trstring::native_type_arg message, unsigned int flags)
{
  assert(!message.isEmpty());
  static_cast<MainUiQt *>(this)->showMessage(flags, message);
}

void MainUiQt::showMessage(unsigned int flags, const QString &message)
{
  QStatusBar *statusbar = static_cast<QMainWindow *>(appdata_t::window)->statusBar();
  if (flags & Brief) {
    statusbar->showMessage(message, 3 * 1000);
  } else if (flags & Busy) {
    m_permanentMessage = new QLabel(message);
    statusbar->addPermanentWidget(m_permanentMessage);
    QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  } else {
    if (!m_currentMessage.isNull()) {
      statusbar->removeWidget(m_currentMessage);
      delete m_currentMessage;
    }
    m_currentMessage = new QLabel(message);
    if (flags & Highlight) {
      auto font = m_currentMessage->font();
      font.setBold(true);
      m_currentMessage->setFont(font);
    }
    statusbar->addWidget(m_currentMessage);
  }
}

void MainUiQt::about_box()
{
  abort(); // FIXME
}
