#include <diff.h>
#include <misc.h>
#include <osm.h>
#include <project.h>

#include <osm2go_cpp.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <gtk/gtk.h>
#include <iostream>

static void verify_diff(osm_t *osm)
{
  g_assert_cmpuint(12, ==, osm->nodes.size());
  g_assert_cmpuint(3, ==, osm->ways.size());
  g_assert_cmpuint(3, ==, osm->relations.size());

  // new tag added in diff
  const node_t * const n72 = osm->nodes[638499572];
  g_assert_nonnull(n72);
  g_assert((n72->flags & OSM_FLAG_DIRTY) != 0);
  g_assert_nonnull(n72->tags.get_value("testtag"));
  g_assert_cmpuint(n72->tags.asMap().size(), ==, 5);
  // in diff, but the same as in .osm
  const node_t * const n23 = osm->nodes[3577031223LL];
  g_assert_nonnull(n23);
  g_assert_cmpuint((n23->flags & OSM_FLAG_DIRTY), ==, 0);
  g_assert_true(n23->tags.empty());
  // deleted in diff
  const node_t * const n26 = osm->nodes[3577031226LL];
  g_assert_nonnull(n26);
  g_assert_cmpuint(n26->flags, ==, OSM_FLAG_DELETED);
  const way_t * const w = osm->ways[351899455];
  g_assert_nonnull(w);
  g_assert((w->flags & OSM_FLAG_DELETED) != 0);
  // added in diff
  const node_t * const nn1 = osm->nodes[-1];
  g_assert_nonnull(nn1);
  g_assert_cmpfloat(nn1->pos.lat, ==, 52.2693518);
  g_assert_cmpfloat(nn1->pos.lon, ==, 9.5760140);
  g_assert_true(nn1->tags.empty());
  // added in diff, same position as existing node
  const node_t * const nn2 = osm->nodes[-2];
  g_assert_nonnull(nn2);
  g_assert_cmpfloat(nn2->pos.lat, ==, 52.269497);
  g_assert_cmpfloat(nn2->pos.lon, ==, 9.5752223);
  g_assert_true(nn2->tags.empty());
  // which is this one
  const node_t * const n27 = osm->nodes[3577031227LL];
  g_assert_nonnull(n27);
  g_assert((n27->flags & OSM_FLAG_DIRTY) == 0);
  g_assert_cmpfloat(nn2->pos.lat, ==, n27->pos.lat);
  g_assert_cmpfloat(nn2->pos.lon, ==, n27->pos.lon);
  // the upstream version has "wheelchair", we have "source"
  // our modification must survive
  const way_t * const w452 = osm->ways[351899452];
  g_assert_nonnull(w452);
  g_assert_nonnull(w452->tags.get_value("source"));
  g_assert_null(w452->tags.get_value("wheelchair"));
  g_assert_cmpuint(w452->tags.asMap().size(), ==, 3);
  const way_t * const w453 = osm->ways[351899453];
  g_assert_nonnull(w453);
  g_assert_cmpuint(w453->flags, ==, 0);
  const relation_t * const r66316 = osm->relations[66316];
  g_assert_nonnull(r66316);
  g_assert_cmpuint(r66316->flags, ==, OSM_FLAG_DELETED);
  const relation_t * const r255 = osm->relations[296255];
  g_assert_nonnull(r255);
  g_assert_cmpuint(r255->flags & OSM_FLAG_DIRTY, ==, OSM_FLAG_DIRTY);
  g_assert_cmpuint(r255->members.size(), ==, 164);
  const object_t r255m572(const_cast<node_t *>(n72));
  std::vector<member_t>::const_iterator r255it = r255->find_member_object(r255m572);
  r255it = r255->find_member_object(r255m572);
  g_assert(r255it != r255->members.end());
  g_assert(r255it->role != 0);
  g_assert_cmpint(strcmp(r255it->role, "forward_stop"), ==, 0);
  g_assert_cmpuint(r255->tags.asMap().size(), ==, 8);

  g_assert_false(diff_is_clean(osm, true));
}

int main(int argc, char **argv)
{
  int result = 0;

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
  osm_t *osm = osm_t::parse(std::string(), osm_path.c_str(), &icons);
  if(!osm) {
    std::cerr << "cannot open " << argv[1] << argv[2] << ": " << strerror(errno) << std::endl;
    return 1;
  }

  const relation_t * const r255 = osm->relations[296255];
  g_assert_nonnull(r255);
  g_assert_cmpuint(r255->flags & OSM_FLAG_DIRTY, ==, 0);
  g_assert_cmpuint(r255->members.size(), ==, 165);
  g_assert_cmpuint(r255->tags.asMap().size(), ==, 8);
  const node_t * const n72 = osm->nodes[638499572];
  g_assert_cmpuint(n72->tags.asMap().size(), ==, 4);
  const object_t r255m572(const_cast<node_t *>(n72));
  std::vector<member_t>::const_iterator r255it = r255->find_member_object(r255m572);
  g_assert(r255it != r255->members.end());
  g_assert(r255it->role != 0);
  g_assert_cmpint(strcmp(r255it->role, "stop"), ==, 0);

  g_assert_cmpuint(10, ==, osm->nodes.size());
  g_assert_cmpuint(3, ==, osm->ways.size());
  g_assert_cmpuint(3, ==, osm->relations.size());

  g_assert_true(diff_is_clean(osm, true));

  project_t project(argv[2], argv[1]);
  diff_restore(O2G_NULLPTR, &project, osm);

  verify_diff(osm);

  xmlChar *rel_str = r255->generate_xml(42);
  printf("%s\n", rel_str);
  xmlFree(rel_str);

  char tmpdir[] = "/tmp/osm2go-diff_restore-XXXXXX";

  if(mkdtemp(tmpdir) == O2G_NULLPTR) {
    std::cerr << "cannot create temporary directory" << std::endl;
    result = 1;
  } else {
    std::string bpath = tmpdir + std::string("/") + argv[2];
    mkdir(bpath.c_str(), 0755);
    bpath.erase(bpath.rfind('/') + 1);
    project_t sproject(argv[2], bpath.c_str());

    diff_restore(O2G_NULLPTR, &sproject, osm);

    diff_save(&sproject, osm);
    bpath += argv[2];
    std::string bdiff = bpath;
    bpath += '/';
    bpath += argv[2];
    bpath += '.';
    bpath += "diff";

    bdiff += "/backup.diff";
    rename(bpath.c_str(), bdiff.c_str());

    delete osm;
    osm = osm_t::parse(std::string(), osm_path.c_str(), &icons);
    g_assert_nonnull(osm);

    diff_restore(O2G_NULLPTR, &sproject, osm);

    verify_diff(osm);

    unlink(bdiff.c_str());
    bpath.erase(bpath.rfind('/'));
    rmdir(bpath.c_str());
    bpath.erase(bpath.rfind('/'));
    rmdir(bpath.c_str());
  }

  delete osm;

  xmlCleanupParser();

  return result;
}

void main_ui_enable(appdata_t *)
{
  abort();
}
