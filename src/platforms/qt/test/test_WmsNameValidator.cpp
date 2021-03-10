/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <WmsNameValidator.h>

#include <WmsModel.h>
#include <wms.h>
#include "../../../test/dummy_appdata.h"

#include <QTest>

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
Q_DECLARE_METATYPE(QValidator::State)
#endif

class TestWmsNameValidator : public QObject {
  Q_OBJECT

private slots:
  void test();
  void test_data();
};

namespace {

class emptySettings : public settings_t {
  emptySettings() = default;
public:
  static std::shared_ptr<settings_t> get()
  {
    std::shared_ptr<settings_t> ret(new emptySettings());

    return ret;
  }

  ~emptySettings()
  {
    // don't needlessly save anything to disk
    std::for_each(wms_server.begin(), wms_server.end(), std::default_delete<wms_server_t>());
    wms_server.clear();
  }
};

}

void TestWmsNameValidator::test()
{
  QFETCH(QString, oldValue);
  QFETCH(QString, otherServer);
  QFETCH(QString, input);
  int dummy;

  auto settings = emptySettings::get();
  if (!oldValue.isEmpty())
    settings->wms_server.emplace_back(new wms_server_t(oldValue.toUtf8().constData(), "http://wms.example.com"));
  if (!otherServer.isEmpty())
    settings->wms_server.emplace_back(new wms_server_t(otherServer.toUtf8().constData(), "http://wms.example.org"));

  WmsModel model(settings);

  WmsNameValidator validator(oldValue, &model);
  QTEST(validator.validate(input, dummy), "result");
}

void TestWmsNameValidator::test_data()
{
  QTest::addColumn<QString>("oldValue");
  QTest::addColumn<QString>("otherServer");
  QTest::addColumn<QString>("input");
  QTest::addColumn<QValidator::State>("result");

  QTest::newRow("empty input") << QStringLiteral("foo") << QString() << QString() << QValidator::Intermediate;
  QTest::newRow("empty input, no old") << QString() << QString() << QString() << QValidator::Intermediate;

  QTest::newRow("old as input") << QStringLiteral("foo") << QString() << QStringLiteral("foo") << QValidator::Acceptable;
  QTest::newRow("other input") << QStringLiteral("bar") << QString() << QStringLiteral("foo") << QValidator::Acceptable;

  QTest::newRow("collision with existing") << QString() << QStringLiteral("bar") << QStringLiteral("bar") << QValidator::Intermediate;
  QTest::newRow("different from existing") << QString() << QStringLiteral("bar") << QStringLiteral("baz") << QValidator::Acceptable;
  QTest::newRow("different from existing and old") << QStringLiteral("baz") << QStringLiteral("bar") << QStringLiteral("foo") << QValidator::Acceptable;
}

QTEST_MAIN(TestWmsNameValidator)

#include "test_WmsNameValidator.moc"
