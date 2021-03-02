#include "../RelationMemberModel.h"

#include "../../../test/dummy_appdata.h"
#include "helper.h"

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

class TestRelationMemberModel : public QObject {
  Q_OBJECT

private slots:
  void noMembers();
  void simpleMembers();
  void refMembers();
  void moveRows();
  void changeRole();
};

namespace {

std::vector<QString> expectedHeaderData()
{
  return { QStringLiteral("Type"), QStringLiteral("Id"), QStringLiteral("Name"), QStringLiteral("Role") };
}

// ensure the data returned for DisplayRole and EditRole are the same
void rowDataSame(const RelationMemberModel &model, int row)
{
  auto checkCell = [&model](const QModelIndex &idx) {
    QCOMPARE(idx.data(Qt::EditRole), idx.data(Qt::DisplayRole));
    if (idx.column() == MEMBER_COL_ROLE)
     QVERIFY(model.flags(idx) & Qt::ItemIsEditable);
    else
     QVERIFY(!(model.flags(idx) & Qt::ItemIsEditable));
    QVERIFY(model.flags(idx) & Qt::ItemNeverHasChildren);
  };

  QCOMPARE(model.columnCount(QModelIndex()), static_cast<int>(MEMBER_NUM_COLS));

  for (int column = 0; column < MEMBER_NUM_COLS; column++)
    checkCell(model.index(row, column));
}

void checkCellModified(const RelationMemberModel &model, int row, int column)
{
  QCOMPARE(model.index(row, column).data(Qt::FontRole), osm2go_platform::modelHightlightModified());
}

void checkCellUnmodified(const RelationMemberModel &model, int row, int column)
{
  const QModelIndex idx = model.index(row, column);
  QCOMPARE(idx.data(Qt::FontRole), QVariant());
}

void checkRowUnmodified(const RelationMemberModel &model, int row)
{
  QCOMPARE(model.columnCount(QModelIndex()), static_cast<int>(MEMBER_NUM_COLS));
  for (int column = 0; column < MEMBER_NUM_COLS; column++)
    checkCellUnmodified(model, row, column);
}

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

relation_t *restrictionOsm(const std::unique_ptr<osm_t> &osm)
{
  base_attributes ba(1);
  ba.version = 1;
  auto *r = new relation_t(ba);
  osm->insert(r);
  r->tags.replace({ tag_t("type", "restriction") });

  auto *w = new way_t(ba);
  osm->insert(w);
  auto *n = osm->node_new(lpos_t(1, 1));
  osm->attach(n);
  w->node_chain.emplace_back(n);
  ba.id = 2;
  n = osm->node_new(osm->bounds.center.toPos(osm->bounds), ba);
  osm->insert(n);
  w->node_chain.emplace_back(n);

  r->members.emplace_back(member_t(object_t(w), "from"));
  r->members.emplace_back(member_t(object_t(n), "via"));

  ba.id = 3;
  w = new way_t(ba);
  osm->insert(w);
  w->node_chain.emplace_back(n);
  n = osm->node_new(lpos_t(2, 2));
  osm->attach(n);
  w->node_chain.emplace_back(n);

  r->members.emplace_back(member_t(object_t(w), "to"));

  return r;
}

} // namespace

void TestRelationMemberModel::noMembers()
{
  auto osm = boundedOsm();
  auto * const r = new relation_t();
  osm->attach(r);

  RelationMemberModel model(r, osm);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 0);

  checkHeaderData(&model, expectedHeaderData(), Qt::Horizontal);
  checkHeaderDataEmpty(&model, Qt::Vertical);
}

