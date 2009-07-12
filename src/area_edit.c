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

#ifdef ENABLE_OSM_GPS_MAP
#include "osm-gps-map.h"
#endif

#define TAB_LABEL_MAP    "Map"
#define TAB_LABEL_DIRECT "Direct"
#define TAB_LABEL_EXTENT "Extent"
#define TAB_LABEL_MM     "Maemo Mapper"

/* limit of square kilometers above the warning is enabled */
#define WARN_OVER  5.0

typedef struct {
  GtkWidget *dialog, *notebook;
  area_edit_t *area;
  pos_t min, max;      /* local copy to work on */
  GtkWidget *minlat, *maxlat, *minlon, *maxlon;
  GtkWidget *warning;

  struct {
    GtkWidget *minlat, *maxlat, *minlon, *maxlon;
  } direct;

  struct {
    GtkWidget *lat, *lon, *height, *width, *mil_km;
    gboolean is_mil;
  } extent;

#ifdef USE_HILDON 
  struct {
    GtkWidget *fetch;
  } mmapper;
#endif

  struct {
    GtkWidget *widget;
    GtkWidget *zoomin, *zoomout;
    gboolean needs_redraw;
  } map;

} context_t;

static void parse_and_set_lat(GtkWidget *src, pos_float_t *store) {
  pos_float_t i = pos_parse_lat((char*)gtk_entry_get_text(GTK_ENTRY(src)));
  if(pos_lat_valid(i)) 
    *store = i;
}

static void parse_and_set_lon(GtkWidget *src, pos_float_t *store) {
  pos_float_t i = pos_parse_lon((char*)gtk_entry_get_text(GTK_ENTRY(src)));
  if(pos_lon_valid(i)) 
    *store = i;
}

#define LOG2(x) (log(x) / log(2))

static gboolean current_tab_is(context_t *context, gint page_num, char *str) {
  if(page_num < 0)
    page_num = 
      gtk_notebook_get_current_page(GTK_NOTEBOOK(context->notebook));

  if(page_num < 0) return FALSE;

  GtkWidget *w = 
    gtk_notebook_get_nth_page(GTK_NOTEBOOK(context->notebook), page_num);
  const char *name = 
    gtk_notebook_get_tab_label_text(GTK_NOTEBOOK(context->notebook), w);
  
  return(strcasecmp(name, _(str)) == 0);
}

static void on_area_warning_clicked(GtkButton *button, gpointer data) {
  context_t *context = (context_t*)data;

  /* compute area size */
  pos_float_t center_lat = (context->max.lat + context->min.lat)/2;
  double vscale = DEG2RAD(POS_EQ_RADIUS / 1000.0);
  double hscale = DEG2RAD(cos(DEG2RAD(center_lat)) * POS_EQ_RADIUS / 1000.0);

  double area = vscale * (context->max.lat - context->min.lat) *
    hscale * (context->max.lon - context->min.lon);

  warningf(context->dialog,
   _("The currently selected area is %.02f km² (%.02f mil²) in size. "
     "This is more than the recommended %.02f km² (%.02f mil²).\n\n"
     "Continuing may result in a big download and low mapping performance "
     "in a densly mapped area (e.g. cities)!"), 
	   area, area/(KMPMIL*KMPMIL),
	   WARN_OVER, WARN_OVER/(KMPMIL*KMPMIL)
	   );

}

static void area_main_update(context_t *context) {
  pos_lat_label_set(context->minlat, context->min.lat);
  pos_lat_label_set(context->maxlat, context->max.lat);
  pos_lon_label_set(context->minlon, context->min.lon);
  pos_lon_label_set(context->maxlon, context->max.lon);

  /* check if area size exceeds recommended values */
  pos_float_t center_lat = (context->max.lat + context->min.lat)/2;
  double vscale = DEG2RAD(POS_EQ_RADIUS / 1000.0);
  double hscale = DEG2RAD(cos(DEG2RAD(center_lat)) * POS_EQ_RADIUS / 1000.0);

  double area = vscale * (context->max.lat - context->min.lat) *
    hscale * (context->max.lon - context->min.lon);

  if(area > WARN_OVER)
    gtk_widget_show(context->warning);
  else
    gtk_widget_hide(context->warning);
}

#ifdef ENABLE_OSM_GPS_MAP
static GSList *pos_append(GSList *list, pos_float_t lat, pos_float_t lon) {
  coord_t *coo = g_new0(coord_t, 1);
  coo->rlat = DEG2RAD(lat); 
  coo->rlon = DEG2RAD(lon);
  list = g_slist_append(list, coo);
  return list;
}

