#include <canvas.h>

#include <misc.h>

bool testSegment()
{
  bool ret = true;

  canvas_points_t * const points = canvas_points_t::create(4);
  g_assert_nonnull(points);

  for (unsigned int i = 0; i < 8; i++)
    points->coords[i] = 1 << i;

  canvas_t * const canvas = canvas_t::create();
  g_assert_nonnull(canvas);

  canvas_item_t * const line = canvas->polyline_new(CANVAS_GROUP_WAYS, points, 1, 0);
  g_assert_nonnull(line);

  points->free();

  canvas_points_t * const seg = canvas_item_get_segment(line, 1);
  g_assert_nonnull(seg);
  g_assert_cmpint(seg->coords[0] , ==, 4);
  g_assert_cmpint(seg->coords[1] , ==, 8);
  g_assert_cmpint(seg->coords[2] , ==, 16);
  g_assert_cmpint(seg->coords[3] , ==, 32);

  seg->free();
  canvas_item_destroy(line);
  delete canvas;

  return ret;
}

int main()
{
  bool ret = true;

  ret &= testSegment();

  return ret ? 0 : 1;
}
