/*
 * Copyright (C) 2009 Till Harbaum <till@harbaum.org>.
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

/* canvas.c
 *
 * this file contains framework independant canvas functionality like
 * e.g. a canvas agnostic way of detecting which items are at a certain
 * position. This is required for some canvas that don't provide this
 * function
 *
 * This also allows for a less precise item selection and especially
 * to differentiate between the clicks on a polygon border and its
 * interior
 *
 * References:
 * http://en.wikipedia.org/wiki/Point_in_polygon
 * http://www.visibone.com/inpoly/
 */

#include "canvas.h"

#include "appdata.h"

/* The fuzziness allows to specify how far besides an object a user may */
/* click so it's still considered a click onto that object. This can */
/* be given in meters _and_ in pixels. Both values will be added to */
/* the total fuzziness. */
#define EXTRA_FUZZINESS_METER  0
#define EXTRA_FUZZINESS_PIXEL  8

static void canvas_item_info_free(canvas_item_info_t *info) {
  if(info->type == CANVAS_ITEM_POLY)
    g_free(info->data.poly.points);

  g_free(info);
}

static void canvas_item_info_dechain(canvas_item_info_t *item_info) {
  canvas_t *canvas = item_info->canvas;

  //   printf("dechain %p\n", item_info);

  /* search for item in chain */
  canvas_item_info_t **itemP = &canvas->item_info[item_info->group].first;
  while(*itemP && *itemP != item_info)
    itemP = &(*itemP)->next;

  g_assert(*itemP);

  /* check if we are removing the last entry in the list and */
  /* adjust last pointer if yes */
  if(canvas->item_info[item_info->group].last == item_info)
    canvas->item_info[item_info->group].last = item_info->prev;

  /* adjust prev pointer in next element (if present) */
  if((*itemP)->next)
    (*itemP)->next->prev = (*itemP)->prev;

  /* adjust current pointer to next */
  *itemP = (*itemP)->next;

  item_info->prev = item_info->next = NULL;

#if 0
  /* do some sanity checks on chain to check if got damaged */
  canvas_item_info_t *prev = NULL, *sc_item = canvas->item_info.first;
  while(sc_item) {
    g_assert(sc_item->prev == prev);

    /* last in chain must be pointed at by last_item_info */
    if(!sc_item->next)
      g_assert(sc_item == canvas->item_info.last);

    prev = sc_item;
    sc_item = sc_item->next;
  }
#endif
}

/* remove item_info from chain as its visual representation */
/* has been destroyed */
static gint item_info_destroy(G_GNUC_UNUSED canvas_item_t *canvas_item,
			      canvas_item_info_t *item_info) {
  //  printf("######## destroy %p\n", item_info);

  canvas_item_info_dechain(item_info);
  canvas_item_info_free(item_info);

  return FALSE;
}

static void canvas_item_prepend(canvas_t *canvas, canvas_group_t group,
			canvas_item_t *canvas_item, canvas_item_info_t *item) {
  if(!canvas->item_info[group].first) {
    g_assert(!canvas->item_info[group].last);
    canvas->item_info[group].last = item;
  } else
    canvas->item_info[group].first->prev = item;

  /* attach destroy event handler if it hasn't already been attached */
  if(!item->item)
    canvas_item_destroy_connect(canvas_item,
				(GCallback)item_info_destroy, item);

  item->group = group;
  item->next = canvas->item_info[group].first;
  canvas->item_info[group].first = item;
  item->item = canvas_item;   /* reference to visual representation */
  item->canvas = canvas;
}

static void canvas_item_append(canvas_t *canvas, canvas_group_t group,
	       canvas_item_t *canvas_item, canvas_item_info_t *item) {
  if(!canvas->item_info[group].last) {
    g_assert(!canvas->item_info[group].first);
    canvas->item_info[group].first = item;
  } else
    canvas->item_info[group].last->next = item;

  /* attach destroy event handler if it hasn't already been attached */
  if(!item->item)
    canvas_item_destroy_connect(canvas_item,
				(GCallback)item_info_destroy, item);

  item->group = group;
  item->prev = canvas->item_info[group].last;
  canvas->item_info[group].last = item;
  item->item = canvas_item;   /* reference to visual representation */
  item->canvas = canvas;
}

