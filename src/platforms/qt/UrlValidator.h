/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "OldOrNotEmptyValidator.h"

#include <QUrl>

class UrlValidator : public OldOrNotEmptyValidator {
  Q_OBJECT

  Q_DISABLE_COPY(UrlValidator)

public:
  UrlValidator(const QString &oldValue, QObject *parent = nullptr) : OldOrNotEmptyValidator(oldValue, parent) {}
  ~UrlValidator() override = default;

  QValidator::State validate(QString &input, int &pos) const override
  {
    if (OldOrNotEmptyValidator::validate(input, pos) == QValidator::Acceptable)
      return QValidator::Acceptable;

    QUrl url(input, QUrl::StrictMode);
    // url.isValid() does only check if there are encoding errors
    return url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty() && !url.isLocalFile() ?
      QValidator::Acceptable : QValidator::Intermediate;
  }
};
