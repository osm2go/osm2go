#include "../TagModel.h"

#include "../../../test/dummy_appdata.h"

#include <josm_presets.h>
#include <osm.h>
#include <osm_objects.h>
#include <osm2go_platform_qt.h>

#include <algorithm>
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
#include <QAbstractItemModelTester>
#else
class QAbstractItemModelTester {
public:
  inline QAbstractItemModelTester(QAbstractItemModel *) {}
};
#endif
#include <QComboBox>
#include <QTest>

class TestTagModel : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();

  void noTags();
  void noTagsWithOld();
  void noTagsWithOldTags();

  void onlyNewTags();
  void onlyNewTagsWithDiscardable();
  void onlyNewTagsOnNewObj();
  void onlyNewTagsWithDiscardableOnNewObj();
  void onlyNewTagsWithCollision();

  void replaceTags();

  void addTag();
  void modifyTagValue();
  void modifyTagKey();

  void deleteCollisions();
  void deleteCollisions_data();

  void modifyCollisions();
  void modifyCollisions_data();

  void setInvalidData();
  void setInvalidData_data();
};

namespace {

// ensure the data returned for DisplayRole and EditRole are the same
void rowDataSame(const TagModel &model, int row)
{
  auto checkCell = [&model](const QModelIndex &idx) {
    QCOMPARE(idx.data(Qt::EditRole), idx.data(Qt::DisplayRole));
    QVERIFY(model.flags(idx) & Qt::ItemIsEditable);
    QVERIFY(model.flags(idx) & Qt::ItemNeverHasChildren);
  };

  checkCell(model.index(row, 0));
  checkCell(model.index(row, 1));
}

void checkToolTipContains(const TagModel &model, int row, int column, const QString &str, Qt::CaseSensitivity cs = Qt::CaseSensitive)
{
    QVariant data = model.index(row, column).data(Qt::ToolTipRole);
    QVERIFY(!data.isNull());
    QCOMPARE(static_cast<QMetaType::Type>(data.type()), QMetaType::QString);
    QVERIFY(!data.toString().isEmpty());
    QVERIFY(data.toString().contains(str, cs));
}

void checkCellModified(const TagModel &model, int row, int column, bool isNew = false)
{
  QCOMPARE(model.index(row, column).data(Qt::FontRole), osm2go_platform::modelHightlightModified());
  if (!isNew && column == 1)
    checkToolTipContains(model, row, column, QStringLiteral("Original value"));
}

void checkRowModified(const TagModel &model, int row, bool isNew = false)
{
  int cols = model.columnCount(QModelIndex());
  QCOMPARE(cols, 2);
  for (int column = 0; column < cols; column++)
    checkCellModified(model, row, column, isNew);
}

void checkCellUnmodified(const TagModel &model, int row, int column, bool discardable = false)
{
  const QModelIndex idx = model.index(row, column);
  QCOMPARE(idx.data(Qt::FontRole), QVariant());
  if (discardable)
    checkToolTipContains(model, row, column, QStringLiteral("discardable"), Qt::CaseInsensitive);
  else
    QCOMPARE(idx.data(Qt::ToolTipRole), QVariant());
}

void checkRowUnmodified(const TagModel &model, int row, bool discardable = false)
{
  int cols = model.columnCount(QModelIndex());
  QCOMPARE(cols, 2);
  for (int column = 0; column < cols; column++)
    checkCellUnmodified(model, row, column, discardable);
}

std::vector<tag_t> simpleTags(void)
{
  return { tag_t("foo", "bar"), tag_t("baz", "boo") };
}

void checkContentsSimpleTags(TagModel &model)
{
  QCOMPARE(model.rowCount(QModelIndex()), 2);
  // the tags are passed through a map so they are sorted alphabetically, so they ignore the order from above
  QCOMPARE(model.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("baz"));
  QCOMPARE(model.index(0, 1).data(Qt::DisplayRole).toString(), QStringLiteral("boo"));
  QCOMPARE(model.index(1, 0).data(Qt::DisplayRole).toString(), QStringLiteral("foo"));
  QCOMPARE(model.index(1, 1).data(Qt::DisplayRole).toString(), QStringLiteral("bar"));
  for (int row : { 0, 1 }) {
    rowDataSame(model, row);
    for (int column : { 0, 1 })
      for (int role : { Qt::ToolTipRole, Qt::DecorationRole } )
        QCOMPARE(model.index(row, column).data(role), QVariant());
  }
}

std::vector<tag_t> simpleTagsWithDiscardable(void)
{
  auto ret = simpleTags();
  ret.push_back(tag_t("created_by", "OSM2go 0.5"));
  return ret;
}

/**
 * @brief check that the data from simpleTagsWithDiscardable() is shown by the model
 * @param rows the number of expected rows
 *
 * rows may be used to allow extra data behind the first 3 rows
 */
void checkContentsSimpleTagsWithDiscardable(const TagModel &model, unsigned int extraRows = 0)
{
  QCOMPARE(model.rowCount(QModelIndex()), static_cast<int>(3 + extraRows));
  // the tags are passed through a map so they are sorted alphabetically, so they ignore the order from above
  QCOMPARE(model.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("baz"));
  QCOMPARE(model.index(0, 1).data(Qt::DisplayRole).toString(), QStringLiteral("boo"));
  QCOMPARE(model.index(1, 0).data(Qt::DisplayRole).toString(), QStringLiteral("created_by"));
  QCOMPARE(model.index(1, 1).data(Qt::DisplayRole).toString(), QStringLiteral("OSM2go 0.5"));
  QCOMPARE(model.index(2, 0).data(Qt::DisplayRole).toString(), QStringLiteral("foo"));
  QCOMPARE(model.index(2, 1).data(Qt::DisplayRole).toString(), QStringLiteral("bar"));

  // the EditRole data for the discardable tags is the same as DisplayRole, but
  // this is just an implementation detail. The row is not editable, so what this
  // actually returns is of no interest.
  for (int row : { 0, 2 }) {
    rowDataSame(model, row);
    for (int column : { 0, 1 })
      for (int role : { Qt::ToolTipRole, Qt::DecorationRole } )
        QCOMPARE(model.index(row, column).data(role), QVariant());
 }

  // the discardable data must not be editable
  for (int column : { 0, 1 }) {
    QVERIFY(!(model.flags(model.index(1, column)) & (Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled)));
    // no localization is loaded, so this should be English
    checkToolTipContains(model, 1, column, QStringLiteral("discardable"), Qt::CaseInsensitive);
    QCOMPARE(model.index(1, column).data(Qt::DecorationRole), QVariant());
  }
}

std::vector<tag_t> collisionTags(void)
{
  auto ret = simpleTags();
  ret.push_back(tag_t("baz", "garbage"));
  ret.push_back(tag_t("baz", "junk"));
  return ret;
}

/**
 * @brief check that the first n rows are marked as collisions and share the given key
 */
void checkContentsCollisions(const TagModel &model, int n, const QString &key)
{
  QVERIFY(model.rowCount(QModelIndex()) > n);
  QSet<QString> collisionValues;
  // the tags are passed through a map so they are sorted alphabetically, so they ignore the order from above
  for (int row = 0; row < n; row++) {
    QCOMPARE(model.index(row, 0).data(Qt::DisplayRole).toString(), key);
    QCOMPARE(model.index(row, 0).data(Qt::DecorationRole).type(), QVariant::Icon);
    const auto data = model.index(row, 1).data(Qt::DisplayRole);
    QCOMPARE(static_cast<QMetaType::Type>(data.type()), QMetaType::QString);
    QVERIFY(!collisionValues.contains(data.toString()));
    collisionValues.insert(data.toString());
  }
  for (int row = n; row < model.rowCount(QModelIndex()); row++) {
    QVERIFY(model.index(row, 0).data(Qt::DisplayRole) != key);
    QCOMPARE(model.index(row, 0).data(Qt::DecorationRole), QVariant());
  }

  for (int row = 0; row < model.rowCount(QModelIndex()); row++) {
    rowDataSame(model, row);
    for (int column : { 0, 1 })
      QCOMPARE(model.index(row, column).data(Qt::ToolTipRole), QVariant());
    QCOMPARE(model.index(row, 1).data(Qt::DecorationRole), QVariant());
  }
}

} // namespace

