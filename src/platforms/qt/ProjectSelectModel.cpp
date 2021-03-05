/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ProjectSelectModel.h"

#include <osm.h>
#include <project.h>
#include <project_p.h>

#include <algorithm>
#include <cassert>
#include <QIcon>
#include <QString>

ProjectSelectModel::ProjectSelectModel(std::vector<std::unique_ptr<project_t>> &projects, project_t::ref c, QObject *parent)
  : QAbstractTableModel(parent)
  , m_projects(projects)
  , m_current(c)
{
}

int ProjectSelectModel::rowCount(const QModelIndex &parent) const
{
  if (parent.isValid())
    return 0;
  return m_projects.size();
}

int ProjectSelectModel::columnCount(const QModelIndex &parent) const
{
  if (parent.isValid())
    return 0;
  return PROJECT_NUM_COLS;
}

QVariant ProjectSelectModel::data(const QModelIndex &index, int role) const
{
  switch (role) {
  case Qt::DisplayRole:
    switch (index.column()) {
      case PROJECT_COL_NAME:
        return QString::fromStdString(m_projects.at(index.row())->name);
      case PROJECT_COL_DESCRIPTION:
        return QString::fromStdString(m_projects.at(index.row())->desc);
      default:
        break;
    }
    break;
  case Qt::DecorationRole:
    if (index.column() == PROJECT_COL_NAME) {
      auto &project = m_projects.at(index.row());
      if (m_current && m_current->name == project->name)
        return QIcon::fromTheme(QStringLiteral("document-open"));
      else if(!project->osm_file_exists())
        return QIcon::fromTheme(QStringLiteral("dialog-warning"));
      else if(project->diff_file_present())
        return QIcon::fromTheme(QStringLiteral("document-properties"));
      else
        return QIcon::fromTheme(QStringLiteral("text-x-generic"));
    }
    break;
  default:
    break;
  }

  return QVariant();
}

QVariant ProjectSelectModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    return QVariant();

  switch (section) {
  case PROJECT_COL_NAME:
    return tr("Name");
  case PROJECT_COL_DESCRIPTION:
    return tr("Description");
  default:
    return QVariant();
  }
}

bool ProjectSelectModel::removeRows(int row, int count, const QModelIndex &parent)
{
  assert(!parent.isValid());

  auto it = std::next(m_projects.begin(), row);
  auto itLast = std::next(it, count);
  beginRemoveRows(parent, row, row + count - 1);
  for(int r = row; r < row + count; r++) {
    project_delete(m_projects.at(row));
  }
  m_projects.erase(it, itLast);
  endRemoveRows();

  return true;
}

QModelIndex ProjectSelectModel::activeProject() const
{
  if (!m_current)
    return QModelIndex();

  const auto itBegin = m_projects.cbegin();
  const auto itEnd = m_projects.cend();
  const auto it = std::find_if(itBegin, itEnd, [&c = m_current](const auto &p){ return c->name == p->name; });
  assert(it != itEnd);
  return createIndex(std::distance(itBegin, it), 0);
}

void ProjectSelectModel::refreshActiveProject()
{
  if (!m_current)
    return;
  const auto idx = activeProject();
  emit dataChanged(idx, idx.sibling(idx.row(), columnCount(idx.parent()) - 1));
}

QModelIndex ProjectSelectModel::addProject(std::unique_ptr<project_t> &&project)
{
  int cnt = rowCount(QModelIndex());
  beginInsertRows(QModelIndex(), cnt, cnt);
  m_projects.emplace_back(std::move(project));
  endInsertRows();

  return index(cnt, 0);
}
