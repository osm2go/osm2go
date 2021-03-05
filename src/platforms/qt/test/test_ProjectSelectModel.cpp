#include "../ProjectSelectModel.h"

#include "../../../test/dummy_appdata.h"
#include "helper.h"

#include <project.h>
#include <osm2go_platform_qt.h>

#include <cerrno>
#include <fcntl.h>
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
#include <QAbstractItemModelTester>
#else
class QAbstractItemModelTester {
public:
  inline QAbstractItemModelTester(QAbstractItemModel *) {}
};
#endif
#include <QIcon>
#include <QTemporaryDir>
#include <QTest>
#include <unistd.h>

class TestProjectSelectModel : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();

  void emptyList();
  void activeProject();
  void activeProject_data();
  void addProject();
  void deleteProject();

private:
  QTemporaryDir m_tempdir;
  std::string m_tempdirString;

  std::vector<std::unique_ptr<project_t>> createProjects();

public:
  static constexpr unsigned int projectCount() { return 3; }
};

namespace {

std::vector<QString> expectedHeaderData()
{
  return { QStringLiteral("Name"), QStringLiteral("Description") };
}

void checkProjects(const ProjectSelectModel &model, const std::vector<std::unique_ptr<project_t>> &projects, int activeRow = -1)
{
  QCOMPARE(model.rowCount(QModelIndex()), static_cast<int>(projects.size()));

  for (unsigned int row = 0; row < projects.size(); row++) {
    const std::unique_ptr<project_t> &project = projects.at(row);
    for (int col : { ProjectSelectModel::PROJECT_COL_NAME, ProjectSelectModel::PROJECT_COL_DESCRIPTION }) {
      auto idx = model.index(row, col);

      QCOMPARE(idx.data(Qt::FontRole), QVariant());
      QCOMPARE(idx.data(Qt::ToolTipRole), QVariant());


      QVariant data = model.data(idx, Qt::DisplayRole);
      if (col == ProjectSelectModel::PROJECT_COL_NAME)
        QCOMPARE(data.toString(), QString::fromStdString(project->name));
      else if (row == 1 && col == ProjectSelectModel::PROJECT_COL_DESCRIPTION)
        QCOMPARE(data.toString(), QStringLiteral("a project description"));
      else
        QCOMPARE(data.toString(), QString());

      auto flags = model.flags(idx);

      QVERIFY(!(flags & Qt::ItemIsEditable));
      QVERIFY(!(flags & Qt::ItemIsUserCheckable));
      QVERIFY(!(flags & Qt::ItemIsUserTristate));
      QVERIFY(flags & Qt::ItemNeverHasChildren);
    }
    QCOMPARE(model.index(row, ProjectSelectModel::PROJECT_COL_DESCRIPTION).data(Qt::DecorationRole), QVariant());
    if (static_cast<int>(row) == activeRow)
      QCOMPARE(model.index(row, ProjectSelectModel::PROJECT_COL_NAME).data(Qt::DecorationRole).value<QIcon>(), QIcon::fromTheme(QStringLiteral("document-open")));
    else {
      switch (row) {
      case 0:
        QCOMPARE(model.index(row, ProjectSelectModel::PROJECT_COL_NAME).data(Qt::DecorationRole).value<QIcon>(), QIcon::fromTheme(QStringLiteral("text-x-generic")));
        break;
      case 2:
        QCOMPARE(model.index(row, ProjectSelectModel::PROJECT_COL_NAME).data(Qt::DecorationRole).value<QIcon>(), QIcon::fromTheme(QStringLiteral("document-properties")));
        break;
      default:
        QCOMPARE(model.index(row, ProjectSelectModel::PROJECT_COL_NAME).data(Qt::DecorationRole).value<QIcon>(), QIcon::fromTheme(QStringLiteral("dialog-warning")));
      }
    }
  }

  checkHeaderData(&model, expectedHeaderData(), Qt::Horizontal);
  checkHeaderDataEmpty(&model, Qt::Vertical);

  if (activeRow < 0)
    QCOMPARE(model.activeProject(), QModelIndex());
  else
    QCOMPARE(model.activeProject().row(), activeRow);
}

} // namespace

