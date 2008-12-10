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

#include <math.h>

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
typedef struct pos {
  double lat, lon;
} pos_t;

/* local position */
typedef struct lpos {
  gint x, y;
} lpos_t;

struct bounds_s;
void pos2lpos(struct bounds_s *bounds, pos_t *pos, lpos_t *lpos);
void pos2lpos_center(pos_t *pos, lpos_t *lpos);
void lpos2pos(struct bounds_s *bounds, lpos_t *lpos, pos_t *pos);

void pos_lat_str(char *str, int len, double latitude);
void pos_lon_str(char *str, int len, double longitude);

double pos_parse_lat(char *str);
double pos_parse_lon(char *str);

GtkWidget *pos_lat_entry_new(double lat);
GtkWidget *pos_lon_entry_new(double lon);
void pos_lat_entry_set(GtkWidget *label, double lat);
void pos_lon_entry_set(GtkWidget *label, double lon);

GtkWidget *pos_lat_label_new(double lat);
GtkWidget *pos_lon_label_new(double lon);
void pos_lat_label_set(GtkWidget *label, double lat);
void pos_lon_label_set(GtkWidget *label, double lon);

double pos_lat_get(GtkWidget *widget);
double pos_lon_get(GtkWidget *widget);

gboolean pos_lat_valid(double lat);
gboolean pos_lon_valid(double lon);

double pos_parse_dist(char *str, gboolean is_mil);
void pos_dist_str(char *str, int len, double dist, gboolean is_mil);
void pos_dist_entry_set(GtkWidget *entry, double dist, gboolean is_mil);
double pos_dist_get(GtkWidget *widget, gboolean is_mil);

#endif // POS_H
