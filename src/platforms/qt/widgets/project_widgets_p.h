/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <project.h>

#include <QDialog>

struct appdata_t;
class ProjectSelectModel;
class QPushButton;
class QSortFilterProxyModel;
class QTableView;

class ProjectDialog : public QDialog {
  Q_OBJECT

  Q_DISABLE_COPY(ProjectDialog)

public:
  ProjectDialog(appdata_t &a);
  ~ProjectDialog() override = default;

  struct sproj {
    project_t *project;
    int row;
  };

  sproj selectedProject();

private:
  appdata_t &appdata;
  std::vector<std::unique_ptr<project_t>> projects;

  QTableView * const tableView;
  ProjectSelectModel * const model;
  QSortFilterProxyModel * const fmodel;
  QPushButton *okBtn;
  QPushButton *editBtn;
  QPushButton *deleteBtn;
  QPushButton *updateBtn;

private slots:
  void slotSelectionChanged();
  void slotNewProject();
  void slotEditProject();
  void slotRemoveProject();
  void slotUpdateAll();
};
