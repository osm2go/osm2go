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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
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
// casting type save
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
    group[gr] = goo_canvas_group_new(root, O2G_NULLPTR);

  g_signal_connect_swapped(GTK_OBJECT(widget), "destroy",
                           G_CALLBACK(canvas_delete), this);
}

/* ------------------------ accessing the canvas ---------------------- */

void canvas_t::set_background(canvas_color_t bg_color) {
  g_object_set(G_OBJECT(widget),
               "background-color-rgb", bg_color >> 8, O2G_NULLPTR);
}

void canvas_t::set_antialias(bool antialias) {
  GooCanvasItem *root = goo_canvas_get_root_item(GOO_CANVAS(widget));
  g_object_set(G_OBJECT(root), "antialias",
               antialias ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE, O2G_NULLPTR);
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
      printf("Removing %d children from group %d\n", children, group);
      while(children--)
	goo_canvas_item_remove_child(gcanvas->group[group], children);
    }
  }
}

canvas_item_t *canvas_t::circle_new(canvas_group_t group,
                                    int x, int y, unsigned int radius, int border,
                                    canvas_color_t fill_col, canvas_color_t border_col) {
  canvas_item_t *item =
    goo_canvas_ellipse_new(static_cast<canvas_goocanvas *>(this)->group[group],
                           x, y, radius, radius,
                           "line-width", static_cast<double>(border),
			   "stroke-color-rgba", border_col,
			   "fill-color-rgba", fill_col,
                           O2G_NULLPTR);

  if(CANVAS_SELECTABLE & (1<<group))
    (void) new canvas_item_info_circle(this, group, item, x, y, radius + border);

  return item;
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

GooCanvasPoints *canvas_points_create(const std::vector<lpos_t> &points) {
  GooCanvasPoints *gpoints = goo_canvas_points_new(points.size());

  std::for_each(points.begin(), points.end(), points_fill(gpoints));

  return gpoints;
}

canvas_item_t *canvas_t::polyline_new(canvas_group_t group, const std::vector<lpos_t> &points,
                                      unsigned int width, canvas_color_t color) {
  pointGuard cpoints(canvas_points_create(points));

  canvas_item_t *item =
    goo_canvas_polyline_new(static_cast<canvas_goocanvas *>(this)->group[group],
                            FALSE, 0, "points", cpoints.get(),
                            "line-width", static_cast<double>(width),
			    "stroke-color-rgba", color,
			    "line-join", CAIRO_LINE_JOIN_ROUND,
			    "line-cap", CAIRO_LINE_CAP_ROUND,
                            O2G_NULLPTR);

  if(CANVAS_SELECTABLE & (1<<group))
    (void) new canvas_item_info_poly(this, group, item, FALSE, width, points);

  return item;
}

canvas_item_t *canvas_t::polygon_new(canvas_group_t group, const std::vector<lpos_t> &points,
                                     unsigned int width, canvas_color_t color, canvas_color_t fill) {
  pointGuard cpoints(canvas_points_create(points));

  canvas_item_t *item =
    goo_canvas_polyline_new(static_cast<canvas_goocanvas *>(this)->group[group],
                            TRUE, 0, "points", cpoints.get(),
                            "line-width", static_cast<double>(width),
			    "stroke-color-rgba", color,
			    "fill-color-rgba", fill,
			    "line-join", CAIRO_LINE_JOIN_ROUND,
			    "line-cap", CAIRO_LINE_CAP_ROUND,
                            O2G_NULLPTR);

  if(CANVAS_SELECTABLE & (1<<group))
    (void) new canvas_item_info_poly(this, group, item, TRUE, width, points);

  return item;
}

/* place the image in pix centered on x/y on the canvas */
canvas_item_t *canvas_t::image_new(canvas_group_t group, GdkPixbuf *pix, int x,
                                   int y, float hscale, float vscale) {
  int width = gdk_pixbuf_get_width(pix);
  int height = gdk_pixbuf_get_height(pix);
  GooCanvasItem *item =
      goo_canvas_image_new(static_cast<canvas_goocanvas *>(this)->group[group],
                           pix, x / hscale - width / 2,
                           y / vscale - height / 2, O2G_NULLPTR);
  goo_canvas_item_scale(item, hscale, vscale);

  if(CANVAS_SELECTABLE & (1<<group)) {
    int radius = 0.75 * hscale * std::max(width, height); /* hscale and vscale are the same */
    (void) new canvas_item_info_circle(this, group, item, x, y, radius);
  }

  return item;
}

void canvas_item_t::operator delete(void *ptr) {
  if(G_LIKELY(ptr != O2G_NULLPTR))
    goo_canvas_item_remove(static_cast<GooCanvasItem *>(ptr));
}

/* ------------------------ accessing items ---------------------- */

void canvas_item_t::set_points(const std::vector<lpos_t> &points) {
  pointGuard cpoints(canvas_points_create(points));
  g_object_set(G_OBJECT(this), "points", cpoints.get(), O2G_NULLPTR);
}

void canvas_item_t::set_pos(lpos_t *lpos) {
  g_object_set(G_OBJECT(this),
               "center-x", static_cast<gdouble>(lpos->x),
               "center-y", static_cast<gdouble>(lpos->y),
               O2G_NULLPTR);
}

void canvas_item_t::set_radius(int radius) {
  g_object_set(G_OBJECT(this),
               "radius-x", static_cast<gdouble>(radius),
               "radius-y", static_cast<gdouble>(radius),
               O2G_NULLPTR);
}

void canvas_item_t::to_bottom() {
  GooCanvasItem *gitem = static_cast<GooCanvasItem *>(this);
  goo_canvas_item_lower(gitem, O2G_NULLPTR);
  canvas_t *canvas =
    static_cast<canvas_t *>(g_object_get_data(G_OBJECT(goo_canvas_item_get_canvas(gitem)),
                                              "canvas-pointer"));

  assert(canvas != O2G_NULLPTR);
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
               O2G_NULLPTR);
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
               O2G_NULLPTR);
  goo_canvas_line_dash_unref(dash);
}

