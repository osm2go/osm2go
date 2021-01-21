/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "RelationModel.h"

#include <osm_objects.h>

#include "osm2go_i18n.h"
#include "osm2go_platform_qt.h"

namespace {

std::vector<std::pair<relation_t *, const relation_t *>> getRelations(osm_t::ref osm)
{
  std::vector<std::pair<relation_t *, const relation_t *>> ret;
  // assume this wastes only little space as deleting objects doesn't happen often
  ret.reserve(osm->relations.size());

  for (auto &&p : osm->relations) {
    if (p.second->isDeleted())
      continue;

    ret.push_back(std::make_pair(p.second, osm->originalObject(p.second)));
  }

  return ret;
}

} // namespace

RelationModel::RelationModel(osm_t::ref osm, QObject *parent)
  : QAbstractTableModel(parent)
  , m_relations(getRelations(osm))
  , m_osm(osm)
{
}

int RelationModel::rowCount(const QModelIndex &parent) const
{
  if (unlikely(parent.isValid()))
    return 0;

  return m_relations.size();
}

int RelationModel::columnCount(const QModelIndex &parent) const
{
  if (unlikely(parent.isValid()))
    return 0;

  return RELATION_NUM_COLS;
}

QVariant RelationModel::data(const QModelIndex &index, int role) const
{
  if (unlikely(!index.isValid()))
    return QVariant();

  const auto &rels = m_relations.at(index.row());
  const relation_t * const rel = rels.first;
  const relation_t * const orig = rels.second;

  switch (role) {
  case Qt::DisplayRole:
    switch (index.column()) {
    case RELATION_COL_TYPE:
      return rel->tags.get_value("type");
    case RELATION_COL_NAME:
      return rel->descriptiveNameOrId();
    case RELATION_COL_MEMBERS:
      return static_cast<unsigned int>(rel->members.size());
    default:
      break;
    }
    break;
  case Qt::ToolTipRole:
    switch (index.column()) {
    case RELATION_COL_NAME:
      return static_cast<qint64>(rel->id);
    case RELATION_COL_MEMBERS:
      if (orig != nullptr && rel->members != orig->members)
        return static_cast<unsigned int>(orig->members.size());
      break;
    default:
      break;
    }
    break;
  case Qt::FontRole:
    if (orig != nullptr) {
      bool changed = false;
      switch (index.column()) {
      case RELATION_COL_TYPE:
        changed = (rel->tags.get_value("type") != orig->tags.get_value("type"));
        break;
      case RELATION_COL_NAME:
        changed = (rel->tags != orig->tags);
        break;
      case RELATION_COL_MEMBERS:
        changed = (rel->members != orig->members);
        break;
      default:
        break;
      }
      if (changed)
        return osm2go_platform::modelHightlightModified();
    } else if (rel->isNew())
      return osm2go_platform::modelHightlightModified();
    break;
  case Qt::UserRole:
    return QVariant::fromValue(const_cast<void *>(static_cast<const void *>(rel)));
  default:
    break;
  }

  return QVariant();
}

QVariant RelationModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    return QVariant();

  switch (section) {
  case RELATION_COL_TYPE:
    return trstring("Type");
  case RELATION_COL_NAME:
    return trstring("Name");
  case RELATION_COL_MEMBERS:
    return trstring("Members");
  default:
    return QVariant();
  }
}

bool RelationModel::removeRows(int row, int count, const QModelIndex &parent)
{
  Q_ASSERT(!parent.isValid());
  Q_ASSERT(row + count <= static_cast<int>(m_relations.size()));

  auto it = std::next(m_relations.begin(), row);
  auto itLast = std::next(it, count);
  beginRemoveRows(parent, row, row + count - 1);
  m_relations.erase(it, itLast);
  endRemoveRows();

  return true;
}

QModelIndex RelationModel::addRelation(relation_t *relation)
{
  beginInsertRows(QModelIndex(), m_relations.size(), m_relations.size());
  m_relations.push_back(std::make_pair(relation, nullptr));
  endInsertRows();

  return createIndex(m_relations.size() - 1, 0);
}

void RelationModel::relationEdited(relation_t *relation)
{
  auto it = std::find_if(m_relations.begin(), m_relations.end(), [relation](const auto &p) {
    return p.first == relation;
  });
  assert(it != m_relations.end());

  int row = std::distance(m_relations.begin(), it);
  it->second = m_osm->originalObject(relation);

  emit dataChanged(createIndex(row, 0), createIndex(row, RELATION_NUM_COLS - 1));
}
