/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "canvas_graphicsscene.h"

#include <canvas_p.h>
#include <map.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>

#include <QDebug>
#include <QGraphicsItem>
#include <QGraphicsItemGroup>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsSvgItem>
#include <QGraphicsView>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QSvgRenderer>
#include <QTransform>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include <osm2go_platform.h>
#include "osm2go_stl.h"

enum DataKeyMagic {
  DATA_KEY_DELETE_ITEM = 42,
  DATA_KEY_MAP_ITEM = 47,
  DATA_KEY_ZOOM = 51
};

canvas_t *
canvas_t_create()
{
  return new canvas_graphicsscene();
}

void CanvasScene::keyPressEvent(QKeyEvent *keyEvent)
{
  if (keyEvent->count() == 1) {
    const int key = keyEvent->key();
    switch (keyEvent->key()) {
    case Qt::Key_Enter:
      if (keyEvent->modifiers() == Qt::KeypadModifier) {
        keyEvent->accept();
        emit keyPress(Qt::Key_Return);
        return;
      }
      break;
    case Qt::Key_Return:
      if (keyEvent->modifiers() == Qt::NoModifier) {
        keyEvent->accept();
        emit keyPress(key);
        return;
      }
      break;
    default:
      break;
    }
  }

  QGraphicsScene::keyPressEvent(keyEvent);
}

/* ------------------- creating and destroying the canvas ----------------- */

namespace {

/**
 * @brief delete the icon
 *
 * This calls the map item deleter if one has been set before.
 */
void
destroyItem(QGraphicsItem *item)
{
  auto *citem = reinterpret_cast<canvas_item_t *>(item);
  if (auto *deleter = static_cast<canvas_item_destroyer *>(item->data(DATA_KEY_DELETE_ITEM).value<void *>()); deleter != nullptr) {
    deleter->run(citem);
    delete deleter;
  }

  if (auto *mapitem = static_cast<map_item_t *>(item->data(DATA_KEY_MAP_ITEM).value<void *>()); mapitem != nullptr) {
    map_item_destroyer mi(mapitem);
    mi.run(citem);
  }

  delete item;
}

} // namespace

/* create a new canvas */
canvas_graphicsscene::canvas_graphicsscene()
  : canvas_t(new QGraphicsView())
  , scene(new CanvasScene(widget))
{
  /* create the groups */
  for(unsigned int gr = 0; gr < group.size(); gr++) {
    QGraphicsItemGroup *g = scene->createItemGroup({});
    group[gr] = g;
    g->setZValue(gr);
  }

  static_cast<QGraphicsView *>(widget)->setScene(scene);
  QObject::connect(widget, &QObject::destroyed, [this]() { delete this; });
}

canvas_graphicsscene::~canvas_graphicsscene()
{
  const auto l = scene->items();
  for (auto &&i : l)
    destroyItem(i);
}

/* ------------------------ accessing the canvas ---------------------- */

void
canvas_t::set_background(color_t bg_color)
{
  static_cast<canvas_graphicsscene *>(this)->scene->setBackgroundBrush(QBrush(QColor::fromRgba(bg_color.rgba())));
}

bool
canvas_t::set_background(const std::string &filename)
{
  auto *gcanvas = static_cast<canvas_graphicsscene *>(this);
  auto *gr = gcanvas->group[CANVAS_GROUP_BG];

  // remove old background image, if any
  if(auto childs = gr->childItems(); !childs.isEmpty()) {
    assert(childs.count() == 1);
    auto *old = childs.takeFirst();
    gr->removeFromGroup(old);
    gcanvas->scene->removeItem(old);
    delete old;
  }

  QPixmap pm;
  if (!pm.load(QString::fromStdString(filename)))
    return false;

  /* calculate required scale factor */
  auto bounds = gcanvas->scene->sceneRect();
  gcanvas->bg.scale.x = bounds.width() / pm.width();
  gcanvas->bg.scale.y = bounds.height() / pm.height();

  pm.scaled(gcanvas->bg.scale.x, gcanvas->bg.scale.y);

  auto *item = new QGraphicsPixmapItem(pm, gr);
  gr->addToGroup(item);

  return true;
}