void TestTagModel::initTestCase()
{
  // to make the preset paths work
  QCoreApplication::setApplicationName("osm2go");
}

// an empty object
void TestTagModel::noTags()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  object_t obj(&n);

  TagModel model(nullptr, obj, nullptr);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 0);
}

// an empty object, with a previous object that has also no tags
void TestTagModel::noTagsWithOld()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  object_t obj(&n);
  node_t oldNode(base_attributes(), lpos_t(1, 2), pos_t(1, 1.5));

  TagModel model(nullptr, obj, &oldNode);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 0);
}

// an empty object, with a previous object that has tags
void TestTagModel::noTagsWithOldTags()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  object_t obj(&n);
  node_t oldNode(base_attributes(), lpos_t(1, 2), pos_t(1, 1.5));
  oldNode.tags.replace(simpleTags());

  TagModel model(nullptr, obj, &oldNode);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 0);
}

void TestTagModel::onlyNewTags()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  n.tags.replace(simpleTags());
  object_t obj(&n);
  node_t oldNode(base_attributes(), lpos_t(1, 2), pos_t(1, 1.5));

  TagModel model(nullptr, obj, &oldNode);
  QAbstractItemModelTester mt(&model);

  checkContentsSimpleTags(model);

  for (int row = 0; row < model.rowCount(QModelIndex()); row++)
    checkRowModified(model, row, true);
}

