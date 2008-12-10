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

#ifndef CANVAS_H
#define CANVAS_H

typedef enum { CANVAS_GROUP_BG=0, CANVAS_GROUP_POLYGONS, 
	       CANVAS_GROUP_TRACK, CANVAS_GROUP_GPS, 
	       CANVAS_GROUP_WAYS_HL, CANVAS_GROUP_WAYS_OL, 
	       CANVAS_GROUP_WAYS, CANVAS_GROUP_NODES_HL, CANVAS_GROUP_NODES, 
	       CANVAS_GROUP_DRAW, CANVAS_GROUPS } canvas_group_t;

#if defined(USE_GNOMECANVAS)

#include <libgnomecanvas/libgnomecanvas.h>

typedef GnomeCanvasItem canvas_item_t;
typedef GnomeCanvasPoints canvas_points_t;
typedef GtkWidget canvas_t;

typedef gulong canvas_color_t;
#define CANVAS_COLOR(r,g,b,a) GNOME_CANVAS_COLOR_A(r,g,b,a)


#elif defined(USE_GOOCANVAS)

#include <goocanvas.h>

typedef GooCanvasItem canvas_item_t;
typedef GooCanvasPoints canvas_points_t;
typedef GtkWidget canvas_t;

typedef gulong canvas_color_t;

#define CANVAS_COLOR(r,g,b,a) ((((guint) (r) & 0xff) << 24)   \
			       | (((guint) (g) & 0xff) << 16)	\
			       | (((guint) (b) & 0xff) << 8)	\
			       | ((guint) (a) & 0xff))



#else 
#error "No canvas type defined!"
#endif



struct map_s;
canvas_item_t *canvas_circle_new(struct map_s *map, canvas_group_t group, 
		 gint x, gint y, gint radius, gint border,
		 canvas_color_t fill_col, canvas_color_t border_col);

void canvas_item_set_pos(canvas_item_t *item, lpos_t *lpos, gint radius);

canvas_points_t *canvas_points_new(gint points);
void canvas_point_set_pos(canvas_points_t *points, gint index, lpos_t *lpos);
void canvas_points_free(canvas_points_t *points);
void canvas_item_set_points(canvas_item_t *item, canvas_points_t *points);

canvas_item_t *canvas_polyline_new(struct map_s *map, canvas_group_t group, 
		 canvas_points_t *points, gint width, canvas_color_t color);

canvas_item_t *canvas_polygon_new(struct map_s *map, canvas_group_t group, 
		  canvas_points_t *points, gint width, canvas_color_t color,
		  canvas_color_t fill);

void canvas_window2world(canvas_t *canvas, gint x, gint y, gint *wx, gint *wy);
canvas_item_t *canvas_get_item_at(canvas_t *canvas, gint x, gint y);

void canvas_item_set_zoom_max(canvas_item_t *item, float zoom_max);

void canvas_item_to_bottom(canvas_item_t *item);

void canvas_item_destroy(canvas_item_t *item);

void canvas_item_set_user_data(canvas_item_t *item, void *data);
void *canvas_item_get_user_data(canvas_item_t *item);

void canvas_item_destroy_connect(canvas_item_t *item, 
				 GCallback c_handler, gpointer data);

void canvas_set_zoom(canvas_t *canvas, double zoom);

void canvas_get_scroll_offsets(canvas_t *canvas, gint *sx, gint *sy);

void canvas_scroll_to(canvas_t *canvas, gint sx, gint sy);

void canvas_set_bounds(canvas_t *canvas, gint minx, gint miny, 
		       gint maxx, gint maxy);

canvas_item_t *canvas_image_new(struct map_s *map, canvas_group_t group,
				GdkPixbuf *pix, gint x, gint y, 
				float hscale, float vscale);

void canvas_image_move(canvas_item_t *item, gint x, gint y, 
		       float hscale, float vscale);

gint canvas_item_get_segment(canvas_item_t *item, gint x, gint y);
void canvas_item_get_segment_pos(canvas_item_t *item, gint seg,
				 gint *x0, gint *y0, gint *x1, gint *y1);

#endif // CANVAS_H
