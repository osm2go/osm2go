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

#ifndef USE_GNOMECANVAS
#error "Config error!"
#endif

#include "appdata.h"

canvas_item_t *canvas_circle_new(map_t *map, canvas_group_t group, 
				 gint x, gint y, gint radius, gint border, 
				 canvas_color_t fill_col, canvas_color_t border_col) {

  return gnome_canvas_item_new(GNOME_CANVAS_GROUP(map->group[group]), 
			       GNOME_TYPE_CANVAS_ELLIPSE,
			       "x1", (double)x-radius, "x2", (double)x+radius,
			       "y1", (double)y-radius, "y2", (double)y+radius,
			       "fill_color_rgba", fill_col, 
			       "outline_color_rgba", border_col, 
			       "width-units", (double)border,
			       NULL);       
}

canvas_points_t *canvas_points_new(gint points) {
  return gnome_canvas_points_new(points);
}

void canvas_point_set_pos(canvas_points_t *points, gint index, lpos_t *lpos) {
  points->coords[2*index+0] = lpos->x;
  points->coords[2*index+1] = lpos->y;
}

void canvas_points_free(canvas_points_t *points) {
  gnome_canvas_points_free(points);
}

canvas_item_t *canvas_polyline_new(struct map_s *map, canvas_group_t group, 
				  canvas_points_t *points, gint width, canvas_color_t color) {

  return  gnome_canvas_item_new(GNOME_CANVAS_GROUP(map->group[group]),
				GNOME_TYPE_CANVAS_LINE,
				"points", points,
				"fill_color_rgba", color,
				"width-units", (double)width,
				"join-style", GDK_JOIN_ROUND,
				"cap-style", GDK_CAP_ROUND,
#if 0
				"last-arrowhead", TRUE,
				"arrow-shape-a", 20.0,
				"arrow-shape-b", 20.0,
				"arrow-shape-c", 10.0,
#endif
				NULL);   
}

canvas_item_t *canvas_polygon_new(struct map_s *map, canvas_group_t group, 
				  canvas_points_t *points, gint width, canvas_color_t color,
				  canvas_color_t fill) {

  return  gnome_canvas_item_new(GNOME_CANVAS_GROUP(map->group[group]),
				GNOME_TYPE_CANVAS_POLYGON,
				"points", points,
				"fill_color_rgba", fill,
				"width-units", (double)width,
				"join-style", GDK_JOIN_ROUND,
				"cap-style", GDK_CAP_ROUND,
				"outline_color_rgba", color,
				NULL);   
}

void canvas_item_set_points(canvas_item_t *item, canvas_points_t *points) {
  gnome_canvas_item_set(item, "points", points, NULL);
}

void canvas_item_set_pos(canvas_item_t *item, lpos_t *lpos, gint radius) {
  gnome_canvas_item_set(item,
			"x1", (double)lpos->x - radius, 
			"x2", (double)lpos->x + radius,
			"y1", (double)lpos->y - radius, 
			"y2", (double)lpos->y + radius,
			NULL);

}

void canvas_window2world(canvas_t *canvas, gint x, gint y, gint *wx, gint *wy) {
  double dwx, dwy;
  gnome_canvas_window_to_world(GNOME_CANVAS(canvas), x, y, &dwx, &dwy);
  *wx = dwx;
  *wy = dwy;
}

canvas_item_t *canvas_get_item_at(canvas_t *canvas, gint x, gint y) {
  return gnome_canvas_get_item_at(GNOME_CANVAS(canvas), x, y);
}

void canvas_item_set_zoom_max(canvas_item_t *item, float zoom_max) {
    /* This is a no-op for gnomecanvas for now. */
}

void canvas_item_to_bottom(canvas_item_t *item) {
  gnome_canvas_item_lower_to_bottom(item);
}

void canvas_item_destroy(canvas_item_t *item) {
  gtk_object_destroy(GTK_OBJECT(item));
}

void canvas_item_set_user_data(canvas_item_t *item, void *data) {
#if 0
  gtk_object_set_user_data(GTK_OBJECT(item), data);
#else
  g_object_set_data(G_OBJECT(item), "user data", data);
#endif
}

void *canvas_item_get_user_data(canvas_item_t *item) {
#if 0
  return gtk_object_get_user_data(GTK_OBJECT(item));
#else
  return g_object_get_data(G_OBJECT(item), "user data");
#endif
}

void canvas_item_destroy_connect(canvas_item_t *item, 
				 GCallback c_handler, gpointer data) {
  g_signal_connect(G_OBJECT(item), "destroy", G_CALLBACK(c_handler), data);
}

void canvas_set_zoom(canvas_t *canvas, double zoom) {
  gnome_canvas_set_pixels_per_unit(GNOME_CANVAS(canvas), zoom);
}

void canvas_get_scroll_offsets(canvas_t *canvas, gint *sx, gint *sy) {
  gnome_canvas_get_scroll_offsets(GNOME_CANVAS(canvas), &sx, &sy);
}

void canvas_scroll_to(canvas_t *canvas, gint sx, gint sy) {
  gnome_canvas_scroll_to(GNOME_CANVAS(canvas), sx, sy);
}

void canvas_set_bounds(canvas_t *canvas, gint minx, gint miny, 
		       gint maxx, gint maxy) {
  gnome_canvas_set_scroll_region(GNOME_CANVAS(canvas), minx, miny, maxx, maxy);
}
