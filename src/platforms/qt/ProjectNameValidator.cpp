/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ProjectNameValidator.h"

#include <project.h>

QValidator::State ProjectNameValidator::validate(QString &input, int &) const
{
  // whitespace-only changes are visually bad, so disallow them
  const QString v = input.trimmed();

  if (v.isEmpty())
    return QValidator::Intermediate;

  if (v.contains(m_badPattern))
    return QValidator::Invalid;

  const std::string sinput = v.toStdString();
  if (m_projects.cend() != std::find_if(m_projects.cbegin(), m_projects.cend(),
                                        [&sinput](auto &&p) { return p->name == sinput; }))
    return QValidator::Intermediate;

  return QValidator::Acceptable;
}
