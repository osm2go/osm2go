/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "UrlValidator.h"

#include "WmsModel.h"

#include <vector>

class WmsNameValidator : public OldOrNotEmptyValidator {
  Q_OBJECT

  Q_DISABLE_COPY(WmsNameValidator)

  const WmsModel * const m_model;
public:
  WmsNameValidator(const QString &oldValue, const WmsModel *model, QObject *parent = nullptr)
    : OldOrNotEmptyValidator(oldValue, parent), m_model(model) {}
  ~WmsNameValidator() override = default;

  QValidator::State validate(QString &input, int &pos) const override
  {
    if (input.isEmpty())
      return QValidator::Intermediate;

    if (OldOrNotEmptyValidator::validate(input, pos) == QValidator::Acceptable)
      return QValidator::Acceptable;

    return m_model->hasName(input) ? QValidator::Intermediate : QValidator::Acceptable;
  }
};
