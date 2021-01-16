/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <settings.h>

#include <memory>
#include <QAbstractListModel>
#include <vector>

struct wms_server_t;

class WmsModel : public QAbstractListModel {
  Q_OBJECT
  Q_DISABLE_COPY(WmsModel)

  settings_t::ref m_settings; ///< settings instance to keep the server list existing
  std::vector<wms_server_t *> &m_servers;

public:
  explicit WmsModel(settings_t::ref settings, QObject *parent = nullptr);
  ~WmsModel() override = default;

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  int rowCount(const QModelIndex &parent) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  bool removeRows(int row, int count, const QModelIndex &parent) override;

  wms_server_t *addServer(std::unique_ptr<wms_server_t> srv);
  bool hasName(const QString &name) const;

  int indexOfServer(const std::string &server) const;
};
