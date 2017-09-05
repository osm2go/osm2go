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

canvas_t::canvas_t(GtkWidget *w)
  : widget(w)
{
  g_object_set_data(G_OBJECT(widget), "canvas-pointer", this);

  g_object_set(G_OBJECT(widget), "anchor", GTK_ANCHOR_CENTER, O2G_NULLPTR);
}

/* remove item_info from chain as its visual representation */
/* has been destroyed */
template<typename T>
void item_info_destroy(gpointer data) {
  delete static_cast<T *>(data);
}

canvas_item_info_t::canvas_item_info_t(canvas_item_type_t t, canvas_t *cv, canvas_group_t g, canvas_item_t *it, void(*deleter)(gpointer))
  : canvas(cv)
  , type(t)
  , group(g)
  , item(it)
{
  canvas->item_info[group].push_back(this);

  canvas_item_destroy_connect(item, deleter, this);
}

canvas_item_info_t::~canvas_item_info_t()
{
  std::vector<canvas_item_info_t *> &info_group = canvas->item_info[group];

  /* search for item in chain */
  const std::vector<canvas_item_info_t *>::iterator itEnd = info_group.end();
  std::vector<canvas_item_info_t *>::iterator it = std::find(info_group.begin(),
                                                             itEnd, this);
  g_assert(it != itEnd);

  info_group.erase(it);
}

canvas_item_info_circle::canvas_item_info_circle(canvas_t *cv, canvas_group_t g, canvas_item_t *it,
                                                 const int cx, const int cy, const unsigned int radius)
  : canvas_item_info_t(CANVAS_ITEM_CIRCLE, cv, g, it, item_info_destroy<canvas_item_info_circle>)
  , r(radius)
{
  center.x = cx;
  center.y = cy;
}

canvas_item_info_poly::canvas_item_info_poly(canvas_t* cv, canvas_group_t g, canvas_item_t* it,
                                             bool poly, unsigned int wd, canvas_points_t *cpoints)
  : canvas_item_info_t(CANVAS_ITEM_POLY, cv, g, it, item_info_destroy<canvas_item_info_poly>)
  , is_polygon(poly)
  , width(wd)
  , num_points(canvas_points_num(cpoints))
  , points(new lpos_t[num_points])
{
  bbox.top_left.x = bbox.top_left.y = G_MAXINT;
  bbox.bottom_right.x = bbox.bottom_right.y = G_MININT;

  for(unsigned int i = 0; i < num_points; i++) {
    canvas_point_get_lpos(cpoints, i, points[i]);

    /* determine bounding box */
    bbox.top_left.x = std::min(bbox.top_left.x, points[i].x);
    bbox.top_left.y = std::min(bbox.top_left.y, points[i].y);
    bbox.bottom_right.x = std::max(bbox.bottom_right.x, points[i].x);
    bbox.bottom_right.y = std::max(bbox.bottom_right.y, points[i].y);
  }

  /* take width of lines into account when calculating bounding box */
  bbox.top_left.x -= width / 2;
  bbox.top_left.y -= width / 2;
  bbox.bottom_right.x += width / 2;
  bbox.bottom_right.y += width / 2;
}

canvas_item_info_poly::~canvas_item_info_poly()
{
  delete[] points;
}

struct item_info_find {
  const canvas_item_t * const citem;
  explicit item_info_find(const canvas_item_t *i) : citem(i) {}
  bool operator()(const canvas_item_info_t *item) {
    return item->item == citem;
  }
};

static canvas_item_info_t *canvas_item_get_info(canvas_t *canvas,
						canvas_item_t *item) {
  /* search for item in all chains */
  for(unsigned int group = 0; group < CANVAS_GROUPS; group++) {
    const std::vector<canvas_item_info_t *>::const_iterator itEnd = canvas->item_info[group].end();
    std::vector<canvas_item_info_t *>::const_iterator it = std::find_if(std::cbegin(canvas->item_info[group]),
                                                                        itEnd, item_info_find(item));
    if(it != itEnd)
      return *it;
  }
  return O2G_NULLPTR;
}

