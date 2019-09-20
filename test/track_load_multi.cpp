#include <track.h>

#include <osm2go_annotations.h>

#include <algorithm>
#include <cerrno>
#include <libxml/parser.h>

namespace {

struct point_count {
  unsigned int &points;
  explicit point_count(unsigned int &p) : points(p) {}
  void operator()(const track_seg_t &seg) {
    points += seg.track_points.size();
  }
};

}

int main(int argc, char **argv)
{
  if(argc != 2)
    return EINVAL;

  xmlInitParser();

  track_t *track = track_import(argv[1]);

  unsigned int points = 0;

  std::for_each(track->segments.begin(), track->segments.end(), point_count(points));

  assert_cmpnum(track->segments.size(), 4);

  delete track;

  assert_cmpnum(points, 11);

  xmlCleanupParser();

  return 0;
}

#include "appdata_dummy.h"
