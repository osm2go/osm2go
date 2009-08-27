/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * Copyright (C) Till Harbaum 2009 <till@harbaum.org>
 *
 * osm-gps-map is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * osm-gps-map is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h" 
#include <stdlib.h>  // abs
#include <math.h>    // M_PI

/* parameters that can be overwritten from the config file: */
/* OSD_DIAMETER */
/* OSD_X, OSD_Y */

#ifndef USE_CAIRO
#error "OSD control display lacks a non-cairo implementation!"
#endif

#include <cairo.h>

#include "osm-gps-map.h"
#include "osm-gps-map-osd-classic.h"

//the osd controls
typedef struct {
    /* the offscreen representation of the OSD */
    cairo_surface_t *overlay;
} osd_priv_t;

/* position and extent of bounding box */
#ifndef OSD_X
#define OSD_X      (10)
#endif

#ifndef OSD_Y
#define OSD_Y      (10)
#endif

/* parameters of the direction shape */
#ifndef OSD_DIAMETER
#define D_RAD  (30)         // diameter of dpad
#else
#define D_RAD  (OSD_DIAMETER)
#endif
#define D_TIP  (4*D_RAD/5)  // distance of arrow tip from dpad center
#define D_LEN  (D_RAD/4)    // length of arrow
#define D_WID  (D_LEN)      // width of arrow

/* parameters of the "zoom" pad */
#define Z_STEP   (D_RAD/4)  // distance between dpad and zoom
#define Z_RAD    (D_RAD/2)  // radius of "caps" of zoom bar

#ifdef OSD_SHADOW_ENABLE
/* shadow also depends on control size */
#define OSD_SHADOW (D_RAD/6)
#else
#define OSD_SHADOW (0)
#endif

/* normally the GPS button is in the center of the dpad. if there's */
/* no dpad it will go into the zoom area */
#if defined(OSD_GPS_BUTTON) && defined(OSD_NO_DPAD)
#define Z_GPS  1
#else
#define Z_GPS  0
#endif

/* total width and height of controls incl. shadow */
#define OSD_W    (2*D_RAD + OSD_SHADOW + Z_GPS * 2 * Z_RAD)
#if !Z_GPS
#define OSD_H    (2*D_RAD + Z_STEP + 2*Z_RAD + OSD_SHADOW)
#else
#define OSD_H    (2*Z_RAD + OSD_SHADOW)
#endif

#ifdef OSD_SHADOW_ENABLE
#define OSD_LBL_SHADOW (OSD_SHADOW/2)
#endif

#define Z_TOP    ((1-Z_GPS) * (2 * D_RAD + Z_STEP))

#define Z_MID    (Z_TOP + Z_RAD)
#define Z_BOT    (Z_MID + Z_RAD)
#define Z_LEFT   (Z_RAD)
#define Z_RIGHT  (2 * D_RAD - Z_RAD + Z_GPS * 2 * Z_RAD)
#define Z_CENTER ((Z_RIGHT + Z_LEFT)/2)

/* create the cairo shape used for the zoom buttons */
static void 
osm_gps_map_osd_zoom_shape(cairo_t *cr, gint x, gint y) 
{
    cairo_move_to (cr, x+Z_LEFT,    y+Z_TOP);
    cairo_line_to (cr, x+Z_RIGHT,   y+Z_TOP);
    cairo_arc     (cr, x+Z_RIGHT,   y+Z_MID, Z_RAD, -M_PI/2,  M_PI/2);
    cairo_line_to (cr, x+Z_LEFT,    y+Z_BOT);
    cairo_arc     (cr, x+Z_LEFT,    y+Z_MID, Z_RAD,  M_PI/2, -M_PI/2);
}

#ifndef OSD_NO_DPAD
/* create the cairo shape used for the dpad */
static void 
osm_gps_map_osd_dpad_shape(cairo_t *cr, gint x, gint y) 
{
    cairo_arc (cr, x+D_RAD, y+D_RAD, D_RAD, 0, 2 * M_PI);
}
#endif

static gboolean
osm_gps_map_in_circle(gint x, gint y, gint cx, gint cy, gint rad) 
{
    return( pow(cx - x, 2) + pow(cy - y, 2) < rad * rad);
}

