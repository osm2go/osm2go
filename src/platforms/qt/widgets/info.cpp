/*
 * SPDX-FileCopyrightText: 2020-2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "info_p.h"

#include "ListEditDialog.h"
#include <map.h>
#include <object_dialogs.h>
#include <pos.h>
#include "TagModel.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStringBuilder>
#include <QTableView>
#include <QTimeZone>
#include <QVBoxLayout>
#include <strings.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>

class QAbstractItemModel;

namespace {

enum {
  TAG_COL_KEY = 0,
  TAG_COL_VALUE,
  TAG_COL_COLLISION,
  TAG_NUM_COLS
};

class info_tag_context_t : public tag_context_t {
public:
  info_tag_context_t(map_t *m, osm_t::ref os, const object_t &o, ListEditDialog *dlg, TagModel *md, presets_items *ps);

  map_t * const map;
  osm_t::ref osm;
  TagModel * const store;
  presets_items * const presets;

  inline QTableView *view()
  { return static_cast<ListEditDialog *>(dialog.data())->view; }
  inline QModelIndex selection()
  { return view()->selectionModel()->currentIndex(); }
  void select(const QModelIndex &index);
  inline void selectSource(const QModelIndex &index)
  { select(static_cast<ListEditDialog *>(dialog.data())->proxymodel->mapFromSource(index)); }
};

void
info_tag_context_t::select(const QModelIndex &index)
{
  auto *dlg = static_cast<ListEditDialog *>(dialog.data());

  dlg->view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);

  dlg->view->scrollTo(index);
}

void
changed(ListEditDialog *dlg, const QItemSelection &selection)
{
  // the selection may contain only empty ranges, one cannot use selection.isEmpty()
  const auto idxs = selection.indexes();
  bool selected = !selection.isEmpty() && !idxs.isEmpty();

  if(selected) {
    auto idx = dlg->proxymodel->mapToSource(idxs.first());
    if (!(idx.flags() & Qt::ItemIsSelectable))
      selected = false;
  }

  dlg->btnRemove->setEnabled(selected);
}

void
on_tag_remove(info_tag_context_t &context)
{
  auto sel = context.selection();

  if (unlikely(!sel.isValid()))
    return;

  auto next = sel.sibling(sel.row() + 1, sel.column());
  if (!next.isValid())
    next = sel.sibling(sel.row() - 1, sel.column());
  else
    // the next one is valid, but when this row is removed next is the current one
    next = sel;
  context.view()->model()->removeRow(sel.row());

  if (next.isValid())
    context.select(next);
}

/**
 * @brief request user input for the given tag
 * @param window the parent window
 * @param k the key
 * @param v the value
 * @return if the tag was actually modified
 * @retval false the tag is the same as before
 */
