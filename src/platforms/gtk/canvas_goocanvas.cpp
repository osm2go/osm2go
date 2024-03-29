/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file canvas_goocanvas.cpp
 *
 * this file contains the canvas functions specific to GooCanvas. It also
 * contains e.g. a canvas agnostic way of detecting which items are at a
 * certain position. This is required for some canvas that don't provide this
 * function
 *
 * This also allows for a less precise item selection and especially
 * to differentiate between the clicks on a polygon border and its
 * interior
 *
 * References:
 * https://en.wikipedia.org/wiki/Point_in_polygon
 * https://www.visibone.com/inpoly/
 */

#include "canvas_goocanvas.h"

#include <canvas_p.h>
#include "map.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include "osm2go_platform_gtk.h"
#include "osm2go_stl.h"

namespace {

struct canvas_points_deleter {
  inline void operator()(void *ptr) {
    goo_canvas_points_unref(static_cast<GooCanvasPoints *>(ptr));
  }
};

typedef std::unique_ptr<GooCanvasPoints, canvas_points_deleter> pointGuard;

} // namespace

struct canvas_dimensions {
  canvas_dimensions(double w, double h)
    : width(w), height(h) {}
  double width, height;
  inline canvas_dimensions operator/(double d) const {
    canvas_dimensions ret = *this;
    ret /= d;
    return ret;
  }
  inline canvas_dimensions &operator/=(double d) {
    width /= d;
    height /= d;
    return *this;
  }
};

// since struct _GooCanvasItem does not exist, but is defined as an interface type
// in the GooCanvas headers define it here and inherit from it to get the internal
// casting type safe
struct _GooCanvasItem : public canvas_item_t {
};

// only for usage in tests
canvas_t *canvas_t_create()
{
  return new canvas_goocanvas();
}

/* ------------------- creating and destroying the canvas ----------------- */

static void canvas_delete(canvas_goocanvas *canvas)
{
  delete canvas;
}

/* create a new canvas */
canvas_goocanvas::canvas_goocanvas()
  : canvas_t(goo_canvas_new())
{
  GooCanvasItem *root = goo_canvas_get_root_item(GOO_CANVAS(widget));

  /* create the groups */
  for(unsigned int gr = 0; gr < group.size(); gr++)
    group[gr] = goo_canvas_group_new(root, nullptr);

  GObject *w = G_OBJECT(widget);
  g_signal_connect_swapped(w, "destroy",
                           G_CALLBACK(canvas_delete), this);

  g_object_set(w, "anchor", GTK_ANCHOR_CENTER, nullptr);

  gtk_widget_set_events(widget,
                        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                        GDK_SCROLL_MASK | GDK_POINTER_MOTION_MASK |
                        GDK_POINTER_MOTION_HINT_MASK);
}

/* ------------------------ accessing the canvas ---------------------- */

void canvas_t::set_background(color_t bg_color) {
  g_object_set(G_OBJECT(widget),
               "background-color-rgb", bg_color.rgb(), nullptr);
}

bool canvas_t::set_background(const std::string &filename)
{
  canvas_goocanvas *gcanvas = static_cast<canvas_goocanvas *>(this);

  GooCanvasItem *gr = gcanvas->group[CANVAS_GROUP_BG];
  int n = goo_canvas_item_get_n_children(gr);
  if(n > 0) {
    assert_cmpnum(n, 1);
    goo_canvas_item_remove_child(gr, 0);
  }

  if(filename.empty())
    return false;

  gcanvas->bg.pix.reset(gdk_pixbuf_new_from_file(filename.c_str(), nullptr));
  if(!gcanvas->bg.pix)
    return false;

  float width = gdk_pixbuf_get_width(gcanvas->bg.pix.get());
  float height = gdk_pixbuf_get_height(gcanvas->bg.pix.get());

  /* calculate required scale factor */
  gcanvas->bg.scale.x = (gcanvas->bounds.max.x - gcanvas->bounds.min.x) / width;
  gcanvas->bg.scale.y = (gcanvas->bounds.max.y - gcanvas->bounds.min.y) / height;

  GooCanvasItem *bg = goo_canvas_image_new(gr, gcanvas->bg.pix.get(),
                                          gcanvas->bounds.min.x / gcanvas->bg.scale.x - width / 2.0f,
                                          gcanvas->bounds.min.y / gcanvas->bg.scale.y - height / 2.0f,
                                          nullptr);
  goo_canvas_item_scale(bg, gcanvas->bg.scale.x, gcanvas->bg.scale.y);

  return true;
}

