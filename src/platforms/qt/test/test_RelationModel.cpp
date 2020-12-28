#include "../RelationModel.h"

#include "../../../test/dummy_appdata.h"

#include <osm.h>
#include <osm_objects.h>
#include <osm2go_platform_qt.h>

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
#include <QAbstractItemModelTester>
#else
class QAbstractItemModelTester {
public:
  inline QAbstractItemModelTester(QAbstractItemModel *) {}
};
#endif
#include <QTest>

class TestRelationModel : public QObject {
  Q_OBJECT

private slots:
  void noRelations();
  void deletedRelations();
  void onlyOldRelations();
  void onlyNewRelations();
  void modifiedRelations();
  void addRelation();
  void removeRelations();
};

namespace {

void set_bounds(osm_t::ref o)
{
  bool b = o->bounds.init(pos_area(pos_t(52.2692786, 9.5750497), pos_t(52.2695463, 9.5755)));
  assert(b);
}

std::unique_ptr<osm_t> boundedOsm()
{
  auto osm = std::make_unique<osm_t>();
  set_bounds(osm);
  return osm;
}

std::vector<relation_t *> createRelations(osm_t::ref osm, bool newRelations)
{
  const int relCount = 3;
  std::vector<relation_t *> rels;

  base_attributes ba;
  if (!newRelations)
    ba.version = 1;

  for (int i = 0; i < relCount; i++) {
    if (!newRelations)
      ba.id = i + 42;
    rels.push_back(new relation_t(ba));
    if (newRelations)
      osm->attach(rels.back());
    else
      osm->insert(rels.back());
  }

  rels.front()->members.emplace_back(member_t(object_t(rels.back())));
  rels.at(1)->tags.replace({ tag_t("type", "route") });

  return rels;
}

void checkOldRelations(const RelationModel &model, const std::vector<relation_t *> &rels)
{
  for (int row = 0; row < 3; row++) {
    const relation_t *rel = rels.at(row);
    for (int col : { RELATION_COL_TYPE, RELATION_COL_MEMBERS, RELATION_COL_NAME }) {
      auto idx = model.index(row, col);

      QCOMPARE(idx.data(Qt::UserRole).value<void *>(), rel);
      QCOMPARE(idx.data(Qt::FontRole), QVariant());

      QVariant data = model.data(idx, Qt::DisplayRole);
      if (col == RELATION_COL_NAME)
        QCOMPARE(data.toString(), static_cast<QString>(rel->idName()));
      else if (row == 1 && col == RELATION_COL_TYPE)
        QCOMPARE(data.toString(), QStringLiteral("route"));
      else if (row == 0 && col == RELATION_COL_MEMBERS)
        QCOMPARE(data.toInt(), 1);
      else if (col == RELATION_COL_MEMBERS)
        QCOMPARE(data.toInt(), 0);
      else
        QCOMPARE(data.toString(), QString());

      auto flags = model.flags(idx);

      QVERIFY(!(flags & Qt::ItemIsEditable));
      QVERIFY(!(flags & Qt::ItemIsUserCheckable));
      QVERIFY(!(flags & Qt::ItemIsUserTristate));
      QVERIFY(flags & Qt::ItemNeverHasChildren);

    }
    QCOMPARE(model.index(row, RELATION_COL_TYPE).data(Qt::ToolTipRole), QVariant());
    QCOMPARE(model.index(row, RELATION_COL_NAME).data(Qt::ToolTipRole).toInt(), static_cast<int>(rel->id));
    QCOMPARE(model.index(row, RELATION_COL_MEMBERS).data(Qt::ToolTipRole), QVariant());
  }
}

} // namespace

void TestRelationModel::noRelations()
{
  auto osm = boundedOsm();

  RelationModel model(nullptr, osm);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 0);
}

void TestRelationModel::deletedRelations()
{
  auto osm = boundedOsm();

  const std::vector<relation_t *> rels = createRelations(osm, false);

  for (auto *r : rels)
    osm->relation_delete(r);

  RelationModel model(nullptr, osm);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 0);
}

void TestRelationModel::onlyOldRelations()
{
  auto osm = boundedOsm();

  const int relCount = 3;
  const std::vector<relation_t *> rels = createRelations(osm, false);

  RelationModel model(nullptr, osm);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), relCount);

  checkOldRelations(model, rels);
}

void TestRelationModel::onlyNewRelations()
{
  auto osm = boundedOsm();

  const int relCount = 3;
  const std::vector<relation_t *> rels = createRelations(osm, true);

  RelationModel model(nullptr, osm);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), relCount);

  for (int row = 0; row < relCount; row++) {
    const relation_t *rel = rels.at(relCount - row - 1);
    for (int col : { RELATION_COL_TYPE, RELATION_COL_MEMBERS, RELATION_COL_NAME }) {
      // the relations are sorted by id, i.e. opposite to creation order
      auto idx = model.index(row, col);

      QCOMPARE(idx.data(Qt::UserRole).value<void *>(), rel);
      QCOMPARE(idx.data(Qt::FontRole), osm2go_platform::modelHightlightModified());

      QVariant data = model.data(idx, Qt::DisplayRole);
      if (col == RELATION_COL_NAME)
        QCOMPARE(data.toString(), static_cast<QString>(rel->idName()));
      else if (row == 1 && col == RELATION_COL_TYPE)
        QCOMPARE(data.toString(), QStringLiteral("route"));
      else if (row == relCount - 1 && col == RELATION_COL_MEMBERS)
        QCOMPARE(data.toInt(), 1);
      else if (col == RELATION_COL_MEMBERS)
        QCOMPARE(data.toInt(), 0);
      else
        QCOMPARE(data.toString(), QString());

      auto flags = model.flags(idx);

      QVERIFY(!(flags & Qt::ItemIsEditable));
      QVERIFY(!(flags & Qt::ItemIsUserCheckable));
      QVERIFY(!(flags & Qt::ItemIsUserTristate));
      QVERIFY(flags & Qt::ItemNeverHasChildren);
    }
    QCOMPARE(model.index(row, RELATION_COL_TYPE).data(Qt::ToolTipRole), QVariant());
    QCOMPARE(model.index(row, RELATION_COL_NAME).data(Qt::ToolTipRole).toInt(), static_cast<int>(rel->id));
    QCOMPARE(model.index(row, RELATION_COL_MEMBERS).data(Qt::ToolTipRole), QVariant());
  }
}

