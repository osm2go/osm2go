#include <track.h>

#include <errno.h>
#include <gtk/gtk.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
  if(argc != 2)
    return EINVAL;

  track_t *track = track_import(argv[1]);

  gint segs = 0;
  gint points = 0;

  const track_seg_t *seg;
  for (seg = track->track_seg; seg != NULL; seg = seg->next, segs++) {
    points += track_points_count(seg->track_point);
  }

  track_delete(track);

  g_assert_cmpint(segs, ==, 4);
  g_assert_cmpint(points, ==, 11);

  return 0;
}