void canvas_t::move_background(int x, int y)
{
  canvas_goocanvas *gcanvas = static_cast<canvas_goocanvas *>(this);
  GooCanvasItem *bgitem = goo_canvas_item_get_child(gcanvas->group[CANVAS_GROUP_BG], 0);
  assert(bgitem != nullptr);

  g_object_set(G_OBJECT(bgitem),
               "x", static_cast<gdouble>(x) / gcanvas->bg.scale.x,
               "y", static_cast<gdouble>(y) / gcanvas->bg.scale.y,
               nullptr);
}

lpos_t canvas_t::window2world(const osm2go_platform::screenpos &p) const
{
  double sx = p.x(), sy = p.y();
  goo_canvas_convert_from_pixels(GOO_CANVAS(widget), &sx, &sy);
  return lpos_t(sx, sy);
}

double canvas_t::set_zoom(double zoom) {
  /* Limit a proposed zoom factor to sane ranges.
   * Specifically the map is allowed to be no smaller than the viewport. */

  canvas_goocanvas *gcanvas = static_cast<canvas_goocanvas *>(this);
  /* get size of visible area in pixels and convert to meters of intended */
  /* zoom by dividing by zoom (which is basically pix/m) */
  const GtkAllocation &dim = widget->allocation;

  double limit;
  int delta;

  if (dim.height < dim.width) {
    limit = dim.height;
    delta = gcanvas->bounds.max.y - gcanvas->bounds.min.y;
  } else {
    limit = dim.width;
    delta = gcanvas->bounds.max.x - gcanvas->bounds.min.x;
  }
  limit *= 0.95 / zoom;

  if (delta < limit) {
    zoom /= (delta / limit);

    printf("Can't zoom further out (%f)\n", zoom);
  }

  goo_canvas_set_scale(GOO_CANVAS(widget), zoom);

  return zoom;
}

double canvas_t::get_zoom() const {
  return goo_canvas_get_scale(GOO_CANVAS(widget));
}

static osm2go_platform::screenpos boundedScroll(canvas_goocanvas *gcanvas,
                                                const osm2go_platform::screenpos &d)
{
  /* get size of visible area in canvas units (meters) */
  canvas_dimensions dim = gcanvas->get_viewport_dimensions() / 2;

  // Data rect minimum and maximum
  // limit stops - prevent scrolling beyond these
  gdouble min_sy_cu = 0.95 * (gcanvas->bounds.min.y - dim.height);
  gdouble min_sx_cu = 0.95 * (gcanvas->bounds.min.x - dim.width);
  gdouble max_sy_cu = 0.95 * (gcanvas->bounds.max.y + dim.height);
  gdouble max_sx_cu = 0.95 * (gcanvas->bounds.max.x + dim.width);

  osm2go_platform::screenpos ret(std::clamp(d.x(), min_sx_cu, max_sx_cu),
                                 std::clamp(d.y(), min_sy_cu, max_sy_cu));

  /* adjust to screen center */
  GooCanvas *gc = GOO_CANVAS(gcanvas->widget);
  gdouble zoom = goo_canvas_get_scale(gc);
  gdouble offx = gcanvas->widget->allocation.width / (2 * zoom);
  gdouble offy = gcanvas->widget->allocation.height / (2 * zoom);

  goo_canvas_scroll_to(gc, ret.x() - offx, ret.y() - offy);

  return ret;
}

