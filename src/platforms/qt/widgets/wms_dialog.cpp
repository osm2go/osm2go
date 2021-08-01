/*
 * SPDX-FileCopyrightText: 2020-2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <wms.h>
#include <wms_p.h>

#include "ListEditDialog.h"
#include <map.h>
#include <misc.h>
#include <project.h>
#include <settings.h>
#include <uicontrol.h>
#include <UrlValidator.h>
#include <WmsModel.h>
#include <WmsNameValidator.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QItemSelectionModel>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QTableView>
#include <QVBoxLayout>
#include <strings.h>

#include <osm2go_annotations.h>
#include <osm2go_stl.h>
#include <osm2go_i18n.h>
#include "osm2go_platform.h"
#include "osm2go_platform_qt.h"

namespace {

const wms_server_t *
select_server(const std::string &wms_server, QTableView *view)
{
  auto model = qobject_cast<const WmsModel *>(view->model());
  assert(model != nullptr);
  int i = model->indexOfServer(wms_server);
  if(i < 0)
    return nullptr;

  const auto idx = model->index(i, 0);
  /* if the projects settings match a list entry, then select this */
  auto srv = static_cast<const wms_server_t *>(idx.data(Qt::UserRole).value<void *>());
  assert(srv != nullptr);

  view->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
  return srv;
}

void
wms_server_selected(ListEditDialog *dlg, QLabel *slabel, const std::string &wms_server, const wms_server_t *selected)
{
  dlg->btnRemove->setEnabled(selected != nullptr);
  dlg->btnEdit->setEnabled(selected != nullptr);

  /* user can click ok if a entry is selected or if both fields are */
  /* otherwise valid */
  const std::string &txt = selected != nullptr ? selected->server : wms_server;
  slabel->setText(QString::fromStdString(txt));

  dlg->windowButtons->button(QDialogButtonBox::Ok)->setEnabled(!txt.empty());
}

void
on_server_remove(ListEditDialog *dlg, QLabel *slabel, const std::string &wms_server)
{
  auto sel = dlg->view->selectionModel()->selectedRows();

  assert_cmpnum(sel.count(), 1); // SingleSelection
  dlg->view->model()->removeRow(sel.front().row());

  wms_server_selected(dlg, slabel, wms_server, select_server(wms_server, dlg->view));
}

