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

#include <osm2go_cpp.h>

/* ------------------- creating and destroying the canvas ----------------- */

static void canvas_delete(canvas_t *canvas) {
  delete canvas;
}

/* create a new canvas */
canvas_t::canvas_t()
  : widget(goo_canvas_new())
{
  g_object_set_data(G_OBJECT(widget), "canvas-pointer", this);

  g_object_set(G_OBJECT(widget), "anchor", GTK_ANCHOR_CENTER, O2G_NULLPTR);

  GooCanvasItem *root = goo_canvas_get_root_item(GOO_CANVAS(widget));

  /* create the groups */
  int gr;
  for(gr = 0; gr < CANVAS_GROUPS; gr++)
    group[gr] = goo_canvas_group_new(root, O2G_NULLPTR);


  g_signal_connect_swapped(GTK_OBJECT(widget), "destroy",
                           G_CALLBACK(canvas_delete), this);
}

GtkWidget *canvas_get_widget(canvas_t *canvas) {
  return canvas->widget;
}

/* ------------------------ accessing the canvas ---------------------- */

void canvas_set_background(canvas_t *canvas, canvas_color_t bg_color) {
  g_object_set(G_OBJECT(canvas->widget),
	       "background-color-rgb", bg_color >> 8,
               O2G_NULLPTR);
}

void canvas_set_antialias(canvas_t *canvas, gboolean antialias) {
  GooCanvasItem *root = goo_canvas_get_root_item(GOO_CANVAS(canvas->widget));
  g_object_set(G_OBJECT(root), "antialias",
               antialias ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE, O2G_NULLPTR);
}

void canvas_window2world(canvas_t *canvas,
			 gint x, gint y, gint *wx, gint *wy) {
  double sx = x, sy = y;
  goo_canvas_convert_from_pixels(GOO_CANVAS(canvas->widget), &sx, &sy);
  *wx = sx; *wy = sy;
}

canvas_item_t *canvas_get_item_at(canvas_t *canvas, gint x, gint y) {
  return canvas_item_info_get_at(canvas, x, y);
}

void canvas_set_zoom(canvas_t *canvas, gdouble zoom) {
  goo_canvas_set_scale(GOO_CANVAS(canvas->widget), zoom);
}

gdouble canvas_get_zoom(canvas_t *canvas) {
  return goo_canvas_get_scale(GOO_CANVAS(canvas->widget));
}

gdouble canvas_get_viewport_width(canvas_t *canvas, canvas_unit_t unit) {
  // Canvas viewport dimensions

  GtkAllocation *a = &(canvas->widget)->allocation;
  if(unit == CANVAS_UNIT_PIXEL) return a->width;

  /* convert to meters by dividing by zoom */
  gdouble zoom = goo_canvas_get_scale(GOO_CANVAS(canvas->widget));
  return a->width / zoom;
}

gdouble canvas_get_viewport_height(canvas_t *canvas, canvas_unit_t unit) {
  // Canvas viewport dimensions
  GtkAllocation *a = &(canvas->widget)->allocation;
  if(unit == CANVAS_UNIT_PIXEL) return a->height;

  /* convert to meters by dividing by zoom */
  gdouble zoom = goo_canvas_get_scale(GOO_CANVAS(canvas->widget));
  return a->height / zoom;
}

/* get scroll position in meters/pixels */
void canvas_scroll_get(canvas_t *canvas, canvas_unit_t unit,
		       gint *sx, gint *sy) {
  gdouble zoom = goo_canvas_get_scale(GOO_CANVAS(canvas->widget));

  GtkAdjustment *hadj = ((struct _GooCanvas*)(canvas->widget))->hadjustment;
  GtkAdjustment *vadj = ((struct _GooCanvas*)(canvas->widget))->vadjustment;

  gdouble hs = gtk_adjustment_get_value(hadj);
  gdouble vs = gtk_adjustment_get_value(vadj);
  goo_canvas_convert_from_pixels(GOO_CANVAS(canvas->widget), &hs, &vs);

  /* convert to position relative to screen center */
  hs += canvas->widget->allocation.width/(2*zoom);
  vs += canvas->widget->allocation.height/(2*zoom);

  if(unit == CANVAS_UNIT_PIXEL) {
    /* make values zoom independant */
    *sx = hs * zoom;
    *sy = vs * zoom;
  } else {
    *sx = hs;
    *sy = vs;
  }
}

