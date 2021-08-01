/*
 * SPDX-FileCopyrightText: 2020-2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <object_dialogs.h>

#include <josm_presets.h>
#include "ListEditDialog.h"
#include <map.h>
#include "RelationMemberModel.h"
#include "RelationMemberRoleDelegate.h"
#include "RelationMembershipModel.h"
#include "RelationModel.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>
#include <set>
#include <string>
#include <strings.h>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include "osm2go_i18n.h"
#include "osm2go_platform.h"
#include "osm2go_platform_qt.h"

/* --------------- relation dialog for an item (node or way) ----------- */

namespace {

struct relitem_context_t {
  relitem_context_t(object_t &o, const presets_items *pr, osm_t::ref os, QWidget *parent);

  object_t &item;
  const presets_items * const presets;
  osm_t::ref osm;
  const osm2go_platform::DialogGuard dialog;
  QAbstractItemModel *store;
};

relitem_context_t::relitem_context_t(object_t &o, const presets_items *pr, osm_t::ref os, QWidget *parent)
  : item(o)
  , presets(pr)
  , osm(os)
  , dialog(new QDialog(parent))
  , store(nullptr)
{
}

struct relation_context_t {
  inline relation_context_t(map_t *m, osm_t::ref o, presets_items *p, QDialog *d)
    : map(m), osm(o), presets(p), dialog(d), store(new RelationModel(osm, d)) {}
  relation_context_t() = delete;
  relation_context_t(const relation_context_t &) = delete;
  relation_context_t(relation_context_t &&) = delete;
  relation_context_t &operator=(const relation_context_t&) = delete;
  relation_context_t &operator=(relation_context_t &&) = delete;

  map_t * const map;
  osm_t::ref osm;
  presets_items * const presets;
  const osm2go_platform::DialogGuard dialog;
  QPushButton *buttonSelect = nullptr;
  QPushButton *buttonMembers = nullptr;
  QPushButton *buttonRemove = nullptr;
  QPushButton *buttonEdit = nullptr;
  RelationModel * const store;
};

bool
relation_info_dialog(relation_context_t *context, relation_t *relation)
{
  object_t object(relation);
  return info_dialog(context->dialog, context->map, context->osm, context->presets, object);
}

QWidget *
relation_item_list_widget(relitem_context_t &context)
{
  auto *view = new QTableView(context.dialog);

  view->setItemDelegateForColumn(RELITEM_COL_ROLE, new RelationMemberRoleDelegate(context.presets, view));

  /* build and fill the store */
  auto *model = new RelationMembershipModel(context.osm, context.item, view);
  view->setModel(model);
  view->setSelectionMode(QAbstractItemView::NoSelection);

  // Debatable whether to sort by the "selected" or the "Name" column by
  // default. Both are be useful, in different ways.
  view->sortByColumn(RELITEM_COL_NAME, Qt::AscendingOrder);
  view->resizeColumnsToContents();
  view->verticalHeader()->hide();

  const int rc = model->rowCount(QModelIndex());
  for (int i = 0; i < rc; i++) {
    auto idx = model->index(i, RELITEM_COL_MEMBER);
    if (idx.data(Qt::CheckStateRole).value<Qt::CheckState>() == Qt::Checked) {
      view->scrollTo(idx);
      break;
    }
  }

  return view;
}

} // namespace

void relation_membership_dialog(osm2go_platform::Widget *parent, const presets_items *presets,
                                osm_t::ref osm, object_t &object)
{
  relitem_context_t context(object, presets, osm, parent);

  context.dialog->setWindowTitle(trstring("Relation memberships of %1 #%2").arg(object.type_string()).arg(object.get_id()));

  auto *ly = new QVBoxLayout(context.dialog);
  ly->addWidget(relation_item_list_widget(context));
  auto *bbox = new QDialogButtonBox(QDialogButtonBox::Close);
  ly->addWidget(bbox);
  QObject::connect(bbox, &QDialogButtonBox::rejected, context.dialog, &QDialog::reject);

  dialog_size_hint(context.dialog, osm2go_platform::MISC_DIALOG_LARGE);
  context.dialog->exec();
}

