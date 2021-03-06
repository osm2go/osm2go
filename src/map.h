/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "map_hl.h"
#include "map_state.h"
#include "osm.h"
#include "osm_objects.h"
#include "pos.h"
#include "track.h"

#include <string>
#include <memory>
#include <unordered_map>

#include <osm2go_i18n.h>
#include <osm2go_platform.h>
#include <osm2go_stl.h>

/* -------- all sizes are in meters ---------- */
#define MAP_COLOR_NONE   0x0

#define ZOOM_FACTOR_MENU   (1.5)
#define ZOOM_FACTOR_WHEEL  (1.1)
#define ZOOM_FACTOR_BUTTON (1.5)

#define MAP_DETAIL_STEP 1.5f

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
struct canvas_item_circle;
struct canvas_item_t;
class style_t;
struct track_seg_t;
struct track_t;

struct map_item_t {
  map_item_t(object_t o = object_t(), canvas_item_t *i = nullptr)
    : object(o), item(i) {}
  map_item_t(const map_item_t &) O2G_DELETED_FUNCTION;

  object_t object;
  canvas_item_t *item;
};

class map_t {
  friend struct map_bg_modifier;
  friend class test_map; // for testing
  std::unordered_map<visible_item_t *, map_item_t *> background_items;
protected:
  explicit map_t(appdata_t &a, canvas_t *c);

  void handle_motion(const osm2go_platform::screenpos &p);
  void scroll_step(const osm2go_platform::screenpos &p);
  void button_press(const osm2go_platform::screenpos &p);
  void button_release(const osm2go_platform::screenpos &p);
  void bg_adjust(const osm2go_platform::screenpos &p);

  canvas_item_circle *gps_item; ///< the purple circle

public:
  enum clearLayers {
    MAP_LAYER_ALL,
    MAP_LAYER_OBJECTS_ONLY
  };

  virtual ~map_t();

  appdata_t &appdata;
  canvas_t * const canvas;

  map_highlight_t highlight;       // list of elements used for highlighting

  map_item_t selected;             // the currently selected item (node or way)

  std::unique_ptr<canvas_item_t> cursor;    ///< the cursor visible on the draw layer
  std::unique_ptr<canvas_item_t> touchnode; ///< the blue "touch node" on the draw layer
  node_t *touchnode_node;                   ///< the underlying node belonging to touchnode

private:
  /* background image related stuff */
  osm2go_platform::screenpos bg_offset;

  struct {
    map_action_t type;            // current action type in progress

    std::unique_ptr<way_t> way;   ///< the temporary way that is currently added
    way_t *extending, *ends_on;   // ways touched by first and last node
  } action;

public:
  /* variables required for pen/mouse handling */
  struct _pd {
    _pd();
    bool is;
    bool drag;
    map_item_t *on_item;
    osm2go_platform::screenpos at;    // point mouse button was last pressed
    bool on_selected_node;      // the currently clicked node
                                // (may be part of a selected way)
  } pen_down;

  std::unique_ptr<style_t> &style;

  size_t elements_drawn;	///< number of elements drawn in last segment

  osm_t::TagMap last_node_tags;           // used to "repeat" tagging
  osm_t::TagMap last_way_tags;

  virtual void set_autosave(bool enable) = 0;
  void init();
  void paint();
  void clear(clearLayers layers);
  void item_deselect();
  void highlight_refresh();
  void draw(node_t *node);
  void select_relation(relation_t *relation);
  template<typename T> void redraw_item(T *obj);
  void draw(way_t *way);
  void drawColorized(way_t *way);
  void select_way(way_t *way);
  void set_action(map_action_t act);
  bool item_is_selected_way(const map_item_t *map_item) const;
  bool item_is_selected_node(const map_item_t *map_item) const;
  bool scroll_to_if_offscreen(lpos_t lpos);

  /* track stuff */
  void track_draw(TrackVisibility visibility, track_t &track);
  void track_draw_seg(track_seg_t &seg);
  void track_update_seg(track_seg_t &seg);
  void track_pos(const lpos_t lpos);

  void cmenu_show();

  /* background stuff */
  bool set_bg_image(const std::string &filename, osm2go_platform::screenpos offset);
  void set_bg_color_from_style();
  void remove_bg_image();
  /* this cancels any wms adjustment in progress */
  void cancel_bg_adjust();

  void hide_selected();
  void show_all();

  void set_zoom(double zoom, bool update_scroll_offsets);

private:
  void detail_change(float detail, trstring::native_type_arg banner_msg);

  /**
   * @brief update the item that was clicked on
   * @param citem the canvas item that was selected
   */
  void pen_down_item(canvas_item_t *citem);

  map_item_t *item_at(canvas_item_t *item);

public:
  void detail_increase();
  void detail_decrease();
  void detail_normal();

  /* various functions required by map_edit */
  map_item_t *item_at(lpos_t pos);

  /**
   * @brief get the current touchnode and remove the screen item
   *
   * This returns the node item associated with the current touchnode
   * (if any) and removes the touchnode from screen (i.e. touchnode_clear()).
   */
  node_t *touchnode_get_node();

  /**
   * @brief show an error message that the current position is outside the project bounds
   */
  static void outside_error();

  /**
   * @brief remove the item that shows the current GPS position
   */
  void remove_gps_position();

  void action_cancel();
  static inline void map_action_cancel(map_t *map)
  { map->action_cancel(); }

  void action_ok();
  static inline void map_action_ok(map_t *map)
  { map->action_ok(); }

  void delete_selected();
  static inline void map_delete_selected(map_t *map)
  { map->delete_selected(); }

  /* edit tags of currently selected object */
  void info_selected();

  static inline void edit_way_reverse(map_t *map)
  { map->way_reverse(); }

protected:
  // edit functions
  void way_cut_highlight(map_item_t *item, lpos_t pos);
  void way_cut(lpos_t pos);

  void way_add_begin();
  void way_add_segment(lpos_t pos);
  void way_add_cancel();
  void way_add_ok();

  void way_node_add_highlight(map_item_t *item, lpos_t pos);

  void way_node_add(lpos_t pos);

  void node_move(map_item_t *map_item, const osm2go_platform::screenpos &p);

  void way_reverse();

  // highlighting

  /**
  * @brief draw highlight cursor on map coordinates
  */
  void hl_cursor_draw(lpos_t pos, float radius);
  void hl_cursor_clear();

private:
  void touchnode_clear();
  void touchnode_update(lpos_t pos);
};