/* the contents of the map tab have been changed */
static void map_update(context_t *context, gboolean forced) {

  /* map is first tab (page 0) */
  if(!forced && !current_tab_is(context, -1, TAB_LABEL_MAP)) {
    context->map.needs_redraw = TRUE;
    return;
  }

  pos_float_t center_lat = (context->max.lat + context->min.lat)/2;
  pos_float_t center_lon = (context->max.lon + context->min.lon)/2;

  /* we know the widgets pixel size, we know the required real size, */
  /* we want the zoom! */
  double vzoom = LOG2((45.0 * context->map.widget->allocation.height)/
		      ((context->max.lat - context->min.lat)*32.0));

  double hzoom = LOG2((45.0 * context->map.widget->allocation.width)/
		      ((context->max.lon - context->min.lon)*32.0));

  osm_gps_map_set_center(OSM_GPS_MAP(context->map.widget),
			 center_lat, center_lon);	    

  /* use smallest zoom, so everything fits on screen */
  osm_gps_map_set_zoom(OSM_GPS_MAP(context->map.widget), 
		       (vzoom < hzoom)?vzoom:hzoom);

  /* ---------- draw border (as a gps track) -------------- */  
  osm_gps_map_clear_tracks(OSM_GPS_MAP(context->map.widget));

  GSList *box = pos_append(NULL, context->min.lat, context->min.lon);
  box = pos_append(box, context->max.lat, context->min.lon);
  box = pos_append(box, context->max.lat, context->max.lon);
  box = pos_append(box, context->min.lat, context->max.lon);
  box = pos_append(box, context->min.lat, context->min.lon);

  osm_gps_map_add_track(OSM_GPS_MAP(context->map.widget), box);

  context->map.needs_redraw = FALSE;
}

static gboolean on_map_configure(GtkWidget *widget,
				 GdkEventConfigure *event,
				 context_t *context) {
  map_update(context, FALSE);
  return FALSE;
}
#endif

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

  /* direct is second tab (page 1) */
  if(!current_tab_is(context, -1, TAB_LABEL_DIRECT)) 
    return;

  /* parse the fields from the direct entry pad */
  parse_and_set_lat(context->direct.minlat, &context->min.lat);
  parse_and_set_lon(context->direct.minlon, &context->min.lon);
  parse_and_set_lat(context->direct.maxlat, &context->max.lat);
  parse_and_set_lon(context->direct.maxlon, &context->max.lon);

  area_main_update(context);

  /* also adjust other views */
  extent_update(context);
#ifdef ENABLE_OSM_GPS_MAP
  map_update(context, FALSE);
#endif
}

static void callback_modified_extent(GtkWidget *widget, gpointer data) {
  context_t *context = (context_t*)data;

  /* extent is third tab (page 2) */
  if(!current_tab_is(context, -1, TAB_LABEL_EXTENT)) 
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
  context->max.lat = center_lat + height;

  width /= 2 * hscale;
  context->min.lon = center_lon - width;
  context->max.lon = center_lon + width;

  area_main_update(context);
  
  /* also update other tabs */
  direct_update(context);
#ifdef ENABLE_OSM_GPS_MAP
  map_update(context, FALSE);
#endif
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

  /* maemo mapper is fourth tab (page 3) */
  if(!current_tab_is(context, -1, TAB_LABEL_MM)) 
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
  context->max.lat = center_lat + height;
  
  double hscale = DEG2RAD(cos(DEG2RAD(center_lat)) * POS_EQ_RADIUS);
  double width  = 16 * (1<<zoom) / hscale;
  context->min.lon = center_lon - width;
  context->max.lon = center_lon + width;

  area_main_update(context);

  /* also update other tabs */
  direct_update(context);
  extent_update(context);
#ifdef ENABLE_OSM_GPS_MAP
  map_update(context, FALSE);
#endif
}
#endif

#ifdef ENABLE_OSM_GPS_MAP
/* the user has changed the map view, update other views accordingly */
static void map_has_changed(context_t *context) {
  coord_t pt1, pt2;

  /* get maps bounding box */
  osm_gps_map_get_bbox(OSM_GPS_MAP(context->map.widget), &pt1, &pt2);

  context->min.lat = RAD2DEG(pt2.rlat);
  context->max.lat = RAD2DEG(pt1.rlat);

  context->min.lon = RAD2DEG(pt1.rlon);
  context->max.lon = RAD2DEG(pt2.rlon);

  area_main_update(context);
  direct_update(context);
  extent_update(context);
}