void
canvas_t::move_background(int x, int y)
{
  static_cast<canvas_graphicsscene *>(this)->group[CANVAS_GROUP_BG]->childItems().first()->setPos(x, y);
}

lpos_t
canvas_t::window2world(const osm2go_platform::screenpos &p) const
{
  // this just uses scene positions everywhere
  return lpos_t(p.x(), p.y());
}

double
canvas_t::set_zoom(double zoom)
{
  QGraphicsView * const view = static_cast<QGraphicsView *>(widget);
  const double cur_zoom = get_zoom();
  if(!view->horizontalScrollBar()->isVisible() && !view->verticalScrollBar()->isVisible() && zoom < cur_zoom)
    return cur_zoom;
  QTransform t = view->transform();
  t.setMatrix(zoom, t.m12(), t.m13(), t.m21(), zoom, t.m23(), t.m31(), t.m32(), t.m33());
  view->setTransform(t);
  return zoom;
}

double
canvas_t::get_zoom() const
{
  const QGraphicsView * const view = static_cast<const QGraphicsView *>(widget);
  QTransform t = view->transform();
  Q_ASSERT(t.m11() == t.m22());
  return t.m11();
}

/* get scroll position */
osm2go_platform::screenpos
canvas_t::scroll_get() const
{
  auto *view = static_cast<const QGraphicsView *>(widget);

  return osm2go_platform::screenpos(view->horizontalScrollBar()->value(),
                                    view->verticalScrollBar()->value());
}

/* set scroll position */
osm2go_platform::screenpos
canvas_t::scroll_to(const osm2go_platform::screenpos &s)
{
  static_cast<QGraphicsView *>(widget)->centerOn(s.x(), s.y());
  return scroll_get();
}

osm2go_platform::screenpos
canvas_t::scroll_step(const osm2go_platform::screenpos &d)
{
  auto *gv = static_cast<QGraphicsView *>(widget);
  auto *sb = gv->horizontalScrollBar();
  if (d.x() != 0)
    sb->setValue(sb->value() + d.x());
  int nx = sb->value();
  sb = gv->verticalScrollBar();
  if (d.y() != 0)
    sb->setValue(sb->value() + d.y());

  return osm2go_platform::screenpos(nx, sb->value());
}

void
canvas_t::set_bounds(lpos_t min, lpos_t max)
{
  QRectF rect;
  rect.setBottomLeft(QPointF(min.x, min.y));
  rect.setTopRight(QPointF(max.x, max.y));
  static_cast<canvas_graphicsscene *>(this)->scene->setSceneRect(rect);
}

/* ------------------- creating and destroying objects ---------------- */

void
canvas_t::erase(unsigned int group_mask)
{
  auto *gcanvas = static_cast<canvas_graphicsscene *>(this);

  for (unsigned group = 0; group < gcanvas->group.size(); group++) {
    if (group_mask & (1 << group)) {
      const auto *gr = gcanvas->group[group];
      const auto childs = gr->childItems();
      if(childs.isEmpty())
        continue;
      qDebug() << "Removing " << childs.count() << "children from group" << group << gr << gr->boundingRect();
      for (auto &&c: childs) {
        gcanvas->scene->removeItem(c);
        destroyItem(c);
      }
    }
  }
}

namespace {

template<typename T>
class ZoomedItem : public T {
public:
  ZoomedItem(canvas_t *canvas, canvas_group_t gr)
    : T(static_cast<canvas_graphicsscene *>(canvas)->group[gr])
  {
    assert(T::group() == nullptr);
    static_cast<QGraphicsItemGroup *>(T::parentItem())->addToGroup(this);
    if(CANVAS_SELECTABLE & (1 << gr))
      T::setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
  }

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
  {
    // TODO: match the zoom specifiers between the Gtk and Qt version, they seem different
    const QVariant zm = T::data(DATA_KEY_ZOOM);
    if (!zm.isNull() && zm.value<float>() > QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform()))
      return;

