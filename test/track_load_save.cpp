#include <track.h>

#include <cassert>
#include <cerrno>
#include <libxml/parser.h>
#include <string>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include <osm2go_platform.h>

int main(int argc, char **argv)
{
  if(argc != 4)
    return EINVAL;

  std::string fn = argv[1];
  fn += argv[2];
  fn += ".trk";

  xmlInitParser();

  track_t *track = track_import(fn.c_str());

  track_export(track, argv[3]);

  delete track;

  osm2go_platform::MappedFile ogpx(fn.c_str());
  osm2go_platform::MappedFile ngpx(argv[3]);

  assert(ogpx);
  assert(ngpx);
  assert_cmpmem(ogpx.data(), ogpx.length(), ngpx.data(), ngpx.length());

  xmlCleanupParser();

  return 0;
}
