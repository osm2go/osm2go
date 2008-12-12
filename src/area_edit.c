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

#include "appdata.h"

typedef struct {
  GtkWidget *dialog, *notebook;
  area_edit_t *area;
  pos_t min, max;      /* local copy to work on */
  GtkWidget *minlat, *maxlat, *minlon, *maxlon;

  struct {
    GtkWidget *minlat, *maxlat, *minlon, *maxlon;
  } direct;

  struct {
    GtkWidget *lat, *lon, *height, *width, *mil_km;
    gboolean is_mil;
  } extent;

  struct {
    GtkWidget *fetch;
  } mmapper;

} context_t;

static void parse_and_set_lat(GtkWidget *src, GtkWidget *dst, pos_float_t *store) {
  pos_float_t i = pos_parse_lat((char*)gtk_entry_get_text(GTK_ENTRY(src)));
  if(pos_lat_valid(i)) {
    *store = i;
    pos_lat_label_set(dst, i);
  }
}

static void parse_and_set_lon(GtkWidget *src, GtkWidget *dst, pos_float_t *store) {
  pos_float_t i = pos_parse_lon((char*)gtk_entry_get_text(GTK_ENTRY(src)));
  if(pos_lon_valid(i)) {
    *store = i;
    pos_lon_label_set(dst, i);
  }
}

/* the contents of the direct tab have been changed */
static void direct_update(context_t *context) {
  pos_lat_entry_set(context->direct.minlat, context->min.lat);
  pos_lon_entry_set(context->direct.minlon, context->min.lon);
  pos_lat_entry_set(context->direct.maxlat, context->max.lat);
  pos_lon_entry_set(context->direct.maxlon, context->max.lon);
}

/* update the contents of the extent tab */
static void extent_update(context_t *context) {
  pos_float_t center_lat = (context->max.lat + context->min.lat)/2;
  pos_float_t center_lon = (context->max.lon + context->min.lon)/2;

  pos_lat_entry_set(context->extent.lat, center_lat);
  pos_lat_entry_set(context->extent.lon, center_lon);

  double vscale = DEG2RAD(POS_EQ_RADIUS / 1000.0);
  double hscale = DEG2RAD(cos(DEG2RAD(center_lat)) * POS_EQ_RADIUS / 1000.0);

  double height = vscale * (context->max.lat - context->min.lat);
  double width  = hscale * (context->max.lon - context->min.lon);

  pos_dist_entry_set(context->extent.width, width, context->extent.is_mil);
  pos_dist_entry_set(context->extent.height, height, context->extent.is_mil);
}

static void callback_modified_direct(GtkWidget *widget, gpointer data) {
  context_t *context = (context_t*)data;

  /* direct is first tab (page 0) */
  if(gtk_notebook_get_current_page(GTK_NOTEBOOK(context->notebook)) != 0)
    return;

  /* parse the fields from the direct entry pad */
  parse_and_set_lat(context->direct.minlat, context->minlat, &context->min.lat);
  parse_and_set_lon(context->direct.minlon, context->minlon, &context->min.lon);
  parse_and_set_lat(context->direct.maxlat, context->maxlat, &context->max.lat);
  parse_and_set_lon(context->direct.maxlon, context->maxlon, &context->max.lon);

  /* also adjust other views */
  extent_update(context);
}

static void callback_modified_extent(GtkWidget *widget, gpointer data) {
  context_t *context = (context_t*)data;

  /* extent is second tab (page 1) */
  if(gtk_notebook_get_current_page(GTK_NOTEBOOK(context->notebook)) != 1)
    return;

  pos_float_t center_lat = pos_lat_get(context->extent.lat);
  pos_float_t center_lon = pos_lon_get(context->extent.lon);

  if(!pos_lat_valid(center_lat) || !pos_lon_valid(center_lon))
    return;
  
  double vscale = DEG2RAD(POS_EQ_RADIUS / 1000.0);
  double hscale = DEG2RAD(cos(DEG2RAD(center_lat)) * POS_EQ_RADIUS / 1000.0);

  double height = pos_dist_get(context->extent.height, context->extent.is_mil);
  double width  = pos_dist_get(context->extent.width, context->extent.is_mil);

  height /= 2 * vscale;
  context->min.lat = center_lat - height;
  pos_lat_label_set(context->minlat, context->min.lat);
  context->max.lat = center_lat + height;
  pos_lat_label_set(context->maxlat, context->max.lat);
  
  width /= 2 * hscale;
  context->min.lon = center_lon - width;
  pos_lon_label_set(context->minlon, context->min.lon);
  context->max.lon = center_lon + width;
  pos_lon_label_set(context->maxlon, context->max.lon);
  
  /* also update other tabs */
  direct_update(context);
}

