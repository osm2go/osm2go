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

#include "area_edit.h"

#include "appdata.h"
#include "gps.h"
#include "misc.h"
#include "settings.h"

#ifdef ENABLE_OSM_GPS_MAP
#include "osm-gps-map.h"
#include "osm-gps-map-osd-select.h"
#endif

#include <osm2go_cpp.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <strings.h>

#define TAB_LABEL_MAP    "Map"
#define TAB_LABEL_DIRECT "Direct"
#define TAB_LABEL_EXTENT "Extent"

/* maemo5 just got maemo mapper */
#if defined(USE_HILDON)
#include "dbus.h"
#define HAS_MAEMO_MAPPER
#ifdef FREMANTLE
#define TAB_LABEL_MM     "M.Mapper"
#else
#define TAB_LABEL_MM     "Maemo Mapper"
#endif
#endif

/* limit of square kilometers above the warning is enabled */
#define WARN_OVER  5.0

struct context_t {
  explicit context_t(area_edit_t &a);

  GtkWidget *dialog, *notebook;
  area_edit_t &area;
  pos_t min, max;      /* local copy to work on */
  GtkWidget *warning;

  struct {
    GtkWidget *minlat, *maxlat, *minlon, *maxlon;
    GtkWidget *error;
  } direct;

  struct {
    GtkWidget *lat, *lon, *height, *width, *mil_km;
    bool is_mil;
    GtkWidget *error;
  } extent;

#ifdef HAS_MAEMO_MAPPER
  struct {
    GtkWidget *fetch;
  } mmapper;
#endif

#ifdef ENABLE_OSM_GPS_MAP
  struct {
    GtkWidget *widget;
    bool needs_redraw;
    guint handler_id;
    OsmGpsMapPoint start;
  } map;
#endif
};

context_t::context_t(area_edit_t& a)
  : dialog(O2G_NULLPTR)
  , notebook(O2G_NULLPTR)
  , area(a)
  , min(a.min)
  , max(a.max)
  , warning(O2G_NULLPTR)
{
  memset(&direct, 0, sizeof(direct));
  memset(&extent, 0, sizeof(extent));
#ifdef HAS_MAEMO_MAPPER
  mmapper.fetch = O2G_NULLPTR;
#endif
#ifdef ENABLE_OSM_GPS_MAP
  memset(&map, 0, sizeof(map));
#endif
}

area_edit_t::area_edit_t(appdata_t &a, pos_t &mi, pos_t &ma, GtkWidget *dlg)
  : appdata(a)
  , parent(dlg)
  , min(mi)
  , max(ma)
{
}

static void parse_and_set_lat(GtkWidget *src, pos_float_t &store) {
  pos_float_t i = pos_parse_lat(gtk_entry_get_text(GTK_ENTRY(src)));
  if(pos_lat_valid(i))
    store = i;
}

static void parse_and_set_lon(GtkWidget *src, pos_float_t &store) {
  pos_float_t i = pos_parse_lon(gtk_entry_get_text(GTK_ENTRY(src)));
  if(pos_lon_valid(i))
    store = i;
}

/**
 * @brief calculate the selected area in square kilometers
 */
static double selected_area(const context_t *context) {
  pos_float_t center_lat = (context->max.lat + context->min.lat)/2;
  double vscale = DEG2RAD(POS_EQ_RADIUS / 1000.0);
  double hscale = DEG2RAD(cos(DEG2RAD(center_lat)) * POS_EQ_RADIUS / 1000.0);

  return vscale * (context->max.lat - context->min.lat) *
         hscale * (context->max.lon - context->min.lon);
}

static bool current_tab_is(context_t *context, gint page_num, const char *str) {
  GtkNotebook *nb = notebook_get_gtk_notebook(context->notebook);

  if(page_num < 0)
    page_num =
      gtk_notebook_get_current_page(nb);

  if(page_num < 0)
    return false;

  GtkWidget *w =
    gtk_notebook_get_nth_page(nb, page_num);
  const char *name =
    gtk_notebook_get_tab_label_text(nb, w);

  return(strcasecmp(name, _(str)) == 0);
}

static inline const char *warn_text() {
  return _("The currently selected area is %.02f km² (%.02f mi²) in size. "
           "This is more than the recommended %.02f km² (%.02f mi²).\n\n"
           "Continuing may result in a big or failing download and low "
           "mapping performance in a densly mapped area (e.g. cities)!");
}