void TestTagModel::onlyNewTagsWithDiscardable()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  n.tags.replace(simpleTagsWithDiscardable());
  object_t obj(&n);
  node_t oldNode(base_attributes(), lpos_t(1, 2), pos_t(1, 1.5));

  TagModel model(nullptr, obj, &oldNode);
  QAbstractItemModelTester mt(&model);

  checkContentsSimpleTagsWithDiscardable(model);

  for (int row = 0; row < model.rowCount(QModelIndex()); row++)
    checkRowModified(model, row, true);
}

void TestTagModel::onlyNewTagsOnNewObj()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  n.tags.replace(simpleTags());
  object_t obj(&n);

  TagModel model(nullptr, obj, nullptr);
  QAbstractItemModelTester mt(&model);

  checkContentsSimpleTags(model);

  for (int row = 0; row < model.rowCount(QModelIndex()); row++)
    checkRowUnmodified(model, row);
}

void TestTagModel::onlyNewTagsWithDiscardableOnNewObj()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  n.tags.replace(simpleTagsWithDiscardable());
  object_t obj(&n);

  TagModel model(nullptr, obj, nullptr);
  QAbstractItemModelTester mt(&model);

  checkContentsSimpleTagsWithDiscardable(model);

  for (int row = 0; row < model.rowCount(QModelIndex()); row++)
    checkRowUnmodified(model, row, row == 1);
}

void TestTagModel::onlyNewTagsWithCollision()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  n.tags.replace(collisionTags());
  object_t obj(&n);

  TagModel model(nullptr, obj, nullptr);
  QAbstractItemModelTester mt(&model);

  checkContentsCollisions(model, 3, QStringLiteral("baz"));
  // the tags are passed through a map so they are sorted alphabetically, so they ignore the order from above
  QCOMPARE(model.index(0, 1).data(Qt::DisplayRole).toString(), QStringLiteral("boo"));
  QCOMPARE(model.index(1, 1).data(Qt::DisplayRole).toString(), QStringLiteral("garbage"));
  QCOMPARE(model.index(2, 1).data(Qt::DisplayRole).toString(), QStringLiteral("junk"));
  QCOMPARE(model.index(3, 0).data(Qt::DisplayRole).toString(), QStringLiteral("foo"));
  QCOMPARE(model.index(3, 1).data(Qt::DisplayRole).toString(), QStringLiteral("bar"));
}

