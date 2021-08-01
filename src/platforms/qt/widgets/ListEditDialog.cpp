/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ListEditDialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

ListEditDialog::ListEditDialog(QWidget *parent, Options buttons, bool sortable)
  : QDialog(parent)
  , view(new QTableView(this))
  , windowButtons(new QDialogButtonBox(this))
  , proxymodel(sortable ? new QSortFilterProxyModel(this) : nullptr)
{
  auto *ly = new QVBoxLayout(this);

  if (sortable) {
    view->setSortingEnabled(true);
    view->setModel(proxymodel);
    proxymodel->setSortCaseSensitivity(Qt::CaseInsensitive);
  }
  view->verticalHeader()->hide();
  view->setSelectionBehavior(QAbstractItemView::SelectRows);
  view->setSelectionMode(QAbstractItemView::SingleSelection);
  ly->insertWidget(0, view);

  auto *bbox = new QDialogButtonBox(this);
  ly->insertWidget(1, bbox);

  if (buttons & LIST_BUTTON_NEW) {
    auto btn = btnNew = bbox->addButton(tr("&New"), QDialogButtonBox::ActionRole);
    btn->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
  }
  if (buttons & LIST_BUTTON_EDIT) {
    btnEdit = bbox->addButton(tr("&Edit"), QDialogButtonBox::ActionRole);
    btnEdit->setEnabled(false);
  }
  if (buttons & LIST_BUTTON_REMOVE) {
    btnRemove = bbox->addButton(tr("&Remove"), QDialogButtonBox::ActionRole);
    btnRemove->setEnabled(false);
    btnRemove->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
  }
  if (buttons & LIST_BUTTON_USER0)
    btnUser0 = bbox->addButton(QString(), QDialogButtonBox::ActionRole);
  if (buttons & LIST_BUTTON_USER1)
    btnUser1 = bbox->addButton(QString(), QDialogButtonBox::ActionRole);
  if (buttons & LIST_BUTTON_USER2)
    btnUser2 = bbox->addButton(QString(), QDialogButtonBox::ActionRole);

  ly->insertWidget(2, windowButtons);
  connect(windowButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(windowButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
