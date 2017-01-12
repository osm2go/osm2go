#include <track.h>

#include <algorithm>
#include <cerrno>
#include <gtk/gtk.h>

struct point_count {
  gint &points;
  point_count(gint &p) : points(p) {}
  void operator()(const track_seg_t &seg) {
    points += seg.track_points.size();
  }
};

int main(int argc, char **argv)
{
  if(argc != 2)
    return EINVAL;

  xmlInitParser();

  track_t *track = track_import(argv[1]);

  gint points = 0;

  std::for_each(track->segments.begin(), track->segments.end(), point_count(points));

  g_assert_cmpint(track->segments.size(), ==, 4);

  delete track;

  g_assert_cmpint(points, ==, 11);

  xmlCleanupParser();

  return 0;
}