/* set scroll position in meters/pixels */
void canvas_scroll_to(canvas_t *canvas, canvas_unit_t unit, gint sx, gint sy) {
  gdouble zoom = goo_canvas_get_scale(GOO_CANVAS(canvas->widget));

  if(unit != CANVAS_UNIT_METER) {
    sx /= zoom; sy /= zoom;
  }

  /* adjust to screen center */
  sx -= canvas->widget->allocation.width/(2*zoom);
  sy -= canvas->widget->allocation.height/(2*zoom);

  goo_canvas_scroll_to(GOO_CANVAS(canvas->widget), sx, sy);
}

void canvas_set_bounds(canvas_t *canvas, gint minx, gint miny,
		       gint maxx, gint maxy) {
  goo_canvas_set_bounds(GOO_CANVAS(canvas->widget), minx, miny, maxx, maxy);
}

/* ------------------- creating and destroying objects ---------------- */

void canvas_erase(canvas_t *canvas, gint group_mask) {
  int group;
  for(group=0;group<CANVAS_GROUPS;group++) {

    if(group_mask & (1<<group)) {
      gint children = goo_canvas_item_get_n_children(canvas->group[group]);
      printf("Removing %d children from group %d\n", children, group);
      while(children--)
	goo_canvas_item_remove_child(canvas->group[group], children);
    }
  }
}


canvas_item_t *canvas_circle_new(canvas_t *canvas, canvas_group_t group,
			 gint x, gint y, gint radius, gint border,
			 canvas_color_t fill_col, canvas_color_t border_col) {

  canvas_item_t *item =
    goo_canvas_ellipse_new(canvas->group[group],
			   (gdouble) x, (gdouble) y,
			   (gdouble) radius, (gdouble) radius,
			   "line-width", (double)border,
			   "stroke-color-rgba", border_col,
			   "fill-color-rgba", fill_col,
                           O2G_NULLPTR);

  if(CANVAS_SELECTABLE & (1<<group))
    canvas_item_info_attach_circle(canvas, group, item, x, y, radius + border);

  return item;
}

canvas_points_t *canvas_points_new(gint points) {
  return goo_canvas_points_new(points);
}

void canvas_point_set_pos(canvas_points_t *points, gint index, const lpos_t *lpos) {
  points->coords[2*index+0] = lpos->x;
  points->coords[2*index+1] = lpos->y;
}

void canvas_points_free(canvas_points_t *points) {
  goo_canvas_points_unref(points);
}

gint canvas_points_num(canvas_points_t *points) {
  return points->num_points;
}

void canvas_point_get_lpos(canvas_points_t *points, gint index, lpos_t *lpos) {
  lpos->x = points->coords[2*index+0];
  lpos->y = points->coords[2*index+1];
}

canvas_item_t *canvas_polyline_new(canvas_t *canvas, canvas_group_t group,
		  canvas_points_t *points, gint width, canvas_color_t color) {
  canvas_item_t *item =
    goo_canvas_polyline_new(canvas->group[group], FALSE, 0,
			    "points", points,
			    "line-width", (double)width,
			    "stroke-color-rgba", color,
			    "line-join", CAIRO_LINE_JOIN_ROUND,
			    "line-cap", CAIRO_LINE_CAP_ROUND,
                            O2G_NULLPTR);

  if(CANVAS_SELECTABLE & (1<<group))
    canvas_item_info_attach_poly(canvas, group, item, FALSE, points, width);

  return item;
}

canvas_item_t *canvas_polygon_new(canvas_t *canvas, canvas_group_t group,
		  canvas_points_t *points, gint width, canvas_color_t color,
				  canvas_color_t fill) {
  canvas_item_t *item =
    goo_canvas_polyline_new(canvas->group[group], TRUE, 0,
			    "points", points,
			    "line-width", (double)width,
			    "stroke-color-rgba", color,
			    "fill-color-rgba", fill,
			    "line-join", CAIRO_LINE_JOIN_ROUND,
			    "line-cap", CAIRO_LINE_CAP_ROUND,
                            O2G_NULLPTR);

  if(CANVAS_SELECTABLE & (1<<group))
    canvas_item_info_attach_poly(canvas, group, item, TRUE, points, width);

  return item;
}