void TestTagModel::replaceTags()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  n.tags.replace(simpleTagsWithDiscardable());
  object_t obj(&n);

  TagModel model(nullptr, obj, nullptr);
  QAbstractItemModelTester mt(&model);

  checkContentsSimpleTagsWithDiscardable(model);

  model.replaceTags({});
  QCOMPARE(model.rowCount(QModelIndex()), 0);
  // the model does not modify the actual object
  QVERIFY(!n.tags.empty());

  tag_list_t tmpTags;
  tmpTags.replace(simpleTags());

  model.replaceTags(tmpTags.asMap());

  checkContentsSimpleTags(model);

  QCOMPARE(model.tags(), tmpTags.asMap());

  // all existing data was in the original data
  for (int row = 0; row < model.rowCount(QModelIndex()); row++)
    checkRowUnmodified(model, row);
}

void TestTagModel::addTag()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  n.tags.replace(simpleTagsWithDiscardable());
  object_t obj(&n);

  TagModel model(nullptr, obj, nullptr);
  QAbstractItemModelTester mt(&model);

  checkContentsSimpleTagsWithDiscardable(model);

  // must be alphabetically behind the previous tags...
  const QString newKey = QStringLiteral("newKey");
  const QString newVal = QStringLiteral("newValue");
  model.addTag(newKey, newVal);

  // ... so this check still works
  checkContentsSimpleTagsWithDiscardable(model, 1);

  QCOMPARE(model.index(3, 0).data(Qt::DisplayRole).toString(), newKey);
  QCOMPARE(model.index(3, 1).data(Qt::DisplayRole).toString(), newVal);
  rowDataSame(model, 3);

  checkRowModified(model, 3, true);
}

void TestTagModel::modifyTagValue()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  n.tags.replace(simpleTagsWithDiscardable());
  object_t obj(&n);

  TagModel model(nullptr, obj, nullptr);
  QAbstractItemModelTester mt(&model);

  checkContentsSimpleTagsWithDiscardable(model);

  const QString newVal = QStringLiteral("newValue");
  QVERIFY(model.setData(model.index(0, 1), newVal, Qt::EditRole));

  QCOMPARE(model.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("baz"));
  QCOMPARE(model.index(0, 1).data(Qt::DisplayRole).toString(), newVal);
  QVERIFY(model.index(0, 1).data(Qt::ToolTipRole).toString().contains(QStringLiteral("boo")));
  rowDataSame(model, 0);

  checkCellUnmodified(model, 0, 0);
  checkCellModified(model, 0, 1);

  // change it back
  QVERIFY(model.setData(model.index(0, 1), QStringLiteral("boo"), Qt::EditRole));

  checkContentsSimpleTagsWithDiscardable(model);
}

void TestTagModel::modifyTagKey()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  n.tags.replace(simpleTagsWithDiscardable());
  object_t obj(&n);

  TagModel model(nullptr, obj, nullptr);
  QAbstractItemModelTester mt(&model);

  checkContentsSimpleTagsWithDiscardable(model);

  const QString newVal = QStringLiteral("newKey");
  QVERIFY(model.setData(model.index(0, 0), newVal, Qt::EditRole));

  QCOMPARE(model.index(0, 0).data(Qt::DisplayRole).toString(), newVal);
  QCOMPARE(model.index(0, 1).data(Qt::DisplayRole).toString(), QStringLiteral("boo"));
  QCOMPARE(model.index(0, 0).data(Qt::ToolTipRole), QVariant());
  rowDataSame(model, 0);

  // the whole row now counts as new
  checkRowModified(model, 0, true);

  // change it back
  QVERIFY(model.setData(model.index(0, 0), QStringLiteral("baz"), Qt::EditRole));

  checkContentsSimpleTagsWithDiscardable(model);
}

