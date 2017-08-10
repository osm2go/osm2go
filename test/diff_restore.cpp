#include <appdata.h>
#include <diff.h>
#include <icon.h>
#include <map.h>
#include <misc.h>
#include <osm.h>
#include <project.h>

#include <osm2go_cpp.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <gtk/gtk.h>
#include <iostream>
#include <sys/stat.h>

appdata_t::appdata_t()
  : window(O2G_NULLPTR)
#ifdef USE_HILDON
  , osso_context(O2G_NULLPTR)
#endif
  , statusbar(O2G_NULLPTR)
  , settings(O2G_NULLPTR)
  , gps_state(O2G_NULLPTR)
{
}

appdata_t::~appdata_t()
{
}

static void verify_diff(osm_t *osm)
{
  g_assert_cmpuint(12, ==, osm->nodes.size());
  g_assert_cmpuint(3, ==, osm->ways.size());
  g_assert_cmpuint(4, ==, osm->relations.size());

  // new tag added in diff
  const node_t * const n72 = osm->nodes[638499572];
  g_assert_nonnull(n72);
  g_assert_cmpuint(n72->flags, ==, OSM_FLAG_DIRTY);
  g_assert_nonnull(n72->tags.get_value("testtag"));
  g_assert_cmpuint(n72->tags.asMap().size(), ==, 5);
  // in diff, but the same as in .osm
  const node_t * const n23 = osm->nodes[3577031223LL];
  g_assert_nonnull(n23);
  g_assert_cmpuint(n23->flags, ==, 0);
  g_assert_true(n23->tags.empty());
  // deleted in diff
  const node_t * const n26 = osm->nodes[3577031226LL];
  g_assert_nonnull(n26);
  g_assert_cmpuint(n26->flags, ==, OSM_FLAG_DELETED);
  const way_t * const w = osm->ways[351899455];
  g_assert_nonnull(w);
  g_assert((w->flags & OSM_FLAG_DELETED) != 0);
  g_assert_cmpint(w->user, ==, 53064);
  g_assert(osm->users.find(53064) != osm->users.end());
  g_assert(osm->users[53064] == "Dakon");
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
  g_assert_cmpuint(n27->flags, ==, 0);
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
  g_assert_cmpuint(r255->flags, ==, OSM_FLAG_DIRTY);
  g_assert_cmpuint(r255->members.size(), ==, 164);
  const object_t r255m572(const_cast<node_t *>(n72));
  std::vector<member_t>::const_iterator r255it = r255->find_member_object(r255m572);
  r255it = r255->find_member_object(r255m572);
  g_assert(r255it != r255->members.end());
  g_assert_nonnull(r255it->role);
  g_assert_cmpint(strcmp(r255it->role, "forward_stop"), ==, 0);
  g_assert_cmpuint(r255->tags.asMap().size(), ==, 8);

  const relation_t * const r853 = osm->relations[5827853];
  g_assert_nonnull(r853);
  g_assert_cmpuint(r853->flags, ==, OSM_FLAG_DIRTY);
  for(std::vector<member_t>::const_iterator it = r853->members.begin(); it != r853->members.end(); it++)
    g_assert(it->object.type == RELATION_ID);

  g_assert_false(diff_is_clean(osm, true));
}

static void compare_with_file(const void *buf, size_t len, const char *fn)
{
  GMappedFile *fdata = g_mapped_file_new(fn, FALSE, O2G_NULLPTR);

  g_assert_nonnull(fdata);
  g_assert_cmpuint(g_mapped_file_get_length(fdata), ==, len);

  g_assert_cmpint(memcmp(g_mapped_file_get_contents(fdata),
                         buf,
                         g_mapped_file_get_length(fdata)), ==, 0);

#if GLIB_CHECK_VERSION(2,22,0)
  g_mapped_file_unref(fdata);
#else
  g_mapped_file_free(fdata);
#endif
}

static void test_osmChange(const osm_t *osm, const char *fn)
{
  xmlDocPtr doc = osmchange_init();
  const item_id_t changeset = 42;

  osmchange_delete(osm, xmlDocGetRootElement(doc), changeset);

  xmlChar *result;
  int len;
  xmlDocDumpFormatMemoryEnc(doc, &result, &len, "UTF-8", 1);
  xmlFreeDoc(doc);

  compare_with_file(result, len, fn);
  xmlFree(result);
}