canvas_dimensions canvas_goocanvas::get_viewport_dimensions() const {
  // Canvas viewport dimensions
  const GtkAllocation &a = widget->allocation;
  canvas_dimensions ret(a.width, a.height);

  /* convert to meters by dividing by zoom */
  ret /= get_zoom();

  return ret;
}

/* get scroll position in meters */
osm2go_platform::screenpos canvas_t::scroll_get() const
{
  GooCanvas *gc = GOO_CANVAS(widget);
  gdouble zoom = goo_canvas_get_scale(gc);

  gdouble hs = gtk_adjustment_get_value(gc->hadjustment);
  gdouble vs = gtk_adjustment_get_value(gc->vadjustment);
  goo_canvas_convert_from_pixels(gc, &hs, &vs);

  /* convert to position relative to screen center */
  hs += widget->allocation.width/(2*zoom);
  vs += widget->allocation.height/(2*zoom);

  return osm2go_platform::screenpos(hs, vs);
}

/* set scroll position in meters */
osm2go_platform::screenpos canvas_t::scroll_to(const osm2go_platform::screenpos &s)
{
  return boundedScroll(static_cast<canvas_goocanvas *>(this), s);
}

osm2go_platform::screenpos canvas_t::scroll_step(const osm2go_platform::screenpos &d)
{
  GooCanvas *gc = GOO_CANVAS(widget);
  gdouble hs = gtk_adjustment_get_value(gc->hadjustment) + d.x();
  gdouble vs = gtk_adjustment_get_value(gc->vadjustment) + d.y();
  goo_canvas_convert_from_pixels(gc, &hs, &vs);

  gdouble zoom = goo_canvas_get_scale(gc);

  return scroll_to(osm2go_platform::screenpos(hs + widget->allocation.width / (2 * zoom),
                                              vs + widget->allocation.height / (2 * zoom)));
}

void canvas_t::set_bounds(lpos_t min, lpos_t max) {
  g_assert_cmpint(min.x, <, 0);
  g_assert_cmpint(min.y, <, 0);
  g_assert_cmpint(max.x, >, 0);
  g_assert_cmpint(max.y, >, 0);
  goo_canvas_set_bounds(GOO_CANVAS(widget), min.x * CANVAS_FRISKET_SCALE, min.y * CANVAS_FRISKET_SCALE,
                                            max.x * CANVAS_FRISKET_SCALE, max.y * CANVAS_FRISKET_SCALE);
  canvas_goocanvas *gcanvas = static_cast<canvas_goocanvas *>(this);
  gcanvas->bounds.min = min;
  gcanvas->bounds.max = max;
}

/* ------------------- creating and destroying objects ---------------- */

void canvas_t::erase(unsigned int group_mask) {
  canvas_goocanvas *gcanvas = static_cast<canvas_goocanvas *>(this);
  GooCanvasItem *root = goo_canvas_get_root_item(GOO_CANVAS(widget));

  if(unlikely((group_mask & (1 << CANVAS_GROUP_BG)) != 0 &&
              goo_canvas_item_get_n_children(gcanvas->group[CANVAS_GROUP_BG]) > 0)) {
    // there can only be a single item in there
    set_background(std::string());
    group_mask ^= 1 << CANVAS_GROUP_BG;
  }
  for(unsigned int group = CANVAS_GROUP_BG + 1; group < gcanvas->group.size() && group_mask != 0; group++) {
    if(group_mask & (1 << group)) {
      goo_canvas_item_remove(gcanvas->group[group]);
      gcanvas->group[group] = goo_canvas_group_new(root, nullptr);
      // restore z-order
      if(group < gcanvas->group.size() - 1)
        goo_canvas_item_lower(gcanvas->group[group], gcanvas->group[group + 1]);
      group_mask ^= 1 << group;
    }
  }
}