void TestRelationModel::modifiedRelations()
{
  auto osm = boundedOsm();

  const int relCount = 3;
  const std::vector<relation_t *> rels = createRelations(osm, false);

  for (unsigned int i : { 0, 1 })
    osm->mark_dirty(rels.at(i));

  osm->attach(osm->node_new(lpos_t(1, 1)));

  rels.front()->members.emplace_back(member_t(object_t(osm->nodes.begin()->second)));
  rels.at(1)->tags.replace({ tag_t("type", "route"), tag_t("name", "foobar") });

  RelationModel model(nullptr, osm);
  QAbstractItemModelTester mt(&model);

  osm->mark_dirty(rels.back());
  rels.back()->tags.replace({ tag_t("type", "multipolygon") });
  model.relationEdited(rels.back());

  QCOMPARE(model.rowCount(QModelIndex()), relCount);

  for (int row = 0; row < relCount; row++) {
    const relation_t *rel = rels.at(row);
    for (int col : { RELATION_COL_TYPE, RELATION_COL_NAME, RELATION_COL_MEMBERS }) {
      auto idx = model.index(row, col);

      QCOMPARE(idx.data(Qt::UserRole).value<void *>(), rel);
      // catch the modified ones
      if (col == relCount - row - 1 ||
         // the tags have changed, even if the actual name did not change
         (row == 2 && col == RELATION_COL_NAME))
        QCOMPARE(idx.data(Qt::FontRole), osm2go_platform::modelHightlightModified());
      else
        QCOMPARE(idx.data(Qt::FontRole), QVariant());

      QVariant data = model.data(idx, Qt::DisplayRole);
      if (row == 1 && col == RELATION_COL_NAME)
        QCOMPARE(data.toString(), QStringLiteral("foobar"));
      else if (col == RELATION_COL_NAME)
        QCOMPARE(data.toString(), static_cast<QString>(rel->idName()));
      else if (row == 1 && col == RELATION_COL_TYPE)
        QCOMPARE(data.toString(), QStringLiteral("route"));
      else if (row == 2 && col == RELATION_COL_TYPE)
        QCOMPARE(data.toString(), QStringLiteral("multipolygon"));
      else if (row == 0 && col == RELATION_COL_MEMBERS)
        QCOMPARE(data.toInt(), 2);
      else if (col == RELATION_COL_MEMBERS)
        QCOMPARE(data.toInt(), 0);
      else
        QCOMPARE(data.toString(), QString());

      auto flags = model.flags(idx);

      QVERIFY(!(flags & Qt::ItemIsEditable));
      QVERIFY(!(flags & Qt::ItemIsUserCheckable));
      QVERIFY(!(flags & Qt::ItemIsUserTristate));
      QVERIFY(flags & Qt::ItemNeverHasChildren);
    }
    const QVariant tipData = model.index(row, RELATION_COL_MEMBERS).data(Qt::ToolTipRole);
    if (row == 0)
      QCOMPARE(tipData.toInt(), 1);
    else
      QCOMPARE(tipData, QVariant());
    QCOMPARE(model.index(row, RELATION_COL_NAME).data(Qt::ToolTipRole).toInt(), static_cast<int>(rel->id));
    QCOMPARE(model.index(row, RELATION_COL_TYPE).data(Qt::ToolTipRole), QVariant());
  }
}

void TestRelationModel::addRelation()
{
  auto osm = boundedOsm();

  const int relCount = 3;
  const std::vector<relation_t *> rels = createRelations(osm, false);

  RelationModel model(nullptr, osm);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), relCount);

  checkOldRelations(model, rels);

  auto *newRel = new relation_t();
  osm->attach(newRel);

  model.addRelation(newRel);

  QCOMPARE(model.rowCount(QModelIndex()), relCount + 1);

  checkOldRelations(model, rels);

  for (int col : { RELATION_COL_TYPE, RELATION_COL_MEMBERS, RELATION_COL_NAME }) {
    auto idx = model.index(relCount, col);

    QCOMPARE(idx.data(Qt::UserRole).value<void *>(), newRel);
    QCOMPARE(idx.data(Qt::FontRole), osm2go_platform::modelHightlightModified());
  }
}

void TestRelationModel::removeRelations()
{
  auto osm = boundedOsm();

  const int relCount = 3;
  const std::vector<relation_t *> rels = createRelations(osm, false);

  RelationModel model(nullptr, osm);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), relCount);

  checkOldRelations(model, rels);

  for (int i = relCount - 1; i >= 0; i--) {
    model.removeRow(i, QModelIndex());
    QCOMPARE(model.rowCount(QModelIndex()), i);
    // check that the other data is unchanged
    for (int row = 0; row < i; row++) {
      const relation_t *rel = rels.at(row);
      auto idx = model.index(row, 0);

      QCOMPARE(idx.data(Qt::UserRole).value<void *>(), rel);
    }
  }
}

QTEST_MAIN(TestRelationModel)

#include "test_RelationModel.moc"
