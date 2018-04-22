/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
 *
 * This file is part of OSM2Go.
 *
 * OSM2Go is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OSM2Go is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
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

#include "canvas.h"
#include "canvas_p.h"

#include "misc.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <goocanvas.h>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include "osm2go_stl.h"

struct canvas_points_deleter {
  void operator()(void *ptr) {
    goo_canvas_points_unref(static_cast<GooCanvasPoints *>(ptr));
  }
};

typedef std::unique_ptr<GooCanvasPoints, canvas_points_deleter> pointGuard;

// since struct _GooCanvasItem does not exist, but is defined as an interface type
// in the GooCanvas headers define it here and inherit from it to get the internal
// casting type safe
struct _GooCanvasItem : public canvas_item_t {
};

struct canvas_goocanvas : public canvas_t {
  canvas_goocanvas();

  std::array<GooCanvasItem *, CANVAS_GROUPS> group;

  std::array<std::vector<canvas_item_info_t *>, CANVAS_GROUPS> item_info;
};

canvas_t *canvas_t::create() {
  return new canvas_goocanvas();
}

/* ------------------- creating and destroying the canvas ----------------- */

static void canvas_delete(canvas_t *canvas) {
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

  g_object_set_data(w, "canvas-pointer", this);

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

lpos_t canvas_t::window2world(int x, int y) const {
  double sx = x, sy = y;
  goo_canvas_convert_from_pixels(GOO_CANVAS(widget), &sx, &sy);
  return lpos_t(sx, sy);
}

void canvas_t::set_zoom(gdouble zoom) {
  goo_canvas_set_scale(GOO_CANVAS(widget), zoom);
}

double canvas_t::get_zoom() const {
  return goo_canvas_get_scale(GOO_CANVAS(widget));
}

canvas_dimensions canvas_t::get_viewport_dimensions(canvas_unit_t unit) const {
  // Canvas viewport dimensions
  canvas_dimensions ret;

  const GtkAllocation &a = widget->allocation;
  if(unit == canvas_t::UNIT_PIXEL) {
    ret.width = a.width;
    ret.height = a.height;
  } else {
    /* convert to meters by dividing by zoom */
    gdouble zoom = goo_canvas_get_scale(GOO_CANVAS(widget));
    ret.width = a.width / zoom;
    ret.height = a.height / zoom;
  }
  return ret;
}

/* get scroll position in meters/pixels */
void canvas_t::scroll_get(canvas_unit_t unit, int &sx, int &sy) const {
  GooCanvas *gc = GOO_CANVAS(widget);
  gdouble zoom = goo_canvas_get_scale(gc);

  gdouble hs = gtk_adjustment_get_value(gc->hadjustment);
  gdouble vs = gtk_adjustment_get_value(gc->vadjustment);
  goo_canvas_convert_from_pixels(gc, &hs, &vs);

  /* convert to position relative to screen center */
  hs += widget->allocation.width/(2*zoom);
  vs += widget->allocation.height/(2*zoom);

  if(unit == canvas_t::UNIT_PIXEL) {
    /* make values zoom independant */
    sx = hs * zoom;
    sy = vs * zoom;
  } else {
    sx = hs;
    sy = vs;
  }
}

/* set scroll position in meters/pixels */
void canvas_t::scroll_to(canvas_unit_t unit, int sx, int sy) {
  gdouble zoom = goo_canvas_get_scale(GOO_CANVAS(widget));

  if(unit != canvas_t::UNIT_METER) {
    sx /= zoom; sy /= zoom;
  }

  /* adjust to screen center */
  sx -= widget->allocation.width / (2 * zoom);
  sy -= widget->allocation.height / (2 * zoom);

  goo_canvas_scroll_to(GOO_CANVAS(widget), sx, sy);
}

void canvas_t::set_bounds(int minx, int miny, int maxx, int maxy) {
  goo_canvas_set_bounds(GOO_CANVAS(widget), minx, miny, maxx, maxy);
}

/* ------------------- creating and destroying objects ---------------- */

void canvas_t::erase(unsigned int group_mask) {
  int group;

  canvas_goocanvas *gcanvas = static_cast<canvas_goocanvas *>(this);

  for(group=0;group<CANVAS_GROUPS;group++) {

    if(group_mask & (1<<group)) {
      gint children = goo_canvas_item_get_n_children(gcanvas->group[group]);
      g_debug("Removing %d children from group %d", children, group);
      while(children--)
	goo_canvas_item_remove_child(gcanvas->group[group], children);
    }
  }
}

/* check whether a given point is inside a polygon */
/* inpoly() taken from https://www.visibone.com/inpoly/ */
static bool inpoly(const canvas_item_info_poly *poly, int x, int y) {
  int xold, yold;

  if(poly->num_points < 3)
    return false;

  xold = poly->points[poly->num_points - 1].x;
  yold = poly->points[poly->num_points - 1].y;
  bool inside = false;
  for (unsigned i = 0 ; i < poly->num_points ; i++) {
    int x1, y1, x2, y2;
    int xnew = poly->points[i].x;
    int ynew = poly->points[i].y;

    if (xnew > xold) {
      x1 = xold;
      x2 = xnew;
      y1 = yold;
      y2 = ynew;
    } else {
      x1 = xnew;
      x2 = xold;
      y1 = ynew;
      y2 = yold;
    }
    if ((xnew < x) == (x <= xold)          /* edge "open" at one end */
        && (y - y1) * (long)(x2 - x1) < (y2 - y1) * (long)(x - x1))
      inside = !inside;

    xold = xnew;
    yold = ynew;
  }

  return inside;
}

/* get the polygon/polyway segment a certain coordinate is over */
static int canvas_item_info_get_segment(const canvas_item_info_poly *item,
                                        int x, int y, int fuzziness) {
  int retval = -1;
  float mindist = static_cast<float>(item->width) / 2 + fuzziness;
  for(unsigned int i = 0; i < item->num_points - 1; i++) {

#define AX (item->points[i].x)
#define AY (item->points[i].y)
#define BX (item->points[i+1].x)
#define BY (item->points[i+1].y)
#define CX static_cast<double>(x)
#define CY static_cast<double>(y)

    float len = pow(BY-AY,2)+pow(BX-AX,2);
    float m = ((CX-AX)*(BX-AX)+(CY-AY)*(BY-AY)) / len;

    /* this is a possible candidate */
    if((m >= 0.0) && (m <= 1.0)) {

      float n;
      if(abs(BX-AX) > abs(BY-AY))
        n = fabs(sqrt(len) * (AY+m*(BY-AY)-CY)/(BX-AX));
      else
        n = fabs(sqrt(len) * -(AX+m*(BX-AX)-CX)/(BY-AY));

      /* check if this is actually on the line and closer than anything */
      /* we found so far */
      if(n < mindist) {
        retval = i;
        mindist = n;
      }
    }
 }
#undef AX
#undef AY
#undef BX
#undef BY
#undef CX
#undef CY

  /* the last and first point are identical for polygons in osm2go. */
  /* goocanvas doesn't need that, but that's how OSM works and it saves */
  /* us from having to check the last->first connection for polygons */
  /* seperately */

  return retval;
}

struct item_at_functor {
  const int x;
  const int y;
  const int fuzziness;
  const canvas_t * const canvas;
  item_at_functor(const lpos_t pos, int f, const canvas_t *cv)
    : x(pos.x), y(pos.y), fuzziness(f), canvas(cv) {}
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
           (static_cast<int>(circle->r) + fuzziness) * (static_cast<int>(circle->r) + fuzziness));
  }

  case CANVAS_ITEM_POLY: {
    const canvas_item_info_poly *poly = static_cast<const canvas_item_info_poly *>(item);
    int on_segment = canvas_item_info_get_segment(poly, x, y, fuzziness);
    return ((on_segment >= 0) || (poly->is_polygon && inpoly(poly, x, y)));
  }
  }
  assert_unreachable();
}

