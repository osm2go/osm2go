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

#include <ctype.h>

#include "appdata.h"
#include "misc.h"

#define TAG_STATE  GTK_STATE_PRELIGHT

void pos_lat_str(char *str, int len, pos_float_t latitude) {
  if(isnan(latitude))
    strcpy(str, "---");
  else {
    snprintf(str, len-1, "%.5f", latitude);

    /* eliminate trailing zeros */
    if((strchr(str, '.') != NULL) || (strchr(str, ',') != NULL)) {
      char *p = str+strlen(str)-1;
      while(*p == '0') *p-- = 0;
      if((*p == '.')||(*p == ','))
	*p = 0;
    }
  }
  strcat(str, "°");
}

void pos_lon_str(char *str, int len, pos_float_t longitude) {
  if(isnan(longitude))
    strcpy(str, "---");
  else {
    snprintf(str, len-1, "%.5f", longitude);

    /* eliminate trailing zeros */
    if((strchr(str, '.') != NULL) || (strchr(str, ',') != NULL)) {
      char *p = str+strlen(str)-1;
      while(*p == '0') *p-- = 0;
      if((*p == '.')||(*p == ','))
	*p = 0;
    }
  }
  strcat(str, "°");
}

pos_float_t pos_parse_lat(char *str) {
  return g_strtod(str, NULL);
}

pos_float_t pos_parse_lon(char *str) {
  return g_strtod(str, NULL);
}

gboolean pos_lat_valid(pos_float_t lat) {
  return(!isnan(lat) && (lat >= -90.0) && (lat <= 90.0));
}

gboolean pos_lon_valid(pos_float_t lon) {
  return(!isnan(lon) && (lon >= -180.0) && (lon <= 180.0));
}

static gboolean mark(GtkWidget *widget, gboolean valid) {
  gtk_widget_set_state(widget, valid?GTK_STATE_NORMAL:TAG_STATE);
  return valid;
}

static void callback_modified_lat(GtkWidget *widget, gpointer data ) {
  pos_float_t i = pos_parse_lat((char*)gtk_entry_get_text(GTK_ENTRY(widget)));
  mark(widget, pos_lat_valid(i));
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
                   G_CALLBACK(callback_modified_lat), NULL);

  return widget;
}

static void callback_modified_lon(GtkWidget *widget, gpointer data ) {
  pos_float_t i = pos_parse_lon((char*)gtk_entry_get_text(GTK_ENTRY(widget)));
  mark(widget, pos_lon_valid(i));
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
                   G_CALLBACK(callback_modified_lon), NULL);

  return widget;
}

pos_float_t pos_lat_get(GtkWidget *widget) {
  char *p = (char*)gtk_entry_get_text(GTK_ENTRY(widget));
  return pos_parse_lat(p);
}

pos_float_t pos_lon_get(GtkWidget *widget) {
  char *p = (char*)gtk_entry_get_text(GTK_ENTRY(widget));
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

void pos2lpos(bounds_t *bounds, pos_t *pos, lpos_t *lpos) {
  lpos->x = POS_EQ_RADIUS * DEG2RAD(pos->lon);
#ifdef USE_FLOAT
  lpos->y = POS_EQ_RADIUS * logf(tanf(M_PI/4 + DEG2RAD(pos->lat)/2));
#else
  lpos->y = POS_EQ_RADIUS * log(tan(M_PI/4 + DEG2RAD(pos->lat)/2));
#endif
  lpos->x = ( lpos->x - bounds->center.x) * bounds->scale;
  lpos->y = (-lpos->y + bounds->center.y) * bounds->scale;
}

/* the maps center is special as it isn't offset (by itself) */
void pos2lpos_center(pos_t *pos, lpos_t *lpos) {
  lpos->x = POS_EQ_RADIUS * DEG2RAD(pos->lon);
#ifdef USE_FLOAT
  lpos->y = POS_EQ_RADIUS * logf(tanf(M_PI/4 + DEG2RAD(pos->lat)/2));
#else
  lpos->y = POS_EQ_RADIUS * log(tan(M_PI/4 + DEG2RAD(pos->lat)/2));
#endif
}

void lpos2pos(bounds_t *bounds, lpos_t *lpos, pos_t *pos) {
  lpos_t tmp = *lpos;

  tmp.x = ( tmp.x/bounds->scale) + bounds->center.x;
  tmp.y = (-tmp.y/bounds->scale) + bounds->center.y;

  pos->lon = RAD2DEG(tmp.x / POS_EQ_RADIUS);
#ifdef USE_FLOAT
  pos->lat = RAD2DEG(2 * atanf(expf(tmp.y/POS_EQ_RADIUS)) - M_PI/2);
#else
  pos->lat = RAD2DEG(2 * atan(exp(tmp.y/POS_EQ_RADIUS)) - M_PI/2);
#endif
}

void pos_dist_str(char *str, int len, pos_float_t dist, gboolean is_mil) {
  if(isnan(dist))
    strcpy(str, "---");
  else {
    /* is this to be displayed as miles? */
    if(is_mil) dist /= KMPMIL;  // kilometer per mile

    snprintf(str, len, "%.4f", dist);
    /* eliminate trailing zeros */
    if((strchr(str, '.') != NULL) || (strchr(str, ',') != NULL)) {
      char *p = str+strlen(str)-1;
      while(*p == '0') *p-- = 0;
      if((*p == '.')||(*p == ','))
	*p = 0;
    }
  }
}

void pos_dist_entry_set(GtkWidget *entry, pos_float_t dist, gboolean is_mil) {
  char str[32];
  pos_dist_str(str, sizeof(str), dist, is_mil);
  gtk_entry_set_text(GTK_ENTRY(entry), str);
}

pos_float_t pos_parse_dist(char *str, gboolean is_mil) {
  return g_strtod(str, NULL) * (is_mil?KMPMIL:1.0);
}

pos_float_t pos_dist_get(GtkWidget *widget, gboolean is_mil) {
  char *p = (char*)gtk_entry_get_text(GTK_ENTRY(widget));
  return pos_parse_dist(p, is_mil);
}
