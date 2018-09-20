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

#ifndef CANVAS_H
#define CANVAS_H

#include "color.h"
#include "pos.h"

#include <array>
#include <unordered_map>
#include <vector>
#include <unordered_map>

#include <osm2go_cpp.h>
#include <osm2go_platform.h>

/* --------- generic canvas --------- */

#define CANVAS_FRISKET_SCALE 2.0

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
#error "More than 16 canvas groups needs adjustment e.g. in map.cpp"
#endif

class canvas_item_info_t;
struct map_item_t;

struct canvas_item_t {
  canvas_item_t() O2G_DELETED_FUNCTION;
  canvas_item_t &operator=(const canvas_item_t &) O2G_DELETED_FUNCTION;

  static void operator delete(void *ptr);

  /****** manipulating items ******/
  void set_zoom_max(float zoom_max);
  void set_dashed(unsigned int line_width, unsigned int dash_length_on,
                  unsigned int dash_length_off);
  void set_user_data(map_item_t *data, void (*c_handler)(map_item_t *));
  map_item_t *get_user_data();
  void destroy_connect(void (*c_handler)(void *), void *data);
};

struct canvas_item_circle : public canvas_item_t {
  canvas_item_circle() O2G_DELETED_FUNCTION;
  canvas_item_circle &operator=(const canvas_item_circle &) O2G_DELETED_FUNCTION;

  void set_radius(int radius);
};

struct canvas_item_polyline : public canvas_item_t {
  canvas_item_polyline() O2G_DELETED_FUNCTION;
  canvas_item_polyline &operator=(const canvas_item_polyline &) O2G_DELETED_FUNCTION;

  void set_points(const std::vector<lpos_t> &points);
};

struct canvas_item_pixmap : public canvas_item_t {
  canvas_item_pixmap() O2G_DELETED_FUNCTION;
  canvas_item_pixmap &operator=(const canvas_item_pixmap &) O2G_DELETED_FUNCTION;

  void image_move(int x, int y, float hscale, float vscale);
};

class canvas_t {
protected:
  explicit canvas_t(osm2go_platform::Widget *w);

public:
  static canvas_t *create();

  osm2go_platform::Widget * const widget;
  typedef std::unordered_map<canvas_item_t *, canvas_item_info_t *> item_mapping_t;
  item_mapping_t item_mapping;

  std::array<std::vector<canvas_item_info_t *>, CANVAS_GROUPS> item_info;

  lpos_t window2world(int x, int y) const;
  void scroll_get(int &sx, int &sy) const;

  /****** manipulating the canvas ******/
  void set_background(color_t bg_color);
  void erase(unsigned int group_mask);
  canvas_item_t *get_item_at(lpos_t pos) const;
  /**
   * @brief set new zoom level
   * @param zoom the intended zoom level
   * @return the zoom factor actually set
   *
   * The zoom factor is limited so the visible map size is never smaller than
   * the screen dimensions.
   */
  double set_zoom(double zoom);
  double get_zoom() const;
  void scroll_to(int sx, int sy);
  void scroll_step(int dx, int dy);
  void set_bounds(lpos_t min, lpos_t max);
  void item_to_bottom(canvas_item_t *item);

  /***** creating/destroying items ******/
  canvas_item_circle *circle_new(canvas_group_t group,
                            int x, int y, unsigned int radius, int border,
                            color_t fill_col, color_t border_col = color_t::transparent());
  canvas_item_polyline *polyline_new(canvas_group_t group, const std::vector<lpos_t> &points,
                              unsigned int width, color_t color);
  canvas_item_t *polygon_new(canvas_group_t group, const std::vector<lpos_t> &points,
                             unsigned int width, color_t color,
                             color_t fill);
  canvas_item_pixmap *image_new(canvas_group_t group, osm2go_platform::Pixmap pix, int x, int y,
                           float hscale, float vscale);

  void item_info_push(canvas_item_t *item);

  /**
   * @brief get the polygon/polyway segment a certain coordinate is over
   */
  int get_item_segment(const canvas_item_t *item, lpos_t pos) const;

  /**
   * @brief make sure the given coordinate is visible on screen
   * @return if the position must be read new
   *
   * The coordinate must be within the project bounds.
   */
  bool ensureVisible(const lpos_t lpos);
};

#endif // CANVAS_H
