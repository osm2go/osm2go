/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "TagModel.h"

#include <discarded.h>
#include <osm_objects.h>

#include <QIcon>
#include <QVariant>

#include <osm2go_annotations.h>
#include "osm2go_platform_qt.h"

// avoid permanent conversion to std::string and back
bool TagModel::tag::is_discardable() const
{
  return std::any_of(discardable_tags.cbegin(), discardable_tags.cend(),
                     [&k = key](const char *d) { return k == QLatin1String(d); });
}

static auto mapIterCheck(osm_t::TagMap &tags, const QString &k, const QString &v)
{
  const osm_t::TagMap::value_type vt(k.toStdString(), v.toStdString());
  auto it = std::find(tags.begin(), tags.end(), vt);
  assert(it != tags.end());
  return it;
}

TagModel::TagState TagModel::tagState(const TagModel::tag &t, osm_t::TagMap::const_iterator *it) const
{
  auto oit = m_originalTags.find(t.key.toStdString());
  if (it != nullptr)
    *it = oit;
  if (oit == m_originalTags.end())
    return Created;
  return oit->second == t.value.toStdString() ? Unchanged : Modified;
}

void TagModel::tagsToVector()
{
  m_data.resize(m_tags.size());

  auto pit = m_tags.cbegin();
  assert(pit != m_tags.cend());
  m_data[0].key = QString::fromStdString(pit->first);
  m_data[0].value = QString::fromStdString(pit->second);

  // update collision information for all elements
  const auto itEnd = std::prev(m_tags.cend());
  unsigned int i = 1;
  for (; pit != itEnd; pit++, i++) {
    auto it = std::next(pit);
    if (unlikely(it->first == pit->first)) {
      m_data[i].collision = true;
      m_data[i - 1].collision = true;
      m_data[i].key = m_data[i - 1].key; // explicitely reuse QString to save some memory
    } else {
      m_data[i].key = QString::fromStdString(it->first);
    }
    m_data[i].value = QString::fromStdString(it->second);
  }
}

TagModel::TagModel(QObject *parent, object_t &obj, const base_object_t *original)
  : QAbstractTableModel(parent)
  , m_tags(static_cast<base_object_t *>(obj)->tags.asMap())
  , m_originalTags(original != nullptr ? original->tags.asMap() : m_tags)
{
  if (!m_tags.empty())
    tagsToVector();
}

int TagModel::columnCount(const QModelIndex &parent) const
{
  if (unlikely(parent.isValid()))
    return 0;

  return 2;
}

int TagModel::rowCount(const QModelIndex &parent) const
{
  if (unlikely(parent.isValid()))
    return 0;

  return m_tags.size();
}

QVariant TagModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    return QVariant();

  switch (section) {
  case 0:
    return tr("Key");
  case 1:
    return tr("Value");
  default:
    return QVariant();
  }
}

QVariant TagModel::data(const QModelIndex &index, int role) const
{
  switch (role) {
  case Qt::EditRole:
  case Qt::DisplayRole:
    switch (index.column()) {
    case 0:
      return m_data.at(index.row()).key;
    case 1:
      return m_data.at(index.row()).value;
    default:
      break;
    }
    break;
  case Qt::DecorationRole:
    if (index.column() == 0 && m_data.at(index.row()).collision)
      return QIcon::fromTheme(QLatin1String("dialog-warning"));
    break;
  case Qt::ToolTipRole:
    if (unlikely(m_data.at(index.row()).is_discardable()))
      return tr("Discardable tags on objects will automatically be removed on object changes.");
    if (index.column() == 1) {
      osm_t::TagMap::const_iterator it;
      // for colliding entries this will return modified for at least all but one because
      // tagState() finds only one instance and compares the other ones, so just skip that
      if (!m_data.at(index.row()).collision && tagState(m_data.at(index.row()), &it) == Modified) {
        assert(it != m_originalTags.end());
        return tr("<i>Original value:</i> %1").arg(QString::fromStdString(it->second));
      }
    }
    break;
  case Qt::FontRole:
    switch (tagState(m_data.at(index.row()))) {
    case Unchanged:
      return QVariant();
    case Modified:
      if (index.column() == 0)
        return QVariant();
      // fallthrough
    case Created:
      return osm2go_platform::modelHightlightModified();
    default:
      assert_unreachable();
    }
    break;
  default:
    break;
  }

  return QVariant();
}

