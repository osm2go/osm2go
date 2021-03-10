/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <UrlValidator.h>

#include "../../../test/dummy_appdata.h"

#include <QTest>

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
Q_DECLARE_METATYPE(QValidator::State)
#endif

class TestUrlValidator : public QObject {
  Q_OBJECT

private slots:
  void test();
  void test_data();
};

void TestUrlValidator::test()
{
  QFETCH(QString, oldValue);
  QFETCH(QString, input);
  int dummy;

  UrlValidator validator(oldValue);
  QTEST(validator.validate(input, dummy), "result");
}

void TestUrlValidator::test_data()
{
  QTest::addColumn<QString>("oldValue");
  QTest::addColumn<QString>("input");
  QTest::addColumn<QValidator::State>("result");

  QTest::newRow("empty input") << QStringLiteral("foo") << QString() << QValidator::Intermediate;
  QTest::newRow("empty input, no old") << QString() << QString() << QValidator::Intermediate;
  // old is always permitted, even if it is no valid URL
  QTest::newRow("old as input") << QStringLiteral("foo") << QStringLiteral("foo") << QValidator::Acceptable;
  QTest::newRow("other input") << QStringLiteral("bar") << QStringLiteral("foo") << QValidator::Intermediate;

  QTest::newRow("valid url") << QStringLiteral("bar") << QStringLiteral("https://www.openstreetmap.org") << QValidator::Acceptable;
  QTest::newRow("local url") << QStringLiteral("bar") << QStringLiteral("file://www.openstreetmap.org") << QValidator::Intermediate;
  QTest::newRow("only scheme") << QStringLiteral("bar") << QStringLiteral("https://") << QValidator::Intermediate;
}

QTEST_MAIN(TestUrlValidator)

#include "test_UrlValidator.moc"
