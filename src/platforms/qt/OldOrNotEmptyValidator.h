/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QValidator>

class OldOrNotEmptyValidator : public QValidator {
  Q_OBJECT

  Q_DISABLE_COPY(OldOrNotEmptyValidator)

  const QString m_oldValue;

public:
  OldOrNotEmptyValidator(const QString &oldValue, QObject *parent = nullptr)
    : QValidator(parent), m_oldValue(oldValue) {}
  ~OldOrNotEmptyValidator() override = default;

  QValidator::State validate(QString &input, int &) const override
  {
    if (input.isEmpty())
      return QValidator::Intermediate;

    return input == m_oldValue ? QValidator::Acceptable : QValidator::Intermediate;
  }
};
