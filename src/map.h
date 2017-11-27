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

#ifndef MAP_H
#define MAP_H

#include "osm.h"
#include "track.h"

#include <vector>

/* -------- all sizes are in meters ---------- */
#define MAP_COLOR_NONE   0x0
#define NO_COLOR         0 /* black */

#define RGBA_COMBINE(a,b) (((a)&~0xff) | ((b)&0xff))

#define ZOOM_FACTOR_MENU   (1.5)
#define ZOOM_FACTOR_WHEEL  (1.1)
#define ZOOM_FACTOR_BUTTON (1.5)

#define MAP_DETAIL_STEP 1.5

/* the "drag limit" is the number of pixels the mouse/pen has to */
/* be moved so the action is actually considered dragging. This is */
/* to prevent accidential dragging when the user only intended to click */
/* something. This is especially useful when using a touchscreen. */
#ifndef FREMANTLE
#define MAP_DRAG_LIMIT   4
#else
#define MAP_DRAG_LIMIT   16
#endif

/* don't reorder these as some things in map.c */
/* rely on a certain order */
enum map_action_t {
  MAP_ACTION_IDLE=0,
  MAP_ACTION_NODE_ADD,
  MAP_ACTION_BG_ADJUST,
  MAP_ACTION_WAY_ADD,
  MAP_ACTION_WAY_NODE_ADD,
  MAP_ACTION_WAY_CUT,
};

struct appdata_t;
class canvas_t;
struct canvas_item_t;
struct track_seg_t;
struct track_t;

struct map_item_t {
  map_item_t(object_t o = object_t(), bool hl = false, canvas_item_t *i = O2G_NULLPTR)
    : object(o), highlight(hl) , item(i) {}

  object_t object;
  bool highlight;
  canvas_item_t *item;

  static inline void free(void *p) {
    delete static_cast<map_item_t *>(p);
  }

  /**
  * @brief get the polygon/polyway segment a certain coordinate is over
  */
  int get_segment(lpos_t pos) const;
};

/* this is a chain of map_items which is attached to all entries */
/* in the osm tree (node_t, way_t, ...) to be able to get a link */
/* to the screen representation of a give node/way/etc */
struct map_item_chain_t;

struct map_state_t {
  map_state_t();

  void reset();

  float zoom;                          // zoom level (1.0 = 1m/pixel
  float detail;                        // deatil level (1.0 = normal)
  struct { int x,y; } scroll_offset;  // initial scroll offset
};

class map_t {
protected:
  explicit map_t(appdata_t &a);

public:
  enum clearLayers {
    MAP_LAYER_ALL,
    MAP_LAYER_OBJECTS_ONLY
  };

  static map_t *create(appdata_t &a);
  ~map_t();

  appdata_t &appdata;
  canvas_t * const canvas;
  map_state_t &state;

  struct map_highlight_t *highlight;      // list of elements used for highlighting

  map_item_t selected;             // the currently selected item (node or way)

  canvas_item_t *cursor;           // the cursor visible on the draw layer
  canvas_item_t *touchnode;        // the blue "touch node" on the draw layer

  /* background image related stuff */
  struct {
    struct { float x, y; } offset;
    struct { float x, y; } scale;
  } bg;

  struct {
    map_action_t type;            // current action type in progress

    way_t *way;
    way_t *extending, *ends_on;   // ways touched by first and last node
  } action;

  /* variables required for pen/mouse handling */
  struct {
    bool is;
    bool drag;
    map_item_t *on_item;
    struct { int x,y; } at;    // point mouse button was last pressed
    struct { int x,y; } so;    // initial scroll offset
    bool on_selected_node;      // the currently clicked node
                                // (may be part of a selected way)
  } pen_down;

  struct style_t *&style;

  size_t elements_drawn;	///< number of elements drawn in last segment

  osm_t::TagMap last_node_tags;           // used to "repeat" tagging
  osm_t::TagMap last_way_tags;

  void set_autosave(bool enable);
  bool key_press_event(unsigned int keyval);
  void init();
  void paint();
  void clear(clearLayers layers);
  void item_deselect();
  void highlight_refresh();
  void draw(node_t *node);
  void select_relation(relation_t *relation);
  void redraw_item(object_t object);
  void draw(way_t *way);
  void select_way(way_t *way);
  void set_action(map_action_t action);
  bool item_is_selected_way(const map_item_t *map_item) const;
  bool item_is_selected_node(const map_item_t *map_item) const;
  bool scroll_to_if_offscreen(const lpos_t lpos);

  /* track stuff */
  void track_draw(TrackVisibility visibiliy, track_t &track);
  void track_draw_seg(track_seg_t &seg);
  void track_update_seg(track_seg_t &seg);
  void track_pos(const lpos_t lpos);

  void show_node(node_t *node);
  void cmenu_show();

  /* background stuff */
  bool set_bg_image(const std::string &filename);
  void set_bg_color_from_style();
  void remove_bg_image();

  void hide_selected();
  void show_all();

  void set_zoom(double zoom, bool update_scroll_offsets);

  void detail_change(float detail, const char *banner_msg = O2G_NULLPTR);
  void detail_increase();
  void detail_decrease();
  void detail_normal();

  /* various functions required by map_edit */
  map_item_t *item_at(int x, int y);

  void pen_down_item();
};

// Gtk callbacks
void map_action_cancel(map_t *map);
void map_action_ok(map_t *map);

void map_delete_selected(map_t *map);

/* track stuff */
void map_track_remove(track_t &track);
void map_track_remove_pos(appdata_t &appdata);

void map_outside_error(appdata_t &appdata);

void map_item_chain_destroy(map_item_chain_t *&chainP);

#endif // MAP_H