static void on_area_warning_clicked(context_t *context) {
  double area = selected_area(context);

  warningf(context->dialog, warn_text(), area, area/(KMPMIL*KMPMIL),
           WARN_OVER, WARN_OVER/(KMPMIL*KMPMIL));
}

static bool area_warning(context_t *context) {
  bool ret = true;

  /* check if area size exceeds recommended values */
  double area = selected_area(context);

  if(area > WARN_OVER) {
    gchar *text = g_strdup_printf(warn_text(), area, area / (KMPMIL * KMPMIL),
                                  WARN_OVER, WARN_OVER/(KMPMIL*KMPMIL));
    ret = yes_no_f(context->dialog, context->area.appdata,
		   MISC_AGAIN_ID_AREA_TOO_BIG, MISC_AGAIN_FLAG_DONT_SAVE_NO,
		   _("Area size warning!"),
		   _("%s\n\nDo you really want to continue?"),
                   text);
    g_free(text);
  }

  return ret;
}

static void area_main_update(context_t *context) {
  /* also setup the local error messages here, so they are */
  /* updated for all entries at once */
  if(!context->min.valid() || !context->max.valid()) {
    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
				      GTK_RESPONSE_ACCEPT, FALSE);
  } else {
    if(context->min.lat >= context->max.lat ||
       context->min.lon >= context->max.lon) {
      gtk_label_set(GTK_LABEL(context->direct.error),
		  _("\"From\" must be smaller than \"to\" value!"));
      gtk_label_set(GTK_LABEL(context->extent.error),
		  _("Extents must be positive!"));
      gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
				      GTK_RESPONSE_ACCEPT, FALSE);
    }
    else
    {
      gtk_label_set(GTK_LABEL(context->direct.error), "");
      gtk_label_set(GTK_LABEL(context->extent.error), "");

      gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
				      GTK_RESPONSE_ACCEPT, TRUE);
    }
  }

  /* check if area size exceeds recommended values */
  double area = selected_area(context);

  if(area > WARN_OVER)
    gtk_widget_show(context->warning);
  else
    gtk_widget_hide(context->warning);
}

#ifdef ENABLE_OSM_GPS_MAP

static GSList *pos_append_rad(GSList *list, pos_float_t lat, pos_float_t lon) {
  OsmGpsMapPoint *coo = g_new0(OsmGpsMapPoint, 1);
  coo->rlat = lat;
  coo->rlon = lon;
  list = g_slist_append(list, coo);
  return list;
}

static GSList *pos_append(GSList *list, pos_float_t lat, pos_float_t lon) {
  return pos_append_rad(list, DEG2RAD(lat), DEG2RAD(lon));
}

struct add_bounds {
  OsmGpsMap * const map;
  explicit add_bounds(OsmGpsMap *m) : map(m) {}
  void operator()(const pos_bounds &b);
};

void add_bounds::operator()(const pos_bounds &b)
{
  GSList *box = pos_append(O2G_NULLPTR, b.min.lat, b.min.lon);
  box = pos_append(box, b.max.lat, b.min.lon);
  box = pos_append(box, b.max.lat, b.max.lon);
  box = pos_append(box, b.min.lat, b.max.lon);
  box = pos_append(box, b.min.lat, b.min.lon);

  osm_gps_map_add_bounds(map, box);
}

