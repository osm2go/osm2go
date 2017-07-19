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

#include "pos.h"

#include "misc.h"

#include <osm2go_cpp.h>

#include <cstring>
#include <ctype.h>
#include <cmath>

#define TAG_STATE  GTK_STATE_PRELIGHT

bool pos_t::valid() const
{
  return pos_lat_valid(lat) && pos_lon_valid(lon);
}

void pos_lat_str(char *str, size_t len, pos_float_t latitude) {
  if(std::isnan(latitude))
    strcpy(str, "---");
  else {
    snprintf(str, len-1, "%.5f", latitude);
    remove_trailing_zeroes(str);
  }
  strcat(str, "°");
}

void pos_lon_str(char *str, size_t len, pos_float_t longitude) {
  pos_lat_str(str, len, longitude);
}

pos_float_t pos_parse_lat(const char *str) {
  return g_strtod(str, O2G_NULLPTR);
}

pos_float_t pos_parse_lon(const char *str) {
  return g_strtod(str, O2G_NULLPTR);
}

bool pos_lat_valid(pos_float_t lat) {
  return(!std::isnan(lat) && (lat >= -90.0) && (lat <= 90.0));
}

bool pos_lon_valid(pos_float_t lon) {
  return(!std::isnan(lon) && (lon >= -180.0) && (lon <= 180.0));
}

static void mark(GtkWidget *widget, bool valid) {
  gtk_widget_set_state(widget, valid?GTK_STATE_NORMAL:TAG_STATE);
}

static void callback_modified_lat(GtkWidget *widget) {
  mark(widget, pos_lat_valid(pos_lat_get(widget)));
}

/* a entry that is colored red when being "active" */
GtkWidget *pos_lat_entry_new(pos_float_t lat) {
  GdkColor color;
  GtkWidget *widget = entry_new();
  gdk_color_parse("red", &color);
  gtk_widget_modify_text(widget, TAG_STATE, &color);

  char str[32];
  pos_lat_str(str, sizeof(str), lat);
  gtk_entry_set_text(GTK_ENTRY(widget), str);

  g_signal_connect(G_OBJECT(widget), "changed",
                   G_CALLBACK(callback_modified_lat), O2G_NULLPTR);

  return widget;
}

static void callback_modified_lon(GtkWidget *widget) {
  mark(widget, pos_lon_valid(pos_lon_get(widget)));
}

/* a entry that is colored red when filled with invalid coordinate */
GtkWidget *pos_lon_entry_new(pos_float_t lon) {
  GdkColor color;
  GtkWidget *widget = entry_new();
  gdk_color_parse("#ff0000", &color);
  gtk_widget_modify_text(widget, TAG_STATE, &color);

  char str[32];
  pos_lon_str(str, sizeof(str), lon);
  gtk_entry_set_text(GTK_ENTRY(widget), str);

  g_signal_connect(G_OBJECT(widget), "changed",
                   G_CALLBACK(callback_modified_lon), O2G_NULLPTR);

  return widget;
}

pos_float_t pos_lat_get(GtkWidget *widget) {
  const char *p = gtk_entry_get_text(GTK_ENTRY(widget));
  return pos_parse_lat(p);
}

pos_float_t pos_lon_get(GtkWidget *widget) {
  const char *p = gtk_entry_get_text(GTK_ENTRY(widget));
  return pos_parse_lon(p);
}

void pos_lat_entry_set(GtkWidget *entry, pos_float_t lat) {
  char str[32];
  pos_lat_str(str, sizeof(str), lat);
  gtk_entry_set_text(GTK_ENTRY(entry), str);
}

void pos_lon_entry_set(GtkWidget *entry, pos_float_t lon) {
  char str[32];
  pos_lon_str(str, sizeof(str), lon);
  gtk_entry_set_text(GTK_ENTRY(entry), str);
}

GtkWidget *pos_lat_label_new(pos_float_t lat) {
  char str[32];
  pos_lat_str(str, sizeof(str), lat);
  return gtk_label_new(str);
}

GtkWidget *pos_lon_label_new(pos_float_t lon) {
  char str[32];
  pos_lon_str(str, sizeof(str), lon);
  return gtk_label_new(str);
}

void pos_lat_label_set(GtkWidget *label, pos_float_t lat) {
  char str[32];
  pos_lat_str(str, sizeof(str), lat);
  gtk_label_set_text(GTK_LABEL(label), str);
}

void pos_lon_label_set(GtkWidget *label, pos_float_t lon) {
  char str[32];
  pos_lon_str(str, sizeof(str), lon);
  gtk_label_set_text(GTK_LABEL(label), str);
}

lpos_t pos_t::toLpos(const bounds_t &bounds) const {
  lpos_t lpos = toLpos();
  lpos.x = ( lpos.x - bounds.center.x) * bounds.scale;
  lpos.y = (-lpos.y + bounds.center.y) * bounds.scale;
  return lpos;
}

lpos_t pos_t::toLpos() const {
  lpos_t lpos;
  lpos.x = POS_EQ_RADIUS * DEG2RAD(lon);
  lpos.y = POS_EQ_RADIUS * log(tan(M_PI / 4 + DEG2RAD(lat) / 2));
  return lpos;
}

pos_t lpos_t::toPos(const bounds_t &bounds) const {
  lpos_t lpos = *this;
  lpos.x = ( lpos.x / bounds.scale) + bounds.center.x;
  lpos.y = (-lpos.y / bounds.scale) + bounds.center.y;

  pos_t pos;
  pos.lon = RAD2DEG(lpos.x / POS_EQ_RADIUS);
  pos.lat = RAD2DEG(2 * atan(exp(lpos.y / POS_EQ_RADIUS)) - M_PI/2);
  return pos;
}

void pos_dist_entry_set(GtkWidget *entry, pos_float_t dist, bool is_mil) {
  char str[32] = "---";
  if(!std::isnan(dist)) {
    /* is this to be displayed as miles? */
    if(is_mil) dist /= KMPMIL;  // kilometer per mile

    snprintf(str, sizeof(str), "%.4f", dist);
    remove_trailing_zeroes(str);
  }
  gtk_entry_set_text(GTK_ENTRY(entry), str);
}

pos_float_t pos_dist_get(GtkWidget *widget, bool is_mil) {
  const gchar *p = gtk_entry_get_text(GTK_ENTRY(widget));
  return g_strtod(p, O2G_NULLPTR) * (is_mil?KMPMIL:1.0);
}