void canvas_item_t::set_user_data(void *data) {
  g_object_set_data(G_OBJECT(this), "user data", data);
}

void *canvas_item_t::get_user_data() {
  return g_object_get_data(G_OBJECT(this), "user data");
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

void canvas_item_t::image_move(gint x, gint y, float hscale, float vscale) {

  g_object_set(G_OBJECT(this),
               "x", static_cast<gdouble>(x) / hscale,
               "y", static_cast<gdouble>(y) / vscale,
               O2G_NULLPTR);
}

int canvas_item_t::get_segment(lpos_t pos) const {
  GooCanvasPoints *points = O2G_NULLPTR;
  double line_width = 0;

  g_object_get(G_OBJECT(this),
	       "points", &points,
	       "line-width", &line_width,
               O2G_NULLPTR);

  if(!points) return -1;

  pointGuard cpoints(points);

  int retval = -1;
  double mindist = 100;
  for(int i = 0; i < cpoints->num_points - 1; i++) {

#define AX (cpoints->coords[2*i+0])
#define AY (cpoints->coords[2*i+1])
#define BX (cpoints->coords[2*i+2])
#define BY (cpoints->coords[2*i+3])
#define CX static_cast<double>(pos.x)
#define CY static_cast<double>(pos.y)

    double len2 = pow(BY-AY,2)+pow(BX-AX,2);
    double m = ((CX-AX)*(BX-AX)+(CY-AY)*(BY-AY)) / len2;

    /* this is a possible candidate */
    if((m >= 0.0) && (m <= 1.0)) {

      double n;
      if(fabs(BX-AX) > fabs(BY-AY))
	n = fabs(sqrt(len2) * (AY+m*(BY-AY)-CY)/(BX-AX));
      else
	n = fabs(sqrt(len2) * -(AX+m*(BX-AX)-CX)/(BY-AY));

      /* check if this is actually on the line and closer than anything */
      /* we found so far */
      if((n <= line_width/2) && (n < mindist)) {
	retval = i;
	mindist = n;
      }
    }
 }

  /* the last and first point are identical for polygons in osm2go. */
  /* goocanvas doesn't need that, but that's how OSM works and it saves */
  /* us from having to check the last->first connection for polygons */
  /* seperately */

  return retval;
}