static void callback_modified_unit(GtkWidget *widget, gpointer data) {
  context_t *context = (context_t*)data;

  /* get current values */
  double height = pos_dist_get(context->extent.height, context->extent.is_mil);
  double width  = pos_dist_get(context->extent.width, context->extent.is_mil);

  /* adjust unit flag */
  context->extent.is_mil = gtk_combo_box_get_active(
		    GTK_COMBO_BOX(context->extent.mil_km)) == 0;

  /* save values */
  pos_dist_entry_set(context->extent.width, width, context->extent.is_mil);
  pos_dist_entry_set(context->extent.height, height, context->extent.is_mil);  
}

#ifdef USE_HILDON
static void callback_fetch_mm_clicked(GtkButton *button, gpointer data) {
  context_t *context = (context_t*)data;

  printf("clicked fetch mm!\n");

  if(!dbus_mm_set_position(context->area->osso_context, NULL)) {
    errorf(context->dialog, 
	   _("Unable to communicate with Maemo Mapper. "
	     "You need to have Maemo Mapper installed "
	     "to use this feature."));
    return;
  }

  if(!context->area->mmpos->valid) {
    errorf(context->dialog, 
	   _("No valid position received yet. You need "
	     "to scroll or zoom the Maemo Mapper view "
	     "in order to force it to send its current "
	     "view position to osm2go."));
    return;
  }

  /* maemo mapper is third tab (page 2) */
  if(gtk_notebook_get_current_page(GTK_NOTEBOOK(context->notebook)) != 2)
    return;

  /* maemo mapper pos data ... */
  pos_float_t center_lat = context->area->mmpos->pos.lat;
  pos_float_t center_lon = context->area->mmpos->pos.lon;
  int zoom = context->area->mmpos->zoom;

  if(!pos_lat_valid(center_lat) || !pos_lon_valid(center_lon))
    return;
  
  double vscale = DEG2RAD(POS_EQ_RADIUS);
  double height = 8 * (1<<zoom) / vscale;
  context->min.lat = center_lat - height;
  pos_lat_label_set(context->minlat, context->min.lat);
  context->max.lat = center_lat + height;
  pos_lat_label_set(context->maxlat, context->max.lat);
  
  double hscale = DEG2RAD(cos(DEG2RAD(center_lat)) * POS_EQ_RADIUS);
  double width  = 16 * (1<<zoom) / hscale;
  context->min.lon = center_lon - width;
  pos_lon_label_set(context->minlon, context->min.lon);
  context->max.lon = center_lon + width;
  pos_lon_label_set(context->maxlon, context->max.lon);

  /* also update other tabs */
  direct_update(context);
  extent_update(context);
}
#endif

