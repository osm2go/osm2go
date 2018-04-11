#include <canvas.h>

#include <misc.h>

#include <cassert>
#include <memory>

#include <osm2go_annotations.h>
#include <osm2go_stl.h>

bool testSegment()
{
  bool ret = true;

  std::vector<lpos_t> points;
  for (unsigned int i = 0; i < 8; i += 2)
    points.push_back(lpos_t(1 << i, 2 << i));

  std::unique_ptr<canvas_t> canvas(canvas_t::create());
  assert(canvas);

  std::unique_ptr<canvas_item_t> line(canvas->polyline_new(CANVAS_GROUP_WAYS, points, 1, 0));
  assert(line);

  int segnum = line->get_segment(lpos_t((4 + 16) / 2, (8 + 32) / 2));
  assert_cmpnum(segnum, 1);

  return ret;
}

bool testInObject()
{
  bool ret = true;

  std::unique_ptr<canvas_t> canvas(canvas_t::create());
  assert(canvas);

  // a circle that should have nothing to do with the initial search
  std::unique_ptr<canvas_item_circle> circle(canvas->circle_new(CANVAS_GROUP_WAYS, 100, 20, 15,
                                                                0, color_t::transparent()));

  // a square, rotated by 45 degrees
  std::vector<lpos_t> points;
  points.push_back(lpos_t(0, 200));
  points.push_back(lpos_t(200, 400));
  points.push_back(lpos_t(400, 200));
  points.push_back(lpos_t(200, 0));
  points.push_back(points.front());

  std::unique_ptr<canvas_item_t> line(canvas->polygon_new(CANVAS_GROUP_WAYS, points, 1, 0, 0));
  assert(line);

  canvas_item_t *search = canvas->get_item_at(lpos_t(200, 200));
  assert(line.get() == search);

  search = canvas->get_item_at(lpos_t(40, 50));
  assert_null(search);

  // now try to find the circle
  // the given position is slightly outside the circle, but the fuzziness
  // should still catch it
  search = canvas->get_item_at(lpos_t(100, 38));
  assert(circle.get() == search);

  return ret;
}

int main()
{
  bool ret = true;

  ret &= testSegment();

  ret &= testInObject();

  return ret ? 0 : 1;
}