void canvas_t::item_info_push(canvas_item_t *item) {
  canvas_item_info_t *info = canvas_item_get_info(this, item);
  g_assert_nonnull(info);

  printf("pushing item_info %p to background\n", info);
  g_assert(info->canvas == this);

  std::vector<canvas_item_info_t *> &info_group = item_info[info->group];
  const std::vector<canvas_item_info_t *>::reverse_iterator itEnd = info_group.rend();
  std::vector<canvas_item_info_t *>::reverse_iterator it = std::find(info_group.rbegin(),
                                                                     itEnd, info);

  std::rotate(it, it + 1, itEnd);
}

/* check whether a given point is inside a polygon */
/* inpoly() taken from http://www.visibone.com/inpoly/ */
static bool inpoly(const canvas_item_info_poly *poly, gint x, gint y) {
  int xold, yold;

  if(poly->num_points < 3)
    return false;

  xold = poly->points[poly->num_points - 1].x;
  yold = poly->points[poly->num_points - 1].y;
  bool inside = false;
  for (unsigned i = 0 ; i < poly->num_points ; i++) {
    int x1, y1, x2, y2;
    int xnew = poly->points[i].x;
    int ynew = poly->points[i].y;

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
        && (y - y1) * (long)(x2 - x1) < (y2 - y1) * (long)(x - x1))
      inside = !inside;

    xold = xnew;
    yold = ynew;
  }

  return inside;
}


/* get the polygon/polyway segment a certain coordinate is over */
static gint canvas_item_info_get_segment(canvas_item_info_poly *item,
					 gint x, gint y, gint fuzziness) {
  gint retval = -1;
  float mindist = 1000000.0;
  for(unsigned int i = 0; i < item->num_points - 1; i++) {

#define AX (item->points[i].x)
#define AY (item->points[i].y)
#define BX (item->points[i+1].x)
#define BY (item->points[i+1].y)
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
      if((n <= (item->width/2+fuzziness)) && (n < mindist)) {
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
canvas_item_t *canvas_t::get_item_at(int x, int y) const {
  printf("************ searching at %d %d *****************\n", x, y);

  /* convert all "fuzziness" into meters */
  gint fuzziness = EXTRA_FUZZINESS_METER +
    EXTRA_FUZZINESS_PIXEL / get_zoom();

  /* search from top to bottom */
  for(unsigned int group = CANVAS_GROUPS - 1; group > 0; group--) {
    /* search through all item infos */
    const std::vector<canvas_item_info_t *>::const_reverse_iterator itEnd = item_info[group].rend();
    for(std::vector<canvas_item_info_t *>::const_reverse_iterator it = item_info[group].rbegin();
        it != itEnd; it++) {
      canvas_item_info_t *item = *it;
      switch(item->type) {
      case CANVAS_ITEM_CIRCLE: {
        canvas_item_info_circle *circle = static_cast<canvas_item_info_circle *>(item);
        int radius = circle->r;
        if((x >= circle->center.x - radius - fuzziness) &&
           (y >= circle->center.y - radius - fuzziness) &&
           (x <= circle->center.x + radius + fuzziness) &&
           (y <= circle->center.y + radius + fuzziness)) {

          gint xdist = circle->center.x - x;
          gint ydist = circle->center.y - y;
          if(xdist * xdist + ydist * ydist < (radius + fuzziness) * (radius + fuzziness)) {
            printf("circle item %p at %d/%d(%u)\n", item,
                   circle->center.x, circle->center.y, circle->r);
            return item->item;
          }
        }
      } break;

      case CANVAS_ITEM_POLY: {
        canvas_item_info_poly *poly = static_cast<canvas_item_info_poly *>(item);
        if((x >= poly->bbox.top_left.x - fuzziness) &&
           (y >= poly->bbox.top_left.y - fuzziness) &&
           (x <= poly->bbox.bottom_right.x + fuzziness) &&
           (y <= poly->bbox.bottom_right.y + fuzziness)) {

          int on_segment = canvas_item_info_get_segment(poly, x, y, fuzziness);
          if((on_segment >= 0) || (poly->is_polygon && inpoly(poly, x, y)))
            return item->item;
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
