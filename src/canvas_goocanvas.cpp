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

#include "misc.h"

#include <cmath>
#include <cstring>
#include <goocanvas.h>

#include <osm2go_cpp.h>

#if __cplusplus >= 201103L
#include <type_traits>
static_assert(std::is_same<decltype(canvas_points_t::coords), decltype(GooCanvasPoints::coords)>::value,
              "coordinate type mismatch");
static_assert(offsetof(canvas_points_t, coords) == offsetof(GooCanvasPoints, coords),
              "coordinate offset mismatch");
#else
struct coord_check {
  inline void operator()(canvas_points_t &cp) {
    GooCanvasPoints gp;
    typeof(gp.coords) &gco = cp.coords;
    typeof(cp.coords) &cco = gp.coords;
    std::swap(gco, cco);

    // canvas_points_t is non-POD, but standard layout
    // the old gcc doesn't support offsetof here
    static_assert(offsetof(GooCanvasPoints, coords) == 0,
                  "coordinate offset mismatch");
    static_assert(sizeof(static_cast<canvas_points_t *>(O2G_NULLPTR)->coords) == sizeof(void *),
                  "coordinate size mismatch");
    static_assert(sizeof(canvas_points_t) == sizeof(void *),
                  "coordinate offset mismatch");
  }
};
#endif

// since struct _GooCanvasItem does not exist, but is defined as an interface type
// in the GooCanvas headers define it here and inherit from it to get the internal
// casting type save

struct _GooCanvasItem {
  inline canvas_item_t *toCanvas();
};
struct canvas_item_t : _GooCanvasItem {};

canvas_item_t *_GooCanvasItem::toCanvas()
{
  return static_cast<canvas_item_t *>(this);
}

struct canvas_goocanvas : public canvas_t {
  canvas_goocanvas();

  GooCanvasItem *group[CANVAS_GROUPS];

  std::vector<canvas_item_info_t *> item_info[CANVAS_GROUPS];
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
  int gr;
  for(gr = 0; gr < CANVAS_GROUPS; gr++)
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

void canvas_t::window2world(int x, int y, int &wx, int &wy) const {
  double sx = x, sy = y;
  goo_canvas_convert_from_pixels(GOO_CANVAS(widget), &sx, &sy);
  wx = sx; wy = sy;
}

canvas_item_t *canvas_t::get_item_at(int x, int y) const {
  return item_info_get_at(x, y);
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
                           O2G_NULLPTR)->toCanvas();

  if(CANVAS_SELECTABLE & (1<<group))
    (void) new canvas_item_info_circle(this, group, item, x, y, radius + border);

  return item;
}

canvas_points_t *canvas_points_new(unsigned int points) {
  return reinterpret_cast<canvas_points_t *>(goo_canvas_points_new(points));
}

void canvas_point_set_pos(canvas_points_t *points, unsigned int index, const lpos_t lpos) {
  points->coords[2*index+0] = lpos.x;
  points->coords[2*index+1] = lpos.y;
}

void canvas_points_free(canvas_points_t *points) {
  goo_canvas_points_unref(reinterpret_cast<GooCanvasPoints *>(points));
}

unsigned int canvas_points_num(const canvas_points_t *points) {
  return reinterpret_cast<const GooCanvasPoints *>(points)->num_points;
}

void canvas_point_get_lpos(const canvas_points_t *points, unsigned int index, lpos_t &lpos) {
  lpos.x = points->coords[2 * index + 0];
  lpos.y = points->coords[2 * index + 1];
}

canvas_item_t *canvas_t::polyline_new(canvas_group_t group, canvas_points_t *points,
                                      unsigned int width, canvas_color_t color) {
  canvas_item_t *item =
    goo_canvas_polyline_new(static_cast<canvas_goocanvas *>(this)->group[group],
                            FALSE, 0, "points", points,
                            "line-width", static_cast<double>(width),
			    "stroke-color-rgba", color,
			    "line-join", CAIRO_LINE_JOIN_ROUND,
			    "line-cap", CAIRO_LINE_CAP_ROUND,
                            O2G_NULLPTR)->toCanvas();

  if(CANVAS_SELECTABLE & (1<<group))
    (void) new canvas_item_info_poly(this, group, item, FALSE, width, points);

  return item;
}

canvas_item_t *canvas_t::polygon_new(canvas_group_t group, canvas_points_t *points,
                                     unsigned int width, canvas_color_t color, canvas_color_t fill) {
  canvas_item_t *item =
    goo_canvas_polyline_new(static_cast<canvas_goocanvas *>(this)->group[group],
                            TRUE, 0, "points", points,
                            "line-width", static_cast<double>(width),
			    "stroke-color-rgba", color,
			    "fill-color-rgba", fill,
			    "line-join", CAIRO_LINE_JOIN_ROUND,
			    "line-cap", CAIRO_LINE_CAP_ROUND,
                            O2G_NULLPTR)->toCanvas();

  if(CANVAS_SELECTABLE & (1<<group))
    (void) new canvas_item_info_poly(this, group, item, TRUE, width, points);

  return item;
}