/* edit url and path of a given wms server entry */
bool
wms_server_edit(QWidget *parent, bool edit_name, wms_server_t *wms_server, const WmsModel *model)
{
  const QString oldname = QString::fromStdString(wms_server->name);
  const QString oldvalue = QString::fromStdString(wms_server->server);

  osm2go_platform::DialogGuard dlg(new QDialog(parent));
  dlg->setWindowTitle(trstring("Edit WMS Server"));
  auto *ly = new QFormLayout(dlg);
  auto *nameEdit = new QLineEdit(oldname);
  nameEdit->setReadOnly(!edit_name);
  if (edit_name)
    nameEdit->setValidator(new WmsNameValidator(oldname, model, nameEdit));
  nameEdit->setClearButtonEnabled(true);
  nameEdit->setPlaceholderText(trstring("<service name>"));
  ly->addRow(trstring("Name:"), nameEdit);

  auto *urlEdit = new QLineEdit(oldvalue);
  urlEdit->setValidator(new UrlValidator(oldvalue, urlEdit));
  urlEdit->setClearButtonEnabled(true);
  urlEdit->setPlaceholderText(trstring("<server url>"));
  ly->addRow(trstring("Server:"), urlEdit);

  auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
  ly->addWidget(bbox);
  QObject::connect(bbox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
  QObject::connect(bbox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

  auto o = bbox->button(QDialogButtonBox::Ok);
  auto switchEn = [o, nameEdit, urlEdit]() {
    o->setEnabled(nameEdit->hasAcceptableInput() && urlEdit->hasAcceptableInput());
  };
  QObject::connect(nameEdit, &QLineEdit::textChanged, switchEn);
  QObject::connect(urlEdit, &QLineEdit::textChanged, switchEn);

  // set initial button state
  switchEn();

  osm2go_platform::dialog_size_hint(dlg, osm2go_platform::MISC_DIALOG_WIDE);

  bool ret = dlg->exec() == QDialog::Accepted;
  if (ret) {
    if (edit_name)
      wms_server->name = nameEdit->text().toStdString();
    wms_server->server = urlEdit->text().toStdString();

    qDebug() << "setting URL for WMS server" << nameEdit->text() << "to" << urlEdit->text();
  }

  return ret;
}

/* user clicked "edit..." button in the wms server list */
void
on_server_edit(WmsModel *model, ListEditDialog *dlg, QLabel *slabel)
{
  auto sel = dlg->view->selectionModel()->selectedRows();

  assert_cmpnum(sel.count(), 1); // SingleSelection

  auto server = static_cast<wms_server_t *>(sel.front().data(Qt::UserRole).value<void *>());
  assert(server != nullptr);

  if (wms_server_edit(dlg, false, server, model))
    // just update the label, the server name was not changed and that is all the model cares for
    wms_server_selected(dlg, slabel, std::string(), server);
}

/* user clicked "add..." button in the wms server list */
void
on_server_add(const std::string &wms_server, WmsModel *model, ListEditDialog *dlg, QLabel *slabel)
{
  auto newserver = std::make_unique<wms_server_t>();
  // in case the project has a server set, but the global list is empty,
  // fill the data of the project server
  if(settings_t::instance()->wms_server.empty() && !wms_server.empty())
    newserver->server = wms_server;

  if(wms_server_edit(dlg, true, newserver.get(), model)) {
    /* attach a new server item to the chain */
    wms_server_selected(dlg, slabel, std::string(), model->addServer(std::move(newserver)));
  }
}

} // namespace

std::string
wms_server_dialog(osm2go_platform::Widget *parent, const std::string &wms_server)
{
  osm2go_platform::OwningPointer<ListEditDialog> dlg(new ListEditDialog(parent,
                                                  LIST_BUTTON_NEW | LIST_BUTTON_EDIT | LIST_BUTTON_REMOVE));
  dlg->setWindowTitle(trstring("WMS Server Selection"));
  dlg->windowButtons->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);

  auto *fly = new QFormLayout();
  auto *slabel = new QLabel(QLatin1String("foobar"));
  fly->addRow(trstring("Server:"), slabel);
  qobject_cast<QVBoxLayout *>(dlg->layout())->insertLayout(2, fly);
  dlg->view->setSelectionMode(QAbstractItemView::SingleSelection);
  QObject::connect(dlg->view->selectionModel(), &QItemSelectionModel::selectionChanged,
               [&dlg, slabel, &wms_server, md = dlg->view->selectionModel()]() {
                 if (md->hasSelection())
                   wms_server_selected(dlg, slabel, wms_server,
                                       static_cast<wms_server_t *>(md->selectedRows().front().data(Qt::UserRole).value<void *>()));
              });

  osm2go_platform::dialog_size_hint(dlg, osm2go_platform::MISC_DIALOG_MEDIUM);

  auto *model = new WmsModel(settings_t::instance(), dlg);
  dlg->proxymodel->setSourceModel(model);

  dlg->view->horizontalHeader()->setStretchLastSection(true);

  QObject::connect(dlg->btnNew, &QPushButton::clicked, [&wms_server, &dlg, model, slabel]() {
    on_server_add(wms_server, model, dlg, slabel); });
  QObject::connect(dlg->btnEdit, &QPushButton::clicked, [&dlg, model, slabel]() {
    on_server_edit(model, dlg, slabel); });
  QObject::connect(dlg->btnRemove, &QPushButton::clicked, [&dlg, slabel, &wms_server]() {
    on_server_remove(dlg, slabel, wms_server); });

  std::string ret;
  if (dlg->exec() == QDialog::Accepted) {
    const auto &s = dlg->view->selectionModel()->selectedRows();
    if(!s.isEmpty()) {
      /* fetch parameters from selected entry */
      ret = static_cast<const wms_server_t *>(s.front().data(Qt::UserRole).value<void *>())->server;
    } else {
      ret = wms_server;
    }
  }

  return ret;
}

std::string
wms_layer_dialog(osm2go_platform::Widget *parent, const pos_area &bounds, const wms_layer_t::list &layers)
{
  osm2go_platform::OwningPointer<ListEditDialog> dlg(new ListEditDialog(parent, ListEditDialog::Options()));
  dlg->setWindowTitle(trstring("WMS layer selection"));
  dlg->windowButtons->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
  auto okbtn = dlg->windowButtons->button(QDialogButtonBox::Ok);
  okbtn->setEnabled(false);

  QStringList layersName;
  for (auto &&layer : layers) {
    if(layer.llbbox.valid && wms_llbbox_fits(bounds, layer.llbbox))
      layersName << QString::fromStdString(layer.title);
  }

  dlg->proxymodel->setSourceModel(new QStringListModel(layersName, dlg));
  QObject::connect(dlg->view->selectionModel(), &QItemSelectionModel::selectionChanged,
               [okbtn, md = dlg->view->selectionModel()]() {
                 okbtn->setEnabled(md->hasSelection());
              });

  dlg->view->setSelectionMode(QAbstractItemView::ExtendedSelection);
  dlg->view->horizontalHeader()->setStretchLastSection(true);
  dlg->view->horizontalHeader()->hide();
  osm2go_platform::dialog_size_hint(dlg, osm2go_platform::MISC_DIALOG_MEDIUM);

  std::string str;
  if (dlg->exec() == QDialog::Accepted) {
    const auto &sel = dlg->view->selectionModel()->selectedRows();

    auto name_for_sel = [&layers, &dlg](const auto s) {
      return layers.at(dlg->proxymodel->mapToSource(s).row()).name;
    };

    str = std::accumulate(std::next(sel.cbegin()), sel.cend(),
                          name_for_sel(sel.front()),
                          [name_for_sel](const auto &s, auto r) {
                            return s + ',' + name_for_sel(r);
                          });
  }

  return str;
}
