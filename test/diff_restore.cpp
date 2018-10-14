#include <appdata.h>
#include <diff.h>
#include <icon.h>
#include <map.h>
#include <misc.h>
#include <osm.h>
#include <project.h>
#include <xml_helpers.h>

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include <osm2go_platform.h>

void appdata_t::track_clear()
{
  assert_unreachable();
}

static void verify_diff(osm_t::ref osm)
{
  assert_cmpnum(12, osm->nodes.size());
  assert_cmpnum(3, osm->ways.size());
  assert_cmpnum(4, osm->relations.size());

  // new tag added in diff
  const node_t * const n72 = osm->node_by_id(638499572);
  assert(n72 != nullptr);
  assert_cmpnum(n72->flags, OSM_FLAG_DIRTY);
  assert(n72->tags.get_value("testtag") != nullptr);
  assert_cmpnum(n72->tags.asMap().size(), 5);
  // in diff, but the same as in .osm
  const node_t * const n23 = osm->node_by_id(3577031223LL);
  assert(n23 != nullptr);
  assert_cmpnum(n23->flags, 0);
  assert(n23->tags.empty());
  // deleted in diff
  const node_t * const n26 = osm->node_by_id(3577031226LL);
  assert(n26 != nullptr);
  assert(n26->isDeleted());
  assert_cmpnum(n26->flags, OSM_FLAG_DELETED);
  const way_t * const w = osm->way_by_id(351899455);
  assert(w != nullptr);
  assert(w->isDeleted());
  assert_cmpnum(w->user, 53064);
  assert(osm->users.find(53064) != osm->users.end());
  assert(osm->users[53064] == "Dakon");
  // added in diff
  const node_t * const nn1 = osm->node_by_id(-1);
  assert(nn1 != nullptr);
  assert_cmpnum(nn1->pos.lat, 52.2693518);
  assert_cmpnum(nn1->pos.lon, 9.576014);
  assert(nn1->tags.empty());
  // added in diff, same position as existing node
  const node_t * const nn2 = osm->node_by_id(-2);
  assert(nn2 != nullptr);
  assert_cmpnum(nn2->pos.lat, 52.269497);
  assert_cmpnum(nn2->pos.lon, 9.5752223);
  assert(nn2->tags.empty());
  // which is this one
  const node_t * const n27 = osm->node_by_id(3577031227LL);
  assert(n27 != nullptr);
  assert_cmpnum(n27->flags, 0);
  assert_cmpnum(nn2->pos.lat, n27->pos.lat);
  assert_cmpnum(nn2->pos.lon, n27->pos.lon);
  // the upstream version has "wheelchair", we have "source"
  // our modification must survive
  const way_t * const w452 = osm->way_by_id(351899452);
  assert(w452 != nullptr);
  assert(w452->tags.get_value("source") != nullptr);
  assert_null(w452->tags.get_value("wheelchair"));
  assert_cmpnum(w452->tags.asMap().size(), 3);
  const way_t * const w453 = osm->way_by_id(351899453);
  assert(w453 != nullptr);
  assert_cmpnum(w453->flags, 0);
  const relation_t * const r66316 = osm->relation_by_id(66316);
  assert(r66316 != nullptr);
  assert(r66316->isDeleted());
  assert_cmpnum(r66316->flags, OSM_FLAG_DELETED);
  const relation_t * const r255 = osm->relation_by_id(296255);
  assert(r255 != nullptr);
  assert_cmpnum(r255->flags, OSM_FLAG_DIRTY);
  assert_cmpnum(r255->members.size(), 164);
  const object_t r255m572(const_cast<node_t *>(n72));
  std::vector<member_t>::const_iterator r255it = r255->find_member_object(r255m572);
  r255it = r255->find_member_object(r255m572);
  assert(r255it != r255->members.end());
  assert(r255it->role != nullptr);
  assert_cmpstr(r255it->role, "forward_stop");
  assert_cmpnum(r255->tags.asMap().size(), 8);

  const relation_t * const r853 = osm->relation_by_id(5827853);
  assert(r853 != nullptr);
  assert_cmpnum(r853->flags, OSM_FLAG_DIRTY);
  for(std::vector<member_t>::const_iterator it = r853->members.begin(); it != r853->members.end(); it++)
    assert_cmpnum(it->object.type, object_t::RELATION_ID);

  assert(!osm->is_clean(true));
}

static void compare_with_file(const void *buf, size_t len, const char *fn)
{
  osm2go_platform::MappedFile fdata(fn);

  assert(fdata);
  assert_cmpnum(fdata.length(), len);

  assert_cmpmem(fdata.data(), fdata.length(), buf, len);
}

static void test_osmChange(osm_t::ref osm, const char *fn)
{
   xmlDocGuard doc(osmchange_init());
  const char *changeset = "42";

  osmchange_delete(osm->modified(), xmlDocGetRootElement(doc.get()), changeset);

  xmlChar *result;
  int len;
  xmlDocDumpFormatMemoryEnc(doc.get(), &result, &len, "UTF-8", 1);

  compare_with_file(result, len, fn);
  xmlFree(result);
}

