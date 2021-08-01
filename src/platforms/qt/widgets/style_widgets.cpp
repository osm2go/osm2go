/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "style_widgets.h"
#include "style_p.h"

#include "appdata.h"
#include "settings.h"
#include "style.h"

#include <cassert>
#include <QDebug>
#include <QInputDialog>

#include "osm2go_i18n.h"

void style_select(appdata_t &appdata)
{
  const auto styles = style_scan();

  int match = -1;
  int cnt = 0;
  QStringList names;
  names.reserve(styles.size());
  auto settings = settings_t::instance();
  const std::string &currentstyle = settings->style;
  for (const auto& [name, fname] : styles) {
    names << QString::fromStdString(name);
    if(match < 0 && style_basename(fname) == currentstyle)
      match = cnt;
    cnt++;
  }

  bool ok;
  const QString &item = QInputDialog::getItem(appdata_t::window, trstring("Select style"),
                                              trstring("Style:"), names, match, false, &ok);

  if (!ok || (match >= 0 && item == names.at(match)))
    return;

  const auto it = styles.find(item.toStdString());
  assert(it != styles.cend());

  qDebug() << "user selected style" << item << QString::fromStdString(it->second);

  style_change(appdata, it->second);
}
