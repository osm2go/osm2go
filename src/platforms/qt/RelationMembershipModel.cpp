/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "RelationMembershipModel.h"

#include <osm_objects.h>

#include <osm2go_annotations.h>
#include "osm2go_i18n.h"

#include <cassert>

RelationMembershipModel::RelationMembershipModel(osm_t::ref o, object_t obj, QObject *parent)
  : QAbstractTableModel(parent)
  , m_osm(o)
  , m_obj(obj)
{
  // assume this wastes only little space as deleting objects doesn't happen often
  m_relations.reserve(m_osm->relations.size());

  for (auto &&m : std::as_const(m_osm->relations))
    if (!m.second->isDeleted())
      m_relations.emplace_back(m.second);
}

int RelationMembershipModel::rowCount(const QModelIndex &parent) const
{
  if (unlikely(parent.isValid()))
    return 0;

  return m_relations.size();
}

int RelationMembershipModel::columnCount(const QModelIndex &parent) const
{
  if (unlikely(parent.isValid()))
    return 0;

  return RELITEM_NUM_COLS;
}

QVariant RelationMembershipModel::data(const QModelIndex &index, int role) const
{
  assert(index.isValid() && !index.parent().isValid());

  switch (role) {
  case Qt::DisplayRole:
  case Qt::EditRole: {
    const auto *relation = m_relations.at(index.row());
    switch (index.column()) {
    case RELITEM_COL_TYPE:
      return QString::fromUtf8(relation->tags.get_value("type"));
    case RELITEM_COL_MEMBER:
      break;
    case RELITEM_COL_ROLE: {
      auto it = relation->find_member_object(m_obj);
      if (it == relation->members.end() || it->role == nullptr)
        return QVariant();
      else
        return QString::fromUtf8(it->role);
    }
    case RELITEM_COL_NAME:
      return relation->descriptiveNameOrId();
    default:
      break;
    }
    break;
    }
  case Qt::CheckStateRole:
    if (index.column() == RELITEM_COL_MEMBER) {
      const auto relation = m_relations.at(index.row());
      return relation->find_member_object(m_obj) == relation->members.end() ? Qt::Unchecked : Qt::Checked;
    }
    break;
  case Qt::UserRole:
    return QVariant::fromValue(static_cast<void *>(m_relations.at(index.row())));
  default:
    break;
  }

  return QVariant();
}

bool RelationMembershipModel::setData(const QModelIndex &idx, const QVariant &value, int role)
{
  relation_t *relation;

  switch (idx.column()) {
  case RELITEM_COL_MEMBER: {
    if (role != Qt::CheckStateRole)
      return false;
    relation = m_relations.at(idx.row());
    if (value.value<Qt::CheckState>() == Qt::Unchecked) {
      auto it = relation->find_member_object(m_obj);
      assert(it != relation->members.end());
      relation->members.erase(it);
    } else {
      relation->members.emplace_back(member_t(m_obj, nullptr));
    }

    break;
    }
  case RELITEM_COL_ROLE: {
    if (role != Qt::EditRole)
      return false;
    relation = m_relations.at(idx.row());
    auto it = relation->find_member_object(m_obj);
    const auto s = value.toString();
    member_t nm(m_obj, s.isEmpty() ? nullptr : s.toUtf8().constData());
    if (it == relation->members.end()) {
      relation->members.emplace_back(nm);
    } else {
      auto mit = std::next(relation->members.begin(), std::distance(relation->members.cbegin(), it));
      *mit = nm;
    }
    break;
    }
  default:
    return false;
  }

  // always update both columns, even if only one changed
  emit dataChanged(index(idx.row(), RELITEM_COL_MEMBER), index(idx.row(), RELITEM_COL_ROLE));
  relation->flags |= OSM_FLAG_DIRTY;
  return true;
}

QVariant RelationMembershipModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
    return QVariant();

  switch (section) {
  case RELITEM_COL_NAME:
    return trstring("Name");
  case RELITEM_COL_MEMBER:
    return trstring("Member");
  case RELITEM_COL_TYPE:
    return trstring("Type");
  case RELITEM_COL_ROLE:
    return trstring("Role");
  default:
    return QVariant();
  }
}

Qt::ItemFlags RelationMembershipModel::flags(const QModelIndex &index) const
{
  auto defaultflags = QAbstractTableModel::flags(index);
  if (unlikely(!index.isValid()))
    return defaultflags;

  switch (index.column()) {
  case RELITEM_COL_MEMBER:
    return defaultflags | Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
  case RELITEM_COL_ROLE:
    return defaultflags | Qt::ItemIsEditable;
  default:
    return defaultflags;
  }
}
