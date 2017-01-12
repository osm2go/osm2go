#include <diff.h>
#include <osm.h>

#include <cerrno>
#include <cstdlib>
#include <gtk/gtk.h>
#include <iostream>

int main(int argc, char **argv)
{
  if(argc != 5)
    return EINVAL;

  struct icon_t *icons = 0;
  osm_t *osm = osm_parse(argv[1], argv[2], &icons);
  if(!osm) {
    std::cerr << "cannot open " << argv[1] << argv[2] << ": " << strerror(errno) << std::endl;
    return 1;
  }

  g_assert_cmpuint(6, ==, osm->nodes.size());
  g_assert_cmpuint(1, ==, osm->ways.size());
  g_assert_cmpuint(3, ==, osm->relations.size());

  project_t project(argv[4]);
  project.path = g_strdup(argv[3]);
  diff_restore(0, &project, osm);

  g_assert_cmpuint(8, ==, osm->nodes.size());
  g_assert_cmpuint(1, ==, osm->ways.size());
  g_assert_cmpuint(3, ==, osm->relations.size());

  // new tag added in diff
  const node_t * const n72 = osm->nodes[638499572];
  g_assert(n72 != 0);
  g_assert((n72->flags & OSM_FLAG_DIRTY) != 0);
  g_assert(n72->get_value("testtag") != 0);
  g_assert(n72->has_value("true"));
  // in diff, but the same as in .osm
  const node_t * const n23 = osm->nodes[3577031223];
  g_assert(n23 != 0);
  g_assert((n23->flags & OSM_FLAG_DIRTY) == 0);
  // deleted in diff
  const way_t * const w = osm->ways[351899455];
  g_assert(w != 0);
  g_assert((w->flags & OSM_FLAG_DELETED) != 0);
  // added in diff
  const node_t * const nn1 = osm->nodes[-1];
  g_assert(nn1 != 0);
  g_assert_cmpfloat(nn1->pos.lat, ==, 52.2693518);
  g_assert_cmpfloat(nn1->pos.lon, ==, 9.5760140);
  // added in diff, same position as existing node
  const node_t * const nn2 = osm->nodes[-2];
  g_assert(nn2 != 0);
  g_assert_cmpfloat(nn2->pos.lat, ==, 52.269497);
  g_assert_cmpfloat(nn2->pos.lon, ==, 9.5752223);
  // which is this one
  const node_t * const n27 = osm->nodes[3577031227];
  g_assert(n27 != 0);
  g_assert((n27->flags & OSM_FLAG_DIRTY) == 0);
  g_assert_cmpfloat(nn2->pos.lat, ==, n27->pos.lat);
  g_assert_cmpfloat(nn2->pos.lon, ==, n27->pos.lon);

  osm_free(osm);

  return 0;
}

void main_ui_enable(appdata_t *appdata)
{
  abort();
}