void TestRelationMemberModel::simpleMembers()
{
  auto osm = boundedOsm();
  relation_t * const r = restrictionOsm(osm);

  RelationMemberModel model(r, osm);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 3);

  for (int row = 0; row < 3; row++) {
    rowDataSame(model, row);
    if (row == 1)
      QCOMPARE(model.index(row, MEMBER_COL_TYPE).data(Qt::DisplayRole).toString(), static_cast<QString>(_("node")));
    else
      QCOMPARE(model.index(row, MEMBER_COL_TYPE).data(Qt::DisplayRole).toString(), static_cast<QString>(_("way")));
    QCOMPARE(model.index(row, MEMBER_COL_ID).data(Qt::DisplayRole).toInt(), row + 1);
    checkRowUnmodified(model, row);
    if (row == 1)
      QCOMPARE(model.index(1, MEMBER_COL_NAME).data(Qt::DisplayRole).toString(), static_cast<QString>(object_t(osm->nodes[2]).get_name(*osm)));
    else
      QCOMPARE(model.index(row, MEMBER_COL_NAME).data(Qt::DisplayRole).toString(), static_cast<QString>(object_t(osm->ways[row + 1]).get_name(*osm)));
    QCOMPARE(model.index(row, MEMBER_COL_ROLE).data(Qt::UserRole).value<void *>(), r);
    for (int column = 0; column < MEMBER_NUM_COLS; column++)
      QVERIFY(model.index(row, column).flags() & Qt::ItemIsEnabled);
  }
  QCOMPARE(model.index(0, MEMBER_COL_ROLE).data(Qt::DisplayRole).toString(), QStringLiteral("from"));
  QCOMPARE(model.index(1, MEMBER_COL_ROLE).data(Qt::DisplayRole).toString(), QStringLiteral("via"));
  QCOMPARE(model.index(2, MEMBER_COL_ROLE).data(Qt::DisplayRole).toString(), QStringLiteral("to"));

  checkHeaderData(&model, expectedHeaderData(), Qt::Horizontal);
  checkHeaderDataEmpty(&model, Qt::Vertical);
}

void TestRelationMemberModel::refMembers()
{
  auto osm = boundedOsm();

  base_attributes ba(1);
  ba.version = 1;
  auto *r = new relation_t(ba);
  osm->insert(r);
  r->tags.replace({ tag_t("type", "restriction") });

  r->members.emplace_back(member_t(object_t(object_t::WAY_ID, 1), "from"));
  r->members.emplace_back(member_t(object_t(object_t::NODE_ID, 2), "via"));
  r->members.emplace_back(member_t(object_t(object_t::WAY_ID, 3), "to"));

  RelationMemberModel model(r, osm);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 3);

  for (int row = 0; row < 3; row++) {
    rowDataSame(model, row);
    if (row == 1)
      QCOMPARE(model.index(row, MEMBER_COL_TYPE).data(Qt::DisplayRole).toString(), static_cast<QString>(_("node id")));
    else
      QCOMPARE(model.index(row, MEMBER_COL_TYPE).data(Qt::DisplayRole).toString(), static_cast<QString>(_("way/area id")));
    QCOMPARE(model.index(row, MEMBER_COL_ID).data(Qt::DisplayRole).toInt(), row + 1);
    checkRowUnmodified(model, row);
    QCOMPARE(model.index(1, MEMBER_COL_NAME).data(Qt::DisplayRole), QVariant());
    QCOMPARE(model.index(row, MEMBER_COL_ROLE).data(Qt::UserRole).value<void *>(), r);
    for (int column = 0; column < MEMBER_NUM_COLS; column++)
      QVERIFY(!(model.index(row, column).flags() & Qt::ItemIsEnabled));
  }
  QCOMPARE(model.index(0, MEMBER_COL_ROLE).data(Qt::DisplayRole).toString(), QStringLiteral("from"));
  QCOMPARE(model.index(1, MEMBER_COL_ROLE).data(Qt::DisplayRole).toString(), QStringLiteral("via"));
  QCOMPARE(model.index(2, MEMBER_COL_ROLE).data(Qt::DisplayRole).toString(), QStringLiteral("to"));
}