/* the contents of the map tab have been changed */
static void map_update(context_t *context, bool forced) {

  /* map is first tab (page 0) */
  if(!forced && !current_tab_is(context, -1, TAB_LABEL_MAP)) {
    printf("schedule map redraw\n");
    context->map.needs_redraw = true;
    return;
  }

  printf("do map redraw\n");

  /* check if the position is invalid */
  if(!context->min.valid() || !context->max.valid()) {
    /* no coordinates given: display around the current GPS position if available */
    pos_t pos = context->area.appdata.gps_state->get_pos();
    int zoom = 12;
    if(!pos.valid()) {
      /* no GPS position available: display the entire world */
      pos.lat = 0.0;
      pos.lon = 0.0;
      zoom = 1;
    }

    osm_gps_map_set_center_and_zoom(OSM_GPS_MAP(context->map.widget),
			      pos.lat, pos.lon, zoom);

    osm_gps_map_track_remove_all(OSM_GPS_MAP(context->map.widget));
  } else {

    pos_float_t center_lat = (context->max.lat + context->min.lat)/2;
    pos_float_t center_lon = (context->max.lon + context->min.lon)/2;

    /* we know the widgets pixel size, we know the required real size, */
    /* we want the zoom! */
    double vzoom = log2((45.0 * context->map.widget->allocation.height)/
			((context->max.lat - context->min.lat)*32.0)) -1;

    double hzoom = log2((45.0 * context->map.widget->allocation.width)/
			((context->max.lon - context->min.lon)*32.0)) -1;

    osm_gps_map_set_center(OSM_GPS_MAP(context->map.widget),
			   center_lat, center_lon);

    /* use smallest zoom, so everything fits on screen */
    osm_gps_map_set_zoom(OSM_GPS_MAP(context->map.widget),
			 (vzoom < hzoom)?vzoom:hzoom);

    /* ---------- draw border (as a gps track) -------------- */
    osm_gps_map_track_remove_all(OSM_GPS_MAP(context->map.widget));

    if(context->max.lat > context->min.lat &&
       context->max.lon > context->min.lon) {
      GSList *box = pos_append(O2G_NULLPTR, context->min.lat, context->min.lon);
      box = pos_append(box, context->max.lat, context->min.lon);
      box = pos_append(box, context->max.lat, context->max.lon);
      box = pos_append(box, context->min.lat, context->max.lon);
      box = pos_append(box, context->min.lat, context->min.lon);

      osm_gps_map_add_track(OSM_GPS_MAP(context->map.widget), box);
    }
  }

  // show all other bounds
  std::for_each(context->area.other_bounds.begin(), context->area.other_bounds.end(),
                add_bounds(OSM_GPS_MAP(context->map.widget)));

  context->map.needs_redraw = false;
}