static gint item_at_compare(gconstpointer i, gconstpointer f)
{
  const item_at_functor &fc = *static_cast<const item_at_functor *>(f);
  const canvas_item_t * const citem = static_cast<const canvas_item_t *>(i);

  const canvas_t::item_mapping_t::const_iterator it = fc.canvas->item_mapping.find(const_cast<canvas_item_t *>(citem));
  if(it == fc.canvas->item_mapping.end()) {
    printf("item %p not in canvas map\n", citem);
    return -1;
  }

  return fc(it->second) ? 0 : -1;
}

struct g_list_deleter {
  inline void operator()(GList *list)
  { g_list_free(list); }
};

/* try to find the object at position x/y by searching through the */
/* item_info list */
canvas_item_t *canvas_t::get_item_at(lpos_t pos) const {
  /* convert all "fuzziness" into meters */
  int fuzziness = EXTRA_FUZZINESS_METER +
    EXTRA_FUZZINESS_PIXEL / get_zoom();

  const item_at_functor fc(pos, fuzziness, this);
  GooCanvasBounds find_bounds;
  find_bounds.x1 = pos.x - fuzziness;
  find_bounds.y1 = pos.y - fuzziness;
  find_bounds.x2 = pos.x + fuzziness;
  find_bounds.y2 = pos.y + fuzziness;
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

canvas_item_circle *canvas_t::circle_new(canvas_group_t group,
                                    int x, int y, unsigned int radius, int border,
                                    color_t fill_col, color_t border_col) {
  canvas_item_t *item =
    goo_canvas_ellipse_new(static_cast<canvas_goocanvas *>(this)->group[group],
                           x, y, radius, radius,
                           "line-width", static_cast<double>(border),
                           "stroke-color-rgba", border_col.rgba(),
                           "fill-color-rgba", fill_col.rgba(),
                           nullptr);

  if(CANVAS_SELECTABLE & (1<<group))
    (void) new canvas_item_info_circle(this, group, item, x, y, radius + border);

  return static_cast<canvas_item_circle *>(item);
}

struct points_fill {
  GooCanvasPoints * const gpoints;
  unsigned int offs;
  points_fill(GooCanvasPoints *g) : gpoints(g), offs(0) {}
  inline void operator()(lpos_t p) {
    gpoints->coords[offs++] = p.x;
    gpoints->coords[offs++] = p.y;
  }
};

static GooCanvasPoints *canvas_points_create(const std::vector<lpos_t> &points) {
  GooCanvasPoints *gpoints = goo_canvas_points_new(points.size());

  std::for_each(points.begin(), points.end(), points_fill(gpoints));

  return gpoints;
}

canvas_item_polyline *canvas_t::polyline_new(canvas_group_t group, const std::vector<lpos_t> &points,
                                      unsigned int width, color_t color) {
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
    (void) new canvas_item_info_poly(this, group, item, false, width, points);

  return static_cast<canvas_item_polyline *>(item);
}

canvas_item_t *canvas_t::polygon_new(canvas_group_t group, const std::vector<lpos_t> &points,
                                     unsigned int width, color_t color, color_t fill) {
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
    (void) new canvas_item_info_poly(this, group, item, true, width, points);

  return item;
}

/* place the image in pix centered on x/y on the canvas */
canvas_item_pixmap *canvas_t::image_new(canvas_group_t group, icon_t::Pixmap pix, int x,
                                   int y, float hscale, float vscale) {
  int width = gdk_pixbuf_get_width(pix);
  int height = gdk_pixbuf_get_height(pix);
  GooCanvasItem *item =
      goo_canvas_image_new(static_cast<canvas_goocanvas *>(this)->group[group],
                           pix, x / hscale - width / 2,
                           y / vscale - height / 2, nullptr);
  goo_canvas_item_scale(item, hscale, vscale);

  if(CANVAS_SELECTABLE & (1<<group)) {
    int radius = 0.75 * hscale * std::max(width, height); /* hscale and vscale are the same */
    (void) new canvas_item_info_circle(this, group, item, x, y, radius);
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

void canvas_item_circle::set_radius(int radius) {
  g_object_set(G_OBJECT(this),
               "radius-x", static_cast<gdouble>(radius),
               "radius-y", static_cast<gdouble>(radius),
               nullptr);
}

void canvas_item_t::to_bottom() {
  GooCanvasItem *gitem = static_cast<GooCanvasItem *>(this);
  goo_canvas_item_lower(gitem, nullptr);
  canvas_t *canvas =
    static_cast<canvas_t *>(g_object_get_data(G_OBJECT(goo_canvas_item_get_canvas(gitem)),
                                              "canvas-pointer"));

  assert(canvas != nullptr);
  canvas->item_info_push(this);
}

void canvas_item_t::set_zoom_max(float zoom_max) {
  gdouble vis_thres = zoom_max;
  GooCanvasItemVisibility vis
    = GOO_CANVAS_ITEM_VISIBLE_ABOVE_THRESHOLD;
  if (vis_thres < 0) {
    vis_thres = 0;
    vis = GOO_CANVAS_ITEM_VISIBLE;
  }
  g_object_set(G_OBJECT(this),
               "visibility", vis,
               "visibility-threshold", vis_thres,
               nullptr);
}

void canvas_item_t::set_dashed(unsigned int line_width, unsigned int dash_length_on,
                               unsigned int dash_length_off) {
  GooCanvasLineDash *dash;
  gfloat off_len = dash_length_off;
  gfloat on_len = dash_length_on;
  guint cap = CAIRO_LINE_CAP_BUTT;
  if(dash_length_on > line_width)
    cap = CAIRO_LINE_CAP_ROUND;

  dash = goo_canvas_line_dash_new(2, on_len, off_len, 0);
  g_object_set(G_OBJECT(this),
               "line-dash", dash,
               "line-cap", cap,
               nullptr);
  goo_canvas_line_dash_unref(dash);
}

void canvas_item_t::set_user_data(map_item_t *data, void (*c_handler)(map_item_t *)) {
  g_object_set_data(G_OBJECT(this), "user data", data);
  destroy_connect(reinterpret_cast<void (*)(void *)>(c_handler), data);
}

map_item_t *canvas_item_t::get_user_data() {
  return static_cast<map_item_t *>(g_object_get_data(G_OBJECT(this), "user data"));
}

class weak_t {
public:
  weak_t(void(*cb)(gpointer), gpointer d)
    : c_handler(cb)
    , data(d)
  {}
  ~weak_t()
  {
    c_handler(data);
  }

private:
  void(* const c_handler)(gpointer);
  gpointer const data;
};

static void canvas_item_weak_notify(gpointer data, GObject *) {
  delete static_cast<weak_t *>(data);
}

void canvas_item_t::destroy_connect(void (*c_handler)(void *), void *data) {
  g_object_weak_ref(G_OBJECT(this), canvas_item_weak_notify,
                    new weak_t(c_handler, data));
}

void canvas_item_pixmap::image_move(int x, int y, float hscale, float vscale) {
  g_object_set(G_OBJECT(this),
               "x", static_cast<gdouble>(x) / hscale,
               "y", static_cast<gdouble>(y) / vscale,
               nullptr);
}

int canvas_item_t::get_segment(lpos_t pos) const {
  GooCanvasPoints *points = nullptr;
  double line_width = 0;

  g_object_get(G_OBJECT(this),
	       "points", &points,
	       "line-width", &line_width,
               nullptr);

  if(!points) return -1;

  pointGuard cpoints(points);

  int retval = -1;
  double mindist = line_width / 2;
  for(int i = 0; i < cpoints->num_points - 1; i++) {

#define AX (cpoints->coords[2*i+0])
#define AY (cpoints->coords[2*i+1])
#define BX (cpoints->coords[2*i+2])
#define BY (cpoints->coords[2*i+3])
#define CX static_cast<double>(pos.x)
#define CY static_cast<double>(pos.y)

    double len = pow(BY-AY,2)+pow(BX-AX,2);
    double m = ((CX-AX)*(BX-AX)+(CY-AY)*(BY-AY)) / len;

    /* this is a possible candidate */
    if((m >= 0.0) && (m <= 1.0)) {

      double n;
      if(fabs(BX-AX) > fabs(BY-AY))
        n = fabs(sqrt(len) * (AY+m*(BY-AY)-CY)/(BX-AX));
      else
        n = fabs(sqrt(len) * -(AX+m*(BX-AX)-CX)/(BY-AY));

      /* check if this is actually on the line and closer than anything */
      /* we found so far */
      if(n < mindist) {
        retval = i;
        mindist = n;
      }
    }
 }
#undef AX
#undef AY
#undef BX
#undef BY
#undef CX
#undef CY

  /* the last and first point are identical for polygons in osm2go. */
  /* goocanvas doesn't need that, but that's how OSM works and it saves */
  /* us from having to check the last->first connection for polygons */
  /* seperately */

  return retval;
}