void TestRelationMemberModel::moveRows()
{
  auto osm = boundedOsm();
  relation_t * const r = restrictionOsm(osm);

  RelationMemberModel model(r, osm);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 3);

  QCOMPARE(r->flags, 0u);
  QVERIFY(!model.commit());
  QCOMPARE(r->flags, 0u);

  // move the first row last
  QVERIFY(model.moveRows(QModelIndex(), 0, 1, QModelIndex(), 3));
  for (int row = 0; row < 3; row++) {
    rowDataSame(model, row);
    if (row == 0)
      QCOMPARE(model.index(row, MEMBER_COL_TYPE).data(Qt::DisplayRole).toString(), static_cast<QString>(_("node")));
    else
      QCOMPARE(model.index(row, MEMBER_COL_TYPE).data(Qt::DisplayRole).toString(), static_cast<QString>(_("way")));
    const int objectId = 1 + ((row + 1) % 3);
    QCOMPARE(model.index(row, MEMBER_COL_ID).data(Qt::DisplayRole).toInt(), objectId);
    if (row == 0) {
      QCOMPARE(*static_cast<object_t *>(model.index(0, MEMBER_COL_ID).data(Qt::UserRole).value<void *>()), object_t(osm->nodes[2]));
      QCOMPARE(model.index(0, MEMBER_COL_NAME).data(Qt::DisplayRole).toString(), static_cast<QString>(object_t(osm->nodes[2]).get_name(*osm)));
    } else {
      QCOMPARE(*static_cast<object_t *>(model.index(row, MEMBER_COL_ID).data(Qt::UserRole).value<void *>()), object_t(osm->ways[objectId]));
      QCOMPARE(model.index(row, MEMBER_COL_NAME).data(Qt::DisplayRole).toString(), static_cast<QString>(object_t(osm->ways[objectId]).get_name(*osm)));
    }
    checkCellModified(model, row, MEMBER_COL_ID);
    checkCellModified(model, row, MEMBER_COL_ROLE);
  }
  checkCellModified(model, 0, MEMBER_COL_TYPE);
  checkCellModified(model, 1, MEMBER_COL_TYPE);
  // this is a different member, but of the same type
  checkCellUnmodified(model, 2, MEMBER_COL_TYPE);
  QCOMPARE(model.index(2, MEMBER_COL_ROLE).data(Qt::DisplayRole).toString(), QStringLiteral("from"));
  QCOMPARE(model.index(0, MEMBER_COL_ROLE).data(Qt::DisplayRole).toString(), QStringLiteral("via"));
  QCOMPARE(model.index(1, MEMBER_COL_ROLE).data(Qt::DisplayRole).toString(), QStringLiteral("to"));

  QCOMPARE(r->flags, 0u);
  QCOMPARE(r->members.at(0).role, "from");
  QCOMPARE(r->members.at(1).role, "via");
  QCOMPARE(r->members.at(2).role, "to");

  QVERIFY(model.commit());

  QCOMPARE(r->flags, static_cast<unsigned int>(OSM_FLAG_DIRTY));
  QCOMPARE(r->members.at(0).role, "via");
  QCOMPARE(r->members.at(0).object, object_t(osm->nodes[2]));
  QCOMPARE(r->members.at(1).role, "to");
  QCOMPARE(r->members.at(1).object, object_t(osm->ways[3]));
  QCOMPARE(r->members.at(2).role, "from");
  QCOMPARE(r->members.at(2).object, object_t(osm->ways[1]));
}

void TestRelationMemberModel::changeRole()
{
  auto osm = boundedOsm();
  relation_t * const r = restrictionOsm(osm);

  RelationMemberModel model(r, osm);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 3);

  for (int row = 0; row < 3; row++) {
    auto idx = model.index(row, MEMBER_COL_ROLE);
    QVERIFY(model.setData(idx, QString::number(row), Qt::EditRole));
    QCOMPARE(model.data(idx, Qt::DisplayRole).toString(), QString::number(row));
    checkCellModified(model, row, MEMBER_COL_ROLE);
  }

  auto idx = model.index(1, MEMBER_COL_ROLE);
  QVERIFY(model.setData(idx, QVariant(), Qt::EditRole));
  QCOMPARE(model.data(idx, Qt::DisplayRole).toString(), QString());
  checkCellModified(model, 1, MEMBER_COL_ROLE);

  QCOMPARE(r->members.at(0).role, "from");
  QCOMPARE(r->members.at(1).role, "via");
  QCOMPARE(r->members.at(2).role, "to");

  QVERIFY(model.commit());

  QCOMPARE(r->members.at(0).role, "0");
  QCOMPARE(r->members.at(1).role, nullptr);
  QCOMPARE(r->members.at(2).role, "2");
}

QTEST_MAIN(TestRelationMemberModel)

#include "test_RelationMemberModel.moc"