    T::paint(painter, option, widget);
  }
};

} // namespace

canvas_item_circle *
canvas_t::circle_new(canvas_group_t group, lpos_t c, float radius, int border, color_t fill_col, color_t border_col)
{
  auto *item = new ZoomedItem<QGraphicsEllipseItem>(this, group);
  item->setRect(c.x - radius, c.y - radius, radius * 2, radius * 2);

  if (border > 0)
    item->setPen(QPen(QColor::fromRgba(border_col.argb()), border));
  item->setBrush(QColor::fromRgba(fill_col.argb()));

  auto *ret = reinterpret_cast<canvas_item_circle *>(item);

  if (CANVAS_SELECTABLE & (1 << group))
    (void) new canvas_item_info_circle(this, ret, c, radius + border);

  return ret;
}

static QPainterPath
canvas_points_create(const std::vector<lpos_t> &points)
{
  assert_cmpnum_op(points.size(), >, 0);
  QPainterPath ret(QPointF(points.front().x, points.front().y));
  std::for_each(std::next(points.cbegin()), points.cend(), [&ret](auto &&p) {
    ret.lineTo(p.x, p.y);
  });

  return ret;
}

canvas_item_polyline *
canvas_t::polyline_new(canvas_group_t group, const std::vector<lpos_t> &points, float width, color_t color)
{
  auto *item = new ZoomedItem<QGraphicsPathItem>(this, group);
  item->setPath(canvas_points_create(points));

  item->setPen(QPen(QColor::fromRgba(color.argb()), width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

  auto *ret = reinterpret_cast<canvas_item_polyline *>(item);

  if(CANVAS_SELECTABLE & (1 << group))
    (void) new canvas_item_info_poly(this, ret, false, width, points);

  return ret;
}

canvas_item_t *
canvas_t::polygon_new(canvas_group_t group, const std::vector<lpos_t> &points, float width,
                      color_t color, color_t fill)
{
  QPolygonF cpoints(points.size());
  cpoints.clear();
  for (const auto p: points)
    cpoints << QPointF(p.x, p.y);

  auto *item = new ZoomedItem<QGraphicsPolygonItem>(this, group);
  item->setPolygon(cpoints);

  item->setPen(QPen(QColor::fromRgba(color.argb()), width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  item->setBrush(QColor::fromRgba(fill.argb()));

  auto *ret = reinterpret_cast<canvas_item_t *>(item);

  if(CANVAS_SELECTABLE & (1 << group))
    (void) new canvas_item_info_poly(this, ret, true, width, points);

  return ret;
}

/* place the image in pix centered on x/y on the canvas */
canvas_item_pixmap *
canvas_t::image_new(canvas_group_t group, icon_item *icon, lpos_t pos, float scale)
{
  QGraphicsItem *item;
  QSvgRenderer *r = osm2go_platform::icon_renderer(icon);
  QPixmap pix = osm2go_platform::icon_pixmap(icon);
  if(r == nullptr) {
    auto *zitem = new ZoomedItem<QGraphicsPixmapItem>(this, group);
    item = zitem;
    zitem->setPixmap(pix);
    zitem->setOffset(- pix.width() / 2.0f, - pix.height() / 2.0f);
    item->setPos(pos.x, pos.y);
  } else {
    auto *sitem = new ZoomedItem<QGraphicsSvgItem>(this, group);
    item = sitem;
    sitem->setSharedRenderer(r);
    auto vr = r->viewBoxF();
    item->setPos(pos.x - vr.width() * scale / 2.0f, pos.y - vr.height() * scale / 2.0f);
    scale *= vr.width() / pix.width();
  }
  item->setScale(scale);

  auto *ret = reinterpret_cast<canvas_item_pixmap *>(item);

  if (CANVAS_SELECTABLE & (1 << group)) {
    int radius = 0.75 * scale * std::max(pix.width(), pix.height());
    (void) new canvas_item_info_circle(this, ret, pos, radius);
  }

  return ret;
}

void
canvas_item_t::operator delete(void *ptr)
{
  delete static_cast<QGraphicsItem *>(ptr);
}

/* ------------------------ accessing items ---------------------- */

void
canvas_item_polyline::set_points(const std::vector<lpos_t> &points)
{
  reinterpret_cast<QGraphicsPathItem *>(this)->setPath(canvas_points_create(points));
}

void
canvas_item_circle::set_radius(float radius)
{
  auto *item = reinterpret_cast<QGraphicsEllipseItem *>(this);
  QRectF r = item->rect();
  const QPointF c = r.center();
  r.setWidth(radius);
  r.setHeight(radius);
  r.moveCenter(c);
  item->setRect(r);
}

bool
canvas_t::ensureVisible(lpos_t lpos)
{
  qobject_cast<QGraphicsView *>(widget)->ensureVisible(lpos.x, lpos.y, lpos.x, lpos.y);

  return true;
}

void
canvas_item_t::set_zoom_max(float zoom_max)
{
  reinterpret_cast<QGraphicsItem *>(this)->setData(DATA_KEY_ZOOM, zoom_max);
}

void
canvas_item_t::set_dashed(float line_width, unsigned int dash_length_on, unsigned int dash_length_off)
{
  auto *item = reinterpret_cast<QGraphicsItem *>(this);

  assert(item->type() == QGraphicsPathItem::Type || item->type() == QGraphicsPolygonItem::Type);

  auto *sitem = static_cast<QAbstractGraphicsShapeItem *>(item);
  auto pen = sitem->pen();
  pen.setDashPattern( { static_cast<qreal>(dash_length_on), static_cast<qreal>(dash_length_off) } );
  pen.setWidthF(line_width);
  sitem->setPen(pen);
}

void
canvas_item_t::set_user_data(map_item_t *data)
{
  reinterpret_cast<QGraphicsItem *>(this)->setData(DATA_KEY_MAP_ITEM, QVariant::fromValue(static_cast<void *>(data)));
}

map_item_t *
canvas_item_t::get_user_data()
{
  return static_cast<map_item_t *>(reinterpret_cast<QGraphicsItem *>(this)->data(DATA_KEY_MAP_ITEM).value<void *>());
}

void
canvas_item_t::destroy_connect(canvas_item_destroyer *d)
{
  reinterpret_cast<QGraphicsItem *>(this)->setData(DATA_KEY_DELETE_ITEM, QVariant::fromValue(static_cast<void *>(d)));
}

namespace {

auto items_in_rect(CanvasScene *sc, lpos_t pos, double zoom)
{
  int fuzziness = EXTRA_FUZZINESS_METER + EXTRA_FUZZINESS_PIXEL / zoom;

  const QRectF cRect(pos.x - fuzziness, pos.y - fuzziness, fuzziness * 2, fuzziness * 2);

  auto items = sc->items(cRect);

  // remove unselectable entries
  items.erase(std::remove_if(items.begin(), items.end(), [](auto item) {
    return !(item->flags() & QGraphicsItem::ItemIsSelectable);
  }), items.end());

  return items;
}

} // namespace

canvas_item_t *
canvas_t::get_item_at(lpos_t pos) const
{
  const auto items = items_in_rect(static_cast<const canvas_graphicsscene *>(this)->scene, pos, get_zoom());

  QGraphicsItem *ret = nullptr;

  for (auto &&item : items) {
    if (ret == nullptr || ret->zValue() < item->zValue())
      ret = item;
  }

  return reinterpret_cast<canvas_item_t *>(ret);
}

canvas_item_t *
canvas_t::get_next_item_at(lpos_t pos, canvas_item_t *oldtop) const
{
  auto *qitem = reinterpret_cast<QGraphicsItem *>(oldtop);
  const auto childs = qitem->parentItem()->childItems();

  qitem->setZValue(-1);

  for (auto &&o : childs)
    if (o != qitem)
      o->setZValue(o->zValue() + 1);

  return get_item_at(pos);
}
