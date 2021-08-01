/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <uicontrol.h>

#include <array>
#include <QPointer>

class QLabel;
class QObject;
class QString;

class MainUiQt : public MainUi {
  std::array<QPointer<QObject>, MainUi::MENU_ITEMS_COUNT> menuitems;

public:
  MainUiQt();
  ~MainUiQt() override = default;

  inline QObject *menu_item(menu_items item)
  { return menuitems.at(item); }

  void about_box();

  void setActionEnable(menu_items item, bool en) override;

  void showMessage(unsigned int flags, const QString &message);

  void clearNotification(NotificationFlags flags) override;

  QPointer<QLabel> m_currentMessage;
  QPointer<QLabel> m_permanentMessage;
};
