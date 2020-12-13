/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <notifications.h>

#include <appdata.h>

#include <QDebug>
#include <QMessageBox>

#include <osm2go_annotations.h>
#include "osm2go_i18n.h"

void
error_dlg(const trstring &msg, osm2go_platform::Widget *parent)
{
  if(unlikely(parent == nullptr)) {
    qDebug() << msg;
    return;
  }

  QMessageBox::critical(parent, QObject::tr("Error"), msg);
}

void
warning_dlg(const trstring &msg, osm2go_platform::Widget *parent)
{
  if(unlikely(parent == nullptr)) {
    qDebug() << msg;
    return;
  }

  QMessageBox::warning(parent, QObject::tr("Warning"), msg);
}

void
message_dlg(const trstring &title, const trstring &msg, osm2go_platform::Widget *parent)
{
  if(unlikely(parent == nullptr)) {
    qDebug() << msg;
    return;
  }

  QMessageBox::warning(parent, title, msg);
}
