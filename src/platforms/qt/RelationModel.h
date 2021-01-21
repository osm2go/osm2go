/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <osm.h>

#include <QAbstractTableModel>

enum {
  RELATION_COL_TYPE = 0,
  RELATION_COL_NAME,
  RELATION_COL_MEMBERS,
  RELATION_NUM_COLS
};

class RelationModel : public QAbstractTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(RelationModel)

  std::vector<std::pair<relation_t *, const relation_t *>> m_relations;
  osm_t::ref m_osm;

public:
  RelationModel(osm_t::ref osm, QObject *parent = nullptr);
  ~RelationModel() override = default;

  int rowCount(const QModelIndex &parent) const override;
  int columnCount(const QModelIndex &parent) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  bool removeRows(int row, int count, const QModelIndex &parent) override;

  QModelIndex addRelation(relation_t *relation);
  /**
   * @brief notify the model that the relation has been modified
   */
  void relationEdited(relation_t *relation);
};