#ifndef OSD_NO_DPAD
/* check whether x/y is within the dpad */
static osd_button_t
osm_gps_map_osd_check_dpad(gint x, gint y) 
{
    /* within entire dpad circle */
    if( osm_gps_map_in_circle(x, y, D_RAD, D_RAD, D_RAD)) 
    {
        /* convert into position relative to dpads centre */
        x -= D_RAD;
        y -= D_RAD;

#ifdef OSD_GPS_BUTTON
        /* check for dpad center goes here! */
        if( osm_gps_map_in_circle(x, y, 0, 0, D_RAD/3)) 
            return OSD_GPS;
#endif

        if( y < 0 && abs(x) < abs(y))
            return OSD_UP;

        if( y > 0 && abs(x) < abs(y))
            return OSD_DOWN;

        if( x < 0 && abs(y) < abs(x))
            return OSD_LEFT;

        if( x > 0 && abs(y) < abs(x))
            return OSD_RIGHT;

        return OSD_BG;
    }
    return OSD_NONE;
}
#endif

/* check whether x/y is within the zoom pads */
static osd_button_t
osm_gps_map_osd_check_zoom(gint x, gint y) {
    if( x > 0 && x < OSD_W && y > Z_TOP && y < Z_BOT) {

        /* within circle around (-) label */
        if( osm_gps_map_in_circle(x, y, Z_LEFT, Z_MID, Z_RAD)) 
            return OSD_OUT;

        /* within circle around (+) label */
        if( osm_gps_map_in_circle(x, y, Z_RIGHT, Z_MID, Z_RAD)) 
            return OSD_IN;

#if Z_GPS == 1
        /* within square around center */
        if( x > Z_CENTER - Z_RAD && x < Z_CENTER + Z_RAD)
            return OSD_GPS;
#endif

        /* between center of (-) button and center of entire zoom control area */
        if(x > OSD_LEFT && x < D_RAD) 
            return OSD_OUT;

        /* between center of (+) button and center of entire zoom control area */
        if(x < OSD_RIGHT && x > D_RAD) 
            return OSD_IN;
    }
 
    return OSD_NONE;
}

static osd_button_t
osm_gps_map_osd_check(osm_gps_map_osd_t *osd, gint x, gint y) {
    osd_button_t but = OSD_NONE;

    x -= OSD_X;
    y -= OSD_Y;

    if(OSD_X < 0)
        x -= (osd->widget->allocation.width - OSD_W);

    if(OSD_Y < 0)
        y -= (osd->widget->allocation.height - OSD_H);

    /* first do a rough test for the OSD area. */
    /* this is just to avoid an unnecessary detailed test */
    if(x > 0 && x < OSD_W && y > 0 && y < OSD_H) {
#ifndef OSD_NO_DPAD
        but = osm_gps_map_osd_check_dpad(x, y);
#endif

        if(but == OSD_NONE) 
            but = osm_gps_map_osd_check_zoom(x, y);
    }

    return but;
}

#ifdef OSD_SHADOW_ENABLE
static void 
osm_gps_map_osd_shape_shadow(cairo_t *cr) {
    cairo_set_source_rgba (cr, 0, 0, 0, 0.2);
    cairo_fill (cr);
    cairo_stroke (cr);
}
#endif

#ifndef OSD_COLOR
/* if no color has been specified we just use the gdks default colors */
static void 
osm_gps_map_osd_shape(cairo_t *cr, GdkColor *bg, GdkColor *fg) {
    gdk_cairo_set_source_color(cr, bg);
    cairo_fill_preserve (cr);
    gdk_cairo_set_source_color(cr, fg);
    cairo_set_line_width (cr, 1);
    cairo_stroke (cr);
}
#else
static void 
osm_gps_map_osd_shape(cairo_t *cr) {
    cairo_set_source_rgb (cr, OSD_COLOR_BG);
    cairo_fill_preserve (cr);
    cairo_set_source_rgb (cr, OSD_COLOR);
    cairo_set_line_width (cr, 1);
    cairo_stroke (cr);
}
#endif

#ifndef OSD_NO_DPAD
static void
osm_gps_map_osd_dpad_labels(cairo_t *cr, gint x, gint y) {
    /* move reference to dpad center */
    x += D_RAD;
    y += D_RAD;

    const static gint offset[][3][2] = {
        /* left arrow/triangle */
        { { -D_TIP+D_LEN, -D_WID }, { -D_LEN, D_WID }, { +D_LEN, D_WID } },
        /* right arrow/triangle */
        { { +D_TIP-D_LEN, -D_WID }, { +D_LEN, D_WID }, { -D_LEN, D_WID } },
        /* top arrow/triangle */
        { { -D_WID, -D_TIP+D_LEN }, { D_WID, -D_LEN }, { D_WID, +D_LEN } },
        /* bottom arrow/triangle */
        { { -D_WID, +D_TIP-D_LEN }, { D_WID, +D_LEN }, { D_WID, -D_LEN } }
    };

    int i;
    for(i=0;i<4;i++) {
        cairo_move_to (cr, x + offset[i][0][0], y + offset[i][0][1]);
        cairo_rel_line_to (cr, offset[i][1][0], offset[i][1][1]);
        cairo_rel_line_to (cr, offset[i][2][0], offset[i][2][1]);
    }
}
#endif

