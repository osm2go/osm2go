/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../WmsModel.h"

#include "../../../test/dummy_appdata.h"
#include "helper.h"

#include <wms.h>

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
#include <QAbstractItemModelTester>
#else
class QAbstractItemModelTester {
public:
  inline QAbstractItemModelTester(QAbstractItemModel *) {}
};
#endif
#include <QTest>

class TestWmsModel : public QObject {
  Q_OBJECT

private slots:
  void emptyList();
  void validEntries();
  void addEntries();
  void removeEntries();
  void removeEntries_data();
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


std::vector<QString> expectedHeaderData()
{
  return { QStringLiteral("Name") };
}

// ensure the data returned for DisplayRole and EditRole are the same
void rowDataSame(const WmsModel &model, int row)
{
  const QModelIndex idx = model.index(row);
  QCOMPARE(idx.data(Qt::EditRole), idx.data(Qt::DisplayRole));
  QVERIFY(model.flags(idx) & Qt::ItemNeverHasChildren);
}

} // namespace

void TestWmsModel::emptyList()
{
  auto settings = emptySettings::get();

  WmsModel model(settings);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 0);

  checkHeaderData(&model, expectedHeaderData(), Qt::Horizontal);
  checkHeaderDataEmpty(&model, Qt::Vertical);
}

void TestWmsModel::validEntries()
{
  auto settings = emptySettings::get();

  settings->wms_server.emplace_back(new wms_server_t("foo", "http://foo"));
  settings->wms_server.emplace_back(new wms_server_t("bar", "https://bar"));

  WmsModel model(settings);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), static_cast<int>(settings->wms_server.size()));
  for (int row = 0; row < model.rowCount(QModelIndex()); row++) {
    rowDataSame(model, row);
    QCOMPARE(model.index(row).data(Qt::UserRole).value<void *>(), settings->wms_server.at(row));
    QVERIFY(model.hasName(QString::fromStdString(settings->wms_server.at(row)->name)));
    QCOMPARE(model.indexOfServer(settings->wms_server.at(row)->server), row);
  }

  QCOMPARE(model.index(0).data().toString(), QStringLiteral("foo"));
  QCOMPARE(model.index(1).data().toString(), QStringLiteral("bar"));

  QVERIFY(!model.hasName(QStringLiteral("baz")));
  QCOMPARE(model.indexOfServer("foo"), -1);

  checkHeaderData(&model, expectedHeaderData(), Qt::Horizontal);
  checkHeaderDataEmpty(&model, Qt::Vertical);
}

void TestWmsModel::addEntries()
{
  auto settings = emptySettings::get();

  settings->wms_server.emplace_back(new wms_server_t("foo", "http://foo"));
  settings->wms_server.emplace_back(new wms_server_t("bar", "https://bar"));

  WmsModel model(settings);
  QAbstractItemModelTester mt(&model);

  wms_server_t *baz = model.addServer(std::make_unique<wms_server_t>("baz", "http://baz"));

  QVERIFY(baz != nullptr);
  QCOMPARE(static_cast<int>(settings->wms_server.size()), 3);
  QCOMPARE(settings->wms_server.back(), baz);

  wms_server_t *boo = model.addServer(std::make_unique<wms_server_t>("boo", "https://boo"));

  QVERIFY(boo != nullptr);
  QCOMPARE(static_cast<int>(settings->wms_server.size()), 4);
  QCOMPARE(settings->wms_server.back(), boo);

  QCOMPARE(model.rowCount(QModelIndex()), static_cast<int>(settings->wms_server.size()));
  for (int row = 0; row < model.rowCount(QModelIndex()); row++) {
    rowDataSame(model, row);
    QCOMPARE(model.index(row).data(Qt::UserRole).value<void *>(), settings->wms_server.at(row));
    QVERIFY(model.hasName(QString::fromStdString(settings->wms_server.at(row)->name)));
    QCOMPARE(model.indexOfServer(settings->wms_server.at(row)->server), row);
  }

  QCOMPARE(model.index(0).data().toString(), QStringLiteral("foo"));
  QCOMPARE(model.index(1).data().toString(), QStringLiteral("bar"));
  QCOMPARE(model.index(2).data().toString(), QStringLiteral("baz"));
  QCOMPARE(model.index(3).data().toString(), QStringLiteral("boo"));

  QVERIFY(model.hasName(QStringLiteral("baz")));
}

void TestWmsModel::removeEntries()
{
  auto settings = emptySettings::get();

  settings->wms_server.emplace_back(new wms_server_t("foo", "http://foo"));
  settings->wms_server.emplace_back(new wms_server_t("bar", "https://bar"));

  WmsModel model(settings);
  QAbstractItemModelTester mt(&model);

  QFETCH(int, firstRow);
  QFETCH(QByteArray, remainingName);

  QVERIFY(model.removeRow(firstRow));

  QCOMPARE(static_cast<int>(settings->wms_server.size()), 1);
  QCOMPARE(settings->wms_server.back()->name, remainingName.toStdString());
  QCOMPARE(model.rowCount(QModelIndex()), 1);

  QCOMPARE(model.index(0).data().toString(), QLatin1String(remainingName));

  QVERIFY(model.removeRow(0));
  QCOMPARE(model.rowCount(QModelIndex()), 0);
  QCOMPARE(static_cast<int>(settings->wms_server.size()), 0);
}

void TestWmsModel::removeEntries_data()
{
  QTest::addColumn<int>("firstRow");
  QTest::addColumn<QByteArray>("remainingName");

  QTest::addRow("0") << 0 << QByteArrayLiteral("bar");
  QTest::addRow("1") << 1 << QByteArrayLiteral("foo");
}

QTEST_MAIN(TestWmsModel)

#include "test_WmsModel.moc"
