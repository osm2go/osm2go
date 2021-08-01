/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QStyledItemDelegate>

class presets_items;

class RelationMemberRoleDelegate : public QStyledItemDelegate {
  Q_OBJECT
  Q_DISABLE_COPY(RelationMemberRoleDelegate)

  const presets_items * const m_presets;
public:
  RelationMemberRoleDelegate(const presets_items *presets, QObject *parent = nullptr);

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
  void setEditorData(QWidget *editor, const QModelIndex &index) const override;
  void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
};