#ifdef OSD_GPS_BUTTON
/* draw the satellite dish icon in the center of the dpad */
#define GPS_V0  (D_RAD/7)
#define GPS_V1  (D_RAD/10)
#define GPS_V2  (D_RAD/5)

/* draw a satellite receiver dish */
/* this is either drawn in the center of the dpad (if present) */
/* or in the middle of the zoom area */
static void
osm_gps_map_osd_dpad_gps(cairo_t *cr, gint x, gint y) {
    /* move reference to dpad center */
    x += (1-Z_GPS) * D_RAD + Z_GPS * Z_RAD * 3;
    y += (1-Z_GPS) * D_RAD + Z_GPS * Z_RAD + GPS_V0;

    cairo_move_to (cr, x-GPS_V0, y+GPS_V0);
    cairo_rel_line_to (cr, +GPS_V0, -GPS_V0);
    cairo_rel_line_to (cr, +GPS_V0, +GPS_V0);
    cairo_close_path (cr);

    cairo_move_to (cr, x+GPS_V1-GPS_V2, y-2*GPS_V2);
    cairo_curve_to (cr, x-GPS_V2, y, x+GPS_V1, y+GPS_V1, x+GPS_V1+GPS_V2, y);
    cairo_close_path (cr);

    x += GPS_V1;
    cairo_move_to (cr, x, y-GPS_V2);
    cairo_rel_line_to (cr, +GPS_V1, -GPS_V1);
}
#endif

#define Z_LEN  (2*Z_RAD/3)

static void
osm_gps_map_osd_zoom_labels(cairo_t *cr, gint x, gint y) {
    cairo_move_to (cr, x + Z_LEFT  - Z_LEN, y + Z_MID);
    cairo_line_to (cr, x + Z_LEFT  + Z_LEN, y + Z_MID);

    cairo_move_to (cr, x + Z_RIGHT,         y + Z_MID - Z_LEN);
    cairo_line_to (cr, x + Z_RIGHT,         y + Z_MID + Z_LEN);
    cairo_move_to (cr, x + Z_RIGHT - Z_LEN, y + Z_MID);
    cairo_line_to (cr, x + Z_RIGHT + Z_LEN, y + Z_MID);
}

#ifndef OSD_COLOR
/* if no color has been specified we just use the gdks default colors */
static void
osm_gps_map_osd_labels(cairo_t *cr, gint width, gboolean enabled,
                       GdkColor *fg, GdkColor *disabled) {
    if(enabled)  gdk_cairo_set_source_color(cr, fg);
    else         gdk_cairo_set_source_color(cr, disabled);
    cairo_set_line_width (cr, width);
    cairo_stroke (cr);
}
#else
static void
osm_gps_map_osd_labels(cairo_t *cr, gint width, gboolean enabled) {
    if(enabled)  cairo_set_source_rgb (cr, OSD_COLOR);
    else         cairo_set_source_rgb (cr, OSD_COLOR_DISABLED);
    cairo_set_line_width (cr, width);
    cairo_stroke (cr);
}
#endif

#ifdef OSD_SHADOW_ENABLE
static void 
osm_gps_map_osd_labels_shadow(cairo_t *cr, gint width, gboolean enabled) {
    cairo_set_source_rgba (cr, 0, 0, 0, enabled?0.3:0.15);
    cairo_set_line_width (cr, width);
    cairo_stroke (cr);
}
#endif

static void
osm_gps_map_osd_render(osm_gps_map_osd_t *osd) {
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    /* first fill with transparency */
    cairo_t *cr = cairo_create(priv->overlay);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);

#ifndef OSD_COLOR
    GdkColor bg = GTK_WIDGET(osd->widget)->style->bg[GTK_STATE_NORMAL];
    GdkColor fg = GTK_WIDGET(osd->widget)->style->fg[GTK_STATE_NORMAL];
    GdkColor da = GTK_WIDGET(osd->widget)->style->fg[GTK_STATE_INSENSITIVE];
#endif

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    /* --------- draw zoom and dpad shape shadow ----------- */
    gint x = 0, y = 0;

#ifdef OSD_SHADOW_ENABLE
    osm_gps_map_osd_zoom_shape(cr, x + OSD_SHADOW, y + OSD_SHADOW);
    osm_gps_map_osd_shape_shadow(cr);
#ifndef OSD_NO_DPAD
    osm_gps_map_osd_dpad_shape(cr, x + OSD_SHADOW, y + OSD_SHADOW);
    osm_gps_map_osd_shape_shadow(cr);