namespace {

/* check whether a given point is inside a polygon */
/* inpoly() taken from https://www.visibone.com/inpoly/ */
bool
inpoly(const canvas_item_info_poly *poly, int x, int y, int fuzziness)
{
  if(poly->num_points < 3)
    return false;

  lpos_t oldPos = poly->points[poly->num_points - 1];
  bool inside = false;

  for (unsigned i = 0 ; i < poly->num_points ; i++) {
    float x1, y1, x2, y2;
    lpos_t newPos = poly->points[i];

    // in contrast to the original algorithm we want to consider the corners as always inside the polygon
    float dist_sq = (x - newPos.x) * (x - newPos.x) + (y - newPos.y) * (y - newPos.y);
    if (dist_sq < fuzziness * fuzziness)
      return true;

    if (newPos.x > oldPos.x) {
      x1 = oldPos.x;
      x2 = newPos.x;
      y1 = oldPos.y;
      y2 = newPos.y;
    } else {
      x1 = newPos.x;
      x2 = oldPos.x;
      y1 = newPos.y;
      y2 = oldPos.y;
    }
    if ((newPos.x < x) == (x <= oldPos.x)          /* edge "open" at one end */
        && (y - y1) * (x2 - x1) < (y2 - y1) * (x - x1))
      inside = !inside;

    oldPos = newPos;
  }

  return inside;
}

class item_at_functor {
  const int x;
  const int y;
  const float ffuzziness;
public:
  const int fuzziness;
  const canvas_t * const canvas;
  inline item_at_functor(const lpos_t pos, float f, const canvas_t *cv)
    : x(pos.x), y(pos.y), ffuzziness(f), fuzziness(f), canvas(cv) {}
  bool operator()(const canvas_item_info_t *item) const;
};

bool item_at_functor::operator()(const canvas_item_info_t *item) const
{
  switch(item->type) {
  case CANVAS_ITEM_CIRCLE: {
    const canvas_item_info_circle *circle = static_cast<const canvas_item_info_circle *>(item);
    int xdist = circle->center.x - x;
    int ydist = circle->center.y - y;
    return (xdist * xdist + ydist * ydist <
           (static_cast<int>(circle->radius) + fuzziness) * (static_cast<int>(circle->radius) + fuzziness));
  }

  case CANVAS_ITEM_POLY: {
    const canvas_item_info_poly *poly = static_cast<const canvas_item_info_poly *>(item);
    return poly->get_segment(x, y, ffuzziness) || (poly->is_polygon && inpoly(poly, x, y, fuzziness));
  }
  }
  assert_unreachable();
}

gint
item_at_compare(gconstpointer i, gconstpointer f)
{
  const item_at_functor &fc = *static_cast<const item_at_functor *>(f);
  const canvas_item_t * const citem = static_cast<const canvas_item_t *>(i);

  const canvas_t::item_mapping_t::const_iterator it = fc.canvas->item_mapping.find(citem);
  if(it == fc.canvas->item_mapping.end()) {
    g_debug("item %p not in canvas map", citem);
    return -1;
  }

  return fc(it->second) ? 0 : -1;
}

struct g_list_deleter {
  inline void operator()(GList *list)
  { g_list_free(list); }
};

} // namespace

/* try to find the object at position x/y by searching through the */
/* item_info list */
canvas_item_t *canvas_t::get_item_at(lpos_t pos) const {
  /* convert all "fuzziness" into meters */
  const float fuzziness = EXTRA_FUZZINESS_METER +
    EXTRA_FUZZINESS_PIXEL / get_zoom();

  const item_at_functor fc(pos, fuzziness, this);
  GooCanvasBounds find_bounds;
  find_bounds.x1 = pos.x - fc.fuzziness;
  find_bounds.y1 = pos.y - fc.fuzziness;
  find_bounds.x2 = pos.x + fc.fuzziness;
  find_bounds.y2 = pos.y + fc.fuzziness;
  std::unique_ptr<GList, g_list_deleter> items(goo_canvas_get_items_in_area(GOO_CANVAS(widget),
                                                                            &find_bounds, TRUE,
                                                                            TRUE, FALSE));

  if (!items)
    return nullptr;

  // items of all kinds and layers are returned, now select the best matching one
  GList *item = g_list_find_custom(items.get(), &fc, item_at_compare);
  if (item == nullptr)
    return nullptr;

  return static_cast<canvas_item_t *>(item->data);
}

