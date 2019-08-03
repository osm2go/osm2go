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

#pragma once

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
  CANVAS_GROUP_BG=0,       ///< background layer (wms overlay)
  CANVAS_GROUP_POLYGONS,   ///< polygons (forrests, buildings, lakes) */
  CANVAS_GROUP_WAYS_HL,    ///< highlighting of ways
  CANVAS_GROUP_WAYS_OL,    ///< outlines for ways (e.g. for highways)
  CANVAS_GROUP_WAYS,       ///< ways
  CANVAS_GROUP_WAYS_INT,   ///< interior of ways with outlines
  CANVAS_GROUP_WAYS_DIR,   ///< direction arrows for ways
  CANVAS_GROUP_NODES_HL,   ///< highlighting for nodes
  CANVAS_GROUP_NODES,      ///< nodes
  CANVAS_GROUP_NODES_IHL,  ///< highlighting for otherwise invisible way nodes
  CANVAS_GROUP_TRACK,      ///< (GPS) track
  CANVAS_GROUP_GPS,        ///< current GPS position
  CANVAS_GROUP_FRISKET,    ///< the (white) frisket limiting the view
  CANVAS_GROUP_DRAW,       ///< "cursor" functionality
  CANVAS_GROUPS            ///< total number of groups
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
class icon_item;
struct map_item_t;

class canvas_item_destroyer;

struct canvas_item_t {
  canvas_item_t() O2G_DELETED_FUNCTION;
  canvas_item_t &operator=(const canvas_item_t &) O2G_DELETED_FUNCTION;

  static void operator delete(void *ptr);

  /****** manipulating items ******/
  void set_zoom_max(float zoom_max);
  void set_dashed(unsigned int line_width, unsigned int dash_length_on,
                  unsigned int dash_length_off);

  /**
   * @brief associates the map item with this canvas item
   *
   * A destroyer will be instantiated so the data is deleted when the
   * canvas item is.
   */
  void set_user_data(map_item_t *data);
  map_item_t *get_user_data();
  void destroy_connect(canvas_item_destroyer *d);
};

class canvas_item_destroyer {
public:
  virtual ~canvas_item_destroyer() {}

  virtual void run(canvas_item_t *item) = 0;
};

class map_item_destroyer : public canvas_item_destroyer {
public:
  map_item_t * const mi;
  explicit inline map_item_destroyer(map_item_t *m) : canvas_item_destroyer(), mi(m) {}

  void run(canvas_item_t *) override;
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
};

class canvas_t {
protected:
  explicit canvas_t(osm2go_platform::Widget *w);

public:
  osm2go_platform::Widget * const widget;
  typedef std::unordered_map<const canvas_item_t *, canvas_item_info_t *> item_mapping_t;
  item_mapping_t item_mapping;

  lpos_t window2world(const osm2go_platform::screenpos &p) const;

  /**
   * @brief query the current position of the scrollbars
   */
  void scroll_get(float &sx, float &sy) const;

  /****** manipulating the canvas ******/
  void set_background(color_t bg_color);

  /**
   * @brief set the background image
   * @param filename the file to load
   * @returns if loading was successful
   *
   * Passing an empty string clears the current image.
   */
  bool set_background(const std::string &filename);

  /**
   * @brief move the background image
   */
  void move_background(int x, int y);

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

  /**
   * @brief scroll to the given position or a valid position close to it
   *
   * This will try to center the view on the given position, or a bounded
   * value if the given position is too close to the canvas edges. The
   * actual position is returned.
   */
  void scroll_to(float &sx, float &sy);

  /**
   * @brief relative move of the visible screen area
   * @param d the offset to the current position
   * @param nx the new x position in canvas coordinates
   * @param ny the new y position in canvas coordinates
   */
  void scroll_step(const osm2go_platform::screenpos &d, float &nx, float &ny);
  void set_bounds(lpos_t min, lpos_t max);
  void item_to_bottom(canvas_item_t *item);

  /***** creating items ******/
  canvas_item_circle *circle_new(canvas_group_t group, lpos_t c, unsigned int radius, int border,
                            color_t fill_col, color_t border_col = color_t::transparent());
  canvas_item_polyline *polyline_new(canvas_group_t group, const std::vector<lpos_t> &points,
                              unsigned int width, color_t color);
  canvas_item_t *polygon_new(canvas_group_t group, const std::vector<lpos_t> &points,
                             unsigned int width, color_t color,
                             color_t fill);
  canvas_item_pixmap *image_new(canvas_group_t group, icon_item *icon, lpos_t pos, float scale);

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
