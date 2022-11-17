/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <canvas.h>

#include <array>
#include <vector>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>

class QGraphicsItemGroup;

class CanvasScene : public QGraphicsScene {
  Q_OBJECT
  Q_DISABLE_COPY(CanvasScene)

public:
  explicit inline CanvasScene(QObject *parent = nullptr)
    : QGraphicsScene(parent) {}

  void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent) override
  {
    if (mouseEvent->buttons() == Qt::LeftButton && mouseEvent->modifiers() == 0) {
      emit mouseMove(mouseEvent->scenePos());
      mouseEvent->accept();
    }
//     QGraphicsScene::mouseMoveEvent(mouseEvent);
  }

  void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent) override
  {
    if (mouseEvent->buttons() == Qt::LeftButton && mouseEvent->modifiers() == 0) {
      emit mousePress(mouseEvent->scenePos());
      mouseEvent->accept();
    }
//     QGraphicsScene::mousePressEvent(mouseEvent);
  }
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent) override
  {
//     QGraphicsScene::mouseReleaseEvent(mouseEvent);
    emit mouseRelease(mouseEvent->scenePos());
    mouseEvent->accept();
  }

  void keyPressEvent(QKeyEvent *keyEvent) override;

signals:
  void mouseMove(const QPointF &p);
  void mousePress(const QPointF &p);
  void mouseRelease(const QPointF &p);
  void keyPress(int k);
};

struct canvas_graphicsscene : public canvas_t {
  explicit canvas_graphicsscene();
  ~canvas_graphicsscene();

  CanvasScene * const scene;

#if 0
  struct {
    lpos_t min, max;
  } bounds;
#endif

  struct {
    struct { float x = 0, y = 0; } scale;
  } bg;

  std::array<QGraphicsItemGroup *, CANVAS_GROUPS> group;

  std::array<std::vector<canvas_item_info_t *>, CANVAS_GROUPS> item_info;
};