#endif
#endif

    /* --------- draw zoom and dpad shape ----------- */

    osm_gps_map_osd_zoom_shape(cr, x, y);
#ifndef OSD_COLOR
    osm_gps_map_osd_shape(cr, &bg, &fg);
#else
    osm_gps_map_osd_shape(cr);
#endif
#ifndef OSD_NO_DPAD
    osm_gps_map_osd_dpad_shape(cr, x, y);
#ifndef OSD_COLOR
    osm_gps_map_osd_shape(cr, &bg, &fg);
#else
    osm_gps_map_osd_shape(cr);
#endif
#endif

    /* --------- draw zoom and dpad labels --------- */

#ifdef OSD_SHADOW_ENABLE
    osm_gps_map_osd_zoom_labels(cr, x + OSD_LBL_SHADOW, y + OSD_LBL_SHADOW);
#ifndef OSD_NO_DPAD
    osm_gps_map_osd_dpad_labels(cr, x + OSD_LBL_SHADOW, y + OSD_LBL_SHADOW);
#endif
    osm_gps_map_osd_labels_shadow(cr, Z_RAD/3, TRUE);
#ifdef OSD_GPS_BUTTON
    osm_gps_map_osd_dpad_gps(cr, x + OSD_LBL_SHADOW, y + OSD_LBL_SHADOW); 
    osm_gps_map_osd_labels_shadow(cr, Z_RAD/6, osd->cb != NULL);
#endif
#endif

    osm_gps_map_osd_zoom_labels(cr, x, y);
#ifndef OSD_NO_DPAD
    osm_gps_map_osd_dpad_labels(cr, x, y);
#endif
#ifndef OSD_COLOR
    osm_gps_map_osd_labels(cr, Z_RAD/3, TRUE, &fg, &da);
#else
    osm_gps_map_osd_labels(cr, Z_RAD/3, TRUE);
#endif
#ifdef OSD_GPS_BUTTON
    osm_gps_map_osd_dpad_gps(cr, x, y);
#ifndef OSD_COLOR
    osm_gps_map_osd_labels(cr, Z_RAD/6, osd->cb != NULL, &fg, &da);
#else
    osm_gps_map_osd_labels(cr, Z_RAD/6, osd->cb != NULL);
#endif
#endif
    
    cairo_destroy(cr);
}

static void
osm_gps_map_osd_draw(osm_gps_map_osd_t *osd, GdkDrawable *drawable)
{
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    /* OSD itself uses some off-screen rendering, so check if the */
    /* offscreen buffer is present and create it if not */
    if(!priv->overlay) {
        /* create overlay ... */
        priv->overlay = 
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, OSD_W, OSD_H);
        /* ... and render it */
        osm_gps_map_osd_render(osd);
    }

    // now draw this onto the original context 
    cairo_t *cr = gdk_cairo_create(drawable);

    int x = OSD_X, y = OSD_Y;
    if(OSD_X < 0)
        x = osd->widget->allocation.width - OSD_W + OSD_X;

    if(OSD_Y < 0)
        y = osd->widget->allocation.height - OSD_H + OSD_Y;

    cairo_set_source_surface(cr, priv->overlay, x, y);
    cairo_paint(cr);
    cairo_destroy(cr);
}

static void
osm_gps_map_osd_free(osm_gps_map_osd_t *osd) 
{
    osd_priv_t *priv = (osd_priv_t *)(osd->priv);

    if (priv->overlay)
         cairo_surface_destroy(priv->overlay);

    g_free(priv);
}

static osm_gps_map_osd_t osd_classic = {
    .draw       = osm_gps_map_osd_draw,
    .check      = osm_gps_map_osd_check,
    .render     = osm_gps_map_osd_render,
    .free       = osm_gps_map_osd_free,

    .cb         = NULL,
    .data       = NULL,

    .priv       = NULL
};

/* this is the only function that's externally visible */
void
osm_gps_map_osd_classic_init(OsmGpsMap *map) 
{
    osd_priv_t *priv = osd_classic.priv = g_new0(osd_priv_t, 1);

    osd_classic.priv = priv;

    osm_gps_map_register_osd(map, &osd_classic);
}

#ifdef OSD_GPS_BUTTON
/* below are osd specific functions which aren't used by osm-gps-map */
/* but instead are to be used by the main application */
void osm_gps_map_osd_enable_gps (OsmGpsMap *map, OsmGpsMapOsdCallback cb, 
                                 gpointer data) {
    osm_gps_map_osd_t *osd = osm_gps_map_osd_get(map);

    g_return_if_fail (osd);

    osd->cb = cb;
    osd->data = data;

    /* this may have changed the state of the gps button */
    /* we thus re-render the overlay */
    osd->render(osd);

    osm_gps_map_redraw(map);
}
#endif