namespace {

/* -------------------- global relation list ----------------- */

/**
 * @brief get the currently selected relation
 * @param context the context pointer
 * @param index optional storage for the index in RelationModel
 */
relation_t *
get_selected_relation(relation_context_t *context, QModelIndex *index = nullptr)
{
  auto tw = context->dialog->findChild<QTableView *>();
  assert(tw != nullptr);

  const auto &idx = tw->selectionModel()->currentIndex();
  if (!idx.isValid())
    return nullptr;

  if (index != nullptr)
    *index = qobject_cast<QSortFilterProxyModel *>(tw->model())->mapToSource(idx);

  auto relation = idx.data(Qt::UserRole).value<void *>();
  assert(relation != nullptr);
  return static_cast<relation_t *>(relation);
}

void
relation_list_changed(relation_context_t &ctx, const QItemSelection &selected)
{
  bool hasMembers = !selected.isEmpty();
  for (auto btn : { ctx.buttonEdit, ctx.buttonRemove })
    btn->setEnabled(hasMembers);
  if (hasMembers) {
    auto relation = static_cast<relation_t *>(selected.indexes().first().data(Qt::UserRole).value<void *>());
    assert(relation != nullptr);
    hasMembers = !relation->members.empty();
  }
  for (auto btn : { ctx.buttonSelect, ctx.buttonMembers })
    btn->setEnabled(hasMembers);
}

} // namespace

