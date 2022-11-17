/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <map.h>

#include <QTimer>

class QGraphicsView;

class map_graphicsview : public map_t {
public:
  map_graphicsview(appdata_t &a);
  ~map_graphicsview() override = default;

  void set_autosave(bool enable) override;

private:
  QTimer autosave;
  QGraphicsView * const view;
};
