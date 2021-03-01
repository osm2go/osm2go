/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
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
#include <QFormLayout>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QStringListModel>
#include <strings.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_platform.h>

struct presets_context_t;

struct preset_attach_context {
  preset_attach_context(QFormLayout *l, presets_context_t &c) : ly(l), context(c) {}
  QFormLayout * const ly;
  presets_context_t &context;
};

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
    rows.push_back(s.row());

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

  attctx.ly->addRow(button);

  return nullptr;
}