std::vector<std::unique_ptr<project_t>> TestProjectSelectModel::createProjects()
{
  std::vector<std::unique_ptr<project_t>> projects;

  for (unsigned int i = 0; i < projectCount(); i++) {
    std::string name = "project #" + std::to_string(i + 1);
    projects.emplace_back(std::make_unique<project_t>(name, m_tempdirString));
    projects.at(i)->osmFile = projects.at(i)->name + ".osm.gz";
  }

  // customize the projects
  projects.at(0)->save();
  int fd = ::openat(projects.at(0)->dirfd, projects.at(0)->osmFile.c_str(), O_CREAT | O_TRUNC, 0644);
  assert(fd >= 0);
  close(fd);
  projects.at(1)->desc = "a project description";
  projects.at(2)->save();
  fd = ::openat(projects.at(2)->dirfd, projects.at(2)->osmFile.c_str(), O_CREAT | O_TRUNC, 0644);
  assert(fd >= 0);
  close(fd);
  const std::string diffName = projects.at(2)->name + ".diff";
  fd = ::openat(projects.at(2)->dirfd, diffName.c_str(), O_CREAT | O_TRUNC, 0644);
  assert(fd >= 0);
  close(fd);

  return projects;
}

void TestProjectSelectModel::initTestCase()
{
  m_tempdirString = m_tempdir.path().toStdString() + '/';
}

void TestProjectSelectModel::emptyList()
{
  std::unique_ptr<project_t> current;
  std::vector<std::unique_ptr<project_t>> projects;

  ProjectSelectModel model(projects, current);
  QAbstractItemModelTester mt(&model);

  QCOMPARE(model.rowCount(QModelIndex()), 0);
}

void TestProjectSelectModel::activeProject()
{
  QFETCH(int, activeRow);

  std::unique_ptr<project_t> empty;
  std::vector<std::unique_ptr<project_t>> projects = createProjects();

  ProjectSelectModel model(projects, activeRow >= 0 ? projects.at(activeRow) : empty);
  QAbstractItemModelTester mt(&model);

  checkProjects(model, projects, activeRow);
}

void TestProjectSelectModel::activeProject_data()
{
  QTest::addColumn<int>("activeRow");

  QTest::newRow("no active project") << -1;
  QTest::newRow("project 0") << 0;
  QTest::newRow("project 1") << 1;
  QTest::newRow("project 2") << 2;
}

void TestProjectSelectModel::addProject()
{
  std::unique_ptr<project_t> current;
  std::vector<std::unique_ptr<project_t>> projects = createProjects();

  ProjectSelectModel model(projects, current);
  QAbstractItemModelTester mt(&model);

  checkProjects(model, projects);

  auto newPrj = std::make_unique<project_t>("newProject", m_tempdirString);
  model.addProject(std::move(newPrj));

  QCOMPARE(projects.size(), static_cast<size_t>(projectCount()) + 1);

  checkProjects(model, projects);

  QCOMPARE(projects.back()->name, std::string("newProject"));
}

void TestProjectSelectModel::deleteProject()
{
  std::unique_ptr<project_t> current;
  std::vector<std::unique_ptr<project_t>> projects = createProjects();

  ProjectSelectModel model(projects, current);
  QAbstractItemModelTester mt(&model);

  checkProjects(model, projects);

  for (int i = projects.size() - 1; i >= 0; i--) {
    model.removeRow(i, QModelIndex());
    QCOMPARE(model.rowCount(QModelIndex()), i);
    QCOMPARE(projects.size(), static_cast<size_t>(i));
    // check that the other data is unchanged
    checkProjects(model, projects);
  }
}

QTEST_MAIN(TestProjectSelectModel)

#include "test_ProjectSelectModel.moc"
