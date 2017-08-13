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

/**
 * @file canvas.cpp
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
#include "misc.h"

#include <cmath>
#include <cstring>

/* The fuzziness allows to specify how far besides an object a user may */
/* click so it's still considered a click onto that object. This can */
/* be given in meters _and_ in pixels. Both values will be added to */
/* the total fuzziness. */
#define EXTRA_FUZZINESS_METER  0
#define EXTRA_FUZZINESS_PIXEL  8

/* remove item_info from chain as its visual representation */
/* has been destroyed */
static void item_info_destroy(gpointer data) {
  delete static_cast<canvas_item_info_t *>(data);
}

canvas_item_info_t::canvas_item_info_t(canvas_item_type_t t, canvas_t *cv, canvas_group_t g, canvas_item_t *it)
  : canvas(cv)
  , type(t)
  , group(g)
  , item(it)
{
  memset(&data, 0, sizeof(data));

  canvas->item_info[group].insert(canvas->item_info[group].begin(), this);

  canvas_item_destroy_connect(item, item_info_destroy, this);
}

canvas_item_info_t::~canvas_item_info_t()
{
  if(type == CANVAS_ITEM_POLY)
    g_free(data.poly.points);

  std::vector<canvas_item_info_t *> &info_group = canvas->item_info[group];

  /* search for item in chain */
  const std::vector<canvas_item_info_t *>::iterator itEnd = info_group.end();
  std::vector<canvas_item_info_t *>::iterator it = std::find(info_group.begin(),
                                                             itEnd, this);
  g_assert(it != itEnd);

  info_group.erase(it);
}

struct item_info_find {
  const canvas_item_t * const citem;
  item_info_find(const canvas_item_t *i) : citem(i) {}
  bool operator()(const canvas_item_info_t *item) {
    return item->item == citem;
  }
};

static canvas_item_info_t *canvas_item_get_info(canvas_t *canvas,
						canvas_item_t *item) {
  /* search for item in all chains */
  for(unsigned int group = 0; group < CANVAS_GROUPS; group++) {
    const std::vector<canvas_item_info_t *>::const_iterator itEnd = canvas->item_info[group].end();
    std::vector<canvas_item_info_t *>::const_iterator it = std::find_if(cbegin(canvas->item_info[group]),
                                                                        itEnd, item_info_find(item));
    if(it != itEnd)
      return *it;
  }
  return O2G_NULLPTR;
}

void canvas_item_info_push(canvas_t *canvas, canvas_item_t *item) {
  canvas_item_info_t *item_info = canvas_item_get_info(canvas, item);
  g_assert_nonnull(item_info);

  printf("pushing item_info %p to background\n", item_info);
  g_assert(item_info->canvas == canvas);

  std::vector<canvas_item_info_t *> &info_group = canvas->item_info[item_info->group];
  const std::vector<canvas_item_info_t *>::iterator itEnd = info_group.end();
  std::vector<canvas_item_info_t *>::iterator it = std::find(info_group.begin(),
                                                             itEnd, item_info);

  std::rotate(it, it + 1, itEnd);
}

/* store local information about the location of a circle to be able */
/* to find it when searching for items at a certain position on screen */
void canvas_item_info_attach_circle(canvas_t *canvas, canvas_group_t group,
		    canvas_item_t *canvas_item, gint x, gint y, gint r) {

  /* create a new object and insert it into the chain */
  canvas_item_info_t *item = new canvas_item_info_t(CANVAS_ITEM_CIRCLE, canvas, group, canvas_item);

  item->data.circle.center.x = x;
  item->data.circle.center.y = y;
  item->data.circle.r = r;
}

void canvas_item_info_attach_poly(canvas_t *canvas, canvas_group_t group,
				  canvas_item_t *canvas_item,
                                  bool is_polygon, canvas_points_t *points, gint width) {

  /* create a new object and insert it into the chain */
  canvas_item_info_t *item = new canvas_item_info_t(CANVAS_ITEM_POLY, canvas, group, canvas_item);

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
static bool inpoly(const lpos_t *poly, gint npoints, gint x, gint y) {
  int xnew, ynew;
  int xold, yold;
  int x1, y1;
  int x2, y2;
  int i;

  if(npoints < 3)
    return false;

  xold = poly[npoints-1].x;
  yold = poly[npoints-1].y;
  bool inside = false;
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

  /* convert all "fuzziness" into meters */
  gint fuzziness = EXTRA_FUZZINESS_METER +
    EXTRA_FUZZINESS_PIXEL / canvas_get_zoom(canvas);

  /* search from top to bottom */
  for(unsigned int group = CANVAS_GROUPS - 1; group > 0; group--) {
    /* search through all item infos */
    const std::vector<canvas_item_info_t *>::const_iterator itEnd = canvas->item_info[group].end();
    for(std::vector<canvas_item_info_t *>::const_iterator it = canvas->item_info[group].begin();
        it != itEnd; it++) {
      canvas_item_info_t *item = *it;
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
          bool in_poly = false;
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
    }
  }
  printf("************* nothing found ******************\n");

  return O2G_NULLPTR;
}