canvas_item_t *canvas_t::get_next_item_at(lpos_t pos, canvas_item_t *oldtop) const
{
  goo_canvas_item_lower(static_cast<GooCanvasItem *>(oldtop), nullptr);

  return get_item_at(pos);
}

canvas_item_circle *canvas_t::circle_new(canvas_group_t group, lpos_t c,
                                    float radius, int border,
                                    color_t fill_col, color_t border_col) {
  canvas_item_t *item =
    goo_canvas_ellipse_new(static_cast<canvas_goocanvas *>(this)->group[group],
                           c.x, c.y, radius, radius,
                           "line-width", static_cast<double>(border),
                           "stroke-color-rgba", border_col.rgba(),
                           "fill-color-rgba", fill_col.rgba(),
                           nullptr);

  if(CANVAS_SELECTABLE & (1<<group))
    (void) new canvas_item_info_circle(this, item, c, static_cast<unsigned int>(radius) + border);

  return static_cast<canvas_item_circle *>(item);
}

namespace {

class points_fill {
  GooCanvasPoints * const gpoints;
  unsigned int offs;
public:
  explicit inline points_fill(GooCanvasPoints *g) : gpoints(g), offs(0) {}
  inline void operator()(lpos_t p) {
    gpoints->coords[offs++] = p.x;
    gpoints->coords[offs++] = p.y;
  }
};

GooCanvasPoints *
canvas_points_create(const std::vector<lpos_t> &points)
{
  GooCanvasPoints *gpoints = goo_canvas_points_new(points.size());

  std::for_each(points.begin(), points.end(), points_fill(gpoints));

  return gpoints;
}

} // namespace

canvas_item_polyline *canvas_t::polyline_new(canvas_group_t group, const std::vector<lpos_t> &points,
                                      float width, color_t color)
{
  pointGuard cpoints(canvas_points_create(points));

  canvas_item_t *item =
    goo_canvas_polyline_new(static_cast<canvas_goocanvas *>(this)->group[group],
                            FALSE, 0, "points", cpoints.get(),
                            "line-width", static_cast<double>(width),
                            "stroke-color-rgba", color.rgba(),
                            "line-join", CAIRO_LINE_JOIN_ROUND,
			    "line-cap", CAIRO_LINE_CAP_ROUND,
                            nullptr);

  if(CANVAS_SELECTABLE & (1<<group))
    (void) new canvas_item_info_poly(this, item, false, width, points);

  return static_cast<canvas_item_polyline *>(item);
}

canvas_item_t *canvas_t::polygon_new(canvas_group_t group, const std::vector<lpos_t> &points,
                                     float width, color_t color, color_t fill) {
  pointGuard cpoints(canvas_points_create(points));

  canvas_item_t *item =
    goo_canvas_polyline_new(static_cast<canvas_goocanvas *>(this)->group[group],
                            TRUE, 0, "points", cpoints.get(),
                            "line-width", static_cast<double>(width),
                            "stroke-color-rgba", color.rgba(),
                            "fill-color-rgba", fill.rgba(),
			    "line-join", CAIRO_LINE_JOIN_ROUND,
			    "line-cap", CAIRO_LINE_CAP_ROUND,
                            nullptr);

  if(CANVAS_SELECTABLE & (1<<group))
    (void) new canvas_item_info_poly(this, item, true, width, points);

  return item;
}

/* place the image in pix centered on x/y on the canvas */
canvas_item_pixmap *canvas_t::image_new(canvas_group_t group, icon_item *icon, lpos_t pos,
                                        float scale)
{
  GdkPixbuf *pix = osm2go_platform::icon_pixmap(icon);
  int width = gdk_pixbuf_get_width(pix);
  int height = gdk_pixbuf_get_height(pix);
  GooCanvasItem *item =
      goo_canvas_image_new(static_cast<canvas_goocanvas *>(this)->group[group],
                           pix, pos.x / scale - width / 2.0f,
                           pos.y / scale - height / 2.0f, nullptr);
  goo_canvas_item_scale(item, scale, scale);

  if(CANVAS_SELECTABLE & (1<<group)) {
    int radius = 0.75f * scale * std::max(width, height);
    (void) new canvas_item_info_circle(this, item, pos, radius);
  }

  return reinterpret_cast<canvas_item_pixmap *>(item);
}

