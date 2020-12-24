/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <osm.h>

#include <QAbstractTableModel>
#include <vector>

enum {
  RELITEM_COL_TYPE = 0,
  RELITEM_COL_MEMBER,
  RELITEM_COL_ROLE,
  RELITEM_COL_NAME,
  RELITEM_NUM_COLS
};

class presets_items;

class RelationMembershipModel : public QAbstractTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(RelationMembershipModel)

  std::vector<relation_t *> m_relations;
  osm_t::ref m_osm;

public:
  RelationMembershipModel(QObject *parent, osm_t::ref o, object_t obj);

  int rowCount(const QModelIndex &parent) const override;
  int columnCount(const QModelIndex &parent) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  bool setData(const QModelIndex &idx, const QVariant &value, int role) override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;

  const object_t m_obj;
};
