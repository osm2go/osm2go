/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ProjectPropertiesDialog.h"

#include <appdata.h>
#include <map.h>
#include <notifications.h>
#include <osm_api.h>
#include <project_p.h>
#include <ProjectNameValidator.h>
#include <settings.h>
#include <wms.h>

#include <cassert>
#include <QDebug>
#include <QGeoRectangle>
#include <QLineEdit>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWidget>
#include <sys/stat.h>

#include <osm2go_i18n.h>
#include "osm2go_platform_qt.h"

ProjectPropertiesDialog::ProjectPropertiesDialog(appdata_t &a, project_t *p, bool n, const std::vector<std::unique_ptr<project_t>> &projects, QWidget *parent)
  : QDialog(parent)
  , appdata(a)
  , project(p)
  , is_new(n)
  , m_projects(projects)
{
  ui.setupUi(this);

  if(!project->desc.empty())
    ui.desc->setText(QString::fromStdString(project->desc));

  okBtn = ui.buttonBox->button(QDialogButtonBox::Ok);
  assert(okBtn != nullptr);

  if(!project->activeOrDirty(appdata))
    ui.diff_remove->setEnabled(false);

  projectFileSize();
  project_diffstat();
  setTitle();
  showBounds();

  connect(ui.diff_remove, &QPushButton::clicked, this, &ProjectPropertiesDialog::slotDiffRemoveClicked);
  connect(ui.download, &QPushButton::clicked, this, &ProjectPropertiesDialog::slotDownloadClicked);
  connect(ui.edit, &QPushButton::clicked, this, &ProjectPropertiesDialog::slotEditClicked);
  connect(ui.rename, &QPushButton::clicked, this, &ProjectPropertiesDialog::slotRenameClicked);

  // save the modified values when the ok button is clicked
  connect(okBtn, &QPushButton::clicked, this, [this]() {
    project->desc = ui.desc->text().toStdString();

#ifdef SERVER_EDITABLE
    project->adjustServer(server->text().toStdString()), settings_t::instance()->server);
#endif

    accept();
  });
}

void ProjectPropertiesDialog::projectFileSize()
{
  const project_t::projectStatus stat = project->status(is_new);

  ui.fsize->setText(stat.message);
  ui.fsizehdr->setText(stat.compressedMessage);
  okBtn->setEnabled(stat.valid);
  if (stat.errorColor) {
    auto pal = ui.fsize->palette();
    pal.setColor(QPalette::Text, osm2go_platform::invalid_text_color());
  }
}

void ProjectPropertiesDialog::project_diffstat()
{
  ui.diff_stat->setText(project->pendingChangesMessage(appdata));
}

void ProjectPropertiesDialog::setTitle()
{
  if(is_new)
    setWindowTitle(trstring("New project - %1").arg(project->name));
  else
    setWindowFilePath(trstring("Edit project - %1").arg(project->name));
}

void ProjectPropertiesDialog::showBounds()
{
  char lb[32], ub[32];
  pos_lat_str(lb, sizeof(lb), project->bounds.min.lat);
  pos_lat_str(ub, sizeof(ub), project->bounds.max.lat);
  ui.latLabel->setText(tr("%1 to %2").arg(QString::fromUtf8(lb), QString::fromUtf8(ub)));
  pos_lon_str(lb, sizeof(lb), project->bounds.min.lon);
  pos_lon_str(ub, sizeof(ub), project->bounds.max.lon);
  ui.lonLabel->setText(tr("%1 to %2").arg(QString::fromUtf8(lb), QString::fromUtf8(ub)));
  ui.download->setEnabled(project->bounds.valid());
}

