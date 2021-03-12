/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ProjectNameValidator.h>

#include <project.h>
#include "../../../test/dummy_appdata.h"

#include <QTest>

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
Q_DECLARE_METATYPE(QValidator::State)
#endif

class TestProjectNameValidator : public QObject {
  Q_OBJECT

private slots:
  void singleTest();
  void singleTest_data();

  void listTest();
  void listTest_data();
};

void TestProjectNameValidator::singleTest()
{
  QFETCH(QString, input);
  int dummy;

  std::vector<std::unique_ptr<project_t>> projects;

  ProjectNameValidator validator(projects);
  QTEST(validator.validate(input, dummy), "result");
}

void TestProjectNameValidator::singleTest_data()
{
  QTest::addColumn<QString>("input");
  QTest::addColumn<QValidator::State>("result");

  QTest::newRow("empty input") << QString() << QValidator::Intermediate;

  // whitespace only
  QTest::newRow("space") << QStringLiteral(" ") << QValidator::Intermediate;
  QTest::newRow("tab") << QStringLiteral("\t") << QValidator::Intermediate;
  QTest::newRow("newline") << QStringLiteral("\n") << QValidator::Intermediate;
  QTest::newRow("whitespaces") << QStringLiteral(" \t \n  \t\t \n\n") << QValidator::Intermediate;

  // good ones
  QTest::newRow("good") << QStringLiteral("foo") << QValidator::Acceptable;
  QTest::newRow("good+ws") << QStringLiteral(" foo \t") << QValidator::Acceptable;

  // bad characters
  QTest::newRow("good*") << QStringLiteral("fo*o") << QValidator::Invalid;
  QTest::newRow("good?") << QStringLiteral("fo?o") << QValidator::Invalid;
  QTest::newRow("good/") << QStringLiteral("fo/o") << QValidator::Invalid;
  QTest::newRow("good\\") << QStringLiteral("fo\\o") << QValidator::Invalid;
}

void TestProjectNameValidator::listTest()
{
  QFETCH(QString, input);
  int dummy;

  std::vector<std::unique_ptr<project_t>> projects;
  for (auto &&pn : { "bar", "baz" })
    projects.emplace_back(std::make_unique<project_t>(pn, std::string()));

  ProjectNameValidator validator(projects);
  QTEST(validator.validate(input, dummy), "result");
}

void TestProjectNameValidator::listTest_data()
{
  QTest::addColumn<QString>("input");
  QTest::addColumn<QValidator::State>("result");

  // good ones
  QTest::newRow("good on other list") << QStringLiteral("foo") << QValidator::Acceptable;
  QTest::newRow("good+ws on other list") << QStringLiteral(" foo  ") << QValidator::Acceptable;

  // errors regarding old list
  QTest::newRow("list collision") << QStringLiteral("bar") << QValidator::Intermediate;
  QTest::newRow("list collision by whitespace") << QStringLiteral(" bar  ") << QValidator::Intermediate;
}

QTEST_MAIN(TestProjectNameValidator)

#include "test_ProjectNameValidator.moc"
