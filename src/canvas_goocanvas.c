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

#ifndef USE_GOOCANVAS
#error "Config error!"
#endif

#include "appdata.h"

canvas_item_t *canvas_circle_new(map_t *map, canvas_group_t group, 
			 gint x, gint y, gint radius, gint border, 
			 canvas_color_t fill_col, canvas_color_t border_col) {
  return goo_canvas_ellipse_new(map->group[group],
				(gdouble) x, (gdouble) y,
				(gdouble) radius, (gdouble) radius,
				"line-width", (double)border,
				"stroke-color-rgba", border_col,
				"fill-color-rgba", fill_col,
				NULL);
}

canvas_points_t *canvas_points_new(gint points) {
  return goo_canvas_points_new(points);
}

void canvas_point_set_pos(canvas_points_t *points, gint index, lpos_t *lpos) {
  points->coords[2*index+0] = lpos->x;
  points->coords[2*index+1] = lpos->y;
}

void canvas_points_free(canvas_points_t *points) {
  goo_canvas_points_unref(points);
}

canvas_item_t *canvas_polyline_new(struct map_s *map, canvas_group_t group, 
		  canvas_points_t *points, gint width, canvas_color_t color) {
  return goo_canvas_polyline_new(map->group[group], FALSE, 0,
				 "points", points,
				 "line-width", (double)width,
				 "stroke-color-rgba", color,
				 "line-join", CAIRO_LINE_JOIN_ROUND,
				 "line-cap", CAIRO_LINE_CAP_ROUND,
				 NULL);
}

canvas_item_t *canvas_polygon_new(struct map_s *map, canvas_group_t group, 
		  canvas_points_t *points, gint width, canvas_color_t color,
				  canvas_color_t fill) {
  return goo_canvas_polyline_new(map->group[group], TRUE, 0,
				 "points", points,
				 "line-width", (double)width,
				 "stroke-color-rgba", color,
				 "fill-color-rgba", fill,
				 "line-join", CAIRO_LINE_JOIN_ROUND,
				 "line-cap", CAIRO_LINE_CAP_ROUND,
				 NULL);
}

void canvas_item_set_points(canvas_item_t *item, canvas_points_t *points) {
  g_object_set(G_OBJECT(item), "points", points, NULL);
}

void canvas_item_set_pos(canvas_item_t *item, lpos_t *lpos, gint radius) {
  g_object_set(G_OBJECT(item), "center-x", (gdouble)lpos->x, 
	       "center-y", (gdouble)lpos->y, NULL);  
}

void canvas_window2world(canvas_t *canvas, gint x, gint y, gint *wx, gint *wy) {
  double sx = x, sy = y;
  goo_canvas_convert_from_pixels(GOO_CANVAS(canvas), &sx, &sy);
  *wx = sx; *wy = sy;
}

canvas_item_t *canvas_get_item_at(canvas_t *canvas, gint x, gint y) {
  return goo_canvas_get_item_at(GOO_CANVAS(canvas), x, y, TRUE);
}

void canvas_item_to_bottom(canvas_item_t *item) {
  goo_canvas_item_lower(item, NULL);
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
               NULL);
}

void canvas_item_destroy(canvas_item_t *item) {
  goo_canvas_item_remove(item);
}

void canvas_item_set_user_data(canvas_item_t *item, void *data) {
  g_object_set_data(G_OBJECT(item), "user data", data);
}

void *canvas_item_get_user_data(canvas_item_t *item) {
  return g_object_get_data(G_OBJECT(item), "user data");
}

typedef struct {
  GCallback c_handler;
  gpointer data;
} weak_t;

static void canvas_item_weak_notify(gpointer data, GObject *invalid) {
  weak_t *weak = data;

  ((void(*)(GtkWidget*, gpointer))weak->c_handler) (NULL, weak->data);
  g_free(weak);
}

void canvas_item_destroy_connect(canvas_item_t *item, 
				 GCallback c_handler, gpointer data) {
  weak_t *weak = g_new(weak_t,1);
  weak->data = data;
  weak->c_handler = c_handler;

  g_object_weak_ref(G_OBJECT(item), canvas_item_weak_notify, weak);
}

void canvas_set_zoom(canvas_t *canvas, double zoom) {
  goo_canvas_set_scale(GOO_CANVAS(canvas), zoom);
}

void canvas_get_scroll_offsets(canvas_t *canvas, gint *sx, gint *sy) {
  GtkAdjustment *hadj = ((struct _GooCanvas*)canvas)->hadjustment;
  GtkAdjustment *vadj = ((struct _GooCanvas*)canvas)->vadjustment;
  gdouble hs, vs;
  gdouble zoom = goo_canvas_get_scale(GOO_CANVAS(canvas));

  hs = gtk_adjustment_get_value(hadj);
  vs = gtk_adjustment_get_value(vadj);
  goo_canvas_convert_from_pixels(GOO_CANVAS(canvas), &hs, &vs);

  /* make values zoom independant */
  *sx = hs * zoom;
  *sy = vs * zoom;
}

void canvas_scroll_to(canvas_t *canvas, gint sx, gint sy) {
  gdouble zoom = goo_canvas_get_scale(GOO_CANVAS(canvas));
  goo_canvas_scroll_to(GOO_CANVAS(canvas), sx/zoom, sy/zoom);
}

void canvas_set_bounds(canvas_t *canvas, gint minx, gint miny, 
		       gint maxx, gint maxy) {
  goo_canvas_set_bounds(GOO_CANVAS(canvas), minx, miny, maxx, maxy);
}

canvas_item_t *canvas_image_new(map_t *map, canvas_group_t group, 
		GdkPixbuf *pix, gint x, gint y, float hscale, float vscale) {

  canvas_item_t *item = goo_canvas_image_new(map->group[group], pix, 
					     x/hscale, y/vscale, NULL);
  goo_canvas_item_scale(item, hscale, vscale);
  return item;
}

void canvas_image_move(canvas_item_t *item, gint x, gint y, 
		       float hscale, float vscale) {

  g_object_set(G_OBJECT(item), 
	       "x", (gdouble)x / hscale, 
	       "y", (gdouble)y / vscale, 
	       NULL);  
}

/* get the polygon/polyway segment a certain coordinate is over */
gint canvas_item_get_segment(canvas_item_t *item, gint x, gint y) {

  canvas_points_t *points = NULL;
  double line_width = 0;
  
  g_object_get(G_OBJECT(item), 
	       "points", &points, 
	       "line-width", &line_width, 
	       NULL);

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

  canvas_points_t *points = NULL;
  g_object_get(G_OBJECT(item), "points", &points, NULL);

  g_assert(points);
  g_assert(seg < points->num_points-1);

  *x0 = points->coords[2 * seg + 0];
  *y0 = points->coords[2 * seg + 1];
  *x1 = points->coords[2 * seg + 2];
  *y1 = points->coords[2 * seg + 3];
}
