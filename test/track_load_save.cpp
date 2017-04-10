#include <track.h>

#include <osm2go_cpp.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <gtk/gtk.h>
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

  GMappedFile *ogpx = g_mapped_file_new(fn.c_str(), FALSE, O2G_NULLPTR);
  GMappedFile *ngpx = g_mapped_file_new(argv[3], FALSE, O2G_NULLPTR);

  g_assert(ogpx != O2G_NULLPTR);
  g_assert(ngpx != O2G_NULLPTR);
  g_assert_cmpuint(g_mapped_file_get_length(ogpx), ==,
                   g_mapped_file_get_length(ngpx));

  g_assert_cmpint(memcmp(g_mapped_file_get_contents(ogpx),
                         g_mapped_file_get_contents(ngpx),
                         g_mapped_file_get_length(ogpx)), ==, 0);

#if GLIB_CHECK_VERSION(2,22,0)
  g_mapped_file_unref(ogpx);
  g_mapped_file_unref(ngpx);
#else
  g_mapped_file_free(ogpx);
  g_mapped_file_free(ngpx);
#endif

  xmlCleanupParser();

  return 0;
}