int main(int argc, char **argv)
{
  int result = 0;

  if(argc != 4)
    return EINVAL;

  xmlInitParser();

  const std::string osm_path = argv[1];
  assert_cmpnum(osm_path[osm_path.size() - 1], '/');

  map_state_t dummystate;
  project_t project(dummystate, argv[2], osm_path);
  project.osmFile = argv[2] + std::string(".osm");

  bool b = project.parse_osm();
  if(!b) {
    std::cerr << "cannot open " << argv[1] << argv[2] << ": " << strerror(errno) << std::endl;
    return 1;
  }
  osm_t::ref osm = project.osm;
  assert(osm);

  assert_cmpnum(osm->uploadPolicy, osm_t::Upload_Blocked);
  assert_null(osm->sanity_check());

  const relation_t * const r255 = osm->relation_by_id(296255);
  assert(r255 != nullptr);
  assert_cmpnum(r255->flags, 0);
  assert_cmpnum(r255->members.size(), 165);
  assert_cmpnum(r255->tags.asMap().size(), 8);
  const node_t * const n72 = osm->node_by_id(638499572);
  assert_cmpnum(n72->tags.asMap().size(), 4);
  const object_t r255m572(const_cast<node_t *>(n72));
  std::vector<member_t>::const_iterator r255it = r255->find_member_object(r255m572);
  assert(r255it != r255->members.end());
  assert(r255it->role != nullptr);
  assert_cmpstr(r255it->role, "stop");
  const relation_t * const r66316 = osm->relation_by_id(66316);
  assert(r66316 != nullptr);
  object_t rmember(object_t::RELATION_ID, 296255);
  assert(!rmember.is_real());
  const std::vector<member_t>::const_iterator r66316it = r66316->find_member_object(rmember);
  assert(r66316it != r66316->members.end());
  // the child relation exists, so it should be stored as real ref
  assert(r66316it->object.is_real());

  assert_cmpnum(10, osm->nodes.size());
  assert_cmpnum(3, osm->ways.size());
  assert_cmpnum(4, osm->relations.size());

  assert(osm->is_clean(true));

  assert(project.diff_file_present());
  unsigned int flags = project.diff_restore();
  assert_cmpnum(flags, DIFF_RESTORED | DIFF_HAS_HIDDEN);

  verify_diff(osm);

  xmlString rel_str(r255->generate_xml("42"));
  printf("%s\n", rel_str.get());

  rel_str.reset(n72->generate_xml("42"));
  printf("%s\n", rel_str.get());

  char tmpdir[] = "/tmp/osm2go-diff_restore-XXXXXX";

  if(mkdtemp(tmpdir) == nullptr) {
    std::cerr << "cannot create temporary directory" << std::endl;
    result = 1;
  } else {
    // create an empty directoy
    std::string bpath = tmpdir + std::string("/") + argv[2];
    const std::string osmpath = bpath + '/' + argv[2] + ".osm";
    mkdir(bpath.c_str(), 0755);
    bpath.erase(bpath.rfind('/') + 1);
    // and create a new project from that
    std::unique_ptr<project_t> sproject(new project_t(dummystate, argv[2], bpath));
    // CAUTION: osm is shared between the projects now
    sproject->osm.reset(osm.get());

    // the directory is empty, there can't be any diff
    flags = sproject->diff_restore();
    assert_cmpnum(flags, DIFF_NONE_PRESENT);
    // should not do anything bad
    diff_restore(sproject, nullptr);

    sproject->diff_save();
    bpath += argv[2];
    std::string bdiff = bpath;
    std::string no_diff = bpath;
    bpath += '/';
    bpath += argv[2];
    bpath += '.';
    bpath += "diff";

    bdiff += "/backup.diff";
    assert(sproject->diff_file_present());
    assert_cmpnum(rename(bpath.c_str(), bdiff.c_str()), 0);
    // having backup.diff should still count as being present
    assert(sproject->diff_file_present());
    no_diff += "/no.diff";
    assert_cmpnum(rename(bdiff.c_str(), no_diff.c_str()), 0);
    assert(!sproject->diff_file_present());

    // saving without OSM data should just do nothing
    sproject->osm.release();
    // CAUTION: end of sharing
    sproject->diff_save();
    assert(!sproject->diff_file_present());

    // put the OSM data into this directory
    const std::string origosmpath = project.path + project.osmFile;
    symlink(origosmpath.c_str(), osmpath.c_str());
    sproject->osmFile = project.osmFile;
    bool pvalid = sproject->parse_osm();
    assert(pvalid);
    assert(sproject->osm);

    // now create a diff file dummy
    fdguard fd(open(bpath.c_str(), O_CREAT | O_WRONLY | O_CLOEXEC, 0600));
    assert(fd.valid());
    {
      fdguard none(-1);
      none.swap(fd);
    }
    assert(sproject->diff_file_present());
    sproject->diff_save();
    assert(!sproject->diff_file_present());

    assert_cmpnum(rename(no_diff.c_str(), bdiff.c_str()), 0);
    flags = sproject->diff_restore();
    assert_cmpnum(flags, DIFF_RESTORED | DIFF_HAS_HIDDEN);

    verify_diff(osm);

    unlink(osmpath.c_str());
    unlink(bdiff.c_str());
    bpath.erase(bpath.rfind('/'));
    rmdir(bpath.c_str());
    bpath.erase(bpath.rfind('/'));
    rmdir(bpath.c_str());
  }

  test_osmChange(osm, argv[3]);

  xmlCleanupParser();

  return result;
}

void appdata_t::main_ui_enable()
{
  assert_unreachable();
}
