/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "project_widgets.h"
#include "project_widgets_p.h"

#include <appdata.h>
#include "area_edit.h"
#include <diff.h>
#include <map.h>
#include <notifications.h>
#include <osm_api.h>
#include <project.h>
#include <project_p.h>
#include "ProjectPropertiesDialog.h"
#include "ProjectSelectModel.h"
#include <settings.h>
#include <wms.h>
#include <uicontrol.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <memory>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include <osm2go_cpp.h>
#include "osm2go_stl.h"
#include <osm2go_i18n.h>
#include "osm2go_platform.h"
#include "osm2go_platform_qt.h"

using namespace osm2go_platform;

static bool project_edit(QWidget *parent, appdata_t &appdata, ProjectSelectModel *model, project_t *project, bool is_new);

void ProjectDialog::slotRemoveProject()
{
  auto pr = selectedProject();
  assert(pr.project != nullptr);

  if(!yes_no(trstring("Delete project?"),
             trstring("Do you really want to delete the project \"%1\"?").arg(pr.project->name),
             0, this))
    return;

  /* check if we are to delete the currently open project */
  if(appdata.project && appdata.project->name == pr.project->name) {
    if(!yes_no(trstring("Delete current project?"),
               trstring("The project you are about to delete is the one you are "
                        "currently working on!\n\nDo you want to delete it anyway?"),
               0, this))
      return;

    project_close(appdata);
  }

  tableView->model()->removeRow(pr.row);
}

void ProjectDialog::slotEditProject()
{
  project_t *project = selectedProject().project;

  if(project_edit(this, appdata, model, project, false)) {
    /* check if we have actually editing the currently open project */
    if(appdata.project && appdata.project->name == project->name) {
      project_t *cur = appdata.project.get();

      qDebug() << "edited project was actually the active one!";

      /* update the currently active project also */

      /* update description */
      cur->desc = project->desc;

      // update OSM file, may have changed (gzip or not)
      cur->osmFile = project->osmFile;

      /* update server */
      cur->adjustServer(project->rserver.c_str(), settings_t::instance()->server);

      /* update coordinates */
      if(cur->bounds != project->bounds) {
        // save modified coordinates
        cur->bounds = project->bounds;

        /* if we have valid osm data loaded: save state first */
        if(cur->osm) {
          /* redraw the entire map by destroying all map items */
          cur->diff_save();
          appdata.map->clear(map_t::MAP_LAYER_ALL);
        }

        /* and load the (hopefully) new file */
        cur->parse_osm();
        diff_restore(appdata.project, appdata.uicontrol.get());
        appdata.map->paint();

        appdata.main_ui_enable();
      }
    }
  }

  /* enable/disable edit/remove buttons */
//   view_selected(context->dialog, project);
}

void ProjectDialog::slotUpdateAll()
{
  for (auto &&prj : model->m_projects) {
    /* if the project was already downloaded do it again */
    if(prj->osm_file_exists()) {
      qDebug() << "found" << QString::fromStdString(prj->name) << "to update";
      if (!osm_download(this, prj.get()))
        break;
    }
  }
}

ProjectDialog::sproj ProjectDialog::selectedProject()
{
  auto sel = tableView->selectionModel()->selectedIndexes();
  if (sel.isEmpty())
    return { nullptr, -1 };

  int r = fmodel->mapToSource(sel.constFirst()).row();

  return { projects[r].get(), sel.constFirst().row() };
}

void ProjectDialog::slotSelectionChanged()
{
  bool en = tableView->selectionModel()->hasSelection();
  deleteBtn->setEnabled(en);
  editBtn->setEnabled(en);
  auto prj = selectedProject().project;
  okBtn->setEnabled(en && prj != nullptr && prj->osm_file_exists());
}

void ProjectDialog::slotNewProject()
{
  const std::string name = ProjectPropertiesDialog::project_name_dialog(this, model->m_projects, std::string());

  if (name.empty())
    return;

  std::unique_ptr<project_t> project(project_t::create(name, settings_t::instance()->base_path, this));

  if(!project_edit(this, appdata, model, project.get(), true)) {
    qDebug() << "creation of  project" << QString::fromStdString(project->name) << "cancelled, deleting";
    project_delete(project);
    return;
  }

  auto idx = fmodel->mapFromSource(model->addProject(std::move(project)));
  tableView->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
  tableView->scrollTo(idx);
}

