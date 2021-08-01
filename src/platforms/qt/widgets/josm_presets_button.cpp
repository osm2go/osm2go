/*
 * SPDX-FileCopyrightText: 2020-2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <josm_presets.h>
#include <josm_presets_p.h>

#include <icon.h>
#include "info_p.h"
#include <object_dialogs.h>
#include <osm.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <numeric>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QStringListModel>
#include <strings.h>
#include <unordered_map>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_platform.h>

namespace {

/* --------------------- the items dialog -------------------- */
struct presets_context_t {
  explicit presets_context_t(presets_items *pr, tag_context_t *t);
  presets_context_t(const presets_context_t &) = delete;
  presets_context_t(presets_context_t &&) = delete;
  presets_context_t &operator=(const presets_context_t &) = delete;
  presets_context_t &operator=(presets_context_t &&) = delete;

  icon_t &icons;
  presets_items * const presets;
  QMenu * const rootmenu;

  tag_context_t * const tag_context;
  const unsigned int presets_mask;
};

} // namespace

struct preset_attach_context {
  preset_attach_context(QFormLayout *l, presets_context_t &c) : ly(l), context(c) {}
  QFormLayout * const ly;
  presets_context_t &context;
};

namespace {

/**
 * @brief update the given tag with the newly entered value
 * @param widget the entry widget description
 * @param tags all tags of the object
 * @param value the new value
 */
bool
store_value(const presets_element_t *widget, osm_t::TagMap &tags, std::string value)
{
  bool changed;
  osm_t::TagMap::iterator ctag = tags.find(widget->key);
  if(!value.empty()) {
    if(ctag != tags.end()) {
      /* only update if the value actually changed */
      changed = ctag->second != value;
      if(changed)
        ctag->second = std::move(value);
    } else {
      tags.insert(osm_t::TagMap::value_type(widget->key, std::move(value)));
      changed = true;
    }
  } else if (ctag != tags.end()) {
    qDebug() << "removed key" << QString::fromStdString(widget->key) << "value" << QString::fromStdString(ctag->second);
    tags.erase(ctag);
    changed = true;
  } else {
    qDebug() << "ignore empty key" << QString::fromStdString(widget->key);
    changed = false;
  }

  return changed;
}

presets_context_t::presets_context_t(presets_items *pr, tag_context_t *t)
  : icons(icon_t::instance())
  , presets(pr)
  , rootmenu(new QMenu(t->dialog))
  , tag_context(t)
  , presets_mask(presets_type_mask(t->object))
{
  QObject::connect(rootmenu, &QObject::destroyed, [context = this](){ delete context; });
}

typedef std::unordered_map<const presets_element_t *, presets_element_t::attach_key *> WidgetMap;

void
add_widget_functor(const WidgetMap::key_type w, QFormLayout *ly, WidgetMap &qt_widgets, const osm_t::TagMap &tags, presets_context_t &ctx)
{
  if(w->type == WIDGET_TYPE_REFERENCE) {
    for (auto &&wd : static_cast<const presets_element_reference *>(w)->item->widgets)
      add_widget_functor(wd, ly, qt_widgets, tags, ctx);
    return;
  }

  /* check if there's a value with this key already */
  const osm_t::TagMap::const_iterator otagIt = !w->key.empty() ?
                                                 tags.find(w->key) :
                                                 tags.end();
  const std::string &preset = otagIt != tags.end() ? otagIt->second : std::string();

  preset_attach_context attctx(ly, ctx);
  presets_element_t::attach_key *widget = w->attach(attctx, preset);

  if(widget != nullptr)
    qt_widgets[w] = widget;
}

void
get_widget_functor(const presets_element_t *w, bool &changed, osm_t::TagMap &tags, const WidgetMap &qt_widgets,
                   const WidgetMap::const_iterator hintEnd)
{
  const WidgetMap::const_iterator hint = qt_widgets.find(w);
  WidgetMap::mapped_type akey = hint != hintEnd ? hint->second : nullptr;
  std::string text;

  switch(w->type) {
  case WIDGET_TYPE_KEY:
  case WIDGET_TYPE_CHECK:
  case WIDGET_TYPE_COMBO:
  case WIDGET_TYPE_MULTISELECT:
  case WIDGET_TYPE_TEXT:
    changed |= store_value(w, tags, w->getValue(akey));
    return;

  case WIDGET_TYPE_REFERENCE:
    for (auto &&wd : static_cast<const presets_element_reference *>(w)->item->widgets)
      get_widget_functor(wd, changed, tags, qt_widgets, hintEnd);
    return;

  default:
    return;
  }
}

template<typename T>
void build_menu(presets_context_t &context, const T &items, QMenu **matches, QMenu *menu)
{
  bool was_separator = false;
  bool was_item = false;

  for (auto &&item : items)
    build_menu_functor(context, item, menu, matches, was_item, was_separator);
}

void
buildLruMenu(std::vector<const presets_item_t *> &lru, presets_context_t &context)
{
  auto menu = context.rootmenu;
  const QString lruname = QStringLiteral("lrumenu");
  QMenu *lrumenu = menu->findChild<QMenu *>(lruname);
  if(lrumenu == nullptr) {
    lrumenu = new QMenu(trstring("Last used presets"), menu);
    lrumenu->setObjectName(lruname);
    menu->insertMenu(menu->actions().first(), lrumenu);
    menu->insertSeparator(menu->actions().at(1));
  } else {
    lrumenu->clear();
  }

  build_menu(context, lru, nullptr, lrumenu);
}

void
presets_item_dialog(const presets_item *item, presets_context_t &context)
{
  qDebug() << "dialog for item" << QString::fromStdString(item->name);

  /* build dialog from items widget list */

  /* check for widgets that have an interactive gui element. We won't show a
   * dialog if there's no interactive gui element at all */
  const std::vector<presets_element_t *>::const_iterator itEnd = item->widgets.end();
  std::vector<presets_element_t *>::const_iterator it = std::find_if(item->widgets.begin(), itEnd,
                                                                    presets_element_t::isInteractive);
  WidgetMap qt_widgets;
  tag_context_t * const tag_context = context.tag_context;
  osm2go_platform::DialogGuard dialog;

  if(it != itEnd)  {
    dialog = new QDialog(tag_context->dialog);
    auto *fly = new QFormLayout(dialog);
    auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    QObject::connect(bbox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(bbox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    /* if a web link has been provided for this item install */
    /* a button for this */
    if(!item->link.empty()) {
      QPushButton *infobtn = bbox->addButton(QDialogButtonBox::Help);
      infobtn->setText(trstring("Info"));
      QObject::connect(infobtn, &QPushButton::clicked, [&l = item->link]() {
        osm2go_platform::open_url(l.c_str());
      });
    }

    /* special handling for the first label/separators */
    if(item->addEditName) {
      dialog->setWindowTitle(trstring("Edit %1").arg(item->name));
    } else {
      // use the first label as title
      const presets_element_t * const w = item->widgets.front();
      if(w->type == WIDGET_TYPE_LABEL)
        dialog->setWindowTitle(QString::fromStdString(w->text));
    }

    /* skip all following non-interactive widgets: use the first one that
     * was found to be interactive above. */
    assert((*it)->is_interactive());

    std::for_each(it, itEnd, [fly, &qt_widgets, &t = tag_context->tags, &context](auto w) {
      add_widget_functor(w, fly, qt_widgets, t, context);
    });

    fly->addRow(bbox);

    if (dialog->exec() != QDialog::Accepted)
      return;
  }

  /* handle all children of the table */
  bool changed = false;

  osm_t::TagMap ntags = tag_context->tags;
  std::for_each(item->widgets.begin(), itEnd, [&changed, &ntags, &qt_widgets](auto w) {
    get_widget_functor(w, changed, ntags, qt_widgets, qt_widgets.cend());
  });

  if(changed)
    tag_context->info_tags_replace(ntags);

  static_cast<presets_items_internal *>(context.presets)->lru_update(item);;
  buildLruMenu(static_cast<presets_items_internal *>(context.presets)->lru, context);
}

/* ------------------- the item list (popup menu) -------------- */

QAction *
create_menuitem(icon_t &icons, QMenu *menu, const presets_item_named *item)
{
  const QString &mname = QString::fromStdString(item->name);

  if(!item->icon.empty()) {
    auto iconitem = icons.load(item->icon, 16);
    if(iconitem != nullptr)
      return menu->addAction(osm2go_platform::icon_pixmap(iconitem), mname);
  }

  return menu->addAction(mname);
}

void
build_menu_functor(presets_context_t &context, const presets_item_t *item, QMenu *menu, QMenu **matches,
                   bool &was_item, bool &was_separator)
{
  /* check if this presets entry is appropriate for the current item */
  if(item->type & context.presets_mask) {
    /* Show a separator if one was requested, but not if there was no item
     * before to prevent to show one as the first entry. */
    if(was_item && was_separator)
      menu->addSeparator();
    was_item = true;
    was_separator = false;

    if(item->type & presets_item_t::TY_GROUP) {
      auto gr = static_cast<const presets_item_group *>(item);
      QPixmap icon;
      if(!gr->icon.empty()) {
        auto iconitem = context.icons.load(gr->icon, 16);
        if(iconitem != nullptr)
          icon = osm2go_platform::icon_pixmap(iconitem);
      }

      build_menu(context, gr->items, matches, menu->addMenu(icon, QString::fromStdString(gr->name)));
    } else {
      auto *nitem = static_cast<const presets_item_named *>(item);
      auto *menu_item = create_menuitem(context.icons, menu, nitem);

      auto clicked = [item, &context]() {
        presets_item_dialog(static_cast<const presets_item *>(item), context);
      };

      QObject::connect(menu_item, &QAction::triggered, clicked);

      if(matches != nullptr && item->matches(context.tag_context->tags)) {
        if(*matches == nullptr)
          *matches = new QMenu(trstring("Used presets"));

        auto *used_item = create_menuitem(context.icons, *matches, nitem);
        QObject::connect(used_item, &QAction::triggered, clicked);
      }
    }
  } else if(item->type == presets_item_t::TY_SEPARATOR)
    /* Record that there was a separator. Do not immediately add it here to
     * prevent to show one as last entry. */
    was_separator = true;
}

} // namespace

QMenu *
osm2go_platform::josm_build_presets_button(presets_items *presets, tag_context_t *tag_context)
{
  presets_context_t *context = new presets_context_t(presets, tag_context);

  QMenu *matches = nullptr;

  auto pinternal = static_cast<presets_items_internal *>(presets);

  build_menu(*context, pinternal->items, &matches, context->rootmenu);
  if(!pinternal->lru.empty())
    buildLruMenu(pinternal->lru, *context);
  if(matches != nullptr) {
    context->rootmenu->insertMenu(context->rootmenu->actions().first(), matches);
    context->rootmenu->insertSeparator(context->rootmenu->actions().at(1));
  }

  return context->rootmenu;
}

presets_element_t::attach_key *
presets_element_text::attach(preset_attach_context &attctx, const std::string &preset) const
{
  auto *ret = new QLineEdit(attctx.ly->parentWidget());
  ret->setClearButtonEnabled(true);
  if(!preset.empty())
    ret->setText(QString::fromStdString(preset));
  else if(!def.empty())
    ret->setText(QString::fromStdString(def));

  attctx.ly->addRow(QString::fromStdString(text), ret);

  return reinterpret_cast<presets_element_t::attach_key *>(ret);
}

std::string
presets_element_text::getValue(presets_element_t::attach_key *akey) const
{
  auto widget = reinterpret_cast<QLineEdit *>(akey);

  return widget->text().toStdString();
}

presets_element_t::attach_key *
presets_element_separator::attach(preset_attach_context &attctx, const std::string &) const
{
  auto *ret = new QFrame(attctx.ly->parentWidget());
  ret->setFrameShape(QFrame::HLine);
  ret->setFrameShadow(QFrame::Sunken);
  attctx.ly->addRow(ret);
  return nullptr;
}

presets_element_t::attach_key *
presets_element_label::attach(preset_attach_context &attctx, const std::string &) const
{
  attctx.ly->addRow(new QLabel(QString::fromStdString(text), attctx.ly->parentWidget()));
  return nullptr;
}

presets_element_t::attach_key *
presets_element_combo::attach(preset_attach_context &attctx, const std::string &preset) const
{
  const std::string &pr = preset.empty() ? def : preset;
  QComboBox *ret = new QComboBox(attctx.ly->parentWidget());

  const auto &d = display_values.empty() ? values : display_values;
  QStringList entries;
  entries.reserve(d.size() + (editable ? 0 : 1));
  for (auto &&s : d)
    entries << QString::fromStdString(s);
  ret->setEditable(editable);

  int idx = -1;

  if(!editable)
    entries.insert(0, trstring("unset"));

  ret->addItems(entries);

  if(!editable && pr.empty()) {
    idx = 0;
  } else if (!pr.empty()) {
    if(auto it = std::find(values.cbegin(), values.cend(), preset); it != values.cend())
      idx = std::distance(values.cbegin(), it);
  }

  if(idx >= 0)
    ret->setCurrentIndex(idx);
  else
    ret->setCurrentText(QString::fromStdString(pr));

  attctx.ly->addRow(QString::fromStdString(text), ret);

  return reinterpret_cast<presets_element_t::attach_key *>(ret);
}

std::string
presets_element_combo::getValue(presets_element_t::attach_key *akey) const
{
  auto combo = reinterpret_cast<QComboBox *>(akey);

  std::string txt = combo->currentText().toStdString();
  if(txt.empty())
    return txt;

  int idx = combo->currentIndex();
  if(!editable && idx == 0) {
    // "unset"
    txt.clear();
    return txt;
  }

  if(!editable && idx > 0) {
    // skip "unset"
    idx--;
  } else if(idx < 0) {
    const auto &d = display_values.empty() ? values : display_values;
    if(auto it = std::find(d.begin(), d.end(), txt); it != d.end())
      idx = std::distance(d.begin(), it);
  }

  if(idx >= 0)
    txt = values.at(idx);

  return txt;
}

presets_element_t::attach_key *
presets_element_multiselect::attach(preset_attach_context &attctx, const std::string &preset) const
{
  const std::string &pr = preset.empty() ? def : preset;

  QStringList entries;
  entries.reserve(values.size());
  const auto &d = display_values.empty() ? values : display_values;
  for(const auto &s : d)
    entries << QString::fromStdString(s);

  auto *ret = new QListView(attctx.ly->parentWidget());
  auto *m = new QStringListModel(entries, ret);
  ret->setSelectionMode(QAbstractItemView::MultiSelection);
  ret->setModel(m);

  const auto &indexes = matchedIndexes(pr);
  QItemSelection sel;

  for (auto i : indexes) {
    QItemSelection c;
    auto idx = m->index(i);
    c.select(idx, idx);
    sel.merge(c, QItemSelectionModel::Select);
  }

  ret->selectionModel()->select(sel, QItemSelectionModel::Select);

  // arbitrary number for height scaling
  ret->setMinimumHeight(rows_height * 24);

  attctx.ly->addRow(QString::fromStdString(text), ret);

  return reinterpret_cast<presets_element_t::attach_key *>(ret);
}

std::string
presets_element_multiselect::getValue(presets_element_t::attach_key *akey) const
{
  auto list = reinterpret_cast<QListView *>(akey);

  const auto sel = list->selectionModel()->selectedRows();
  if(sel.isEmpty())
    return std::string();

  std::vector<int> rows;
  rows.reserve(sel.size());

  for (auto &&s : sel)
    rows.emplace_back(s.row());

  std::sort(rows.begin(), rows.end());

  return std::accumulate(std::next(rows.cbegin()), rows.cend(), values.at(rows.front()),
                         [d = delimiter, &v = values](const auto &s, int r) {
                           return s + d + v.at(r);
                         });
}

presets_element_t::attach_key *
presets_element_checkbox::attach(preset_attach_context &attctx, const std::string &preset) const
{
  bool deflt;
  if(!preset.empty())
    deflt = matchValue(preset);
  else
    deflt = def;

  auto *ret = new QCheckBox(attctx.ly->parentWidget());
  attctx.ly->addRow(QString::fromStdString(text), ret);
  ret->setChecked(deflt);

  return reinterpret_cast<presets_element_t::attach_key *>(ret);
}

bool
presets_element_checkbox::widgetValue(presets_element_t::attach_key *akey)
{
  return reinterpret_cast<QCheckBox *>(akey)->isChecked();
}

presets_element_t::attach_key *
presets_element_link::attach(preset_attach_context &attctx, const std::string &) const
{
  QIcon icon;
  if (!item->icon.empty()) {
    auto icon_item = icon_t::instance().load(item->icon, 16);
    if(icon_item != nullptr)
      icon = osm2go_platform::icon_pixmap(icon_item);
  }
  auto *button = new QPushButton(icon, trstring("[Preset] %1").arg(item->name), attctx.ly->parentWidget());
  QObject::connect(button, &QPushButton::clicked, [i = item, &c = attctx.context]() {
    presets_item_dialog(i, c);
  });

  attctx.ly->addRow(button);

  return nullptr;
}
