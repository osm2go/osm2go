/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "WmsModel.h"

#include <wms.h>

#include <QString>
#include <QVariant>

#include <osm2go_annotations.h>
#include <osm2go_i18n.h>

WmsModel::WmsModel(settings_t::ref settings, QObject *parent)
  : QAbstractListModel(parent)
  , m_settings(std::move(settings))
  , m_servers(m_settings->wms_server)
{
}

QVariant
WmsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (section == 0 && orientation == Qt::Horizontal && role == Qt::DisplayRole)
    return trstring("Name");
  return QVariant();
}

QVariant
WmsModel::data(const QModelIndex &index, int role) const
{
  assert(index.isValid() && index.column() == 0 && index.row() < rowCount(QModelIndex()));

  switch (role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    return QString::fromStdString(m_servers.at(index.row())->name);
  case Qt::UserRole:
    return QVariant::fromValue(static_cast<void *>(m_servers.at(index.row())));
  default:
    return QVariant();
  }
}

int
WmsModel::rowCount(const QModelIndex &parent) const
{
  if (unlikely(parent.isValid()))
    return 0;

  return m_servers.size();
}

bool
WmsModel::removeRows(int row, int count, const QModelIndex &parent)
{
  assert_cmpnum_op(row + count, <=, rowCount(parent));

  beginRemoveRows(parent, row, row + count - 1);
  for (int i = row + count - 1; i >= row; i--) {
    wms_server_t *server = m_servers.at(i);

    // free tag itself
    delete server;
    // delete only one at a time, this will usually be called this way
    m_servers.erase(std::next(m_servers.begin(), i));
  }
  endRemoveRows();

  return true;
}

wms_server_t *
WmsModel::addServer(std::unique_ptr<wms_server_t> srv)
{
  beginInsertRows(QModelIndex(), m_servers.size(), m_servers.size());
  m_servers.emplace_back(srv.release());
  endInsertRows();
  return m_servers.back();
}

bool
WmsModel::hasName(const QString &name) const
{
  const std::string &sname = name.toStdString();
  return std::any_of(m_servers.cbegin(), m_servers.cend(), [&sname](const auto *s) {
                     return s->name == sname; });
}

int
WmsModel::indexOfServer(const std::string &server) const
{
  const auto itEnd = m_servers.cend();
  const auto itBegin = m_servers.cbegin();
  auto it = std::find_if(itBegin, itEnd, [&server](const auto *s) {
    return s->server == server;
  });
  if (it == itEnd)
    return -1;
  return std::distance(itBegin, it);
}
