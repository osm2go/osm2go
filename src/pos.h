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

#ifndef POS_H
#define POS_H

#include <glib.h>
#include <gtk/gtk.h>
#include <math.h>

/* format string used to write lat/lon coordinates, altitude and time */
#define LL_FORMAT   "%.07f"
#define ALT_FORMAT  "%.02f"
#define DATE_FORMAT "%FT%T"

#ifdef USE_FLOAT
/* use float instead of double on small machines */
typedef float pos_float_t;
#else
typedef double pos_float_t;
#endif

#ifndef NAN
#define NAN (0.0/0.0)
#endif

/* equatorial radius in meters */
#define POS_EQ_RADIUS     (6378137.0)
#define KMPMIL   (1.609344)
#define KM2MIL(a)  ((a)/KMPMIL)
#define MIL2KM(a)  ((a)*KMPMIL)

#define DEG2RAD(a)  ((a) * M_PI / 180.0)
#define RAD2DEG(a)  ((a) * 180.0 / M_PI)

/* global position */
typedef struct pos_t {
  pos_float_t lat, lon;
#ifdef __cplusplus
  inline pos_t() {}
  inline pos_t(pos_float_t a, pos_float_t o) : lat(a), lon(o) {}
  bool operator==(const pos_t &other)
  { return lat == other.lat && lon == other.lon; }
#endif
} pos_t;

/* local position */
typedef struct lpos_t {
#ifdef __cplusplus
  lpos_t() {}
  lpos_t(gint px, gint py)
    : x(px) , y(py) {}
  bool operator==(const lpos_t &other)
  { return x == other.x && y == other.y; }
#endif
  gint x, y;
} lpos_t;

#ifdef __cplusplus
extern "C" {
#endif

struct bounds_t;
void pos2lpos(const struct bounds_t *bounds, const pos_t *pos, lpos_t *lpos);
void pos2lpos_center(const pos_t *pos, lpos_t *lpos);
void lpos2pos(const struct bounds_t *bounds, const lpos_t *lpos, pos_t *pos);

void pos_lat_str(char *str, int len, pos_float_t latitude);
void pos_lon_str(char *str, int len, pos_float_t longitude);

pos_float_t pos_parse_lat(const char *str);
pos_float_t pos_parse_lon(const char *str);

GtkWidget *pos_lat_entry_new(pos_float_t lat);
GtkWidget *pos_lon_entry_new(pos_float_t lon);
void pos_lat_entry_set(GtkWidget *label, pos_float_t lat);
void pos_lon_entry_set(GtkWidget *label, pos_float_t lon);

GtkWidget *pos_lat_label_new(pos_float_t lat);
GtkWidget *pos_lon_label_new(pos_float_t lon);
void pos_lat_label_set(GtkWidget *label, pos_float_t lat);
void pos_lon_label_set(GtkWidget *label, pos_float_t lon);

pos_float_t pos_lat_get(GtkWidget *widget);
pos_float_t pos_lon_get(GtkWidget *widget);

gboolean pos_lat_valid(pos_float_t lat);
gboolean pos_lon_valid(pos_float_t lon);

void pos_dist_entry_set(GtkWidget *entry, pos_float_t dist, gboolean is_mil);
pos_float_t pos_dist_get(GtkWidget *widget, gboolean is_mil);

#ifdef __cplusplus
}
#endif

#endif // POS_H