/* place the image in pix centered on x/y on the canvas */
canvas_item_t *canvas_image_new(canvas_t *canvas, canvas_group_t group,
		GdkPixbuf *pix, gint x, gint y, float hscale, float vscale) {

  canvas_item_t *item = goo_canvas_image_new(canvas->group[group], pix,
			                     x/hscale - gdk_pixbuf_get_width(pix)/2,
                                             y/vscale - gdk_pixbuf_get_height(pix)/2, O2G_NULLPTR);
  goo_canvas_item_scale(item, hscale, vscale);

  if(CANVAS_SELECTABLE & (1<<group)) {
    gint radius = 0.75 * hscale * MAX(gdk_pixbuf_get_width(pix), gdk_pixbuf_get_height(pix)); /* hscale and vscale are the same */
    canvas_item_info_attach_circle(canvas, group, item, x, y, radius);
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
	       "center-x", (gdouble)lpos->x,
	       "center-y", (gdouble)lpos->y,
               O2G_NULLPTR);
}

void canvas_item_set_radius(canvas_item_t *item, gint radius) {
  g_object_set(G_OBJECT(item),
	       "radius-x", (gdouble)radius,
	       "radius-y", (gdouble)radius,
               O2G_NULLPTR);
}

void canvas_item_to_bottom(canvas_item_t *item) {


  goo_canvas_item_lower(item, O2G_NULLPTR);
  canvas_t *canvas =
    static_cast<canvas_t *>(g_object_get_data(G_OBJECT(goo_canvas_item_get_canvas(item)),
                                              "canvas-pointer"));

  g_assert_nonnull(canvas);
  canvas_item_info_push(canvas, item);
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

void canvas_item_set_dashed(canvas_item_t *item,
                            gint line_width, guint dash_length_on,
                            guint dash_length_off) {
  GooCanvasLineDash *dash;
  gfloat off_len = dash_length_off;
  gfloat on_len = dash_length_on;
  guint cap = CAIRO_LINE_CAP_BUTT;
  if ((gint)dash_length_on > line_width) {
    cap = CAIRO_LINE_CAP_ROUND;
  }
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

struct weak_t {
  weak_t(GCallback cb, gpointer d)
    : c_handler(cb)
    , data(d)
  {}

  const GCallback c_handler;
  gpointer const data;
};

static void canvas_item_weak_notify(gpointer data, GObject *) {
  weak_t *weak = static_cast<weak_t *>(data);

  ((void(*)(GtkWidget*, gpointer))weak->c_handler) (O2G_NULLPTR, weak->data);
  delete weak;
}

void canvas_item_destroy_connect(canvas_item_t *item,
				 GCallback c_handler, gpointer data) {
  weak_t *weak = new weak_t(c_handler, data);

  g_object_weak_ref(G_OBJECT(item), canvas_item_weak_notify, weak);
}

void canvas_image_move(canvas_item_t *item, gint x, gint y,
		       float hscale, float vscale) {

  g_object_set(G_OBJECT(item),
	       "x", (gdouble)x / hscale,
	       "y", (gdouble)y / vscale,
               O2G_NULLPTR);
}

/* get the polygon/polyway segment a certain coordinate is over */
gint canvas_item_get_segment(canvas_item_t *item, gint x, gint y) {

  canvas_points_t *points = O2G_NULLPTR;
  double line_width = 0;

  g_object_get(G_OBJECT(item),
	       "points", &points,
	       "line-width", &line_width,
               O2G_NULLPTR);

  if(!points) return -1;

  gint retval = -1, i;
  double mindist = 100;
  for(i=0;i<points->num_points-1;i++) {

#define AX (points->coords[2*i+0])
#define AY (points->coords[2*i+1])
#define BX (points->coords[2*i+2])
#define BY (points->coords[2*i+3])
#define CX ((double)x)
#define CY ((double)y)

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

void canvas_item_get_segment_pos(canvas_item_t *item, gint seg,
				 gint *x0, gint *y0, gint *x1, gint *y1) {
  printf("get segment %d of item %p\n", seg, item);

  canvas_points_t *points = O2G_NULLPTR;
  g_object_get(G_OBJECT(item), "points", &points, O2G_NULLPTR);

  g_assert_nonnull(points);
  g_assert_cmpint(seg, <, points->num_points-1);

  *x0 = points->coords[2 * seg + 0];
  *y0 = points->coords[2 * seg + 1];
  *x1 = points->coords[2 * seg + 2];
  *y1 = points->coords[2 * seg + 3];
}
