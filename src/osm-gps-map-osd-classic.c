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
#include <math.h>    // M_PI/cos()

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

#ifdef OSD_SOURCE_SEL
    /* values to handle the "source" menu */
    cairo_surface_t *map_source;
    gboolean expanded;
    gint shift, dir, count;
    gint handler_id;
    gint width, height;
#endif

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
osd_zoom_shape(cairo_t *cr, gint x, gint y) 
{
    cairo_move_to (cr, x+Z_LEFT,    y+Z_TOP);
    cairo_line_to (cr, x+Z_RIGHT,   y+Z_TOP);
    cairo_arc     (cr, x+Z_RIGHT,   y+Z_MID, Z_RAD, -M_PI/2,  M_PI/2);
    cairo_line_to (cr, x+Z_LEFT,    y+Z_BOT);
    cairo_arc     (cr, x+Z_LEFT,    y+Z_MID, Z_RAD,  M_PI/2, -M_PI/2);
}

/* ------------------- color/shadow functions ----------------- */

#ifndef OSD_COLOR
/* if no color has been specified we just use the gdks default colors */
static void
osd_labels(cairo_t *cr, gint width, gboolean enabled,
                       GdkColor *fg, GdkColor *disabled) {
    if(enabled)  gdk_cairo_set_source_color(cr, fg);
    else         gdk_cairo_set_source_color(cr, disabled);
    cairo_set_line_width (cr, width);
}
#else
static void
osd_labels(cairo_t *cr, gint width, gboolean enabled) {
    if(enabled)  cairo_set_source_rgb (cr, OSD_COLOR);
    else         cairo_set_source_rgb (cr, OSD_COLOR_DISABLED);
    cairo_set_line_width (cr, width);
}
#endif

#ifdef OSD_SHADOW_ENABLE
static void 
osd_labels_shadow(cairo_t *cr, gint width, gboolean enabled) {
    cairo_set_source_rgba (cr, 0, 0, 0, enabled?0.3:0.15);
    cairo_set_line_width (cr, width);
}
#endif

#ifndef OSD_NO_DPAD
/* create the cairo shape used for the dpad */
static void 
osd_dpad_shape(cairo_t *cr, gint x, gint y) 
{
    cairo_arc (cr, x+D_RAD, y+D_RAD, D_RAD, 0, 2 * M_PI);
}
#endif

#ifdef OSD_SHADOW_ENABLE
static void 
osd_shape_shadow(cairo_t *cr) {
    cairo_set_source_rgba (cr, 0, 0, 0, 0.2);
    cairo_fill (cr);
    cairo_stroke (cr);
}
#endif

