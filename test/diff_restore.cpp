#include <diff.h>
#include <osm.h>
#include <project.h>

#include <osm2go_cpp.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <gtk/gtk.h>
#include <iostream>

int main(int argc, char **argv)
{
  if(argc != 3)
    return EINVAL;

  xmlInitParser();

  struct icon_t *icons = O2G_NULLPTR;
  std::string osm_path = argv[1];
  g_assert(osm_path[osm_path.size() - 1] == '/');
  osm_path += argv[2];
  osm_path += "/";
  osm_path += argv[2];
  osm_path += ".osm";
  osm_t *osm = osm_parse(std::string(), osm_path.c_str(), &icons);
  if(!osm) {
    std::cerr << "cannot open " << argv[1] << argv[2] << ": " << strerror(errno) << std::endl;
    return 1;
  }

  g_assert_cmpuint(10, ==, osm->nodes.size());
  g_assert_cmpuint(3, ==, osm->ways.size());
  g_assert_cmpuint(3, ==, osm->relations.size());

  g_assert(diff_is_clean(osm, true));

  project_t project(argv[2], argv[1]);
  diff_restore(O2G_NULLPTR, &project, osm);

  g_assert_cmpuint(12, ==, osm->nodes.size());
  g_assert_cmpuint(3, ==, osm->ways.size());
  g_assert_cmpuint(3, ==, osm->relations.size());

  // new tag added in diff
  const node_t * const n72 = osm->nodes[638499572];
  g_assert(n72 != O2G_NULLPTR);
  g_assert((n72->flags & OSM_FLAG_DIRTY) != 0);
  g_assert(n72->tags.get_value("testtag") != O2G_NULLPTR);
  // in diff, but the same as in .osm
  const node_t * const n23 = osm->nodes[3577031223LL];
  g_assert(n23 != O2G_NULLPTR);
  g_assert((n23->flags & OSM_FLAG_DIRTY) == 0);
  // deleted in diff
  const way_t * const w = osm->ways[351899455];
  g_assert(w != O2G_NULLPTR);
  g_assert((w->flags & OSM_FLAG_DELETED) != 0);
  // added in diff
  const node_t * const nn1 = osm->nodes[-1];
  g_assert(nn1 != O2G_NULLPTR);
  g_assert_cmpfloat(nn1->pos.lat, ==, 52.2693518);
  g_assert_cmpfloat(nn1->pos.lon, ==, 9.5760140);
  // added in diff, same position as existing node
  const node_t * const nn2 = osm->nodes[-2];
  g_assert(nn2 != O2G_NULLPTR);
  g_assert_cmpfloat(nn2->pos.lat, ==, 52.269497);
  g_assert_cmpfloat(nn2->pos.lon, ==, 9.5752223);
  // which is this one
  const node_t * const n27 = osm->nodes[3577031227LL];
  g_assert(n27 != O2G_NULLPTR);
  g_assert((n27->flags & OSM_FLAG_DIRTY) == 0);
  g_assert_cmpfloat(nn2->pos.lat, ==, n27->pos.lat);
  g_assert_cmpfloat(nn2->pos.lon, ==, n27->pos.lon);
  // the upstream version has "wheelchair", we have "source"
  // our modification must survive
  const way_t * const w452 = osm->ways[351899452];
  g_assert(w452 != NULL);
  g_assert(w452->tags.get_value("source") != NULL);
  g_assert(w452->tags.get_value("wheelchair") == NULL);

  g_assert(!diff_is_clean(osm, true));

  delete osm;

  xmlCleanupParser();

  return 0;
}

void main_ui_enable(appdata_t *)
{
  abort();
}