// remove 2 of the 3 colliding rows and see the collision flag vanish
void TestTagModel::deleteCollisions()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  n.tags.replace(collisionTags());
  object_t obj(&n);

  TagModel model(nullptr, obj, nullptr);
  QAbstractItemModelTester mt(&model);

  checkContentsCollisions(model, 3, QStringLiteral("baz"));

  QFETCH(int, removeFirst);
  QVERIFY(model.removeRow(removeFirst));
  checkContentsCollisions(model, 2, QStringLiteral("baz"));

  QFETCH(int, removeSecond);
  // the rows have already been shifted by one
  if (removeSecond > removeFirst)
    removeSecond--;
  QVERIFY(model.removeRow(removeSecond));

  QFETCH(QString, remainingData);

  if (remainingData == QStringLiteral("boo")) {
    checkContentsSimpleTags(model);
  } else {
    QCOMPARE(model.rowCount(QModelIndex()), 2);
    // the tags are passed through a map so they are sorted alphabetically, so they ignore the order from above
    QCOMPARE(model.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("baz"));
    QCOMPARE(model.index(0, 1).data(Qt::DisplayRole).toString(), remainingData);
    QCOMPARE(model.index(1, 0).data(Qt::DisplayRole).toString(), QStringLiteral("foo"));
    QCOMPARE(model.index(1, 1).data(Qt::DisplayRole).toString(), QStringLiteral("bar"));
    for (int row : { 0, 1 }) {
      rowDataSame(model, row);
      for (int column : { 0, 1 })
        QCOMPARE(model.index(row, column).data(Qt::DecorationRole), QVariant());
      QCOMPARE(model.index(row, 0).data(Qt::ToolTipRole), QVariant());
    }
    checkToolTipContains(model, 0, 1, QStringLiteral("boo"));
    QCOMPARE(model.index(1, 1).data(Qt::ToolTipRole), QVariant());
  }
}

void TestTagModel::deleteCollisions_data()
{
  QTest::addColumn<int>("removeFirst");
  QTest::addColumn<int>("removeSecond");
  QTest::addColumn<QString>("remainingData");

  const auto cTags = collisionTags();
  std::vector<int> entries = { 0, 1, 2 };

  do {
    QByteArray testName = QByteArray::number(entries.at(0)) + ' ' + QByteArray::number(entries.at(1));
    // the remaining index is +1 because collisionTags() is not sorted yet, so the unrelated other tag is first
    QTest::newRow(testName.constData()) << entries.at(0) << entries.at(1) <<
        QString(QLatin1String(cTags.at(entries.at(2) + 1).value));
  } while (std::next_permutation(entries.begin(), entries.end()));
}

