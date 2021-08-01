/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <project.h>

#include "ui_ProjectPropertiesDialog.h"

#include <QDialog>
#include <vector>

struct appdata_t;
class QPushButton;

class ProjectPropertiesDialog : public QDialog {
  Q_OBJECT
  Q_DISABLE_COPY(ProjectPropertiesDialog)

  Ui_ProjectPropertiesDialog ui;
public:
  ProjectPropertiesDialog(appdata_t &a, project_t *p, bool n, const std::vector<std::unique_ptr<project_t>> &projects, QWidget *parent = nullptr);

  static std::string project_name_dialog(QWidget *parent, const std::vector<std::unique_ptr<project_t>> &projects,
                                       const std::string &oldname);

private:
  appdata_t &appdata;
  project_t * const project;
  const bool is_new;
  const std::vector<std::unique_ptr<project_t>> &m_projects;

  QPushButton *okBtn;

  void projectFileSize();
  void project_diffstat();
  void setTitle();
  void showBounds();

private slots:
  void slotDiffRemoveClicked();
  void slotDownloadClicked();
  void slotEditClicked();
  void slotRenameClicked();
};