static canvas_item_info_t *canvas_item_get_info(canvas_t *canvas,
						canvas_item_t *item) {
  /* search for item in all chains */
  canvas_group_t group;
  for(group = 0; group < CANVAS_GROUPS; group++) {
    canvas_item_info_t *item_info = canvas->item_info[group].first;
    while(item_info) {
      if(item_info->item == item)
	return item_info;

      item_info = item_info->next;
    }
  }
  return NULL;
}

void canvas_item_info_push(canvas_t *canvas, canvas_item_t *item) {
  canvas_item_info_t *item_info = canvas_item_get_info(canvas, item);
  g_assert(item_info);

  printf("pushing item_info %p to background\n", item_info);

  canvas_item_info_dechain(item_info);
  canvas_item_append(canvas, item_info->group,
		     item_info->item, item_info);
}

/* store local information about the location of a circle to be able */
/* to find it when searching for items at a certain position on screen */
void canvas_item_info_attach_circle(canvas_t *canvas, canvas_group_t group,
		    canvas_item_t *canvas_item, gint x, gint y, gint r) {

  /* create a new object and insert it into the chain */
  canvas_item_info_t *item = g_new0(canvas_item_info_t, 1);
  canvas_item_prepend(canvas, group, canvas_item, item);

  item->type = CANVAS_ITEM_CIRCLE;
  item->data.circle.center.x = x;
  item->data.circle.center.y = y;
  item->data.circle.r = r;
}

void canvas_item_info_attach_poly(canvas_t *canvas, canvas_group_t group,
				  canvas_item_t *canvas_item,
		  gboolean is_polygon, canvas_points_t *points, gint width) {

  /* create a new object and insert it into the chain */
  canvas_item_info_t *item = g_new0(canvas_item_info_t, 1);
  canvas_item_prepend(canvas, group, canvas_item, item);

  item->type = CANVAS_ITEM_POLY;
  item->data.poly.is_polygon = is_polygon;
  item->data.poly.width = width;

  /* allocate space for point list */
  item->data.poly.num_points = canvas_points_num(points);
  item->data.poly.points = g_new0(lpos_t, item->data.poly.num_points);
  gint i;

  item->data.poly.bbox.top_left.x =
    item->data.poly.bbox.top_left.y = G_MAXINT;
  item->data.poly.bbox.bottom_right.x =
    item->data.poly.bbox.bottom_right.y = G_MININT;

  for(i=0;i<item->data.poly.num_points;i++) {
    canvas_point_get_lpos(points, i, item->data.poly.points+i);

    /* determine bounding box */
    if(item->data.poly.points[i].x < item->data.poly.bbox.top_left.x)
      item->data.poly.bbox.top_left.x = item->data.poly.points[i].x;
    if(item->data.poly.points[i].y < item->data.poly.bbox.top_left.y)
      item->data.poly.bbox.top_left.y = item->data.poly.points[i].y;
    if(item->data.poly.points[i].x > item->data.poly.bbox.bottom_right.x)
      item->data.poly.bbox.bottom_right.x = item->data.poly.points[i].x;
    if(item->data.poly.points[i].y > item->data.poly.bbox.bottom_right.y)
      item->data.poly.bbox.bottom_right.y = item->data.poly.points[i].y;
  }

  /* take width of lines into account when calculating bounding box */
  item->data.poly.bbox.top_left.x -= width/2;
  item->data.poly.bbox.top_left.y -= width/2;
  item->data.poly.bbox.bottom_right.x += width/2;
  item->data.poly.bbox.bottom_right.y += width/2;
}

/* check whether a given point is inside a polygon */
/* inpoly() taken from http://www.visibone.com/inpoly/ */
static gboolean inpoly(lpos_t *poly, gint npoints, gint x, gint y) {
  int xnew, ynew;
  int xold, yold;
  int x1, y1;
  int x2, y2;
  int i;
  gboolean inside = FALSE;

  if(npoints < 3)
    return 0;

  xold = poly[npoints-1].x;
  yold = poly[npoints-1].y;
  for (i=0 ; i < npoints ; i++) {
    xnew = poly[i].x;
    ynew = poly[i].y;
    if (xnew > xold) {
      x1 = xold;
      x2 = xnew;
      y1 = yold;
      y2 = ynew;
    } else {
      x1 = xnew;
      x2 = xold;
      y1 = ynew;
      y2 = yold;
    }
    if ((xnew < x) == (x <= xold)          /* edge "open" at one end */
	&& ((long)y-(long)y1)*(long)(x2-x1)
	< ((long)y2-(long)y1)*(long)(x-x1))
      inside = !inside;

    xold = xnew;
    yold = ynew;
  }

  return inside;
}