// modify 2 of the 3 colliding rows and see the collision flag vanish
void TestTagModel::modifyCollisions()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  n.tags.replace(collisionTags());
  object_t obj(&n);

  TagModel model(nullptr, obj, nullptr);
  QAbstractItemModelTester mt(&model);

  checkContentsCollisions(model, 3, QStringLiteral("baz"));

  // set the row to modify to an unrelated value, this should not change something on the general setup
  QFETCH(int, modifyFirst);
  const auto idxFirst = model.index(modifyFirst, 1);
  QVERIFY(model.setData(idxFirst, QStringLiteral("unrelated"), Qt::EditRole));
  // also set the key to the same value again, which should not change anyting
  QVERIFY(model.setData(model.index(modifyFirst, 0), QStringLiteral("baz"), Qt::EditRole));
  checkContentsCollisions(model, 3, QStringLiteral("baz"));
  QCOMPARE(idxFirst.data(Qt::DisplayRole).toString(), QStringLiteral("unrelated"));

  // set one value to the value of a different collision row, so it should be automatically removed
  QFETCH(int, modifySecond);
  QVariant newData = model.data(model.index(modifySecond, 1), Qt::EditRole);
  QVERIFY(model.setData(idxFirst, newData, Qt::EditRole));
  checkContentsCollisions(model, 2, QStringLiteral("baz"));

  // the rows have already been shifted by one
  if (modifySecond > modifyFirst)
    modifySecond--;
  if (modifySecond == 0)
    newData = model.data(model.index(1, 1), Qt::EditRole);
  else
    newData = model.data(model.index(0, 1), Qt::EditRole);
  QVERIFY(model.setData(model.index(modifySecond, 1), newData, Qt::EditRole));

  QFETCH(QString, remainingData);

  if (remainingData == QStringLiteral("boo")) {
    checkContentsSimpleTags(model);
  } else {
    QCOMPARE(model.rowCount(QModelIndex()), 2);
    // the tags are passed through a map so they are sorted alphabetically, so they ignore the order from above
    QCOMPARE(model.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("baz"));
    QCOMPARE(model.index(0, 1).data(Qt::DisplayRole).toString(), remainingData);
    QCOMPARE(model.index(1, 0).data(Qt::DisplayRole).toString(), QStringLiteral("foo"));
    QCOMPARE(model.index(1, 1).data(Qt::DisplayRole).toString(), QStringLiteral("bar"));
    for (int row : { 0, 1 }) {
      rowDataSame(model, row);
      for (int column : { 0, 1 })
        QCOMPARE(model.index(row, column).data(Qt::DecorationRole), QVariant());
      QCOMPARE(model.index(row, 0).data(Qt::ToolTipRole), QVariant());
    }
    checkToolTipContains(model, 0, 1, QStringLiteral("boo"));
    QCOMPARE(model.index(1, 1).data(Qt::ToolTipRole), QVariant());
  }
}

void TestTagModel::modifyCollisions_data()
{
  QTest::addColumn<int>("modifyFirst");
  QTest::addColumn<int>("modifySecond");
  QTest::addColumn<QString>("remainingData");

  const auto cTags = collisionTags();
  std::vector<int> entries = { 0, 1, 2 };

  do {
    QByteArray testName = QByteArray::number(entries.at(0)) + ' ' + QByteArray::number(entries.at(1));
    // the remaining index is +1 because collisionTags() is not sorted yet, so the unrelated other tag is first
    QTest::newRow(testName.constData()) << entries.at(0) << entries.at(1) <<
        QString(QLatin1String(cTags.at(entries.at(2) + 1).value));
  } while (std::next_permutation(entries.begin(), entries.end()));
}

// try to set invalid values, should not change the model
void TestTagModel::setInvalidData()
{
  node_t n(base_attributes(), lpos_t(1, 1), pos_t(1, 1));
  n.tags.replace(simpleTags());
  object_t obj(&n);

  TagModel model(nullptr, obj, nullptr);
  QAbstractItemModelTester mt(&model);

  checkContentsSimpleTags(model);

  QFETCH(int, column);
  QFETCH(QVariant, newData);

  QVERIFY(!model.setData(model.index(0, column), newData, Qt::EditRole));

  checkContentsSimpleTags(model);
}

void TestTagModel::setInvalidData_data()
{
  QTest::addColumn<int>("column");
  QTest::addColumn<QVariant>("newData");

  QTest::newRow("create collision") << 0 << QVariant(QStringLiteral("foo"));
  QTest::newRow("empty key") << 0 << QVariant();
  QTest::newRow("empty value") << 1 << QVariant();
  QTest::newRow("invalid index") << -1 << QVariant(QStringLiteral("junk"));
}

QTEST_MAIN(TestTagModel)

#include "test_TagModel.moc"
