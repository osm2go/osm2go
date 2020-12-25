/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <osm.h>

#include <QAbstractTableModel>
#include <QScopedPointer>

enum {
  MEMBER_COL_TYPE = 0,
  MEMBER_COL_ID,
  MEMBER_COL_NAME,
  MEMBER_COL_ROLE,
  MEMBER_NUM_COLS
};

class RelationMemberModel : public QAbstractTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(RelationMemberModel)

  class RelationMemberModelPrivate;
  const QScopedPointer<RelationMemberModelPrivate> d_ptr;
  Q_DECLARE_PRIVATE(RelationMemberModel)

public:
  RelationMemberModel(QObject *parent, relation_t *rel, osm_t::ref o) __attribute__((nonnull(3)));
  ~RelationMemberModel() override;

  int rowCount(const QModelIndex &parent) const override;
  int columnCount(const QModelIndex &parent) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;

  bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild) override;

  bool commit();
};
