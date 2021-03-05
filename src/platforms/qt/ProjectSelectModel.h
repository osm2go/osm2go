/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <project.h>

#include <memory>
#include <QAbstractTableModel>
#include <vector>

class ProjectSelectModel : public QAbstractTableModel {
  Q_OBJECT

  Q_DISABLE_COPY(ProjectSelectModel);
public:
  enum {
    PROJECT_COL_NAME = 0,
    PROJECT_COL_DESCRIPTION,
    PROJECT_NUM_COLS
  };

  /**
   * @constructor
   * @param projects list of all available projects
   * @param c the currently active project
   */
  ProjectSelectModel(std::vector<std::unique_ptr<project_t>> &projects, project_t::ref c, QObject *parent = nullptr);
  ~ProjectSelectModel() override = default;

  int rowCount(const QModelIndex &parent) const override;
  int columnCount(const QModelIndex &parent) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  bool removeRows(int row, int count, const QModelIndex &parent) override;

  /**
   * @brief get the model index of the active project
   */
  QModelIndex activeProject() const;
  /**
   * @brief refresh the data shown about the active project
   */
  void refreshActiveProject();
  QModelIndex addProject(std::unique_ptr<project_t> &&project);

  std::vector<std::unique_ptr<project_t>> &m_projects;
  project_t::ref m_current;
};
