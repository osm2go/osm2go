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

#include "pos.h"

#include <glib.h>
#include <gtk/gtk.h>

/* --------- generic canvas --------- */

typedef enum {
  CANVAS_GROUP_BG=0,       // 0: background layer (wms overlay)
  CANVAS_GROUP_POLYGONS,   // 1: polygons (forrests, buildings, lakes) */
  CANVAS_GROUP_WAYS_HL,    // 2: highlighting of ways
  CANVAS_GROUP_WAYS_OL,    // 3: outlines for ways (e.g. for highways)
  CANVAS_GROUP_WAYS,       // 4: ways
  CANVAS_GROUP_WAYS_INT,   // 5: interior of ways with outlines
  CANVAS_GROUP_WAYS_DIR,   // 6: direction arrows for ways
  CANVAS_GROUP_NODES_HL,   // 7: highlighting for nodes
  CANVAS_GROUP_NODES,      // 8: nodes
  CANVAS_GROUP_NODES_IHL,  // 9: highlighting for otherwise invisible way nodes
  CANVAS_GROUP_TRACK,      // 10: (GPS) track
  CANVAS_GROUP_GPS,        // 11: current GPS position
  CANVAS_GROUP_FRISKET,    // 12: the (white) frisket limiting the view
  CANVAS_GROUP_DRAW,       // 13: "cursor" functionality
  CANVAS_GROUPS            // 14: total number of groups
} canvas_group_t;

/* only objects in the "selectable" groups are returned by item_at */
/* (the fuzzy search of custom_item_at makes it possible to ignore the */
/* selection layer) */
#define CANVAS_HIGHLIGHTS   (1<<CANVAS_GROUP_NODES_IHL)

#define CANVAS_SELECTABLE   ((1<<CANVAS_GROUP_POLYGONS) | (1<<CANVAS_GROUP_WAYS) | (1<<CANVAS_GROUP_WAYS_OL) | (1<<CANVAS_GROUP_WAYS_INT) | (1<<CANVAS_GROUP_NODES) | CANVAS_HIGHLIGHTS)

#if CANVAS_GROUPS >= 16
#error "More than 16 canvas groups needs adjustment e.g. in map.h"
#endif

/* --------- goocanvas specific --------- */
#if defined(USE_GOOCANVAS)

#include <goocanvas.h>

typedef GooCanvasItem canvas_item_t;
typedef GooCanvasPoints canvas_points_t;

#ifdef __cplusplus

struct canvas_item_info_t;

#include <vector>

struct canvas_t {
  canvas_t();

  GtkWidget * const widget;
  GooCanvasItem *group[CANVAS_GROUPS];

  std::vector<canvas_item_info_t *> item_info[CANVAS_GROUPS];
};

typedef guint canvas_color_t;

#endif

#else
#error "No canvas type defined!"
#endif

#ifdef __cplusplus

enum canvas_item_type_t { CANVAS_ITEM_CIRCLE, CANVAS_ITEM_POLY };

struct canvas_item_info_t {
  canvas_item_info_t(canvas_item_type_t t, canvas_t *cv, canvas_group_t g, canvas_item_t *it);
  ~canvas_item_info_t();
  canvas_t * const canvas;
  const canvas_item_type_t type;

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

      bool is_polygon;
      gint width, num_points;
      lpos_t *points;

    } poly;

  } data;

  canvas_group_t group;
  canvas_item_t *item;   ///< reference to visual representation
};

enum canvas_unit_t { CANVAS_UNIT_METER = 0, CANVAS_UNIT_PIXEL };

struct canvas_dimensions {
  gdouble width, height;
  inline canvas_dimensions operator/(double d) const {
    canvas_dimensions ret = *this;
    ret.width /= d; ret.height /= d;
    return ret;
  }
};

canvas_dimensions canvas_get_viewport_dimensions(canvas_t *canvas, canvas_unit_t unit);
void canvas_window2world(canvas_t *canvas, gint x, gint y, gint &wx, gint &wy);
void canvas_scroll_get(canvas_t *canvas, canvas_unit_t unit, gint &sx, gint &sy);
void canvas_point_set_pos(canvas_points_t *points, gint index, const lpos_t &lpos);
void canvas_item_get_segment_pos(canvas_item_t *item, gint seg,
                                 gint &x0, gint &y0, gint &x1, gint &y1);

/****** manipulating the canvas ******/
void canvas_set_background(canvas_t *canvas, canvas_color_t bg_color);
void canvas_set_antialias(canvas_t *canvas, gboolean antialias);
void canvas_erase(canvas_t *canvas, gint group_mask);
canvas_item_t *canvas_get_item_at(canvas_t *canvas, gint x, gint y);
void canvas_set_zoom(canvas_t *canvas, gdouble zoom);
gdouble canvas_get_zoom(canvas_t *canvas);
void canvas_scroll_to(canvas_t *canvas, canvas_unit_t unit, gint sx, gint sy);
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
void canvas_points_free(canvas_points_t *points);
gint canvas_points_num(canvas_points_t *points);
void canvas_point_get_lpos(canvas_points_t *points, gint index, lpos_t *lpos);
void canvas_item_destroy(canvas_item_t *item);

/****** manipulating items ******/
void canvas_item_set_pos(canvas_item_t *item, lpos_t *lpos);
void canvas_item_set_radius(canvas_item_t *item, gint radius);
void canvas_item_set_points(canvas_item_t *item, canvas_points_t *points);
void canvas_item_set_zoom_max(canvas_item_t *item, float zoom_max);
void canvas_item_set_dashed(canvas_item_t *item, gint line_width, guint dash_length_on, guint dash_length_off);
void canvas_item_to_bottom(canvas_item_t *item);
void canvas_item_set_user_data(canvas_item_t *item, void *data);
void *canvas_item_get_user_data(canvas_item_t *item);
void canvas_item_destroy_connect(canvas_item_t *item, void(*c_handler)(gpointer), gpointer data);
void canvas_image_move(canvas_item_t *item, gint x, gint y,
		       float hscale, float vscale);
gint canvas_item_get_segment(canvas_item_t *item, gint x, gint y);

void canvas_item_info_attach_circle(canvas_t *canvas, canvas_group_t group,
			    canvas_item_t *item, gint x, gint y, gint r);
void canvas_item_info_attach_poly(canvas_t *canvas, canvas_group_t group,
		  canvas_item_t *item,
                                  bool is_polygon, canvas_points_t *points, gint width);
canvas_item_t *canvas_item_info_get_at(canvas_t *canvas, gint x, gint y);
void canvas_item_info_push(canvas_t *canvas, canvas_item_t *item);

#endif

#endif // CANVAS_H
