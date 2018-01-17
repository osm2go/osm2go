#include <track.h>

#include <misc.h>
#include <osm2go_annotations.h>
#include <osm2go_cpp.h>

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <glib.h>
#include <libxml/parser.h>
#include <string>

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

  g_mapped_file ogpx(g_mapped_file_new(fn.c_str(), FALSE, O2G_NULLPTR));
  g_mapped_file ngpx(g_mapped_file_new(argv[3], FALSE, O2G_NULLPTR));

  assert(ogpx);
  assert(ngpx);
  assert_cmpmem(g_mapped_file_get_contents(ogpx.get()), g_mapped_file_get_length(ogpx.get()),
                g_mapped_file_get_contents(ngpx.get()), g_mapped_file_get_length(ngpx.get()));

  xmlCleanupParser();

  return 0;
}
