#include <canvas.h>

#include <misc.h>

#include <cassert>
#include <memory>

#include <osm2go_stl.h>

bool testSegment()
{
  bool ret = true;

  std::vector<lpos_t> points;
  for (unsigned int i = 0; i < 8; i += 2)
    points.push_back(lpos_t(1 << i, 2 << i));

  std::unique_ptr<canvas_t> canvas(canvas_t::create());
  g_assert_true(canvas);

  std::unique_ptr<canvas_item_t> line(canvas->polyline_new(CANVAS_GROUP_WAYS, points, 1, 0));
  g_assert_true(line);

  int segnum = line->get_segment(lpos_t((4 + 16) / 2, (8 + 32) / 2));
  g_assert_cmpint(segnum, ==, 1);

  return ret;
}

bool testInObject()
{
  bool ret = true;

  std::unique_ptr<canvas_t> canvas(canvas_t::create());
  g_assert_true(canvas);

  // a square, rotated by 45 degrees
  std::vector<lpos_t> points;
  points.push_back(lpos_t(0, 200));
  points.push_back(lpos_t(200, 400));
  points.push_back(lpos_t(400, 200));
  points.push_back(lpos_t(200, 0));
  points.push_back(points.front());

  std::unique_ptr<canvas_item_t> line(canvas->polygon_new(CANVAS_GROUP_WAYS, points, 1, 0, 0));
  g_assert_true(line);

  canvas_item_t *search = canvas->get_item_at(200, 200);
  assert(line.get() == search);

  search = canvas->get_item_at(40, 50);
  g_assert_null(search);

  return ret;
}

int main()
{
  bool ret = true;

  ret &= testSegment();

  ret &= testInObject();

  return ret ? 0 : 1;
}
