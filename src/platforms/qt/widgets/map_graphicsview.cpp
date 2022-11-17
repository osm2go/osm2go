/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "map_graphicsview.h"

#include <appdata.h>
#include "canvas_graphicsscene.h"
#include <iconbar.h>

#include <cassert>
#include <chrono>
#include <QApplication>
#include <QDebug>
#include <QGraphicsView>

namespace {

void map_autosave(QGraphicsView *view, appdata_t &appdata)
{
  /* only do this if root window has focus as otherwise */
  /* a dialog may be open and modifying the basic structures */
  if(view->hasFocus()) {
    qDebug() << "autosave ...";

    if(appdata.project && appdata.project->osm) {
      track_save(appdata.project, appdata.track.track.get());
      appdata.project->diff_save();
    }
  } else
    qDebug() << "autosave suppressed";
}

} // namespace

map_graphicsview::map_graphicsview(appdata_t &a)
  : map_t(a, new canvas_graphicsscene())
  , view(qobject_cast<QGraphicsView *>(canvas->widget))
{
  assert(view != nullptr);

  QObject::connect(static_cast<QGuiApplication *>(QApplication::instance()), &QGuiApplication::lastWindowClosed, [this](){ delete this; });
  autosave.setInterval(std::chrono::minutes(2));
  autosave.setSingleShot(false);
  QObject::connect(&autosave, &QTimer::timeout, [v = view, &a = appdata](){ map_autosave(v, a); });

  auto cs = static_cast<CanvasScene *>(view->scene());
  QObject::connect(cs, &CanvasScene::mouseMove, [this](const QPointF &p) {
    if(unlikely(!appdata.project || !appdata.project->osm))
      return;

    if(!pen_down.is)
      return;

    handle_motion(p);
  });
  QObject::connect(cs, &CanvasScene::mousePress, [this](const QPointF &p) { button_press(p); });
  QObject::connect(cs, &CanvasScene::mouseRelease, [this](const QPointF &p) { button_release(p); });
  QObject::connect(cs, &CanvasScene::keyPress, [this](int key) {
    switch (key) {
    case Qt::Key_Return:
      /* if the ok button is enabled, call its function */
      if(appdata.iconbar->isOkEnabled())
        action_ok();
      /* otherwise if info is enabled call that */
      else if(appdata.iconbar->isInfoEnabled())
        info_selected();
      break;

    default:
      assert_unreachable();
    }
  });
}

void map_graphicsview::set_autosave(bool enable)
{
  if(enable)
    autosave.start();
  else
    autosave.stop();
}
