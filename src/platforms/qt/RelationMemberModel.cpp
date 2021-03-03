/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "RelationMemberModel.h"

#include <osm.h>
#include <osm_objects.h>

#include <QDataStream>
#include <QHash>
#include <QMimeData>

#include "osm2go_i18n.h"
#include "osm2go_platform_qt.h"

namespace {

QHash<const char *, QString> roleCache;

class memberQ {
public:
  memberQ(const member_t &m);
  bool operator==(const member_t &other) const;
  bool operator==(const memberQ &other) const
  {
    return object == other.object && role == other.role;
  }
  inline bool operator!=(const member_t &other) const
  { return !operator==(other); }
  inline bool operator!=(const memberQ &other) const
  { return !operator==(other); }

  object_t object;
  QString role;
};

memberQ::memberQ(const member_t &m)
  : object(m.object)
{
  if (m.role != nullptr) {
    auto it = roleCache.find(m.role);
    if (it == roleCache.end())
      it = roleCache.insert(m.role, QString::fromUtf8(m.role));
    role = *it;
  }
}

bool memberQ::operator==(const member_t &other) const
{
  if (object != other.object)
    return false;

  if (role.isEmpty())
    return other.role == nullptr;

  const auto it = roleCache.find(other.role);
  if (it == roleCache.end())
    return false;

  return *it == role;
}

const std::vector<member_t> emptyMembers;

} // namespace

class RelationMemberModel::RelationMemberModelPrivate {
public:
  RelationMemberModelPrivate(relation_t *rel, osm_t::ref o);

  relation_t * const m_relation;
  const relation_t * const m_origRelation;
  osm_t::ref m_osm;
  std::vector<memberQ> m_members;  ///< editable member list
  const std::vector<member_t> &m_origMembers; ///< upstream member list
};

RelationMemberModel::RelationMemberModelPrivate::RelationMemberModelPrivate(relation_t *rel, osm_t::ref o)
  : m_relation(rel)
  , m_origRelation(o->originalObject(rel))
  , m_osm(o)
  , m_origMembers(m_origRelation != nullptr ? m_origRelation->members : m_relation->isNew() ? emptyMembers : m_relation->members)
{
  assert(roleCache.empty());
  m_members.reserve(m_relation->members.size());
  for (auto &&m : m_relation->members)
    m_members.emplace_back(m);
}

RelationMemberModel::RelationMemberModel(relation_t *rel, osm_t::ref o, QObject *parent)
  : QAbstractTableModel(parent)
  , d_ptr(new RelationMemberModelPrivate(rel, o))
{
}

RelationMemberModel::~RelationMemberModel()
{
  roleCache.clear();
}

int RelationMemberModel::rowCount(const QModelIndex &parent) const
{
  const Q_D(RelationMemberModel);

  if (unlikely(parent.isValid()))
    return 0;
  return d->m_members.size();
}

int RelationMemberModel::columnCount(const QModelIndex &parent) const
{
  if (unlikely(parent.isValid()))
    return 0;
  return MEMBER_NUM_COLS;
}

QVariant RelationMemberModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    return QVariant();

  switch (section) {
  case MEMBER_COL_TYPE:
    return trstring("Type");
  case MEMBER_COL_ID:
    return trstring("Id");
  case MEMBER_COL_NAME:
    return trstring("Name");
  case MEMBER_COL_ROLE:
    return trstring("Role");
  default:
    return QVariant();
  }
}

QVariant RelationMemberModel::data(const QModelIndex &index, int role) const
{
  const Q_D(RelationMemberModel);

  if (unlikely(!index.isValid()))
    return QVariant();

  const auto &member = d->m_members.at(index.row());

  switch (role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    switch (index.column()) {
    case MEMBER_COL_TYPE:
      return member.object.type_string();
    case MEMBER_COL_ID:
      return static_cast<qint64>(member.object.get_id());
    case MEMBER_COL_NAME:
      if (member.object.is_real())
        return member.object.get_name(*d->m_osm);
      break;
    case MEMBER_COL_ROLE:
      return member.role;
    default:
      break;
    }
    break;
  case Qt::FontRole:
    if (static_cast<unsigned int>(index.row()) < d->m_origMembers.size()) {
      bool changed = false;
      const auto &oldMember = d->m_origMembers.at(index.row());
      switch (index.column()) {
      case MEMBER_COL_TYPE:
        changed = (member.object.type != oldMember.object.type);
        break;
      case MEMBER_COL_ID:
        changed = (member.object.get_id() != oldMember.object.get_id());
        break;
      case MEMBER_COL_ROLE:
        changed = (member.role != oldMember.role);
        break;
      default:
        break;
      }
      if (changed)
        return osm2go_platform::modelHightlightModified();
    }
    break;
  case Qt::UserRole:
    switch (index.column()) {
    case MEMBER_COL_ID:
      return QVariant::fromValue(const_cast<void *>(static_cast<const void *>(&member.object)));
    case MEMBER_COL_ROLE:
      return QVariant::fromValue(static_cast<void *>(d->m_relation));
    default:
      break;
    }
    break;
  default:
    break;
  }

  return QVariant();
}

bool RelationMemberModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
  Q_D(RelationMemberModel);

  if (unlikely(!index.isValid()))
    return false;

  auto &member = d->m_members.at(index.row());

  switch (role) {
  case Qt::EditRole:
    switch (index.column()) {
    case MEMBER_COL_ROLE:
      member.role = value.toString();
      return true;
    default:
      break;
    }
    break;
  }

  return false;
}

Qt::ItemFlags RelationMemberModel::flags(const QModelIndex &index) const
{
  const Q_D(RelationMemberModel);

  auto r = QAbstractTableModel::flags(index);

  if (unlikely(!index.isValid()))
    return r;

  const auto &member = d->m_members.at(index.row());

  if (!member.object.is_real())
    r &= ~Qt::ItemIsEnabled;

  if (index.column() == MEMBER_COL_ROLE)
    r |= Qt::ItemIsEditable;

  return r;
}

bool RelationMemberModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild)
{
  Q_D(RelationMemberModel);

  assert(!sourceParent.isValid());
  assert(!destinationParent.isValid());
  assert_cmpnum(count, 1);
  // move onto itself
  assert_cmpnum_op(sourceRow, !=, destinationChild - 1);

  beginMoveRows(sourceParent, sourceRow, sourceRow, destinationParent, destinationChild);
  auto itOld = std::next(d->m_members.begin(), sourceRow);
  const auto member = *itOld;
  d->m_members.erase(itOld);
  auto itNew = std::next(d->m_members.begin(), destinationChild < sourceRow ? destinationChild : destinationChild - 1);
  d->m_members.insert(itNew, member);
  endMoveRows();

  return true;
}

bool RelationMemberModel::commit()
{
  Q_D(RelationMemberModel);

  const auto ritEnd = d->m_relation->members.end();
  auto rit = d->m_relation->members.begin();
  auto it = d->m_members.cbegin();

  for (; rit != ritEnd; it++, rit++)
    if (*it != *rit)
      break;

  if (rit == ritEnd)
    return false;

  d->m_osm->mark_dirty(d->m_relation);

  for (; it != d->m_members.cend(); it++, rit++) {
    if (it->role.isEmpty()) {
      rit->object = it->object;
      rit->role = nullptr;
      continue;
    }
    // use the constructor as that will match the object in the valueCache
    // and add it there instead of copying a temporary
    *rit = member_t(it->object, it->role.toStdString().c_str());
  }

  return true;
}
