#include <canvas.h>

#include <misc.h>

#include <memory>

#include <osm2go_stl.h>

bool testSegment()
{
  bool ret = true;

  std::unique_ptr<canvas_points_t> points(canvas_points_t::create(4));
  g_assert_true(points);

  for (unsigned int i = 0; i < 8; i++)
    points->coords()[i] = 1 << i;

  std::unique_ptr<canvas_t> canvas(canvas_t::create());
  g_assert_true(canvas);

  std::unique_ptr<canvas_item_t> line(canvas->polyline_new(CANVAS_GROUP_WAYS, points.get(), 1, 0));
  g_assert_true(line);

  double coords[4] = { 0, 0, 0, 0 };
  int segnum = line->get_segment(lpos_t((4 + 16) / 2, (8 + 32) / 2), coords);
  g_assert_cmpint(segnum, ==, 1);
  g_assert_cmpfloat(coords[0] , ==, 4);
  g_assert_cmpfloat(coords[1] , ==, 8);
  g_assert_cmpfloat(coords[2] , ==, 16);
  g_assert_cmpfloat(coords[3] , ==, 32);

  return ret;
}

bool testInObject()
{
  bool ret = true;

  std::unique_ptr<canvas_t> canvas(canvas_t::create());
  g_assert_true(canvas);

  std::unique_ptr<canvas_points_t> points(canvas_points_t::create(5));
  g_assert_true(points);

  points->coords()[0] = 0;
  points->coords()[1] = 200;
  points->coords()[2] = 200;
  points->coords()[3] = 400;
  points->coords()[4] = 400;
  points->coords()[5] = 200;
  points->coords()[6] = 200;
  points->coords()[7] = 0;
  points->coords()[8] = points->coords()[0];
  points->coords()[9] = points->coords()[1];

  std::unique_ptr<canvas_item_t> line(canvas->polygon_new(CANVAS_GROUP_WAYS, points.get(), 1, 0, 0));
  g_assert_true(line);

  canvas_item_t *search = canvas->get_item_at(200, 200);
  g_assert(line.get() == search);

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