bool
tag_edit(QWidget *window, QString &k, QString &v, const osm_t::TagMap &tags)
{
  osm2go_platform::DialogGuard dlg(new QDialog(window));
  dlg->setWindowTitle(trstring("Add Tag"));

  auto *ly = new QVBoxLayout(dlg);

  auto *fly = new QFormLayout();
  ly->addLayout(fly);
  auto *kedit = new QLineEdit(dlg);
  fly->addRow(trstring("Key:"), kedit);
  auto *vedit = new QLineEdit(dlg);
  fly->addRow(trstring("Value:"), vedit);

  auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
  ly->addWidget(bbox);
  QObject::connect(bbox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
  QObject::connect(bbox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

  auto *okbtn = bbox->button(QDialogButtonBox::Ok);
  auto validEntry = [kedit, vedit, tags, okbtn]() {
    bool valid = !kedit->text().isEmpty() && !vedit->text().isEmpty();
    QString tooltip;
    if (valid) {
      // prevent insertion of duplicate keys
      const std::string kstr = kedit->text().toStdString();
      if (tag_t::is_discardable(kstr.c_str())) {
        valid = false;
        tooltip = trstring("This tag is considered deprecated and should not be added to objects anymore.");
      } else {
        valid = std::none_of(tags.begin(), tags.end(), [&kstr](const auto &p) {
                             return p.first == kstr; } );
      }
    }
    kedit->setToolTip(tooltip);
    okbtn->setEnabled(valid);
  };
  okbtn->setEnabled(false);

  QObject::connect(vedit, &QLineEdit::textChanged, validEntry);
  QObject::connect(kedit, &QLineEdit::textChanged, validEntry);

  bool ret = dlg->exec() == QDialog::Accepted;
  if (ret) {
    k = kedit->text();
    v = vedit->text();
  }

  return ret;
}

bool
replace_with_last(const info_tag_context_t &context, const osm_t::TagMap &ntags)
{
  // if the new object has no tags replacing is always permitted
  if(context.tags.empty())
    return true;

  // if all tags of the object are part of the new tag list no information will be lost
  if(osm_t::tagSubset(context.tags, ntags))
    return true;

  return osm2go_platform::yes_no(trstring("Overwrite tags?"),
                trstring("This will overwrite all tags of this %1 with the ones from "
                         "the %1 selected last.\n\nDo you really want this?").arg(context.object.type_string()),
                MISC_AGAIN_ID_OVERWRITE_TAGS, context.dialog);
}

void
on_tag_last(info_tag_context_t &context)
{
  const osm_t::TagMap &ntags = context.object.type == object_t::NODE ?
                               context.map->last_node_tags :
                               context.map->last_way_tags;

  if(!replace_with_last(context, ntags))
    return;

  context.info_tags_replace(ntags);

  // Adding those tags above will usually make the first of the newly
  // added tags selected. Enable edit/remove buttons now.
  auto *dlg = static_cast<ListEditDialog *>(context.dialog.data());
  changed(dlg, dlg->view->selectionModel()->selection());
}

void
on_tag_add(info_tag_context_t &context)
{
  QString k, v;

  if(!tag_edit(context.dialog, k, v, context.store->tags())) {
    qDebug() << "cancelled";
    return;
  }

  auto index = context.store->addTag(k, v);
  qDebug() << index;
  context.selectSource(index);
}

QLayout *
details_widget(const info_tag_context_t &context)
{
  auto *table = new QFormLayout();
  const auto &users = context.osm->users;
  const base_object_t * const obj = static_cast<base_object_t *>(context.object);

  if (const auto userIt = users.find(obj->user); likely(userIt != users.end()))
    table->addRow(trstring("User:"), new QLabel(QString::fromStdString(userIt->second), context.dialog));

  /* ------------ time ----------------- */

  QString tstr;
  if(obj->time > 0) {
    const QDateTime tm = QDateTime::fromSecsSinceEpoch(obj->time, QTimeZone::utc());
    tstr = QLocale::system().toString(tm, QLocale::ShortFormat);
  } else {
    tstr = trstring("Not yet uploaded");
  }

  table->addRow(trstring("Date/Time:"), new QLabel(tstr));

  /* ------------ coordinate (only for nodes) ----------------- */
  switch(context.object.type) {
  case object_t::NODE: {
    char pos_str[32];
    const pos_t pos = static_cast<node_t *>(context.object)->pos;
    pos_lat_str(pos_str, sizeof(pos_str), pos.lat);
    table->addRow(trstring("Latitude:"), new QLabel(QString::fromUtf8(pos_str)));
    pos_lon_str(pos_str, sizeof(pos_str), pos.lon);
    table->addRow(trstring("Longitude:"), new QLabel(QString::fromUtf8(pos_str)));
  } break;

  case object_t::WAY: {
    way_t * const way = static_cast<way_t *>(context.object);
    size_t ncount = way->node_chain.size();
    QString msg = trstring(ngettext("%n node", "%n nodes", ncount), nullptr, ncount);
    table->addRow(trstring("Length:"), new QLabel(msg));

    msg = trstring("%1 (%2)").arg(way->is_closed() ? "closed way" : "open way")
                             .arg((way->draw.flags & OSM_DRAW_FLAG_AREA) ? "area" : "line");
    table->addRow(trstring("Type:"), new QLabel(msg));
  } break;

  case object_t::RELATION: {
    /* relations tell something about their members */
    relation_t * const rel = static_cast<relation_t *>(context.object);
    auto counts = rel->members_by_type();

    const auto msg = trstring("Members: %1 nodes, %2 ways, %3 relations") .arg(counts.nodes).arg(counts.ways).arg(counts.relations);
    auto *label = new QLabel(QLatin1String("<a href=\"\">") % static_cast<QString>(msg) % QLatin1String("</a>"));
    table->addRow(trstring("Members:"), label);
    QObject::connect(label, &QLabel::linkActivated, [&context, rel]() {
      relation_show_members(context.dialog, rel, context.osm, context.presets);
    });
    break;
  }

  default:
    assert_unreachable();
  }

  return table;
}

} // namespace

void
tag_context_t::info_tags_replace(const osm_t::TagMap &ntags)
{
  static_cast<info_tag_context_t *>(this)->store->replaceTags(ntags);
}

/* edit tags of currently selected node or way or of the relation */
/* given */
bool
info_dialog(osm2go_platform::Widget *parent, map_t *map, osm_t::ref osm, presets_items *presets, object_t &object)
{
  assert(object.is_real());

  trstring msgtpl;

  switch(object.type) {
  case object_t::NODE:
    msgtpl = trstring("Node #%1");
    break;

  case object_t::WAY:
    msgtpl = trstring("Way #%1");
    break;

  case object_t::RELATION:
    msgtpl = trstring("Relation #%1");
    break;

  default:
    assert_unreachable();
  }

  trstring str = msgtpl.arg(object.get_id());
  osm2go_platform::OwningPointer<ListEditDialog> dlg = new ListEditDialog(parent, LIST_BUTTON_NEW | LIST_BUTTON_REMOVE | LIST_BUTTON_USER0 | LIST_BUTTON_USER1 | LIST_BUTTON_USER2);
  const auto *original = osm->originalObject(object);
  auto *model = new TagModel(dlg, object, original);
  info_tag_context_t context(map, osm, object, dlg, model, presets);
  dlg->setWindowTitle(str);
  dlg->windowButtons->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
  QObject::connect(dlg->btnNew, &QPushButton::clicked, [&context](){ on_tag_add(context); });
  QObject::connect(dlg->btnRemove, &QPushButton::clicked, [&context](){ on_tag_remove(context); });

  dlg->btnUser0->setText(trstring("Last"));
  dlg->btnUser0->setIcon(QIcon::fromTheme(QStringLiteral("edit-redo")));
  QObject::connect(dlg->btnUser0, &QPushButton::clicked, dlg, [&context](){ on_tag_last(context); });
  /* disable if no appropriate "last" tags have been stored or if the */
  /* selected item isn't a node or way */
  if((context.object.type == object_t::NODE && context.map->last_node_tags.empty()) ||
     (context.object.type == object_t::WAY && context.map->last_way_tags.empty()) ||
     (context.object.type != object_t::NODE && context.object.type != object_t::WAY))
    dlg->btnUser0->setEnabled(false);

  dlg->btnUser1->setText(trstring("Presets"));
  if (unlikely(presets == nullptr)) {
    dlg->btnUser1->setEnabled(false);
  } else {
    auto menu = osm2go_platform::josm_build_presets_button(presets, &context);
    assert(menu != nullptr);
    dlg->btnUser1->setMenu(menu);
  }
  dlg->btnUser2->setText(trstring("Relations"));
  QObject::connect(dlg->btnUser2, &QPushButton::clicked, dlg, [&context, presets](){
    relation_membership_dialog(context.dialog, presets, context.osm, context.object);
  });

  dialog_size_hint(dlg, osm2go_platform::MISC_DIALOG_LARGE);

  dlg->proxymodel->setSourceModel(model);

  qobject_cast<QBoxLayout *>(dlg->layout())->insertLayout(0, details_widget(context));

  dlg->view->resizeColumnsToContents();
  dlg->view->horizontalHeader()->setStretchLastSection(true);
  QObject::connect(dlg->view->selectionModel(), &QItemSelectionModel::selectionChanged,
                   [&dlg](const QItemSelection &selected) {
                     changed(dlg, selected);
                   });

  dlg->view->sortByColumn(0, Qt::AscendingOrder);

  bool ok = (dlg->exec() == QDialog::Accepted);

  if(ok)
    osm->updateTags(context.object, context.tags);

  return ok;
}

tag_context_t::tag_context_t(const object_t &o, const osm_t::TagMap &t, const osm_t::TagMap &ot, QDialog *dlg)
  : dialog(dlg)
  , object(o)
  , tags(t)
  , originalTags(ot)
{
}

info_tag_context_t::info_tag_context_t(map_t *m, osm_t::ref os, const object_t &o, ListEditDialog *dlg, TagModel *md, presets_items *ps)
  : tag_context_t(o, md->tags(), md->m_originalTags, dlg)
  , map(m)
  , osm(os)
  , store(md)
  , presets(ps)
{
}
