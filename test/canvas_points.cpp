#include <canvas.h>

#include <cassert>
#include <memory>

#include <osm2go_annotations.h>
#include <osm2go_stl.h>
#include <osm2go_test.h>

static void testSegment()
{
  std::vector<lpos_t> points;
  for (unsigned int i = 0; i < 8; i += 2)
    points.push_back(lpos_t(1 << i, 2 << i));

  canvas_holder canvas;

  canvas_item_t * const line = canvas->polyline_new(CANVAS_GROUP_WAYS, points, 1, 0);
  assert(line);

  int segnum = canvas->get_item_segment(line, lpos_t((4 + 16) / 2, (8 + 32) / 2));
  assert_cmpnum(segnum, 1);
}

static void testInObject()
{
  canvas_holder canvas;

  // a circle that should have nothing to do with the initial search
  canvas_item_circle * const circle = canvas->circle_new(CANVAS_GROUP_WAYS, lpos_t(100, 20), 15,
                                                         0, color_t::transparent());
  assert(circle != nullptr);

  // a square, rotated by 45 degrees
  std::vector<lpos_t> points;
  points.push_back(lpos_t(0, 200));
  points.push_back(lpos_t(200, 400));
  points.push_back(lpos_t(400, 200));
  points.push_back(lpos_t(200, 0));
  points.push_back(points.front());

  canvas_item_t * const line = canvas->polygon_new(CANVAS_GROUP_WAYS, points, 1, 0, 0);
  assert(line != nullptr);

  canvas_item_t *search = canvas->get_item_at(lpos_t(200, 200));
  assert(line == search);

  search = canvas->get_item_at(lpos_t(40, 50));
  assert_null(search);

  // now try to find the circle
  // the given position is slightly outside the circle, but the fuzziness
  // should still catch it
  search = canvas->get_item_at(lpos_t(100, 38));
  assert(circle == search);
}

static void testToBottom()
{
  std::vector<lpos_t> points;
  for (unsigned int i = 0; i < 3; i += 2)
    points.push_back(lpos_t(1 << i, 2 << i));

  canvas_holder canvas;

  // just to be sure that this does no harm
  assert_null(canvas->get_item_at(lpos_t(3, 6)));

  // 2 polygons that overlap
  canvas_item_t * const line = canvas->polyline_new(CANVAS_GROUP_WAYS, points, 1, 0);
  assert(line != nullptr);

  for (unsigned int i = 0; i < points.size(); i += 2) {
    lpos_t p = points[i];
    p.x *= 2;
    p.y *= 2;
    points[i] = p;
  }

  canvas_item_t *line2 = canvas->polyline_new(CANVAS_GROUP_WAYS, points, 1, 0);
  assert(line2 != nullptr);

  canvas_item_t *search1 = canvas->get_item_at(lpos_t(3, 6));
  // must be one of the items, it's exactly on them
  assert(search1 == line || search1 == line2);

  // now the other one must be on top
  canvas->item_to_bottom(search1);
  canvas_item_t *search2 = canvas->get_item_at(lpos_t(3, 6));
  assert(search2 == line || search2 == line2);
  assert(search1 != search2);

  // and back to the first
  canvas->item_to_bottom(search2);
  canvas_item_t *search3 = canvas->get_item_at(lpos_t(3, 6));

  assert(search1 == search3);
}

int main()
{
  testSegment();
  testInObject();
  testToBottom();

  return 0;
}

#include "appdata_dummy.h"