static gboolean on_map_configure(GtkWidget *, GdkEventConfigure *,
				 context_t *context) {
  map_update(context, false);
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

static void callback_modified_direct(context_t *context) {
  /* direct is second tab (page 1) */
  if(!current_tab_is(context, -1, TAB_LABEL_DIRECT))
    return;

  /* parse the fields from the direct entry pad */
  parse_and_set_lat(context->direct.minlat, context->min.lat);
  parse_and_set_lon(context->direct.minlon, context->min.lon);
  parse_and_set_lat(context->direct.maxlat, context->max.lat);
  parse_and_set_lon(context->direct.maxlon, context->max.lon);

  area_main_update(context);

  /* also adjust other views */
  extent_update(context);
#ifdef ENABLE_OSM_GPS_MAP
  map_update(context, false);
#endif
}

static void callback_modified_extent(context_t *context) {
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
  map_update(context, false);
#endif
}

static void callback_modified_unit(context_t *context) {
  /* get current values */
  double height = pos_dist_get(context->extent.height, context->extent.is_mil);
  double width  = pos_dist_get(context->extent.width, context->extent.is_mil);

  /* adjust unit flag */
  context->extent.is_mil =
    combo_box_get_active(context->extent.mil_km) == 0;

  /* save values */
  pos_dist_entry_set(context->extent.width, width, context->extent.is_mil);
  pos_dist_entry_set(context->extent.height, height, context->extent.is_mil);
}

#ifdef HAS_MAEMO_MAPPER
static void callback_fetch_mm_clicked(context_t *context) {
  dbus_mm_pos_t mmpos;
  if(!dbus_mm_set_position(context->area.appdata.osso_context, &mmpos)) {
    errorf(context->dialog, _("Unable to communicate with Maemo Mapper. "
              "You need to have Maemo Mapper installed to use this feature."));
    return;
  }

  if(!mmpos.valid) {
    errorf(context->dialog, _("No valid position received yet. You need to "
           "scroll or zoom the Maemo Mapper view in order to force it to send "
           "its current view position to osm2go."));
    return;
  }

  /* maemo mapper is fourth tab (page 3) */
  if(!current_tab_is(context, -1, TAB_LABEL_MM))
    return;

  /* maemo mapper pos data ... */
  pos_float_t center_lat = mmpos.pos.lat;
  pos_float_t center_lon = mmpos.pos.lon;
  int zoom = mmpos.zoom;

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
  map_update(context, false);
#endif
}
#endif

#ifdef ENABLE_OSM_GPS_MAP

static gboolean
on_map_button_press_event(GtkWidget *widget,
			  GdkEventButton *event, context_t *context) {
  OsmGpsMap *map = OSM_GPS_MAP(context->map.widget);
  osm_gps_map_osd_t *osd = osm_gps_map_osd_get(map);

  /* osm-gps-map needs this event to handle the OSD */
  if(osd->check(osd, TRUE, event->x, event->y) != OSD_NONE)
    return FALSE;

  if(osm_gps_map_osd_get_state(OSM_GPS_MAP(widget)))
    return FALSE;

  /* remove existing marker */
  osm_gps_map_track_remove_all(map);

  /* and remember this location as the start */
  context->map.start = osm_gps_map_convert_screen_to_geographic(map, event->x, event->y);

  return TRUE;
}

static gboolean
on_map_motion_notify_event(GtkWidget *widget,
			   GdkEventMotion  *event, context_t *context) {
  if(!std::isnan(context->map.start.rlon) &&
     !std::isnan(context->map.start.rlat)) {
    OsmGpsMap *map = OSM_GPS_MAP(context->map.widget);

    /* remove existing marker */
    osm_gps_map_track_remove_all(map);

    OsmGpsMapPoint start = context->map.start;
    OsmGpsMapPoint end = osm_gps_map_convert_screen_to_geographic(map, event->x, event->y);

    GSList *box = pos_append_rad(O2G_NULLPTR, start.rlat, start.rlon);
    box = pos_append_rad(box, end.rlat,   start.rlon);
    box = pos_append_rad(box, end.rlat,   end.rlon);
    box = pos_append_rad(box, start.rlat, end.rlon);
    box = pos_append_rad(box, start.rlat, start.rlon);

    osm_gps_map_add_track(map, box);
  }

  /* returning true here disables dragging in osm-gps-map */
  return !osm_gps_map_osd_get_state(OSM_GPS_MAP(widget));
}

static gboolean
on_map_button_release_event(GtkWidget *widget,
			    GdkEventButton *event, context_t *context) {

  OsmGpsMap *map = OSM_GPS_MAP(context->map.widget);
  osm_gps_map_osd_t *osd = osm_gps_map_osd_get(map);

  if(!std::isnan(context->map.start.rlon) &&
     !std::isnan(context->map.start.rlat)) {

    OsmGpsMapPoint start = context->map.start;
    OsmGpsMapPoint end = osm_gps_map_convert_screen_to_geographic(map, event->x, event->y);

    GSList *box = pos_append_rad(O2G_NULLPTR, start.rlat, start.rlon);
    box = pos_append_rad(box, end.rlat,   start.rlon);
    box = pos_append_rad(box, end.rlat,   end.rlon);
    box = pos_append_rad(box, start.rlat, end.rlon);
    box = pos_append_rad(box, start.rlat, start.rlon);

    osm_gps_map_add_track(map, box);

    if(start.rlat < end.rlat) {
      context->min.lat = RAD2DEG(start.rlat);
      context->max.lat = RAD2DEG(end.rlat);
    } else {
      context->min.lat = RAD2DEG(end.rlat);
      context->max.lat = RAD2DEG(start.rlat);
    }

    if(start.rlon < end.rlon) {
      context->min.lon = RAD2DEG(start.rlon);
      context->max.lon = RAD2DEG(end.rlon);
    } else {
      context->min.lon = RAD2DEG(end.rlon);
      context->max.lon = RAD2DEG(start.rlon);
    }

    area_main_update(context);
    direct_update(context);
    extent_update(context);

    context->map.start.rlon = context->map.start.rlat = NAN;
  }

  /* osm-gps-map needs this event to handle the OSD */
  if(osd->check(osd, TRUE, event->x, event->y) != OSD_NONE)
    return FALSE;

  /* returning true here disables dragging in osm-gps-map */
  return !osm_gps_map_osd_get_state(OSM_GPS_MAP(widget));
}

static void on_page_switch(GtkNotebook *, GtkNotebookPage *,
			   guint page_num, context_t *context) {

  /* updating the map while the user manually changes some coordinates */
  /* may confuse the map. so we delay those updates until the map tab */
  /* is becoming visible */
  if(current_tab_is(context, page_num, TAB_LABEL_MAP) &&
     context->map.needs_redraw)
    map_update(context, true);
}

static gboolean map_gps_update(gpointer data) {
  context_t *context = static_cast<context_t *>(data);

  pos_t pos(NAN, NAN);
  if(context->area.appdata.settings->enable_gps)
    pos = context->area.appdata.gps_state->get_pos();

  if(pos.valid()) {
    g_object_set(context->map.widget, "gps-track-highlight-radius", 0, O2G_NULLPTR);
    osm_gps_map_gps_add(OSM_GPS_MAP(context->map.widget),
			 pos.lat, pos.lon, NAN);
  } else
    osm_gps_map_gps_clear(OSM_GPS_MAP(context->map.widget));

  return TRUE;
}

#endif

bool area_edit_t::run() {
  GtkWidget *vbox;
  GdkColor color;
  gdk_color_parse("red", &color);

  context_t context(*this);

  context.dialog =
    misc_dialog_new(MISC_DIALOG_HIGH, _("Area editor"),
	  GTK_WINDOW(parent),
	  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
          O2G_NULLPTR);

  context.warning =
    gtk_dialog_add_button(GTK_DIALOG(context.dialog), _("Warning"),
			  GTK_RESPONSE_HELP);

  gtk_button_set_image(GTK_BUTTON(context.warning),
		       gtk_image_new_from_stock(GTK_STOCK_DIALOG_WARNING,
						GTK_ICON_SIZE_BUTTON));
  g_signal_connect_swapped(context.warning, "clicked",
                           G_CALLBACK(on_area_warning_clicked), &context);

  context.notebook = notebook_new();

#ifdef ENABLE_OSM_GPS_MAP
  /* ------------- fetch from map ------------------------ */

  context.map.needs_redraw = false;
  context.map.widget = GTK_WIDGET(g_object_new(OSM_TYPE_GPS_MAP,
 	        "map-source", OSM_GPS_MAP_SOURCE_OPENSTREETMAP,
		"proxy-uri", g_getenv("http_proxy"),
		"auto-center", FALSE,
	        "tile-cache", O2G_NULLPTR,
		 O2G_NULLPTR));

  osm_gps_map_osd_select_init(OSM_GPS_MAP(context.map.widget));

  g_signal_connect(G_OBJECT(context.map.widget), "configure-event",
		   G_CALLBACK(on_map_configure), &context);
  g_signal_connect(G_OBJECT(context.map.widget), "button-press-event",
		   G_CALLBACK(on_map_button_press_event), &context);
  g_signal_connect(G_OBJECT(context.map.widget), "motion-notify-event",
		   G_CALLBACK(on_map_motion_notify_event), &context);
  g_signal_connect(G_OBJECT(context.map.widget), "button-release-event",
		   G_CALLBACK(on_map_button_release_event), &context);

  /* install handler for timed updates of the gps button */
  context.map.handler_id = g_timeout_add_seconds(1, map_gps_update, &context);
  context.map.start.rlon = context.map.start.rlat = NAN;

  notebook_append_page(context.notebook, context.map.widget, _(TAB_LABEL_MAP));
#endif

  /* ------------ direct min/max edit --------------- */

  vbox = gtk_vbox_new(FALSE, 10);

  GtkWidget *table = gtk_table_new(3, 4, FALSE);  // x, y
  gtk_table_set_col_spacings(GTK_TABLE(table), 10);
  gtk_table_set_row_spacings(GTK_TABLE(table), 5);

  context.direct.minlat = pos_lat_entry_new(0.0);
  misc_table_attach(table, context.direct.minlat, 0, 0);
  GtkWidget *label = gtk_label_new(_("to"));
  misc_table_attach(table,  label, 1, 0);
  context.direct.maxlat = pos_lat_entry_new(0.0);
  misc_table_attach(table, context.direct.maxlat, 2, 0);

  context.direct.minlon = pos_lon_entry_new(min.lon);
  misc_table_attach(table, context.direct.minlon, 0, 1);
  label = gtk_label_new(_("to"));
  misc_table_attach(table,  label, 1, 1);
  context.direct.maxlon = pos_lon_entry_new(0.0);
  misc_table_attach(table, context.direct.maxlon, 2, 1);

  /* setup this page */
  direct_update(&context);

  g_signal_connect_swapped(G_OBJECT(context.direct.minlat), "changed",
                           G_CALLBACK(callback_modified_direct), &context);
  g_signal_connect_swapped(G_OBJECT(context.direct.minlon), "changed",
                           G_CALLBACK(callback_modified_direct), &context);
  g_signal_connect_swapped(G_OBJECT(context.direct.maxlat), "changed",
                           G_CALLBACK(callback_modified_direct), &context);
  g_signal_connect_swapped(G_OBJECT(context.direct.maxlon), "changed",
                           G_CALLBACK(callback_modified_direct), &context);

  /* --- hint --- */
  label = gtk_label_new(_("(recommended min/max diff <0.03 degrees)"));
  gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 3, 2, 3);

  /* error label */
  context.direct.error = gtk_label_new(O2G_NULLPTR);
  gtk_widget_modify_fg(context.direct.error, GTK_STATE_NORMAL, &color);
  gtk_table_attach_defaults(GTK_TABLE(table), context.direct.error, 0, 3, 3, 4);

  gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
  notebook_append_page(context.notebook, vbox, _(TAB_LABEL_DIRECT));

  /* ------------- center/extent edit ------------------------ */

  vbox = gtk_vbox_new(FALSE, 10);
  table = gtk_table_new(3, 5, FALSE);  // x, y
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
  context.extent.width = entry_new();
  gtk_table_attach_defaults(GTK_TABLE(table), context.extent.width, 1, 2, 1, 2);

  label = gtk_label_new(_("Height:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 2, 3);
  context.extent.height = entry_new();
  gtk_table_attach_defaults(GTK_TABLE(table),
			    context.extent.height, 1, 2, 2, 3);

  context.extent.mil_km = combo_box_new(_("Unit"));
  combo_box_append_text(context.extent.mil_km, _("mi"));
  combo_box_append_text(context.extent.mil_km, _("km"));
  combo_box_set_active(context.extent.mil_km, 1); // km

  gtk_table_attach(GTK_TABLE(table), context.extent.mil_km, 2, 3, 1, 3,
		   static_cast<GtkAttachOptions>(0), static_cast<GtkAttachOptions>(0),
		   0, 0);

  /* setup this page */
  extent_update(&context);

  /* connect signals after inital update to avoid confusion */
  g_signal_connect_swapped(G_OBJECT(context.extent.lat), "changed",
                           G_CALLBACK(callback_modified_extent), &context);
  g_signal_connect_swapped(G_OBJECT(context.extent.lon), "changed",
                           G_CALLBACK(callback_modified_extent), &context);
  g_signal_connect_swapped(G_OBJECT(context.extent.width), "changed",
                           G_CALLBACK(callback_modified_extent), &context);
  g_signal_connect_swapped(G_OBJECT(context.extent.height), "changed",
                           G_CALLBACK(callback_modified_extent), &context);
  g_signal_connect_swapped(G_OBJECT(context.extent.mil_km), "changed",
                           G_CALLBACK(callback_modified_unit), &context);

  /* --- hint --- */
  label = gtk_label_new(_("(recommended width/height < 2km/1.25mi)"));
  gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 3, 3, 4);

  /* error label */
  context.extent.error = gtk_label_new(O2G_NULLPTR);
  gtk_widget_modify_fg(context.extent.error, GTK_STATE_NORMAL, &color);
  gtk_table_attach_defaults(GTK_TABLE(table), context.extent.error, 0, 3, 4, 5);

  gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
  notebook_append_page(context.notebook, vbox, _(TAB_LABEL_EXTENT));

#ifdef HAS_MAEMO_MAPPER
  /* ------------- fetch from maemo mapper ------------------------ */

  vbox = gtk_vbox_new(FALSE, 8);
  context.mmapper.fetch =
    button_new_with_label(_("Get from Maemo Mapper"));
  gtk_box_pack_start(GTK_BOX(vbox), context.mmapper.fetch, FALSE, FALSE, 0);

  g_signal_connect_swapped(G_OBJECT(context.mmapper.fetch), "clicked",
                           G_CALLBACK(callback_fetch_mm_clicked), &context);

  /* --- hint --- */
  label = gtk_label_new(_("(recommended MM zoom level < 7)"));
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

  notebook_append_page(context.notebook, vbox, _(TAB_LABEL_MM));
#endif

  /* ------------------------------------------------------ */

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
			      context.notebook);

#ifdef ENABLE_OSM_GPS_MAP
  g_signal_connect(G_OBJECT(notebook_get_gtk_notebook(context.notebook)),
		   "switch-page", G_CALLBACK(on_page_switch), &context);
#endif

  gtk_widget_show_all(context.dialog);

  area_main_update(&context);

  bool leave = false, ok = false;
  do {
    int response = gtk_dialog_run(GTK_DIALOG(context.dialog));

    if(GTK_RESPONSE_ACCEPT == response) {
      if(area_warning(&context)) {
	leave = true;
	ok = true;
      }
    } else if(response != GTK_RESPONSE_HELP)
      leave = true;
  } while(!leave);

  if(ok) {
    /* copy modified values back to given storage */
    min = context.min;
    max = context.max;
  }

#ifdef ENABLE_OSM_GPS_MAP
  g_source_remove(context.map.handler_id);
#endif

  gtk_widget_destroy(context.dialog);

  return ok;
}