namespace {

std::vector<std::unique_ptr<project_t>>
project_scan_unique()
{
  auto projects = project_scan(settings_t::instance()->base_path,
                          settings_t::instance()->base_path_fd, settings_t::instance()->server);

  std::vector<std::unique_ptr<project_t>> ret;
  ret.reserve(projects.size());

  for (auto *p : projects)
    ret.emplace_back(p);

  return ret;
}

} //namespace

ProjectDialog::ProjectDialog(appdata_t &a)
  : QDialog()
  , appdata(a)
  , projects(project_scan_unique())
  , tableView(new QTableView(this))
  , model(new ProjectSelectModel(projects, appdata.project, tableView))
  , fmodel(new QSortFilterProxyModel(this))
{
  setWindowTitle(tr("Project selection"));
  setSizeGripEnabled(true);

  auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  auto newBtn = btnBox->addButton(tr("&New"), QDialogButtonBox::ActionRole);
  newBtn->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
  connect(newBtn, &QPushButton::clicked, this, &ProjectDialog::slotNewProject);
  editBtn = btnBox->addButton(tr("&Edit"), QDialogButtonBox::ActionRole);
  connect(editBtn, &QPushButton::clicked, this, &ProjectDialog::slotEditProject);
  deleteBtn = btnBox->addButton(tr("&Remove"), QDialogButtonBox::ActionRole);
  deleteBtn->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
  connect(deleteBtn, &QPushButton::clicked, this, &ProjectDialog::slotRemoveProject);
  updateBtn = btnBox->addButton(tr("&Update all"), QDialogButtonBox::ActionRole);
  connect(updateBtn, &QPushButton::clicked, this, &ProjectDialog::slotUpdateAll);
  connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  okBtn = btnBox->button(QDialogButtonBox::Ok);

  auto *hl = new QVBoxLayout(this);
  setLayout(hl);

  auto *filterWidget = new QWidget(this);
  auto *flayout = new QFormLayout(filterWidget);
  filterWidget->setLayout(flayout);
  auto *filterEdit = new QLineEdit(filterWidget);
  filterEdit->setClearButtonEnabled(true);
  flayout->addRow(tr("Filter:"), filterEdit);
  hl->addWidget(filterWidget);

  tableView->setSelectionMode(QAbstractItemView::SingleSelection);
  tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  tableView->horizontalHeader()->setStretchLastSection(true);
  tableView->verticalHeader()->hide();

  fmodel->setSourceModel(model);
  fmodel->setFilterCaseSensitivity(Qt::CaseInsensitive);
  fmodel->setSortCaseSensitivity(Qt::CaseInsensitive);

  connect(filterEdit, &QLineEdit::textEdited, fmodel, &QSortFilterProxyModel::setFilterFixedString);

  tableView->setModel(fmodel);
  tableView->setSortingEnabled(true);
  tableView->sortByColumn(0, Qt::AscendingOrder);

  QModelIndex activeIndex = model->activeProject();
  if (activeIndex.isValid()) {
    auto idx = fmodel->mapFromSource(activeIndex);
    tableView->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
    tableView->scrollTo(idx);
  }

  tableView->resizeColumnToContents(0);

  hl->addWidget(tableView);
  hl->addWidget(btnBox);

  connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ProjectDialog::slotSelectionChanged);
  slotSelectionChanged();

  dialog_size_hint(this, osm2go_platform::MISC_DIALOG_MEDIUM);
}

std::unique_ptr<project_t> project_select(appdata_t &appdata)
{
  osm2go_platform::OwningPointer<ProjectDialog> dialog(new ProjectDialog(appdata));

  if (dialog->exec() != QDialog::Accepted)
    return nullptr;

  project_t *pr = dialog->selectedProject().project; // TODO: take?
  assert(pr != nullptr);

  return std::make_unique<project_t>(*pr);
}

static bool __attribute__((nonnull(3, 4)))
project_edit(QWidget *parent, appdata_t &appdata, ProjectSelectModel *model, project_t *project, bool is_new)
{
  if(project->check_demo(parent))
    return false;

  osm2go_platform::OwningPointer<ProjectPropertiesDialog> dlg = new ProjectPropertiesDialog(appdata, project, is_new, model->m_projects, parent);

  bool ret = dlg->exec() == QDialog::Accepted;
  if (ret) {
    project->save(parent);
    model->refreshActiveProject();
  }

  return ret;
}
