/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <QRegularExpression>
#include <QValidator>
#include <vector>

struct project_t;

class ProjectNameValidator : public QValidator {
  Q_OBJECT
  Q_DISABLE_COPY(ProjectNameValidator)

  const std::vector<std::unique_ptr<project_t>> &m_projects;
  const QRegularExpression m_badPattern;
public:
  explicit ProjectNameValidator(const std::vector<std::unique_ptr<project_t>> &projects, QObject *parent = nullptr)
    : QValidator(parent)
    , m_projects(projects)
    // disallow ':', '\\', '*', '?' because they cause trouble e.g. on FAT filesystems
    // disallow '/' because it's a path separator everywhere
    // also disallow '(' and ')'
    , m_badPattern("[\n\t\r:" R"#(/\\\*?\(\)])#")
  {
  }
  ~ProjectNameValidator() override = default;

  QValidator::State validate(QString &input, int &pos) const override;
};