gboolean area_edit(area_edit_t *area) {
  gboolean ok = FALSE;

  context_t context;
  memset(&context, 0, sizeof(context_t));
  context.area = area;
  context.min.lat = area->min->lat;
  context.min.lon = area->min->lon;
  context.max.lat = area->max->lat;
  context.max.lon = area->max->lon;

  context.dialog = gtk_dialog_new_with_buttons(
	  _("Area editor"),
	  GTK_WINDOW(area->parent), GTK_DIALOG_MODAL,
	  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
          NULL);

#ifdef USE_HILDON
  //  gtk_window_set_default_size(GTK_WINDOW(context.dialog), 640, 100);
#else
  //  gtk_window_set_default_size(GTK_WINDOW(context.dialog), 400, 100);
#endif

  GtkWidget *table = gtk_table_new(3, 3, FALSE);  // x, y

  GtkWidget *label = gtk_label_new(_("Latitude"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 1, 2, 0, 1);
  label = gtk_label_new(_("Longitude"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 2, 3, 0, 1);

  label = gtk_label_new(_("Min:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 1, 2);
  context.minlat = pos_lat_label_new(area->min->lat);
  gtk_table_attach_defaults(GTK_TABLE(table), context.minlat, 1, 2, 1, 2);
  context.minlon = pos_lon_label_new(area->min->lon);
  gtk_table_attach_defaults(GTK_TABLE(table), context.minlon, 2, 3, 1, 2);

  label = gtk_label_new(_("Max:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 2, 3);
  context.maxlat = pos_lat_label_new(area->max->lat);
  gtk_table_attach_defaults(GTK_TABLE(table), context.maxlat, 1, 2, 2, 3);
  context.maxlon = pos_lon_label_new(area->max->lon);
  gtk_table_attach_defaults(GTK_TABLE(table), context.maxlon, 2, 3, 2, 3);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context.dialog)->vbox), 
			      table);

  context.notebook = gtk_notebook_new();

  /* ------------ direct min/max edit --------------- */

  table = gtk_table_new(3, 3, FALSE);  // x, y

  label = gtk_label_new(_("Min:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 0, 1);
  context.direct.minlat = pos_lat_entry_new(0.);
  gtk_table_attach_defaults(GTK_TABLE(table), context.direct.minlat, 1, 2, 0, 1);
  context.direct.minlon = pos_lon_entry_new(area->min->lon);
  gtk_table_attach_defaults(GTK_TABLE(table), context.direct.minlon, 2, 3, 0, 1);

  label = gtk_label_new(_("Max:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 1, 2);
  context.direct.maxlat = pos_lat_entry_new(0.0);
  gtk_table_attach_defaults(GTK_TABLE(table), context.direct.maxlat, 1, 2, 1, 2);
  context.direct.maxlon = pos_lon_entry_new(0.0);
  gtk_table_attach_defaults(GTK_TABLE(table), context.direct.maxlon, 2, 3, 1, 2);

  /* setup this page */
  direct_update(&context);

  g_signal_connect(G_OBJECT(context.direct.minlat), "changed",
		   G_CALLBACK(callback_modified_direct), &context);
  g_signal_connect(G_OBJECT(context.direct.minlon), "changed",
		   G_CALLBACK(callback_modified_direct), &context);
  g_signal_connect(G_OBJECT(context.direct.maxlat), "changed",
		   G_CALLBACK(callback_modified_direct), &context);
  g_signal_connect(G_OBJECT(context.direct.maxlon), "changed",
		   G_CALLBACK(callback_modified_direct), &context);


  /* --- hint --- */
  label = gtk_label_new(_("(recommended min/max diff <0.03 degrees)"));
  gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 3, 2, 3);

  gtk_notebook_append_page(GTK_NOTEBOOK(context.notebook),
		 table, gtk_label_new(_("Direct")));

  /* ------------- center/extent edit ------------------------ */

  table = gtk_table_new(3, 4, FALSE);  // x, y

  label = gtk_label_new(_("Center:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 0, 1);
  context.extent.lat = pos_lat_entry_new(0.0);
  gtk_table_attach_defaults(GTK_TABLE(table), context.extent.lat, 1, 2, 0, 1);
  context.extent.lon = pos_lon_entry_new(0.0);
  gtk_table_attach_defaults(GTK_TABLE(table), context.extent.lon, 2, 3, 0, 1);

  gtk_table_set_row_spacing(GTK_TABLE(table), 0, 8);

  label = gtk_label_new(_("Width:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 1, 2);
  context.extent.width = gtk_entry_new();
  gtk_table_attach_defaults(GTK_TABLE(table), context.extent.width, 1, 2, 1, 2);

  label = gtk_label_new(_("Height:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 2, 3);
  context.extent.height = gtk_entry_new();
  gtk_table_attach_defaults(GTK_TABLE(table), context.extent.height, 1, 2, 2, 3);

  context.extent.mil_km = gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(context.extent.mil_km), _("mi"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(context.extent.mil_km), _("km"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(context.extent.mil_km), 1); // km

  gtk_table_attach(GTK_TABLE(table), context.extent.mil_km, 2, 3, 1, 3, 
		   0, 0, 0, 0);

  /* setup this page */
  extent_update(&context);

  /* connect signals after inital update to avoid confusion */
  g_signal_connect(G_OBJECT(context.extent.lat), "changed",
		   G_CALLBACK(callback_modified_extent), &context);
  g_signal_connect(G_OBJECT(context.extent.lon), "changed",
		   G_CALLBACK(callback_modified_extent), &context);
  g_signal_connect(G_OBJECT(context.extent.width), "changed",
		   G_CALLBACK(callback_modified_extent), &context);
  g_signal_connect(G_OBJECT(context.extent.height), "changed",
		   G_CALLBACK(callback_modified_extent), &context);
  g_signal_connect(G_OBJECT(context.extent.mil_km), "changed",
		   G_CALLBACK(callback_modified_unit), &context);

  /* --- hint --- */
  label = gtk_label_new(_("(recommended width/height < 2km/1.25mi)"));
  gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 3, 3, 4);


  gtk_notebook_append_page(GTK_NOTEBOOK(context.notebook),
		   table, gtk_label_new(_("Extent")));

#ifdef USE_HILDON 
  /* ------------- fetch from maemo mapper ------------------------ */

  GtkWidget *vbox = gtk_vbox_new(FALSE, 8);
  context.mmapper.fetch = 
    gtk_button_new_with_label(_("Get from Maemo Mapper"));
  gtk_box_pack_start_defaults(GTK_BOX(vbox), context.mmapper.fetch);

  g_signal_connect(G_OBJECT(context.mmapper.fetch), "clicked",
		   G_CALLBACK(callback_fetch_mm_clicked), &context);
  //  gtk_widget_set_sensitive(context.mmapper.fetch, context.area->mmpos->valid);

  /* --- hint --- */
  label = gtk_label_new(_("(recommended MM zoom level < 7)"));
  gtk_box_pack_start_defaults(GTK_BOX(vbox), label);


  gtk_notebook_append_page(GTK_NOTEBOOK(context.notebook),
		   vbox, gtk_label_new(_("Maemo Mapper")));
#endif

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context.dialog)->vbox), 
			      context.notebook);


  gtk_widget_show_all(context.dialog);

  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context.dialog))) {
    /* copy modified values back to given storage */
    area->min->lat = context.min.lat;
    area->min->lon = context.min.lon;
    area->max->lat = context.max.lat;
    area->max->lon = context.max.lon;
    ok = TRUE;
  }

  gtk_widget_destroy(context.dialog);

  return ok;
}
