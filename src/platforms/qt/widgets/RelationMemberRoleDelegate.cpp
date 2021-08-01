/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "RelationMemberRoleDelegate.h"

#include <josm_presets.h>
#include <osm_objects.h>
#include "RelationMemberModel.h"
#include "RelationMembershipModel.h"

#include <cassert>
#include <QComboBox>

RelationMemberRoleDelegate::RelationMemberRoleDelegate(const presets_items *presets, QObject *parent)
  : QStyledItemDelegate(parent)
  , m_presets(presets)
{
}

QWidget *RelationMemberRoleDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
  auto *ret = new QComboBox(parent);
  ret->setEditable(true);
  return ret;
}

void RelationMemberRoleDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
  const auto object = [index]() {
    auto *rm = qobject_cast<const RelationMembershipModel *>(index.model());
    if (rm != nullptr)
      return rm->m_obj;
    auto *rmm = qobject_cast<const RelationMemberModel *>(index.model());
    assert(rmm != nullptr);
    return *static_cast<object_t *>(rmm->index(index.row(), MEMBER_COL_ID).data(Qt::UserRole).value<void *>());
  }();

  auto *relation = static_cast<const relation_t *>(index.data(Qt::UserRole).value<void *>());

  const auto roles = m_presets->roles(relation, object);

  const auto role = index.data(Qt::EditRole).toString();
  auto *cb = qobject_cast<QComboBox *>(editor);
  cb->setCurrentText(role);

  if (roles.empty())
    return;

  QStringList values;
  values.reserve(roles.size());
  for (auto &&v : roles)
    values << QString::fromStdString(v);

  cb->addItems(values);
  if (int idx = values.indexOf(role); idx >= 0)
    cb->setCurrentIndex(idx);
}

void RelationMemberRoleDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
  model->setData(index, static_cast<QComboBox*>(editor)->currentText(), Qt::EditRole);
}
