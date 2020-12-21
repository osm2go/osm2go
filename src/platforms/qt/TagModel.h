/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <osm.h>

#include <QAbstractTableModel>
#include <string>
#include <vector>

class TagModel : public QAbstractTableModel {
  Q_OBJECT
  Q_DISABLE_COPY(TagModel)

  osm_t::TagMap m_tags;
  struct tag {
    tag() = default;
    tag(const QString &k, const QString &v)
      : key(k), value(v) {}
    tag(const tag &) = default;
    tag(tag &&) = default;
    tag &operator=(const tag &) = default;
    tag &operator=(tag &&) = default;

    QString key, value;
    bool collision = false;

    bool is_discardable() const;
  };
  std::vector<tag> m_data;

  enum TagState {
    Unchanged,  ///< the same key/value pair is in m_originalTags
    Created,    ///< the key does not exist in m_originalTags
    Modified,   ///< the key is in m_originalTags, but with a different value
    Deleted,    ///< the key exists only in m_originalTags
  };

  /**
   * @brief determine the state of a given tag
   *
   * This will check in which state the tag is, but will ignore collisions,
   * i.e. it will usually return Modified for most of the colliding entries
   * as their value differ from the first tag it finds with the given key.
   */
  TagState tagState(const tag &t, osm_t::TagMap::const_iterator *it = nullptr) const;

  /**
   * @brief convert m_tags to m_data
   */
  void tagsToVector();

public:
  TagModel(QObject *parent, object_t &obj, const base_object_t *original);

  int rowCount(const QModelIndex &parent) const override;
  int columnCount(const QModelIndex &parent) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  bool removeRows(int row, int count, const QModelIndex &parent) override;
  bool setData(const QModelIndex &index, const QVariant &value, int role) override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;

  void replaceTags(const osm_t::TagMap &tags);

  inline const osm_t::TagMap &tags() const
  { return m_tags; }

  QModelIndex addTag(const QString &key, const QString &value);

  const osm_t::TagMap m_originalTags;
};