static gboolean
on_map_button_release_event(GtkWidget *widget, 
			    GdkEventButton *event, context_t *context) {
  map_has_changed(context);
  return FALSE;
}

static void map_zoom(context_t *context, int step) {
  int zoom;
  OsmGpsMap *map = OSM_GPS_MAP(context->map.widget);
  g_object_get(map, "zoom", &zoom, NULL);
  zoom = osm_gps_map_set_zoom(map, zoom+step);

  /* enable/disable zoom buttons as required */
  gtk_widget_set_sensitive(context->map.zoomin, zoom<17);
  gtk_widget_set_sensitive(context->map.zoomout, zoom>1);

  map_has_changed(context);
}

static gboolean
cb_map_zoomin(GtkButton *button, context_t *context) {
  map_zoom(context, +1);
  return FALSE;
}

static gboolean
cb_map_zoomout(GtkButton *button, context_t *context) {
  map_zoom(context, -1);
  return FALSE;
}

static void on_page_switch(GtkNotebook *notebook, GtkNotebookPage *page,
			   guint page_num, context_t *context) {

  /* updating the map while the user manually changes some coordinates */
  /* may confuse the map. so we delay those updates until the map tab */
  /* is becoming visible */
  if(current_tab_is(context, page_num, TAB_LABEL_MAP) && 
     context->map.needs_redraw) 
    map_update(context, TRUE);
}
#endif

gboolean area_edit(area_edit_t *area) {
  GtkWidget *vbox;
  gboolean ok = FALSE;

  context_t context;
  memset(&context, 0, sizeof(context_t));
  context.area = area;
  context.min.lat = area->min->lat;
  context.min.lon = area->min->lon;
  context.max.lat = area->max->lat;
  context.max.lon = area->max->lon;

  context.dialog = 
    misc_dialog_new(MISC_DIALOG_HIGH, _("Area editor"),
	  GTK_WINDOW(area->parent),
	  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
          NULL);

  GtkWidget *table = gtk_table_new(5, 2, FALSE);  // x, y

  GtkWidget *label = gtk_label_new(_("Latitude:"));
  misc_table_attach(table, label, 0, 0);
  context.minlat = pos_lat_label_new(area->min->lat);
  misc_table_attach(table, context.minlat, 1, 0);
  label = gtk_label_new(_("to"));
  misc_table_attach(table, label, 2, 0);
  context.maxlat = pos_lat_label_new(area->max->lat);
  misc_table_attach(table, context.maxlat, 3, 0);

  label = gtk_label_new(_("Longitude:"));
  misc_table_attach(table, label, 0, 1);
  context.minlon = pos_lon_label_new(area->min->lon);
  misc_table_attach(table, context.minlon, 1, 1);
  label = gtk_label_new(_("to"));
  misc_table_attach(table, label, 2, 1);
  context.maxlon = pos_lon_label_new(area->max->lon);
  misc_table_attach(table, context.maxlon, 3, 1);

  context.warning = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(context.warning), 
		       gtk_image_new_from_stock(GTK_STOCK_DIALOG_WARNING, 
						GTK_ICON_SIZE_BUTTON));
  g_signal_connect(context.warning, "clicked", 
  		   G_CALLBACK(on_area_warning_clicked), &context);
  gtk_table_attach_defaults(GTK_TABLE(table), context.warning, 4, 5, 0, 2);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
			      table, FALSE, FALSE, 0);

  context.notebook = gtk_notebook_new();

