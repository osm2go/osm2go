/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "osm2go_platform.h"

#include <color.h>

#include <QDialog>
#include <QPixmap>
#include <QPointer>
#include <QWidget>

class icon_item;
struct pos_area;
class presets_items;
class QGeoRectangle;
class QMenu;
class QString;
class QSvgRenderer;
class QVariant;
class tag_context_t;

namespace osm2go_platform {
  template<typename T>
  class OwningPointer : public QPointer<T> {
  public:
    OwningPointer() = default;
    OwningPointer(T *p) : QPointer<T>(p) {}
    OwningPointer(const OwningPointer<T> &) = delete;
    OwningPointer(OwningPointer<T> &&) = default;
    ~OwningPointer()
    {
      delete QPointer<T>::data();
    }
    inline OwningPointer &operator=(const QPointer<T> &other)
    { QPointer<T>::operator=(other); return *this; }
    inline OwningPointer &operator=(T *other)
    { QPointer<T>::operator=(other); return *this; }
  };

  typedef OwningPointer<QWidget> WidgetGuard;
  typedef OwningPointer<QDialog> DialogGuard;

  QPixmap icon_pixmap(icon_item *icon);
  QSvgRenderer *icon_renderer(const icon_item *icon);

  constexpr Qt::GlobalColor invalid_text_color() { return Qt::red; }

  /* dialog size are specified rather fuzzy */
  enum DialogSizeHint {
    MISC_DIALOG_SMALL  =  0,
    MISC_DIALOG_MEDIUM =  1,
    MISC_DIALOG_LARGE  =  2,
    MISC_DIALOG_WIDE   =  3,
    MISC_DIALOG_HIGH   =  4
  };

  void dialog_size_hint(QWidget *window, DialogSizeHint hint);

  QString find_file(const QString &n);

  QGeoRectangle rectFromArea(const pos_area &area);
  pos_area areaFromRect(const QGeoRectangle &rect);

  QMenu *josm_build_presets_button(presets_items *presets, tag_context_t *tag_context) __attribute__((nonnull(1,2)));

  QVariant modelHightlightModified();
};