bool
relation_show_members(QWidget *parent, relation_t *relation, osm_t::ref osm, const presets_items *presets)
{
  const char *str = relation->tags.get_value("name");
  if(str == nullptr)
    str = relation->tags.get_value("ref");

  const trstring &nstr = str == nullptr ?
                trstring("Members of relation #%1").arg(relation->id) :
                trstring("Members of relation \"%1\"").arg(str);

  osm2go_platform::DialogGuard dlg(new QDialog(parent));

  dlg->setWindowTitle(nstr);
  auto *ly = new QVBoxLayout();
  dlg->setLayout(ly);
  auto *view = new QTableView(dlg);
  ly->insertWidget(0, view);
  auto * const model = new RelationMemberModel(relation, osm, dlg);
  view->setModel(model);
  view->setItemDelegateForColumn(MEMBER_COL_ROLE, new RelationMemberRoleDelegate(presets, view));
  view->verticalHeader()->hide();
  view->setSelectionBehavior(QAbstractItemView::SelectRows);
  view->setSelectionMode(QAbstractItemView::SingleSelection);

  auto *bbox = new QDialogButtonBox(dlg);
  auto pbUp = bbox->addButton(trstring("Up"), QDialogButtonBox::ActionRole);
  auto pbDown = bbox->addButton(trstring("Down"), QDialogButtonBox::ActionRole);
  ly->insertWidget(1, bbox);

  auto selChanged = [pbUp, pbDown,
                     first = view->model()->index(0, 0),
                     last = view->model()->index(view->model()->rowCount() - 1, 0)](const auto &selected, const auto &) {
    pbUp->setEnabled(!selected.empty() && !selected.contains(first));
    pbDown->setEnabled(!selected.empty() && !selected.contains(last));
  };

  QObject::connect(pbUp, &QPushButton::clicked, [model = view->model(), sel = view->selectionModel(), selChanged] {
    int sr = sel->selectedRows().first().row();
    model->moveRow(QModelIndex(), sr, QModelIndex(), sr - 1);
    selChanged(sel->selection(), QItemSelection());
  });
  QObject::connect(pbDown, &QPushButton::clicked, [model = view->model(), sel = view->selectionModel(), selChanged] {
    int sr = sel->selectedRows().first().row();
    // the latter index gives the element this is now placed before, if that is 1 a row would be moved onto itself
    model->moveRow(QModelIndex(), sr, QModelIndex(), sr + 2);
    selChanged(sel->selection(), QItemSelection());
  });

  QObject::connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, selChanged);
  selChanged(QItemSelection(), QItemSelection());

  QObject::connect(bbox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
  bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
  ly->insertWidget(2, bbox);
  QObject::connect(bbox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
  QObject::connect(bbox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

  view->setMinimumSize(QSize(300, 150));
  view->resizeColumnsToContents();

  if (dlg->exec() == QDialog::Accepted)
    return model->commit();

  return false;
}

namespace {

/* user clicked "members" button in relation list */
void on_relation_members(relation_context_t *context)
{
  relation_t *sel = get_selected_relation(context);

  if(sel == nullptr)
    return;

  if (relation_show_members(context->dialog, sel, context->osm, context->presets))
    context->store->relationEdited(sel);
}

/* user clicked "select" button in relation list */
void on_relation_select(relation_context_t *context)
{
  relation_t *sel = get_selected_relation(context);
  context->map->item_deselect();

  if(sel == nullptr)
    return;

  context->map->select_relation(sel);

  /* tell dialog to close as we want to see the selected relation */
  context->dialog->close();
}

void on_relation_add(relation_context_t *context)
{
  /* create a new relation */
  auto relation = std::make_unique<relation_t>();
  if(relation_info_dialog(context, relation.get())) {
    relation_t *r = context->osm->attach(relation.release());

    /* append a row for the new data */
    QModelIndex sidx = context->store->addRelation(r);

    auto tw = context->dialog->findChild<QTableView *>();
    assert(tw != nullptr);

    QModelIndex proxyIndex = qobject_cast<QSortFilterProxyModel *>(tw->model())->mapFromSource(sidx);
    tw->selectionModel()->select(proxyIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
    tw->scrollTo(proxyIndex);
  }
}

/* user clicked "edit..." button in relation list */
void on_relation_edit(relation_context_t *context)
{
  QModelIndex idx;
  relation_t *sel = get_selected_relation(context, &idx);
  if(sel == nullptr)
    return;

  qDebug() << "edit relation #" << sel->id;

  if (!relation_info_dialog(context, sel))
    return;

  context->store->relationEdited(sel);
  QTableView *tw = context->dialog->findChild<QTableView *>();
  tw->scrollTo(qobject_cast<QSortFilterProxyModel *>(tw->model())->mapFromSource(idx));
}

/* remove the selected relation */
void on_relation_remove(relation_context_t *context)
{
  QModelIndex idx;
  relation_t *sel = get_selected_relation(context, &idx);
  if(sel == nullptr)
    return;

  qDebug() << "remove relation #" << sel->id;

  if(!sel->members.empty()) {
    const trstring msg(ngettext("This relation still has %n member. Delete it anyway?",
                                "This relation still has %n members. Delete it anyway?",
                                sel->members.size()), nullptr, sel->members.size());

    if(!osm2go_platform::yes_no(trstring("Delete non-empty relation?"), msg, 0, context->dialog))
      return;
  }

  /* first remove selected row from list */
  context->store->removeRow(idx.row(), idx.parent());

  /* then really delete it */
  context->osm->relation_delete(sel);
}

} // namespace

/* a global view on all relations */
void relation_list(osm2go_platform::Widget *parent, map_t *map, osm_t::ref osm, presets_items *presets)
{
  osm2go_platform::OwningPointer<ListEditDialog> dlg = new ListEditDialog(parent, LIST_BUTTON_NEW | LIST_BUTTON_EDIT | LIST_BUTTON_REMOVE | LIST_BUTTON_USER0 | LIST_BUTTON_USER1);
  relation_context_t context(map, osm, presets, dlg);

  dlg->setWindowTitle(trstring("All relations"));

  QObject::connect(dlg->btnNew, &QPushButton::clicked, [&context](){ on_relation_add(&context); });
  QObject::connect(dlg->btnEdit, &QPushButton::clicked, [&context](){ on_relation_edit(&context); });
  context.buttonEdit = dlg->btnEdit;
  QObject::connect(dlg->btnRemove, &QPushButton::clicked, [&context](){ on_relation_remove(&context); });
  context.buttonRemove = dlg->btnRemove;
  dlg->btnUser0->setText(trstring("&Members"));
  QObject::connect(dlg->btnUser0, &QPushButton::clicked, dlg, [&context](){ on_relation_members(&context); });
  context.buttonMembers = dlg->btnUser0;
  dlg->btnUser0->setEnabled(false);
  dlg->btnUser1->setText(trstring("&Select"));
  QObject::connect(dlg->btnUser1, &QPushButton::clicked, dlg, [&context](){ on_relation_select(&context); });
  context.buttonSelect = dlg->btnUser1;
  dlg->btnUser1->setEnabled(false);

  dlg->windowButtons->setStandardButtons(QDialogButtonBox::Close);

  dlg->proxymodel->setSourceModel(context.store);

  dlg->view->resizeColumnsToContents();
  QObject::connect(dlg->view->selectionModel(), &QItemSelectionModel::selectionChanged,
                   [&context](const QItemSelection &selected) {
                     relation_list_changed(context, selected);
                   });

  // Sorting by ref/name by default is useful for places with lots of numbered
  // bus routes. Especially for small screens.
  dlg->view->sortByColumn(RELATION_COL_NAME, Qt::AscendingOrder);
  dlg->view->setMinimumSize(QSize(300, 150));

  context.dialog->exec();
}
