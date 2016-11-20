#include <appdata.h>
#include <track.h>

#include <errno.h>
#include <gtk/gtk.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
  appdata_t appdata;
  project_t project;

  if(argc != 4)
    return EINVAL;

  memset(&appdata, 0, sizeof(appdata));
  memset(&project, 0, sizeof(project));
  appdata.project = &project;
  project.path = argv[1];
  project.name = argv[2];

  appdata.track.track = track_restore(&appdata);

  track_export(&appdata, argv[3]);

  track_clear(&appdata);

  gchar *fn = g_strconcat(argv[1], argv[2], ".trk", NULL);
  GMappedFile *ogpx = g_mapped_file_new(fn, FALSE, NULL);
  g_free(fn);
  GMappedFile *ngpx = g_mapped_file_new(argv[3], FALSE, NULL);

  g_assert(ogpx != NULL);
  g_assert(ngpx != NULL);
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

  return 0;
}