#ifndef OSD_COLOR
/* if no color has been specified we just use the gdks default colors */
static void 
osd_shape(cairo_t *cr, GdkColor *bg, GdkColor *fg) {
    gdk_cairo_set_source_color(cr, bg);
    cairo_fill_preserve (cr);
    gdk_cairo_set_source_color(cr, fg);
    cairo_set_line_width (cr, 1);
    cairo_stroke (cr);
}
#else
static void 
osd_shape(cairo_t *cr) {
    cairo_set_source_rgb (cr, OSD_COLOR_BG);
    cairo_fill_preserve (cr);
    cairo_set_source_rgb (cr, OSD_COLOR);
    cairo_set_line_width (cr, 1);
    cairo_stroke (cr);
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
osd_check_dpad(gint x, gint y) 
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
osd_check_zoom(gint x, gint y) {
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

#ifdef OSD_SOURCE_SEL

/* place source selection at right border */
#define OSD_S_RAD (Z_RAD)
#define OSD_S_X   (-OSD_X)
#define OSD_S_Y   (OSD_Y)
#define OSD_S_PW  (2 * Z_RAD)
#define OSD_S_W   (OSD_S_PW)
#define OSD_S_PH  (2 * Z_RAD)
#define OSD_S_H   (OSD_S_PH + OSD_SHADOW)

/* size of usable area when expanded */
#define OSD_S_AREA_W (priv->width)
#define OSD_S_AREA_H (priv->height)
#define OSD_S_EXP_W  (OSD_S_PW + OSD_S_AREA_W + OSD_SHADOW)
#define OSD_S_EXP_H  (OSD_S_AREA_H + OSD_SHADOW)

/* internal value to draw the arrow on the "puller" */
#define OSD_S_D0  (OSD_S_RAD/2)
#ifndef OSD_FONT_SIZE
#define OSD_FONT_SIZE 16.0
#endif
#define OSD_TEXT_BORDER   (OSD_FONT_SIZE/2)
#define OSD_TEXT_SKIP     (OSD_FONT_SIZE/8)

/* draw the shape of the source selection OSD, either only the puller (not expanded) */
/* or the entire menu incl. the puller (expanded) */
static void
osd_source_shape(osd_priv_t *priv, cairo_t *cr, gint x, gint y) {
    if(!priv->expanded) {
        /* just draw the puller */
        cairo_move_to (cr, x + OSD_S_PW, y + OSD_S_PH);    
        cairo_arc (cr, x+OSD_S_RAD, y+OSD_S_RAD, OSD_S_RAD, M_PI/2, -M_PI/2);
        cairo_line_to (cr, x + OSD_S_PW, y);    
    } else {
        /* draw the puller and the area itself */
        cairo_move_to (cr, x + OSD_S_PW + OSD_S_AREA_W, y + OSD_S_AREA_H);    
        cairo_line_to (cr, x + OSD_S_PW, y + OSD_S_AREA_H);    
        if(OSD_S_Y > 0) {
            cairo_line_to (cr, x + OSD_S_PW, y + OSD_S_PH);    
            cairo_arc (cr, x+OSD_S_RAD, y+OSD_S_RAD, OSD_S_RAD, M_PI/2, -M_PI/2);
        } else {
            cairo_arc (cr, x+OSD_S_RAD, y+OSD_S_AREA_H-OSD_S_RAD, OSD_S_RAD, M_PI/2, -M_PI/2);
            cairo_line_to (cr, x + OSD_S_PW, y + OSD_S_AREA_H - OSD_S_PH);    
            cairo_line_to (cr, x + OSD_S_PW, y);    
        }
        cairo_line_to (cr, x + OSD_S_PW + OSD_S_AREA_W, y);    
        cairo_close_path (cr);
    }
}

static void
osd_source_content(osm_gps_map_osd_t *osd, cairo_t *cr, gint offset) {
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    int py = offset + OSD_S_RAD - OSD_S_D0;

    if(!priv->expanded) {
        /* draw the "puller" open (<) arrow */
        cairo_move_to (cr, offset + OSD_S_RAD + OSD_S_D0/2, py);
        cairo_rel_line_to (cr, -OSD_S_D0, +OSD_S_D0);
        cairo_rel_line_to (cr, +OSD_S_D0, +OSD_S_D0);
    } else {
        if(OSD_S_Y < 0) 
            py += OSD_S_AREA_H - OSD_S_PH;

        /* draw the "puller" close (>) arrow */
        cairo_move_to (cr, offset + OSD_S_RAD - OSD_S_D0/2, py);
        cairo_rel_line_to (cr, +OSD_S_D0, +OSD_S_D0);
        cairo_rel_line_to (cr, -OSD_S_D0, +OSD_S_D0);
        cairo_stroke(cr);

        /* don't draw a shadow for the text content */
        if(offset == 1) {
            gint source;
            g_object_get(osd->widget, "map-source", &source, NULL);

            cairo_select_font_face (cr, "Sans",
                                    CAIRO_FONT_SLANT_NORMAL,
                                    CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size (cr, OSD_FONT_SIZE);
            
            int i, step = (priv->height - 2*OSD_TEXT_BORDER) / 
                OSM_GPS_MAP_SOURCE_LAST;
            for(i=OSM_GPS_MAP_SOURCE_NULL+1;i<=OSM_GPS_MAP_SOURCE_LAST;i++) {
                cairo_text_extents_t extents;
                const char *src = osm_gps_map_source_get_friendly_name(i);
                cairo_text_extents (cr, src, &extents);
                
                int x = offset + OSD_S_PW + OSD_TEXT_BORDER;
                int y = offset + step * (i-1) + OSD_TEXT_BORDER;

                /* draw filled rectangle if selected */
                if(source == i) {
                    cairo_rectangle(cr, x - OSD_TEXT_BORDER/2, 
                                    y - OSD_TEXT_SKIP, 
                                    priv->width - OSD_TEXT_BORDER, 
                                    step + OSD_TEXT_SKIP);
                    cairo_fill(cr);

                    /* temprarily draw with background color */
#ifndef OSD_COLOR
                    GdkColor bg = osd->widget->style->bg[GTK_STATE_NORMAL];
                    gdk_cairo_set_source_color(cr, &bg);
#else
                    cairo_set_source_rgb (cr, OSD_COLOR_BG);
#endif
                }

                cairo_move_to (cr, x, y + OSD_TEXT_SKIP - extents.y_bearing);
                cairo_show_text (cr, src);

                /* restore color */
                if(source == i) {
#ifndef OSD_COLOR
                    GdkColor fg = osd->widget->style->fg[GTK_STATE_NORMAL];
                    gdk_cairo_set_source_color(cr, &fg);
#else
                    cairo_set_source_rgb (cr, OSD_COLOR);
#endif
                }
            }
        }
    }
}

static void
osd_render_source_sel(osm_gps_map_osd_t *osd) {
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

#ifndef OSD_COLOR
    GdkColor bg = GTK_WIDGET(osd->widget)->style->bg[GTK_STATE_NORMAL];
    GdkColor fg = GTK_WIDGET(osd->widget)->style->fg[GTK_STATE_NORMAL];
    GdkColor da = GTK_WIDGET(osd->widget)->style->fg[GTK_STATE_INSENSITIVE];
#endif

    /* draw source selector */
    cairo_t *cr = cairo_create(priv->map_source);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

#ifdef OSD_SHADOW_ENABLE
    osd_source_shape(priv, cr, 1+OSD_SHADOW, 1+OSD_SHADOW);
    osd_shape_shadow(cr);
#endif

    osd_source_shape(priv, cr, 1, 1);
#ifndef OSD_COLOR
    osd_shape(cr, &bg, &fg);
#else
    osd_shape(cr);
#endif

#ifdef OSD_SHADOW_ENABLE
    osd_labels_shadow(cr, Z_RAD/3, TRUE);
    osd_source_content(osd, cr, 1+OSD_LBL_SHADOW);
    cairo_stroke (cr);
#endif
#ifndef OSD_COLOR
    osd_labels(cr, Z_RAD/3, TRUE, &fg, &da);
#else
    osd_labels(cr, Z_RAD/3, TRUE);
#endif
    osd_source_content(osd, cr, 1);
    cairo_stroke (cr);

    cairo_destroy(cr);
}

/* re-allocate the buffer used to draw the menu. This is used */
/* to collapse/expand the buffer */
static void
osd_source_reallocate(osm_gps_map_osd_t *osd) {
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    /* re-allocate offscreen bitmap */
    g_assert (priv->map_source);

    int w = OSD_S_W, h = OSD_S_H;
    if(priv->expanded) {
        /* ... and right of it the waypoint id */
        cairo_text_extents_t extents;

        /* determine content size */
        cairo_t *cr = cairo_create(priv->map_source);
        cairo_select_font_face (cr, "Sans",
                                CAIRO_FONT_SLANT_NORMAL,
                                CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size (cr, OSD_FONT_SIZE);

        /* calculate menu size */
        int i, max_h = 0, max_w = 0;
        for(i=OSM_GPS_MAP_SOURCE_NULL+1;i<=OSM_GPS_MAP_SOURCE_LAST;i++) {
            const char *src = osm_gps_map_source_get_friendly_name(i);
            cairo_text_extents (cr, src, &extents);

            if(extents.width > max_w) max_w = extents.width;
            if(extents.height > max_h) max_h = extents.height;
        }
        cairo_destroy(cr);
       
        priv->width  = max_w + 2*OSD_TEXT_BORDER;
        priv->height = OSM_GPS_MAP_SOURCE_LAST * 
            (max_h + 2*OSD_TEXT_SKIP) + 2*OSD_TEXT_BORDER;

        w = OSD_S_EXP_W;
        h = OSD_S_EXP_H;
    }

    cairo_surface_destroy(priv->map_source);
    priv->map_source = 
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w+2, h+2);

    osd_render_source_sel(osd);

}

#define OSD_HZ      15
#define OSD_TIME    500

static gboolean osd_source_animate(gpointer data) {
    osm_gps_map_osd_t *osd = (osm_gps_map_osd_t*)data;
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 
    int diff = OSD_S_EXP_W - OSD_S_W - OSD_S_X;
    gboolean done = FALSE;
    priv->count += priv->dir;

    /* shifting in */
    if(priv->dir < 0) {
        if(priv->count <= 0) {
            priv->count = 0;
            done = TRUE;
        }
    } else {
        if(priv->count >= 1000) {
            priv->expanded = FALSE;
            osd_source_reallocate(osd);

            priv->count = 1000;
            done = TRUE;
        }
    }


    /* count runs linearly from 0 to 1000, map this nicely onto a position */

    /* nicer sinoid mapping */
    float m = 0.5-cos(priv->count * M_PI / 1000.0)/2;
    priv->shift = (osd->widget->allocation.width - OSD_S_EXP_W + OSD_S_X) + 
        m * diff;

    osm_gps_map_repaint(OSM_GPS_MAP(osd->widget));

    if(done) 
        priv->handler_id = 0;

    return !done;
}

/* switch between expand and collapse mode of source selection */
static void
osd_source_toggle(osm_gps_map_osd_t *osd)
{
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    /* ignore clicks while animation is running */
    if(priv->handler_id)
        return;

    /* expand immediately, collapse is handle at the end of the collapse animation */
    if(!priv->expanded) {
        priv->expanded = TRUE;
        osd_source_reallocate(osd);

        priv->count = 1000;
        priv->shift = osd->widget->allocation.width - OSD_S_W;
        priv->dir = -1000/OSD_HZ;
    } else {
        priv->count =  0;
        priv->shift = osd->widget->allocation.width - OSD_S_EXP_W + OSD_S_X;
        priv->dir = +1000/OSD_HZ;
    }

    priv->handler_id = gtk_timeout_add(OSD_TIME/OSD_HZ, osd_source_animate, osd);
}

/* check if the user clicked inside the source selection area */
static osd_button_t
osd_source_check(osm_gps_map_osd_t *osd, gint x, gint y) {
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    if(!priv->expanded)
        x -= osd->widget->allocation.width - OSD_S_W;
    else
        x -= osd->widget->allocation.width - OSD_S_EXP_W + OSD_S_X;

    if(OSD_S_Y > 0)
        y -= OSD_S_Y;
    else
        y -= osd->widget->allocation.height - OSD_S_PH + OSD_S_Y;
    
    /* within square around puller? */
    if(y > 0 && y < OSD_S_PH && x > 0 && x < OSD_S_PW) {
        /* really within puller shape? */
        if(x > Z_RAD || osm_gps_map_in_circle(x, y, Z_RAD, Z_RAD, Z_RAD)) {
            /* expand source selector */
            osd_source_toggle(osd);

            /* tell upper layers that user clicked some background element */
            /* of the OSD */
            return OSD_BG;
        }
    }

    /* check for clicks into data area */
    if(priv->expanded && !priv->handler_id) {
        if(x > OSD_S_PW && 
           x < OSD_S_PW + OSD_S_EXP_W &&
           y > 0 &&
           y < OSD_S_EXP_H) {

            int step = (priv->height - 2*OSD_TEXT_BORDER) 
                / OSM_GPS_MAP_SOURCE_LAST;

            y -= OSD_TEXT_BORDER - OSD_TEXT_SKIP;
            y /= step;
            y += 1;

            gint old = 0;
            g_object_get(osd->widget, "map-source", &old, NULL);

            if(y > OSM_GPS_MAP_SOURCE_NULL &&
               y <= OSM_GPS_MAP_SOURCE_LAST &&
               old != y) {
                g_object_set(osd->widget, "map-source", y, NULL);
                
                osd_render_source_sel(osd);
                osm_gps_map_repaint(OSM_GPS_MAP(osd->widget));
            }

            /* return "clicked in OSD background" to prevent further */
            /* processing by application */
            return OSD_BG;
        }
    }

    return OSD_NONE;
}
#endif // OSD_SOURCE_SEL

static osd_button_t
osd_check(osm_gps_map_osd_t *osd, gint x, gint y) {
    osd_button_t but = OSD_NONE;

#ifdef OSD_SOURCE_SEL
    /* the source selection area is handles internally */
    but = osd_source_check(osd, x, y);
    if(but != OSD_NONE) 
        return but;
#endif

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
        but = osd_check_dpad(x, y);
#endif

        if(but == OSD_NONE) 
            but = osd_check_zoom(x, y);
    }

    return but;
}

#ifndef OSD_NO_DPAD
static void
osd_dpad_labels(cairo_t *cr, gint x, gint y) {
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
osd_dpad_gps(cairo_t *cr, gint x, gint y) {
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
osd_zoom_labels(cairo_t *cr, gint x, gint y) {
    cairo_move_to (cr, x + Z_LEFT  - Z_LEN, y + Z_MID);
    cairo_line_to (cr, x + Z_LEFT  + Z_LEN, y + Z_MID);

    cairo_move_to (cr, x + Z_RIGHT,         y + Z_MID - Z_LEN);
    cairo_line_to (cr, x + Z_RIGHT,         y + Z_MID + Z_LEN);
    cairo_move_to (cr, x + Z_RIGHT - Z_LEN, y + Z_MID);
    cairo_line_to (cr, x + Z_RIGHT + Z_LEN, y + Z_MID);
}

static void
osd_render(osm_gps_map_osd_t *osd) {
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

#ifndef OSD_COLOR
    GdkColor bg = GTK_WIDGET(osd->widget)->style->bg[GTK_STATE_NORMAL];
    GdkColor fg = GTK_WIDGET(osd->widget)->style->fg[GTK_STATE_NORMAL];
    GdkColor da = GTK_WIDGET(osd->widget)->style->fg[GTK_STATE_INSENSITIVE];
#endif

    /* first fill with transparency */
    cairo_t *cr = cairo_create(priv->overlay);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    /* --------- draw zoom and dpad shape shadow ----------- */
#ifdef OSD_SHADOW_ENABLE
    osd_zoom_shape(cr, 1+OSD_SHADOW, 1+OSD_SHADOW);
    osd_shape_shadow(cr);
#ifndef OSD_NO_DPAD
    osd_dpad_shape(cr, 1+OSD_SHADOW, 1+OSD_SHADOW);
    osd_shape_shadow(cr);
#endif
#endif

    /* --------- draw zoom and dpad shape ----------- */

    osd_zoom_shape(cr, 1, 1);
#ifndef OSD_COLOR
    osd_shape(cr, &bg, &fg);
#else
    osd_shape(cr);
#endif
#ifndef OSD_NO_DPAD
    osd_dpad_shape(cr, 1, 1);
#ifndef OSD_COLOR
    osd_shape(cr, &bg, &fg);
#else
    osd_shape(cr);
#endif
#endif

    /* --------- draw zoom and dpad labels --------- */

#ifdef OSD_SHADOW_ENABLE
    osd_labels_shadow(cr, Z_RAD/3, TRUE);
    osd_zoom_labels(cr, 1+OSD_LBL_SHADOW, 1+OSD_LBL_SHADOW);
#ifndef OSD_NO_DPAD
    osd_dpad_labels(cr, 1+OSD_LBL_SHADOW, 1+OSD_LBL_SHADOW);
#endif
    cairo_stroke(cr);
#ifdef OSD_GPS_BUTTON
    osd_labels_shadow(cr, Z_RAD/6, osd->cb != NULL);
    osd_dpad_gps(cr, 1+OSD_LBL_SHADOW, 1+OSD_LBL_SHADOW); 
    cairo_stroke(cr);
#endif
#endif

#ifndef OSD_COLOR
    osd_labels(cr, Z_RAD/3, TRUE, &fg, &da);
#else
    osd_labels(cr, Z_RAD/3, TRUE);
#endif
    osd_zoom_labels(cr, 1, 1);
#ifndef OSD_NO_DPAD
    osd_dpad_labels(cr, 1, 1);
#endif
    cairo_stroke(cr);

#ifndef OSD_COLOR
    osd_labels(cr, Z_RAD/6, osd->cb != NULL, &fg, &da);
#else
    osd_labels(cr, Z_RAD/6, osd->cb != NULL);
#endif
#ifdef OSD_GPS_BUTTON
    osd_dpad_gps(cr, 1, 1);
#endif
    cairo_stroke(cr);
    
    cairo_destroy(cr);

#ifdef OSD_SOURCE_SEL
    osd_render_source_sel(osd);
#endif
}

static void
osd_draw(osm_gps_map_osd_t *osd, GdkDrawable *drawable)
{
    osd_priv_t *priv = (osd_priv_t*)osd->priv; 

    /* OSD itself uses some off-screen rendering, so check if the */
    /* offscreen buffer is present and create it if not */
    if(!priv->overlay) {
        /* create overlay ... */
        priv->overlay = 
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, OSD_W+2, OSD_H+2);

#ifdef OSD_SOURCE_SEL
        /* the initial OSD state is alway not-expanded */
        priv->map_source = 
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                           OSD_S_W+2, OSD_S_H+2);
#endif

        /* ... and render it */
        osd_render(osd);
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

#ifdef OSD_SOURCE_SEL
    if(!priv->handler_id) {
        /* the OSD source selection is not being animated */
        if(!priv->expanded)
            x = osd->widget->allocation.width - OSD_S_W;
        else
            x = osd->widget->allocation.width - OSD_S_EXP_W + OSD_S_X;
    } else
        x = priv->shift;

    y = OSD_S_Y;
    if(OSD_S_Y < 0) {
        if(!priv->expanded)
            y = osd->widget->allocation.height - OSD_S_H + OSD_S_Y;
        else
            y = osd->widget->allocation.height - OSD_S_EXP_H + OSD_S_Y;
    }

    cairo_set_source_surface(cr, priv->map_source, x, y);
    cairo_paint(cr);
#endif

    cairo_destroy(cr);
}

static void
osd_free(osm_gps_map_osd_t *osd) 
{
    osd_priv_t *priv = (osd_priv_t *)(osd->priv);

    if (priv->overlay)
         cairo_surface_destroy(priv->overlay);

#ifdef OSD_SOURCE_SEL
    if(priv->handler_id)
        gtk_timeout_remove(priv->handler_id);

    if (priv->map_source)
         cairo_surface_destroy(priv->map_source);
#endif

    g_free(priv);
}

static gboolean
osd_busy(osm_gps_map_osd_t *osd) 
{
#ifdef OSD_SOURCE_SEL
    osd_priv_t *priv = (osd_priv_t *)(osd->priv);
    return (priv->handler_id != 0);
#else
    return FALSE;
#endif
}

static osm_gps_map_osd_t osd_classic = {
    .widget     = NULL,

    .draw       = osd_draw,
    .check      = osd_check,
    .render     = osd_render,
    .free       = osd_free,
    .busy       = osd_busy,

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

osd_button_t
osm_gps_map_osd_check(OsmGpsMap *map, gint x, gint y) {
    osm_gps_map_osd_t *osd = osm_gps_map_osd_get(map);
    g_return_val_if_fail (osd, OSD_NONE);
    
    return osd_check(osd, x, y);
}