void ProjectPropertiesDialog::slotEditClicked()
{
  if(project->activeOrDirty(appdata))
    message_dlg(trstring("Pending changes"),
                trstring("You have pending changes in this project.\n\nChanging "
                         "the area may cause pending changes to be "
                         "lost if they are outside the updated area."), this);

  osm2go_platform::DialogGuard dlg = new QDialog(this);
  auto *ly = new QVBoxLayout(dlg);
  auto *qv = new QQuickWidget(dlg);

  QVariantList obounds;
  obounds.reserve(m_projects.size());
  for (auto &&p : m_projects) {
    const auto b = p->bounds;
    if(b.valid())
      obounds << QVariant::fromValue(osm2go_platform::rectFromArea(b));
  }
  qv->rootContext()->setContextProperty("otherBounds", obounds);
  qv->setResizeMode(QQuickWidget::SizeRootObjectToView);

  qv->setSource(QStringLiteral("qrc:/AreaEdit.qml"));
  ly->addWidget(qv);

  settings_t::ref settings = settings_t::instance();

  auto *area_edit = qv->rootObject();
  area_edit->setProperty("initialArea", QVariant::fromValue(osm2go_platform::rectFromArea(project->bounds)));
  area_edit->setProperty("imperialUnits", settings->imperial_units ? 1 : 0);

  auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  ly->addWidget(bbox);
  connect(bbox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
  connect(bbox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

  osm2go_platform::dialog_size_hint(dlg, osm2go_platform::MISC_DIALOG_HIGH);

  if (dlg->exec() != QDialog::Accepted)
    return;

  QGeoRectangle rect = area_edit->property("selectedArea").value<QGeoRectangle>();
  auto imp = area_edit->property("imperialUnits");
  settings->imperial_units = imp.toInt() == 1;

  /* the wms layer isn't usable with new coordinates */
  wms_remove_file(*project);

  const auto nbounds = osm2go_platform::areaFromRect(rect);
  if (nbounds != project->bounds) {
    qDebug() << "coordinates changed to " << rect.topLeft() << rect.bottomRight();

    project->bounds = nbounds;
    bool pos_valid = project->bounds.valid();
    ui.download->setEnabled(pos_valid);

    showBounds();
    /* (re-) download area */
    if(pos_valid && osm_download(this, project))
      project->data_dirty = false;
    projectFileSize();
  }
}

void ProjectPropertiesDialog::slotRenameClicked()
{
  const std::string &name = project_name_dialog(this, m_projects, project->name);

  if(name.empty() || name == project->name)
    return;

  project_t::ref openProject = appdata.project;
  const bool isOpen = openProject && openProject->name == project->name;

  if(!project->rename(name, openProject, this))
    return;

  setTitle();

  if(isOpen)
    appdata.set_title();
}

void ProjectPropertiesDialog::slotDownloadClicked()
{
  if(osm_download(this, project))
    project->data_dirty = false;

  projectFileSize();
}

void ProjectPropertiesDialog::slotDiffRemoveClicked()
{
  if(osm2go_platform::yes_no(trstring("Discard changes?"),
                             trstring("Do you really want to discard your changes? This will permanently undo "
                                      "all changes you have made so far and which you did not upload yet."),
                             0, this)) {
    project->diff_remove_file();

    /* if this is the currently open project, we need to undo */
    /* the map changes as well */
    if(appdata.project && appdata.project->name == project->name) {
      qDebug() << "undo all on current project: delete map changes as well";

      /* just reload the map */
      appdata.map->clear(map_t::MAP_LAYER_OBJECTS_ONLY);
      appdata.project->parse_osm();
      appdata.map->paint();
    }

    /* update button/label state */
    project_diffstat();
    ui.diff_remove->setEnabled(false);
  }
}

/**
 * @brief query the user for a project name
 * @param parent the parent widget
 * @param oldname the current name of the project
 * @return the new project name
 * @retval std::string() the user cancelled the dialog
 *
 * This will prevent the user entering an invalid project name, which
 * includes a name that is already in use.
 */
std::string ProjectPropertiesDialog::project_name_dialog(QWidget *parent,
                                                         const std::vector<std::unique_ptr<project_t>> &projects,
                                                         const std::string &oldname)
{
  osm2go_platform::DialogGuard dlg(new QDialog(parent));
  dlg->setWindowTitle(trstring("Project name"));
  auto *ly = new QVBoxLayout(dlg);
  ly->addWidget(new QLabel(trstring("Name:")));
  auto *le = new QLineEdit(QString::fromStdString(oldname), dlg);
  ly->addWidget(le);
  le->setValidator(new ProjectNameValidator(projects, dlg));
  le->setClearButtonEnabled(true);

  auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
  ly->addWidget(bbox);
  connect(bbox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
  connect(bbox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

  auto *o = bbox->button(QDialogButtonBox::Ok);
  connect(le, &QLineEdit::textChanged, [o, le]() {
    o->setEnabled(le->hasAcceptableInput());
  });

  // whatever text is set initially is not valid, either collision or empty string
  QMetaObject::invokeMethod(o, "setEnabled", Qt::QueuedConnection, Q_ARG(bool, false));

  if (dlg->exec() == QDialog::Accepted)
    return le->text().toStdString();

  return std::string();
}
