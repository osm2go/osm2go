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

#ifndef MAP_H
#define MAP_H

#include "appdata.h"
#include "canvas.h"
#include "osm.h"
#include "style.h"

#define MAP_LAYER_ALL (0xffff)
#define MAP_LAYER_OBJECTS_ONLY ((1<<CANVAS_GROUP_POLYGONS) | (1<<CANVAS_GROUP_WAYS_HL) | (1<<CANVAS_GROUP_WAYS_OL) | (1<<CANVAS_GROUP_WAYS) | (1<<CANVAS_GROUP_WAYS_INT) | (1<<CANVAS_GROUP_NODES_HL) | (1<<CANVAS_GROUP_NODES_IHL) | (1<<CANVAS_GROUP_NODES) | (1<<CANVAS_GROUP_WAYS_DIR))

/* -------- all sizes are in meters ---------- */
#define MAP_COLOR_NONE   0x0
#define NO_COLOR         CANVAS_COLOR(0x00,0x00,0x00,0x00)

#define RGBA_COMBINE(a,b) (((a)&~0xff) | ((b)&0xff))

#define ZOOM_FACTOR_MENU   (1.5)
#define ZOOM_FACTOR_WHEEL  (1.1)
#define ZOOM_FACTOR_BUTTON (1.5)

#define MAP_DETAIL_STEP 1.5

/* the "drag limit" is the number of pixels the mouse/pen has to */
/* be moved so the action is actually considered dragging. This is */
/* to prevent accidential dragging when the user only intended to click */
/* something. This is especially useful when using a touchscreen. */
#ifndef USE_HILDON
#define MAP_DRAG_LIMIT   4
#else
#define MAP_DRAG_LIMIT   16
#endif

/* don't reorder these as some things in map.c */
/* rely on a certain order */
typedef enum {
  MAP_ACTION_IDLE=0,
  MAP_ACTION_NODE_ADD,
  MAP_ACTION_BG_ADJUST,
  MAP_ACTION_WAY_ADD,
  MAP_ACTION_WAY_NODE_ADD,
  MAP_ACTION_WAY_CUT,
  MAP_ACTION_NUM
} map_action_t;

typedef struct map_item_s {
  object_t object;
  gboolean highlight;
  canvas_item_t *item;
} map_item_t;

/* this is a chain of map_items which is attached to all entries */
/* in the osm tree (node_t, way_t, ...) to be able to get a link */
/* to the screen representation of a give node/way/etc */
typedef struct map_item_chain_s map_item_chain_t;

typedef struct {
  gint refcount;
  float zoom;                          // zoom level (1.0 = 1m/pixel
  float detail;                        // deatil level (1.0 = normal)
  struct { gint x,y; } scroll_offset;  // initial scroll offset
} map_state_t;

typedef struct map_s {
  appdata_t *appdata;

  canvas_t *canvas;

  map_state_t *state;

  gint autosave_handler_id;

  struct map_highlight_s *highlight;      // list of elements used for highlighting

  map_item_t selected;             // the currently selected item (node or way)

  canvas_item_t *cursor;           // the cursor visible on the draw layer
  canvas_item_t *touchnode;        // the blue "touch node" on the draw layer

  tag_t *last_node_tags;           // used to "repeat" tagging
  tag_t *last_way_tags;

  /* background image related stuff */
  struct {
    GdkPixbuf *pix;
    canvas_item_t *item;
    struct { float x, y; } offset;
    struct { float x, y; } scale;
  } bg;

  struct {
    map_action_t type;            // current action type in progress

    struct {                       // action related variables/states
      way_t *way;
      way_t *extending, *ends_on;  // ways touched by first and last node
    };
  } action;

  /* variables required for pen/mouse handling */
  struct {
    gboolean is;
    gboolean drag;
    map_item_t *on_item;
    struct { gint x,y; } at;    // point mouse button was last pressed
    struct { gint x,y; } so;    // initial scroll offset
    gboolean on_selected_node;  // the currently clicked node
                                // (may be part of a selected way)
  } pen_down;

  style_t *style;

} map_t;

GtkWidget *map_new(appdata_t *appdata);
map_state_t *map_state_new(void);
void map_state_free(map_state_t *state);
void map_state_reset(map_state_t *state);
void map_init(appdata_t *appdata);
gboolean map_key_press_event(appdata_t *appdata, GdkEventKey *event);
void map_item_set_flags(map_item_t *map_item, int set, int clr);
void map_show_node(map_t *map, node_t *node);
void map_cmenu_show(map_t *map);
void map_highlight_refresh(appdata_t *appdata);

void map_item_redraw(appdata_t *appdata, map_item_t *map_item);

void map_clear(appdata_t *appdata, gint layer_mask);
void map_paint(appdata_t *appdata);

void map_action_set(appdata_t *appdata, map_action_t action);
void map_action_cancel(appdata_t *appdata);
void map_action_ok(appdata_t *appdata);

void map_delete_selected(appdata_t *appdata);

/* track stuff */
void map_track_draw(map_t *map, struct track_s *track);
struct track_seg_s;
void map_track_draw_seg(map_t *map, struct track_seg_s *seg);
void map_track_update_seg(map_t *map, struct track_seg_s *seg);
void map_track_remove(appdata_t *appdata);
void map_track_pos(appdata_t *appdata, const lpos_t *lpos);
void map_track_remove_pos(appdata_t *appdata);

/* background stuff */
void map_set_bg_image(map_t *map, const char *filename);
void map_remove_bg_image(map_t *map);

void map_hide_selected(appdata_t *appdata);
void map_show_all(appdata_t *appdata);

void map_set_zoom(map_t *map, double zoom, gboolean update_scroll_offsets);
gboolean map_scroll_to_if_offscreen(map_t *map, const lpos_t *lpos);

/* various functions required by map_edit */
gboolean map_item_is_selected_node(map_t *map, map_item_t *map_item);
gboolean map_item_is_selected_way(map_t *map, map_item_t *map_item);
void map_item_chain_destroy(map_item_chain_t **chainP);
map_item_t *map_item_at(map_t *map, gint x, gint y);
map_item_t *map_real_item_at(map_t *map, gint x, gint y);
void map_item_deselect(appdata_t *appdata);
void map_way_delete(appdata_t *appdata, way_t *way);
void map_way_draw(map_t *map, way_t *way);
void map_way_select(appdata_t *appdata, way_t *way);
void map_outside_error(appdata_t *appdata);
void map_node_draw(map_t *map, node_t *node);
void map_relation_select(appdata_t *appdata, relation_t *relation);

void map_detail_change(map_t *map, float detail);
void map_detail_increase(map_t *map);
void map_detail_decrease(map_t *map);
void map_detail_normal(map_t *map);

#endif // MAP_H