int main(int argc, char **argv)
{
  int result = 0;

  if(argc != 4)
    return EINVAL;

  xmlInitParser();

  icon_t icons;
  const std::string osm_path = argv[1];
  g_assert(osm_path[osm_path.size() - 1] == '/');

  map_state_t dummystate;
  project_t project(dummystate, argv[2], osm_path);
  project.osm = argv[2] + std::string(".osm");

  osm_t *osm = project.parse_osm(icons);
  if(!osm) {
    std::cerr << "cannot open " << argv[1] << argv[2] << ": " << strerror(errno) << std::endl;
    return 1;
  }

  g_assert_cmpint(osm->uploadPolicy, ==, osm_t::Upload_Blocked);
  g_assert(osm->sanity_check() == O2G_NULLPTR);

  const relation_t * const r255 = osm->relations[296255];
  g_assert_nonnull(r255);
  g_assert_cmpuint(r255->flags, ==, 0);
  g_assert_cmpuint(r255->members.size(), ==, 165);
  g_assert_cmpuint(r255->tags.asMap().size(), ==, 8);
  const node_t * const n72 = osm->nodes[638499572];
  g_assert_cmpuint(n72->tags.asMap().size(), ==, 4);
  const object_t r255m572(const_cast<node_t *>(n72));
  std::vector<member_t>::const_iterator r255it = r255->find_member_object(r255m572);
  g_assert(r255it != r255->members.end());
  g_assert_nonnull(r255it->role);
  g_assert_cmpint(strcmp(r255it->role, "stop"), ==, 0);
  const relation_t * const r66316 = osm->relations[66316];
  g_assert_nonnull(r66316);
  object_t rmember(RELATION_ID, 296255);
  g_assert_false(rmember.is_real());
  const std::vector<member_t>::const_iterator r66316it = r66316->find_member_object(rmember);
  g_assert(r66316it != r66316->members.end());
  // the child relation exists, so it should be stored as real ref
  g_assert_true(r66316it->object.is_real());

  g_assert_cmpuint(10, ==, osm->nodes.size());
  g_assert_cmpuint(3, ==, osm->ways.size());
  g_assert_cmpuint(4, ==, osm->relations.size());

  g_assert_true(diff_is_clean(osm, true));

  g_assert_true(diff_present(&project));
  unsigned int flags = diff_restore_file(O2G_NULLPTR, &project, osm);
  g_assert_cmpuint(flags, ==, DIFF_RESTORED | DIFF_HAS_HIDDEN);

  verify_diff(osm);

  xmlChar *rel_str = r255->generate_xml("42");
  printf("%s\n", rel_str);
  xmlFree(rel_str);

  rel_str = n72->generate_xml("42");
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
    project_t sproject(dummystate, argv[2], bpath.c_str());

    flags = diff_restore_file(O2G_NULLPTR, &sproject, osm);
    g_assert_cmpuint(flags, ==, DIFF_NONE_PRESENT);

    diff_save(&sproject, osm);
    bpath += argv[2];
    std::string bdiff = bpath;
    bpath += '/';
    bpath += argv[2];
    bpath += '.';
    bpath += "diff";

    bdiff += "/backup.diff";
    g_assert_true(diff_present(&sproject));
    rename(bpath.c_str(), bdiff.c_str());
    g_assert_false(diff_present(&sproject));

    delete osm;
    osm = osm_t::parse(project.path, project.osm, icons);
    g_assert_nonnull(osm);

    flags = diff_restore_file(O2G_NULLPTR, &sproject, osm);
    g_assert_cmpuint(flags, ==, DIFF_RESTORED | DIFF_HAS_HIDDEN);

    verify_diff(osm);

    unlink(bdiff.c_str());
    bpath.erase(bpath.rfind('/'));
    rmdir(bpath.c_str());
    bpath.erase(bpath.rfind('/'));
    rmdir(bpath.c_str());
  }

  test_osmChange(osm, argv[3]);

  delete osm;

  xmlCleanupParser();

  return result;
}

void main_ui_enable(appdata_t &)
{
  g_assert_not_reached();
}