#ifdef ENABLE_OSM_GPS_MAP
  /* ------------- fetch from map ------------------------ */

  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);

  context.map.needs_redraw = FALSE;
  context.map.widget = g_object_new(OSM_TYPE_GPS_MAP,
	        "repo-uri", MAP_SOURCE_OPENSTREETMAP,
		"proxy-uri", misc_get_proxy_uri(area->settings),
		 NULL);

  g_signal_connect(G_OBJECT(context.map.widget), "configure-event",
		   G_CALLBACK(on_map_configure), &context);
  g_signal_connect(G_OBJECT(context.map.widget), "button-release-event",
		   G_CALLBACK(on_map_button_release_event), &context);

  gtk_box_pack_start_defaults(GTK_BOX(hbox), context.map.widget);

  /* zoom button box */
  vbox = gtk_vbox_new(FALSE,0);

  context.map.zoomin = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(context.map.zoomin), 
       gtk_image_new_from_stock(GTK_STOCK_ZOOM_IN, GTK_ICON_SIZE_MENU));
  g_signal_connect(context.map.zoomin, "clicked", 
		   G_CALLBACK(cb_map_zoomin), &context);
  gtk_box_pack_start(GTK_BOX(vbox), context.map.zoomin, FALSE, FALSE, 0);

  context.map.zoomout = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(context.map.zoomout), 
       gtk_image_new_from_stock(GTK_STOCK_ZOOM_OUT, GTK_ICON_SIZE_MENU));
  g_signal_connect(context.map.zoomout, "clicked", 
		   G_CALLBACK(cb_map_zoomout), &context);
  gtk_box_pack_start(GTK_BOX(vbox), context.map.zoomout, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

  gtk_notebook_append_page(GTK_NOTEBOOK(context.notebook),
		   hbox, gtk_label_new(_(TAB_LABEL_MAP)));
#endif

  /* ------------ direct min/max edit --------------- */

  vbox = gtk_vbox_new(FALSE, 10);
  table = gtk_table_new(3, 3, FALSE);  // x, y
  gtk_table_set_col_spacings(GTK_TABLE(table), 10);
  gtk_table_set_row_spacings(GTK_TABLE(table), 5);

  context.direct.minlat = pos_lat_entry_new(0.0);
  misc_table_attach(table, context.direct.minlat, 0, 0);
  label = gtk_label_new(_("to"));
  misc_table_attach(table,  label, 1, 0);
  context.direct.maxlat = pos_lat_entry_new(0.0);
  misc_table_attach(table, context.direct.maxlat, 2, 0);

  context.direct.minlon = pos_lon_entry_new(area->min->lon);
  misc_table_attach(table, context.direct.minlon, 0, 1);
  label = gtk_label_new(_("to"));
  misc_table_attach(table,  label, 1, 1);
  context.direct.maxlon = pos_lon_entry_new(0.0);
  misc_table_attach(table, context.direct.maxlon, 2, 1);

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

  gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
  gtk_notebook_append_page(GTK_NOTEBOOK(context.notebook),
	   vbox, gtk_label_new(_(TAB_LABEL_DIRECT)));

  /* ------------- center/extent edit ------------------------ */

  vbox = gtk_vbox_new(FALSE, 10);
  table = gtk_table_new(3, 4, FALSE);  // x, y
  gtk_table_set_col_spacings(GTK_TABLE(table), 10);
  gtk_table_set_row_spacings(GTK_TABLE(table), 5);

  label = gtk_label_new(_("Center:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 0, 1);
  context.extent.lat = pos_lat_entry_new(0.0);
  gtk_table_attach_defaults(GTK_TABLE(table), context.extent.lat, 1, 2, 0, 1);
  context.extent.lon = pos_lon_entry_new(0.0);
  gtk_table_attach_defaults(GTK_TABLE(table), context.extent.lon, 2, 3, 0, 1);

  gtk_table_set_row_spacing(GTK_TABLE(table), 0, 10);

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

  gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
  gtk_notebook_append_page(GTK_NOTEBOOK(context.notebook),
		   vbox, gtk_label_new(_(TAB_LABEL_EXTENT)));

#ifdef USE_HILDON 
  /* ------------- fetch from maemo mapper ------------------------ */

  vbox = gtk_vbox_new(FALSE, 8);
  context.mmapper.fetch = 
    gtk_button_new_with_label(_("Get from Maemo Mapper"));
  gtk_box_pack_start(GTK_BOX(vbox), context.mmapper.fetch, FALSE, FALSE, 0);

  g_signal_connect(G_OBJECT(context.mmapper.fetch), "clicked",
		   G_CALLBACK(callback_fetch_mm_clicked), &context);

  /* --- hint --- */
  label = gtk_label_new(_("(recommended MM zoom level < 7)"));
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);


  gtk_notebook_append_page(GTK_NOTEBOOK(context.notebook),
		   vbox, gtk_label_new(_(TAB_LABEL_MM)));
#endif

  /* ------------------------------------------------------ */

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context.dialog)->vbox), 
			      context.notebook);

#ifdef ENABLE_OSM_GPS_MAP
  g_signal_connect(G_OBJECT(context.notebook), "switch-page",
		   G_CALLBACK(on_page_switch), &context);
#endif

  gtk_widget_show_all(context.dialog);

  area_main_update(&context);

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