/* place the image in pix centered on x/y on the canvas */
canvas_item_t *canvas_t::image_new(canvas_group_t group, GdkPixbuf *pix, int x,
                                   int y, float hscale, float vscale) {
  canvas_item_t *item =
      goo_canvas_image_new(static_cast<canvas_goocanvas *>(this)->group[group],
                           pix, x / hscale - gdk_pixbuf_get_width(pix) / 2,
                           y / vscale - gdk_pixbuf_get_height(pix) / 2, O2G_NULLPTR)->toCanvas();
  goo_canvas_item_scale(item, hscale, vscale);

  if(CANVAS_SELECTABLE & (1<<group)) {
    int radius = 0.75 * hscale * MAX(gdk_pixbuf_get_width(pix), gdk_pixbuf_get_height(pix)); /* hscale and vscale are the same */
    (void) new canvas_item_info_circle(this, group, item, x, y, radius);
  }

  return item;
}

void canvas_item_destroy(canvas_item_t *item) {
  goo_canvas_item_remove(item);
}

/* ------------------------ accessing items ---------------------- */

void canvas_item_set_points(canvas_item_t *item, canvas_points_t *points) {
  g_object_set(G_OBJECT(item), "points", points, O2G_NULLPTR);
}

void canvas_item_set_pos(canvas_item_t *item, lpos_t *lpos) {
  g_object_set(G_OBJECT(item),
               "center-x", static_cast<gdouble>(lpos->x),
               "center-y", static_cast<gdouble>(lpos->y),
               O2G_NULLPTR);
}

void canvas_item_set_radius(canvas_item_t *item, int radius) {
  g_object_set(G_OBJECT(item),
               "radius-x", static_cast<gdouble>(radius),
               "radius-y", static_cast<gdouble>(radius),
               O2G_NULLPTR);
}

void canvas_item_to_bottom(canvas_item_t *item) {
  GooCanvasItem *gitem = item;
  goo_canvas_item_lower(gitem, O2G_NULLPTR);
  canvas_t *canvas =
    static_cast<canvas_t *>(g_object_get_data(G_OBJECT(goo_canvas_item_get_canvas(gitem)),
                                              "canvas-pointer"));

  g_assert_nonnull(canvas);
  canvas->item_info_push(item);
}

void canvas_item_set_zoom_max(canvas_item_t *item, float zoom_max) {
  gdouble vis_thres = zoom_max;
  GooCanvasItemVisibility vis
    = GOO_CANVAS_ITEM_VISIBLE_ABOVE_THRESHOLD;
  if (vis_thres < 0) {
    vis_thres = 0;
    vis = GOO_CANVAS_ITEM_VISIBLE;
  }
  g_object_set(G_OBJECT(item),
               "visibility", vis,
               "visibility-threshold", vis_thres,
               O2G_NULLPTR);
}

void canvas_item_set_dashed(canvas_item_t *item, unsigned int line_width,
                            unsigned int dash_length_on, guint dash_length_off) {
  GooCanvasLineDash *dash;
  gfloat off_len = dash_length_off;
  gfloat on_len = dash_length_on;
  guint cap = CAIRO_LINE_CAP_BUTT;
  if(dash_length_on > line_width)
    cap = CAIRO_LINE_CAP_ROUND;

  dash = goo_canvas_line_dash_new(2, on_len, off_len, 0);
  g_object_set(G_OBJECT(item),
               "line-dash", dash,
               "line-cap", cap,
               O2G_NULLPTR);
  goo_canvas_line_dash_unref(dash);
}

void canvas_item_set_user_data(canvas_item_t *item, void *data) {
  g_object_set_data(G_OBJECT(item), "user data", data);
}

void *canvas_item_get_user_data(canvas_item_t *item) {
  return g_object_get_data(G_OBJECT(item), "user data");
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

void canvas_item_destroy_connect(canvas_item_t *item, void(*c_handler)(void *),
                                 void *data) {
  g_object_weak_ref(G_OBJECT(item), canvas_item_weak_notify,
                    new weak_t(c_handler, data));
}

void canvas_image_move(canvas_item_t *item, gint x, gint y,
		       float hscale, float vscale) {

  g_object_set(G_OBJECT(item),
               "x", static_cast<gdouble>(x) / hscale,
               "y", static_cast<gdouble>(y) / vscale,
               O2G_NULLPTR);
}

int canvas_item_get_segment(canvas_item_t *item, int x, int y) {

  canvas_points_t *points = O2G_NULLPTR;
  double line_width = 0;

  g_object_get(G_OBJECT(item),
	       "points", &points,
	       "line-width", &line_width,
               O2G_NULLPTR);

  if(!points) return -1;

  gint retval = -1, i;
  double mindist = 100;
  const int max = canvas_points_num(points);
  for(i = 0; i < max - 1; i++) {

#define AX (points->coords[2*i+0])
#define AY (points->coords[2*i+1])
#define BX (points->coords[2*i+2])
#define BY (points->coords[2*i+3])
#define CX static_cast<double>(x)
#define CY static_cast<double>(y)

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

void canvas_item_get_segment_pos(canvas_item_t *item, int seg,
                                 int &x0, int &y0, int &x1, int &y1) {
  printf("get segment %d of item %p\n", seg, item);

  canvas_points_t *points = O2G_NULLPTR;
  g_object_get(G_OBJECT(item), "points", &points, O2G_NULLPTR);

  g_assert_nonnull(points);
  g_assert_cmpint(seg, <, canvas_points_num(points) - 1);

  x0 = points->coords[2 * seg + 0];
  y0 = points->coords[2 * seg + 1];
  x1 = points->coords[2 * seg + 2];
  y1 = points->coords[2 * seg + 3];
}
