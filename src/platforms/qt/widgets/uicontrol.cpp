/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <uicontrol.h>

#include <appdata.h>

#include <cassert>
#include <cstdlib>

QWidget *appdata_t::window;

namespace {

class MainUiQt : public MainUi {
public:
  MainUiQt() = default;
  ~MainUiQt() override = default;

  void setActionEnable(menu_items, bool) override
  {
    abort();
  }

  void showMessage(unsigned int flags, const QString &message);
};

} // namespace

void MainUi::clearNotification(NotificationFlags)
{
  abort();
}

void MainUi::showNotification(trstring::native_type_arg message, unsigned int flags)
{
  assert(!message.isEmpty());
  static_cast<MainUiQt *>(this)->showMessage(flags, message);
}

void MainUiQt::showMessage(unsigned int , const QString &)
{
  abort();
}
