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

/* --------- generic canvas --------- */

typedef enum { 
  CANVAS_GROUP_BG=0,       // background layer (wms overlay)
  CANVAS_GROUP_POLYGONS,   // polygons (forrests, buildings, lakes) */
  CANVAS_GROUP_WAYS_HL,    // highlighting of ways
  CANVAS_GROUP_WAYS_OL,    // outlines for ways (e.g. for highways)
  CANVAS_GROUP_WAYS,       // ways
  CANVAS_GROUP_WAYS_INT,   // interior of ways with outlines
  CANVAS_GROUP_WAYS_DIR,   // direction arrows for ways
  CANVAS_GROUP_NODES_HL,   // highlighting for nodes
  CANVAS_GROUP_NODES,      // nodes
  CANVAS_GROUP_TRACK,      // (GPS) track
  CANVAS_GROUP_GPS,        // current GPS position
  CANVAS_GROUP_FRISKET,    // the (white) frisket limiting the view
  CANVAS_GROUP_DRAW,       // "cursor" functionality
  CANVAS_GROUPS
} canvas_group_t;

#define CANVAS_HIGHLIGHTS   ((1<<CANVAS_GROUP_WAYS_HL) | (1<<CANVAS_GROUP_NODES_HL))

/* only objects in the "selectable" groups are returned by item_at */
#define CANVAS_SELECTABLE   ((1<<CANVAS_GROUP_POLYGONS) | (1<<CANVAS_GROUP_WAYS) | (1<<CANVAS_GROUP_WAYS_OL) | (1<<CANVAS_GROUP_WAYS_INT) | (1<<CANVAS_GROUP_NODES) | CANVAS_HIGHLIGHTS)


#if CANVAS_GROUPS >= 16
#error "More than 16 canvas groups needs adjustment e.g. in map.h"
#endif

/* --------- goocanvas specific --------- */
#if defined(USE_GOOCANVAS)

#include <goocanvas.h>

typedef GooCanvasItem canvas_item_t;
typedef GooCanvasPoints canvas_points_t;

typedef struct {
  GtkWidget *widget;
  GooCanvasItem *group[CANVAS_GROUPS];

#ifdef CANVAS_CUSTOM_ITEM_AT
  struct canvas_item_info_s *item_info, *last_item_info;
#endif

} canvas_t;

typedef gulong canvas_color_t;

#define CANVAS_COLOR(r,g,b,a) ((((guint) (r) & 0xff) << 24)   \
			       | (((guint) (g) & 0xff) << 16)	\
			       | (((guint) (b) & 0xff) << 8)	\
			       | ((guint) (a) & 0xff))



#else 
#error "No canvas type defined!"
#endif

#ifdef CANVAS_CUSTOM_ITEM_AT
typedef enum { CANVAS_ITEM_CIRCLE, CANVAS_ITEM_POLY } canvas_item_type_t;

typedef struct canvas_item_info_s {
  canvas_t *canvas;
  canvas_item_type_t type;

  union {
    struct {
      struct {
	gint x, y;
      } center;
      gint r;
    } circle;

    struct {
      struct {
	struct {
	  gint x,y;
	} top_left, bottom_right;
      } bbox;

      gboolean is_polygon;
      gint width, num_points;
      lpos_t *points;

    } poly;

  } data;

  canvas_item_t *item;
  struct canvas_item_info_s *prev, *next;
} canvas_item_info_t;

#endif // CANVAS_CUSTOM_ITEM_AT


typedef enum { CANVAS_UNIT_METER = 0, CANVAS_UNIT_PIXEL } canvas_unit_t;

/***** creating/destroying the canvas ******/
canvas_t *canvas_new(canvas_color_t color);
GtkWidget *canvas_get_widget(canvas_t *canvas);

/****** manipulating the canvas ******/
void canvas_set_antialias(canvas_t *canvas, gboolean antialias);
void canvas_erase(canvas_t *canvas, gint group_mask);
void canvas_window2world(canvas_t *canvas, gint x, gint y, gint *wx, gint *wy);
canvas_item_t *canvas_get_item_at(canvas_t *canvas, gint x, gint y);
void canvas_set_zoom(canvas_t *canvas, gdouble zoom);
gdouble canvas_get_zoom(canvas_t *canvas);
gdouble canvas_get_viewport_width(canvas_t *canvas, canvas_unit_t unit);
gdouble canvas_get_viewport_height(canvas_t *canvas, canvas_unit_t unit);
void canvas_scroll_to(canvas_t *canvas, canvas_unit_t unit, gint sx, gint sy);
void canvas_scroll_get(canvas_t *canvas, canvas_unit_t unit, gint *sx, gint *sy);
void canvas_set_bounds(canvas_t *canvas, gint minx, gint miny, 
		       gint maxx, gint maxy);


/***** creating/destroying items ******/
canvas_item_t *canvas_circle_new(canvas_t *canvas, canvas_group_t group, 
		 gint x, gint y, gint radius, gint border,
		 canvas_color_t fill_col, canvas_color_t border_col);
canvas_item_t *canvas_polyline_new(canvas_t *canvas, canvas_group_t group, 
		 canvas_points_t *points, gint width, canvas_color_t color);
canvas_item_t *canvas_polygon_new(canvas_t *canvas, canvas_group_t group, 
		  canvas_points_t *points, gint width, canvas_color_t color,
		  canvas_color_t fill);
canvas_item_t *canvas_image_new(canvas_t *canvas, canvas_group_t group,
				GdkPixbuf *pix, gint x, gint y, 
				float hscale, float vscale);

canvas_points_t *canvas_points_new(gint points);
void canvas_point_set_pos(canvas_points_t *points, gint index, lpos_t *lpos);
void canvas_points_free(canvas_points_t *points);
gint canvas_points_num(canvas_points_t *points);
void canvas_point_get_lpos(canvas_points_t *points, gint index, lpos_t *lpos);
void canvas_item_destroy(canvas_item_t *item);

/****** manipulating items ******/
void canvas_item_set_pos(canvas_item_t *item, lpos_t *lpos, gint radius);
void canvas_item_set_points(canvas_item_t *item, canvas_points_t *points);
void canvas_item_set_zoom_max(canvas_item_t *item, float zoom_max);
void canvas_item_set_dashed(canvas_item_t *item, gint line_width, gint dash_length);
void canvas_item_to_bottom(canvas_item_t *item);
void canvas_item_set_user_data(canvas_item_t *item, void *data);
void *canvas_item_get_user_data(canvas_item_t *item);
void canvas_item_destroy_connect(canvas_item_t *item, 
				 GCallback c_handler, gpointer data);
void canvas_image_move(canvas_item_t *item, gint x, gint y, 
		       float hscale, float vscale);
gint canvas_item_get_segment(canvas_item_t *item, gint x, gint y);
void canvas_item_get_segment_pos(canvas_item_t *item, gint seg,
				 gint *x0, gint *y0, gint *x1, gint *y1);

#ifdef CANVAS_CUSTOM_ITEM_AT
void canvas_item_info_attach_circle(canvas_t *canvas, canvas_item_t *item, 
				    gint x, gint y, gint r);
void canvas_item_info_attach_poly(canvas_t *canvas, canvas_item_t *item, 
		  gboolean is_polygon, canvas_points_t *points, gint width);
void canvas_item_info_get_at(canvas_t *canvas, gint x, gint y);
void canvas_item_info_push(canvas_t *canvas, canvas_item_t *item);
#endif

#endif // CANVAS_H