bool TagModel::removeRows(int row, int count, const QModelIndex &parent)
{
  assert(!parent.isValid());

  beginRemoveRows(parent, row, row + count - 1);
  auto it = std::next(m_data.begin(), row);
  // if the first removed item is a collision with the one before
  bool preCollision = (row > 0 && it->collision && std::prev(it)->key == it->key);
  bool postCollision = [&]() {
    if (static_cast<unsigned int>(row + count) >= m_data.size())
      return false;
    auto itPost = std::next(it, count);
    return std::prev(itPost)->key == itPost->key;
  }();
  for (int i = 0; i < count; i++) {
    m_tags.erase(mapIterCheck(m_tags, it->key, it->value));
    it = m_data.erase(it);
  }
  endRemoveRows();

  auto removeCollision = [this](auto &iter, int r) {
    iter->collision = false;
    emit dataChanged(index(r - 1, 0), index(r - 1, 1));
  };

  if (preCollision) {
    // there was a collision with the first removed row. There are 2 possible
    // cases where there still could be a collision of (row-1): with (row-2) or
    // with row, which is the old (row+count)
    assert_cmpnum_op(row, >, 0);
    auto itBefore = std::next(m_data.begin(), row - 1);
    bool keepColl = false;
    if (row > 1)
      keepColl = std::prev(itBefore)->key == itBefore->key;
    if (it != m_data.end() && itBefore->key == it->key) {
      keepColl = true;
      // no need to check for post-collision anymore,
      // we already found out it is still there
      postCollision = false;
    }
    if (!keepColl)
      removeCollision(itBefore, row);
  }
  if (postCollision) {
    // the line after the removed data had a collision, but the new collision
    // can only be with the line afterwards
    auto itPost = std::next(it);
    if (itPost == m_data.end() || itPost->key != it->key)
      removeCollision(it, row + count);
  }

  return true;
}

bool TagModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
  if (unlikely(!index.isValid() || index.parent().isValid()))
    return false;

  if (unlikely(role != Qt::EditRole))
    return false;

  if (unlikely(value.isNull()))
    return false;

  assert_cmpnum_op(static_cast<unsigned int>(index.row()), <, m_data.size());
  auto it = std::next(m_data.begin(), index.row());

  const QString &nval = value.toString();
  switch (index.column()) {
  case 0: {
    // nothing changed, fine
    if (it->key == nval)
      return true;

    auto oldIter = mapIterCheck(m_tags, it->key, it->value);
    // prevent creation of collisions
    std::string nkey = nval.toStdString();
    if (std::any_of(m_tags.cbegin(), m_tags.cend(), [&nkey](const auto &tg) {
                    return tg.first == nkey; }))
      return false;

    std::string v = std::move(oldIter->second);
    it->key = nval;
    m_tags.erase(oldIter);
    m_tags.insert(osm_t::TagMap::value_type(std::move(nkey), std::move(v)));
    emit dataChanged(index, index);
    return true;
  }
  case 1: {
    if (unlikely(m_data.at(index.row()).collision)) {
      // There has been a collision. Check if the new setting would become a duplicate.
      auto is_coll = [&](const auto &coit) {
        // the collision check is cheaper to find tag boundaries, as there are only few of them
        if (!coit->collision || coit->key != it->key)
          return -1;
        if (coit->value == nval) {
          removeRow(index.row());
          return 1;
        }
        return 0;
      };

      // the ones before
      for (int row = index.row() - 1; row >= 0; row--)
        switch (is_coll(std::next(m_data.begin(), row))) {
        case -1:
          break;
        case 1:
          return true;
        }

      // the ones afterwards
      for (auto cit = std::next(it); cit != m_data.end(); cit++) {
        switch (is_coll(cit)) {
        case -1:
          break;
        case 1:
          return true;
        }
      }
    }
    auto mit = mapIterCheck(m_tags, it->key, it->value);
    it->value = nval;
    mit->second = nval.toStdString();
    emit dataChanged(index, index);
    return true;
    }
  default:
    assert_unreachable();
    return false;
  }
}

Qt::ItemFlags TagModel::flags(const QModelIndex &index) const
{
  auto r = QAbstractTableModel::flags(index);

  if (unlikely(!index.isValid()))
    return r;

  auto tg = m_data.at(index.row());
  if (tg.is_discardable())
    r &= ~(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
  else
    r |= Qt::ItemIsEditable;

  return r;
}

void TagModel::replaceTags(const osm_t::TagMap &tags)
{
  beginResetModel();
  m_tags = tags;

  if (!m_tags.empty())
    tagsToVector();
  else
    m_data.clear();
  endResetModel();
}

QModelIndex TagModel::addTag(const QString &key, const QString &value)
{
  // the dialog must not accept duplicate keys, so there should be none here
  assert(std::none_of(m_data.begin(), m_data.end(), [key](const auto &tg) {
                      return tg.key == key; }));

  beginInsertRows(QModelIndex(), m_data.size(), m_data.size());
  m_tags.insert(osm_t::TagMap::value_type(key.toStdString(), value.toStdString()));

  m_data.emplace_back(tag(key, value));
  endInsertRows();

  return createIndex(m_data.size() - 1, 0);
}
