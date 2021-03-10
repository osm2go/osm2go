/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <OldOrNotEmptyValidator.h>

#include "../../../test/dummy_appdata.h"

#include <QTest>

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
Q_DECLARE_METATYPE(QValidator::State)
#endif

class TestOldOrNotEmptyValidator : public QObject {
  Q_OBJECT

private slots:
  void test();
  void test_data();
};

void TestOldOrNotEmptyValidator::test()
{
  QFETCH(QString, oldValue);
  QFETCH(QString, input);
  int dummy;

  OldOrNotEmptyValidator validator(oldValue);
  QTEST(validator.validate(input, dummy), "result");
}

void TestOldOrNotEmptyValidator::test_data()
{
  QTest::addColumn<QString>("oldValue");
  QTest::addColumn<QString>("input");
  QTest::addColumn<QValidator::State>("result");

  QTest::newRow("empty input") << QStringLiteral("foo") << QString() << QValidator::Intermediate;
  QTest::newRow("empty input, no old") << QString() << QString() << QValidator::Intermediate;
  QTest::newRow("old as input") << QStringLiteral("foo") << QStringLiteral("foo") << QValidator::Acceptable;
  QTest::newRow("other input") << QStringLiteral("bar") << QStringLiteral("foo") << QValidator::Intermediate;
}

QTEST_MAIN(TestOldOrNotEmptyValidator)

#include "test_OldOrNotEmptyValidator.moc"
