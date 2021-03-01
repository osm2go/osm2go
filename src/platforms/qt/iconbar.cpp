/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iconbar.h>

#include <appdata.h>
#include <object_dialogs.h>
#include <map.h>
#include <osm.h>
#include <project.h>

#include <array>
#include <cassert>
#include <QAction>
#include <QToolBar>

#include <osm2go_cpp.h>
#include <osm2go_i18n.h>

namespace {

class IconbarQt : public iconbar_t {
public:
  explicit IconbarQt(appdata_t &appdata);

  QToolBar * const toolbar;

  QAction * const info;
  QAction * const trash;

  QAction * const node_add;

  QAction * const way_add;
  QAction * const way_node_add;
  QAction * const way_cut;
  QAction * const way_reverse;

  QAction * const cancel;
  QAction * const ok;

  inline void map_action_idle(bool idle, const object_t &selected);
  inline void map_cancel_ok(bool cancelv, bool okv);
};

} // namespace

/* enable/disable ok and cancel button */
void
IconbarQt::map_cancel_ok(bool cancelv, bool okv)
{
  ok->setEnabled(okv);
  cancel->setEnabled(cancelv);
}

void
iconbar_t::map_cancel_ok(bool cancel, bool ok)
{
  static_cast<IconbarQt *>(this)->map_cancel_ok(cancel, ok);
}

namespace {

void
iconbar_toggle_sel_widgets(IconbarQt *iconbar, bool value)
{
  for(auto &&w: { iconbar->trash, iconbar->info })
    w->setEnabled(value);
}

void
iconbar_toggle_way_widgets(IconbarQt *iconbar, bool value, const object_t &selected)
{
  for(auto &&w: { iconbar->way_node_add, iconbar->way_reverse})
    w->setEnabled(value);

  if(value)
    assert(selected.type != object_t::ILLEGAL);

  iconbar->way_cut->setEnabled(value && static_cast<way_t *>(selected)->node_chain.size() > 2);
}

} // namespace

void
iconbar_t::map_item_selected(const object_t &item)
{
  bool selected = item.type != object_t::ILLEGAL;
  iconbar_toggle_sel_widgets(static_cast<IconbarQt *>(this), selected);

  bool way_en = selected && item.type == object_t::WAY;
  iconbar_toggle_way_widgets(static_cast<IconbarQt *>(this), way_en, item);
}

void
IconbarQt::map_action_idle(bool idle, const object_t &selected)
{
  /* icons that are enabled in idle mode */
  for(auto &&w: { node_add, way_add })
    w->setEnabled(idle);

  bool way_en = idle && selected.type == object_t::WAY;

  iconbar_toggle_sel_widgets(this, false);
  iconbar_toggle_way_widgets(this, way_en, selected);
}

void
iconbar_t::map_action_idle(bool idle, const object_t &selected)
{
  static_cast<IconbarQt *>(this)->map_action_idle(idle, selected);
}

void
iconbar_t::setToolbarEnable(bool en)
{
  static_cast<IconbarQt *>(this)->toolbar->setEnabled(en);
}

bool
iconbar_t::isCancelEnabled() const
{
  return static_cast<const IconbarQt *>(this)->cancel->isEnabled();
}

bool
iconbar_t::isInfoEnabled() const
{
  return static_cast<const IconbarQt *>(this)->info->isEnabled();
}

bool
iconbar_t::isOkEnabled() const
{
  return static_cast<const IconbarQt *>(this)->ok->isEnabled();
}

bool
iconbar_t::isTrashEnabled() const
{
  return static_cast<const IconbarQt *>(this)->trash->isEnabled();
}

namespace {

QAction * __attribute__((nonnull(1)))
tool_add(QToolBar *toolbar, const QString &icon_str, const QString &tooltip_str, bool separator = false)
{
  QAction *item = toolbar->addAction(QIcon::fromTheme(icon_str), tooltip_str);

  if(separator)
    toolbar->addSeparator();

  return item;
}

} // namespace

IconbarQt::IconbarQt(appdata_t &appdata)
  : iconbar_t()
  , toolbar(new QToolBar())
  , info(tool_add(toolbar, QStringLiteral("dialog-information"), QObject::tr("Properties"), true))
  , trash(tool_add(toolbar, QStringLiteral("edit-delete"), QObject::tr("Delete"), true))
  , node_add(tool_add(toolbar, QStringLiteral("list-add"), QObject::tr("New node"), true))
  , way_add(tool_add(toolbar, QStringLiteral("way_add"), QObject::tr("Add way")))
  , way_node_add(tool_add(toolbar, QStringLiteral("way_node_add"), QObject::tr("Add node")))
  , way_cut(tool_add(toolbar, QStringLiteral("way_cut"), QObject::tr("Split way")))
  , way_reverse(tool_add(toolbar, QStringLiteral("way_reverse"), QObject::tr("Reverse way")))
  , cancel(toolbar->addAction(QIcon::fromTheme(QStringLiteral("dialog-cancel")), QObject::tr("Cancel")))
  , ok(toolbar->addAction(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")), QObject::tr("Ok")))
{
  map_t *map = appdata.map;

  QObject::connect(info, &QAction::triggered,
                [map](){ map->info_selected(); });
  QObject::connect(trash, &QAction::triggered, [map](){ map->delete_selected(); });
  trash->setShortcut(Qt::Key_Delete);
  QObject::connect(node_add, &QAction::triggered,
                    [map](){ map->set_action(MAP_ACTION_NODE_ADD); });
  QObject::connect(way_add, &QAction::triggered,
                   [map](){ map->set_action(MAP_ACTION_WAY_ADD); });
  QObject::connect(way_node_add, &QAction::triggered,
                        [map](){ map->set_action(MAP_ACTION_WAY_NODE_ADD); });
  QObject::connect(way_cut, &QAction::triggered,
                   [map](){ map->set_action(MAP_ACTION_WAY_CUT); });
  QObject::connect(way_reverse, &QAction::triggered, [map](){ map_t::edit_way_reverse(map); });
  cancel->setShortcut(Qt::Key_Escape);
  toolbar->setOrientation(Qt::Vertical);
  toolbar->setToolButtonStyle(Qt::ToolButtonFollowStyle);
}

osm2go_platform::Widget *
iconbar_t::create(appdata_t &appdata)
{
  auto *iconbar = new IconbarQt(appdata);
  appdata.iconbar.reset(iconbar);

  map_t *map = appdata.map;
  QObject::connect(iconbar->ok, &QAction::triggered, [map](){ map->action_ok(); });
  QObject::connect(iconbar->cancel, &QAction::triggered, [map](){ map->action_cancel(); });

  iconbar->map_cancel_ok(false, false);

  /* --------------------------------------------------------- */

  iconbar->map_item_selected(object_t());

  return iconbar->toolbar;
}
