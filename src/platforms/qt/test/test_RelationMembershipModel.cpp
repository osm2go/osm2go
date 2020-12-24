#include "../RelationMembershipModel.h"

#include "../../../test/dummy_appdata.h"

#include <josm_presets.h>
#include <osm.h>
#include <osm_objects.h>

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

class TestRelationMembershipModel : public QObject {
  Q_OBJECT

private slots:
  void noRelations();
  void notInRelations();
  void addToRelations();

  void changeRole();
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

} // namespace

void TestRelationMembershipModel::noRelations()
{
  auto osm = boundedOsm();
  node_t *n = osm->node_new(lpos_t(1, 1));
  osm->insert(n);

  RelationMembershipModel model(nullptr, osm, object_t(n));
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 0);
}

void TestRelationMembershipModel::notInRelations()
{
  auto osm = boundedOsm();
  node_t *n = osm->node_new(lpos_t(1, 1));
  osm->insert(n);

  const int relCount = 3;
  std::vector<relation_t *> rels;

  for (int i = 0; i < relCount; i++) {
    rels.push_back(new relation_t());
    osm->attach(rels.back());
  }

  rels.at(1)->tags.replace({ tag_t("type", "route") });

  RelationMembershipModel model(nullptr, osm, object_t(n));
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), relCount);

  for (int row = 0; row < relCount; row++) {
    for (int col : { RELITEM_COL_TYPE, RELITEM_COL_MEMBER, RELITEM_COL_ROLE, RELITEM_COL_NAME }) {
      // the relations are sorted by id, i.e. opposite to creation order
      const relation_t *rel = rels.at(relCount - row - 1);
      QCOMPARE(model.data(model.index(row, col), Qt::UserRole).value<void *>(), rel);

      for (int role : { Qt::DisplayRole, Qt::EditRole }) {
        QVariant data = model.data(model.index(row, col), role);
        if (col == RELITEM_COL_NAME)
          QCOMPARE(data.toString(), static_cast<QString>(rel->idName()));
        else if (row == 1 && col == RELITEM_COL_TYPE)
          QCOMPARE(data.toString(), QStringLiteral("route"));
        else
          QCOMPARE(data.toString(), QString());
      }
      auto idx = model.index(row, col);
      auto flags = model.flags(idx);
      int editableRole;

      if (col == RELITEM_COL_MEMBER) {
        QCOMPARE(model.data(idx, Qt::CheckStateRole).value<Qt::CheckState>(), Qt::Unchecked);
        QVERIFY(flags & Qt::ItemIsUserCheckable);
        editableRole = Qt::ItemIsEditable;
      } else {
        QCOMPARE(model.data(idx, Qt::CheckStateRole), QVariant());
        editableRole = col == RELITEM_COL_ROLE ? Qt::ItemIsEditable : 0;
      }
      QCOMPARE(flags & Qt::ItemIsEditable, editableRole);
      QVERIFY(!(flags & Qt::ItemIsUserTristate));
      QVERIFY(flags & Qt::ItemNeverHasChildren);
    }
  }
}

void TestRelationMembershipModel::addToRelations()
{
  auto osm = boundedOsm();
  node_t *n = osm->node_new(lpos_t(1, 1));
  osm->insert(n);

  const int relCount = 3;
  std::vector<relation_t *> rels;

  for (int i = 0; i < relCount; i++) {
    rels.push_back(new relation_t());
    osm->attach(rels.back());
  }

  RelationMembershipModel model(nullptr, osm, object_t(n));
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), relCount);

  // toggle all combinations, and also the toggle back to nothing changed
  for (unsigned int memberMask = 0; memberMask <= (1 << relCount); memberMask++) {
    for (int row = 0; row < relCount; row++) {
      const relation_t *rel = rels.at(relCount - row - 1);
      auto idx = model.index(row, RELITEM_COL_MEMBER);
      auto data = model.data(idx, Qt::CheckStateRole);
      const bool isMember = (memberMask & (1 << row));
      QVariant targetData = isMember ? Qt::Checked : Qt::Unchecked;
      if (data != targetData)
        QVERIFY(model.setData(idx, targetData, Qt::CheckStateRole));
      QVERIFY(!model.setData(idx, QVariant(), Qt::EditRole));
      if (isMember) {
        QCOMPARE(rel->members.size(), static_cast<size_t>(1));
        QVERIFY(rel->members.front().object == n);
      } else {
        QCOMPARE(rel->members.size(), static_cast<size_t>(0));
      }
    }
  }

  for (auto *rel : rels)
    QCOMPARE(rel->members.size(), static_cast<size_t>(0));
}

void TestRelationMembershipModel::changeRole()
{
  auto osm = boundedOsm();
  node_t *n = osm->node_new(lpos_t(1, 1));
  osm->insert(n);

  relation_t *rel = new relation_t();
  osm->attach(rel);
  rel->members.push_back(member_t(object_t(n)));

  RelationMembershipModel model(nullptr, osm, object_t(n));
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 1);

  auto idx = model.index(0, RELITEM_COL_ROLE);

  const QString customText = QStringLiteral("custom");
  QVERIFY(model.setData(idx, customText, Qt::EditRole));

  QCOMPARE(QLatin1String(rel->members.front().role), customText);
  QCOMPARE(model.data(idx, Qt::DisplayRole).toString(), customText);
  QCOMPARE(model.data(idx, Qt::EditRole).toString(), customText);

  QVERIFY(model.setData(idx, QVariant(), Qt::EditRole));

  QCOMPARE(rel->members.front().role, nullptr);
}

QTEST_MAIN(TestRelationMembershipModel)

#include "test_RelationMembershipModel.moc"