void canvas_item_t::operator delete(void *ptr) {
  if(G_LIKELY(ptr != nullptr))
    goo_canvas_item_remove(static_cast<GooCanvasItem *>(ptr));
}

/* ------------------------ accessing items ---------------------- */

void canvas_item_polyline::set_points(const std::vector<lpos_t> &points) {
  pointGuard cpoints(canvas_points_create(points));
  g_object_set(G_OBJECT(this), "points", cpoints.get(), nullptr);
}

void
canvas_item_circle::set_radius(float radius)
{
  g_object_set(G_OBJECT(this),
               "radius-x", static_cast<gdouble>(radius),
               "radius-y", static_cast<gdouble>(radius),
               nullptr);
}

void canvas_item_t::set_zoom_max(float zoom_max) {
  gdouble vis_thres = zoom_max;
  GooCanvasItemVisibility vis
    = GOO_CANVAS_ITEM_VISIBLE_ABOVE_THRESHOLD;
  if (vis_thres <= 0) {
    vis_thres = 0;
    vis = GOO_CANVAS_ITEM_VISIBLE;
  }
  g_object_set(G_OBJECT(this),
               "visibility", vis,
               "visibility-threshold", vis_thres,
               nullptr);
}

void canvas_item_t::set_dashed(float line_width, unsigned int dash_length_on,
                               unsigned int dash_length_off) {
  GooCanvasLineDash *dash;
  guint cap = CAIRO_LINE_CAP_BUTT;
  if(dash_length_on > line_width)
    cap = CAIRO_LINE_CAP_ROUND;

  dash = goo_canvas_line_dash_new(2, static_cast<gdouble>(dash_length_on),
                                  static_cast<gdouble>(dash_length_off));
  g_object_set(G_OBJECT(this),
               "line-dash", dash,
               "line-cap", cap,
               nullptr);
  goo_canvas_line_dash_unref(dash);
}

void canvas_item_t::set_user_data(map_item_t *data)
{
  g_object_set_data(G_OBJECT(this), "user data", data);
  destroy_connect(new map_item_destroyer(data));
}

map_item_t *canvas_item_t::get_user_data() {
  return static_cast<map_item_t *>(g_object_get_data(G_OBJECT(this), "user data"));
}

static void canvas_item_weak_notify(gpointer data, GObject *obj)
{
  canvas_item_destroyer *d = static_cast<canvas_item_destroyer *>(data);
  d->run(reinterpret_cast<canvas_item_t *>(obj));
  delete d;
}

void canvas_item_t::destroy_connect(canvas_item_destroyer *d)
{
  g_object_weak_ref(G_OBJECT(this), canvas_item_weak_notify, d);
}

bool canvas_goocanvas::isVisible(const lpos_t lpos) const
{
  // Viewport dimensions in canvas space

  /* get size of visible area in canvas units (meters) */
  const canvas_dimensions dim = get_viewport_dimensions();

  // Is the point still onscreen?
  osm2go_platform::screenpos s = scroll_get();

  return (lpos.x <= s.x() + dim.width / 2) &&
         (lpos.x >= s.x() - dim.width / 2) &&
         (lpos.y <= s.y() + dim.height / 2) &&
         (lpos.y >= s.y() - dim.height / 2);
}

bool canvas_t::ensureVisible(const lpos_t lpos)
{
  if(static_cast<canvas_goocanvas *>(this)->isVisible(lpos))
    return false;

  scroll_to(osm2go_platform::screenpos(lpos.x, lpos.y));

  return true;
}
