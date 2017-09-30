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

  int i, j, k, l;

  canvas_item_get_segment_pos(line, 1, i, j, k, l);
  g_assert_cmpint(i, ==, 4);
  g_assert_cmpint(j, ==, 8);
  g_assert_cmpint(k, ==, 16);
  g_assert_cmpint(l, ==, 32);

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