/* get the polygon/polyway segment a certain coordinate is over */
static gint canvas_item_info_get_segment(canvas_item_info_t *item,
					 gint x, gint y, gint fuzziness) {

  g_assert(item->type == CANVAS_ITEM_POLY);

  if(item->data.poly.num_points < 2) return -1;

  gint retval = -1, i;
  float mindist = 1000000.0;
  for(i=0;i<item->data.poly.num_points-1;i++) {

#define AX (item->data.poly.points[i].x)
#define AY (item->data.poly.points[i].y)
#define BX (item->data.poly.points[i+1].x)
#define BY (item->data.poly.points[i+1].y)
#define CX ((double)x)
#define CY ((double)y)

    float len2 = pow(BY-AY,2)+pow(BX-AX,2);
    float m = ((CX-AX)*(BX-AX)+(CY-AY)*(BY-AY)) / len2;

    /* this is a possible candidate */
    if((m >= 0.0) && (m <= 1.0)) {

      float n;
      if(abs(BX-AX) > abs(BY-AY))
	n = fabs(sqrt(len2) * (AY+m*(BY-AY)-CY)/(BX-AX));
      else
	n = fabs(sqrt(len2) * -(AX+m*(BX-AX)-CX)/(BY-AY));

      /* check if this is actually on the line and closer than anything */
      /* we found so far */
      if((n <= (item->data.poly.width/2+fuzziness)) && (n < mindist)) {
	retval = i;
	mindist = n;
      }
    }
 }

  /* the last and first point are identical for polygons in osm2go. */
  /* goocanvas doesn't need that, but that's how OSM works and it saves */
  /* us from having to check the last->first connection for polygons */
  /* seperately */

  return retval;
}

/* try to find the object at position x/y by searching through the */
/* item_info list */
canvas_item_t *canvas_item_info_get_at(canvas_t *canvas, gint x, gint y) {

  printf("************ searching at %d %d *****************\n", x, y);

  /* search through all groups */
  canvas_group_t group;

  /* convert all "fuzziness" into meters */
  gint fuzziness = EXTRA_FUZZINESS_METER +
    EXTRA_FUZZINESS_PIXEL / canvas_get_zoom(canvas);

  /* search from top to bottom */
  for(group = CANVAS_GROUPS - 1; group > 0; group--) {
    canvas_item_info_t *item = canvas->item_info[group].first;
    //    if(item) printf("searching in group %d\n", group);

    /* search through all item infos */
    while(item) {
      switch(item->type) {
      case CANVAS_ITEM_CIRCLE: {
	if((x >= item->data.circle.center.x - item->data.circle.r - fuzziness) &&
	   (y >= item->data.circle.center.y - item->data.circle.r - fuzziness) &&
	   (x <= item->data.circle.center.x + item->data.circle.r + fuzziness) &&
	   (y <= item->data.circle.center.y + item->data.circle.r + fuzziness)) {

	  gint xdist = item->data.circle.center.x - x;
	  gint ydist = item->data.circle.center.y - y;
	  if(xdist*xdist + ydist*ydist <
	     (item->data.circle.r+fuzziness)*(item->data.circle.r+fuzziness)) {
	    printf("circle item %p at %d/%d(%d)\n", item,
		   item->data.circle.center.x,
		   item->data.circle.center.y,
		   item->data.circle.r);
	    return item->item;
	  }
	}
      } break;

      case CANVAS_ITEM_POLY: {
	if((x >= item->data.poly.bbox.top_left.x - fuzziness) &&
	   (y >= item->data.poly.bbox.top_left.y - fuzziness) &&
	   (x <= item->data.poly.bbox.bottom_right.x + fuzziness) &&
	   (y <= item->data.poly.bbox.bottom_right.y + fuzziness)) {

	  int on_segment = canvas_item_info_get_segment(item, x, y, fuzziness);
	  gboolean in_poly = FALSE;
	  if(item->data.poly.is_polygon)
	    in_poly = inpoly(item->data.poly.points,
			     item->data.poly.num_points, x, y);

	  if((on_segment >= 0) || in_poly) {
	    printf("bbox item %p, %d pts -> %d %s\n", item,
		   item->data.poly.num_points, on_segment,
		   in_poly?"in_poly":"");

	    return item->item;
	  }
	}
      } break;

      default:
	g_assert_not_reached();
	break;
      }

      item = item->next;
    }
  }
  printf("************* nothing found ******************\n");

  return NULL;
}
