#define _FILE_OFFSET_BITS 64

#include <project.h>
#include <project_p.h>

#include <appdata.h>
#include <diff.h>
#include <fdguard.h>
#include <gps_state.h>
#include <icon.h>
#include <iconbar.h>
#include <josm_presets.h>
#include <map.h>
#include <style.h>
#include <uicontrol.h>
#include <wms.h>

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include <osm2go_platform.h>
#include <osm2go_test.h>

namespace {

class MainUiDummy : public MainUi {
public:
  MainUiDummy() : MainUi() {}
  void setActionEnable(menu_items, bool) override
  { abort(); }
  void showNotification(trstring::arg_type message, unsigned int flags) override;
  void clearNotification(MainUi::NotificationFlags) override
  { abort(); }
  std::vector<std::string> messages;
};

void MainUiDummy::showNotification(trstring::arg_type message, unsigned int)
{
  assert(!message.isEmpty());
  trstring::native_type nativeMsg = static_cast<trstring::native_type>(message);
  printf("%s: %s\n", __PRETTY_FUNCTION__, nativeMsg.toStdString().c_str());
  messages.push_back(nativeMsg.toStdString());
}

const char *proj_name = "test_proj";

void
testNoFiles(const std::string &tmpdir)
{
  appdata_t appdata;
  appdata.project.reset(new project_t(proj_name, tmpdir));

  assert(!track_restore(appdata));
  assert(!appdata.track.track);

  wms_remove_file(*appdata.project);

  const std::string pfile = tmpdir + '/' + std::string(proj_name) + ".proj";
  assert(!project_read(pfile, appdata.project, std::string(), -1));

  {
    fdguard fd(open(pfile.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR));
    assert_cmpnum_op(static_cast<int>(fd), >=, 0);
    const char *xml_minimal = "<a><b/></a>";
    write(fd, xml_minimal, strlen(xml_minimal));
  }
  fdguard empty(pfile.c_str(), O_RDONLY);
  assert(empty.valid());

  assert(!project_read(pfile, appdata.project, std::string(), -1));

  // no diff, name does not match
  std::unique_ptr<project_t> other(new project_t("other", tmpdir));
  assert(!other->activeOrDirty(appdata));
  // no diff, name does match, but still no changes
  std::unique_ptr<project_t> same(new project_t(proj_name, tmpdir));
  assert(!appdata.project->osm);
  assert(!same->activeOrDirty(appdata));
  // now with osm object, but still nothing changed
  appdata.project->osm.reset(new osm_t());
  assert(!same->activeOrDirty(appdata));
  assert_cmpstr(same->pendingChangesMessage(appdata), _("no pending changes"));
  // add something new to make it dirty
  appdata.project->osm->attach(new relation_t());
  assert(same->activeOrDirty(appdata));
  assert_cmpstr(same->pendingChangesMessage(appdata), _("unsaved changes pending"));

  appdata.project.reset();

  unlink(pfile.c_str());
}

void
testSave(const std::string &tmpdir, const std::string &readonly, const char *empty_proj)
{
  std::unique_ptr<project_t> project(std::make_unique<project_t>(proj_name, tmpdir));

  assert(project->save());

  const std::string &pfile = project_filename(*project);

  osm2go_platform::MappedFile empty(empty_proj);
  assert(empty);
  osm2go_platform::MappedFile proj(pfile);
  assert(proj);

  assert_cmpmem(empty.data(), empty.length(), proj.data(), proj.length());

  const std::array<const char *, 3> fnames = { { "wms.jpg", "wms.gif", "wms.png" } };
  for (unsigned int i = 0; i < fnames.size(); i++) {
    const char * const fname = fnames.at(i);
    {
      fdguard fd(openat(project->dirfd, fname, O_WRONLY | O_CREAT | O_EXCL));
      assert_cmpnum_op(static_cast<int>(fd), >=, 0);
    }
    struct stat st;
    int ret = fstatat(project->dirfd, fname, &st, 0);
    assert_cmpnum(ret, 0);
    wms_remove_file(*project);
    ret = fstatat(project->dirfd, fname, &st, 0);
    assert_cmpnum(ret, -1);
    assert_cmpnum(errno, ENOENT);
  }

  project_delete(project);

  project.reset(new project_t(proj_name, readonly));
  assert(project);
  assert(!project->save());
}

void
testNoData(const std::string &tmpdir)
{
  std::unique_ptr<project_t> project(std::make_unique<project_t>(proj_name, tmpdir));

  assert(project->save());

  const std::string &pfile = project_filename(*project);
  project_read(pfile, project, project->server(std::string()), -1);

  const std::string &ofile = project->osmFile;

  assert(!project->osm_file_exists());
  project_t::projectStatus status = project->status(false);
  assert(status.valid);
  assert(status.errorColor);
  assert_cmpstr(status.message, _("Not downloaded!"));
  assert_cmpstr(status.compressedMessage, _("Map data:"));

  const char *not_gzip = "<?xml version='1.0' encoding='UTF-8'?>\n<osm/>";
  {
    fdguard osmfd(openat(project->dirfd, ofile.c_str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR));
    assert_cmpnum_op(static_cast<int>(osmfd), >=, 0);

    bool b = project->parse_osm();
    assert(!b);

    write(osmfd, not_gzip, strlen(not_gzip));
  }

  assert(!project->check_demo(nullptr));
  assert(project->osm_file_exists());
  bool b = project->parse_osm();
  assert(!b);
  status = project->status(false);
  assert(status.valid);
  assert(!status.errorColor);
  assert(status.message.toStdString().find(std::to_string(strlen(not_gzip))) != std::string::npos);
  assert_cmpstr(status.compressedMessage, _("Map data:"));

  // add an empty directories to see if project_delete() also cleans those
  assert_cmpnum(mkdir((project->path + ".foo").c_str(), 0755), 0);
  assert_cmpnum(mkdirat(project->dirfd, ".bar", 0755), 0);

  project_delete(project);
}

void
testServer(const std::string &tmpdir)
{
  const std::string defaultserver = "https://api.openstreetmap.org/api/0.6";
  const std::string oldserver = "http://api.openstreetmap.org/api/0.5";
  project_t project(proj_name, tmpdir);

  assert_cmpstr(project.server(defaultserver), defaultserver);
  assert_cmpstr(project.server(oldserver), oldserver);
  assert(project.rserver.empty());

  project.adjustServer(defaultserver.c_str(), defaultserver);
  assert(project.rserver.empty());

  project.adjustServer(oldserver.c_str(), defaultserver);
  assert(!project.rserver.empty());
  assert_cmpstr(project.server(defaultserver), oldserver);

  project.adjustServer(nullptr, defaultserver);
  assert(project.rserver.empty());
  assert_cmpstr(project.server(defaultserver), defaultserver);

  project.adjustServer(oldserver.c_str(), defaultserver);
  assert(!project.rserver.empty());
  assert_cmpstr(project.server(defaultserver), oldserver);

  project.adjustServer("", defaultserver);
  assert(project.rserver.empty());
  assert_cmpstr(project.server(defaultserver), defaultserver);
}

void
helper_createOsm(project_t::ref project, const std::string &tmpdir, const char *data, size_t datalen)
{
  // save for the base directory
  assert(project->save());
  // copy the OSM data
  fdguard osmfd(open((tmpdir + proj_name + '/' + proj_name + ".osm").c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR));
  assert_cmpnum_op(static_cast<int>(osmfd), >=, 0);
  assert_cmpnum(write(osmfd, data, datalen), datalen);
}

void
testLoad(const std::string &tmpdir, osm2go_platform::MappedFile &osmfile)
{
  appdata_t appdata;

  // 3 attempts of loading, the first will fail because of missing OSM data
  const size_t loopcnt = 3;
  // loopcnt is doubled here, the excess loop iterations use the other overload
  // of project_load() but behave otherwise the same
  for (size_t i = loopcnt * 2; i > 0; i--) {
    fflush(stdout); // output readability
    // create dummy project
    std::unique_ptr<project_t> project(std::make_unique<project_t>(proj_name, tmpdir));
    project->bounds.min.lat = 0.5;
    project->bounds.min.lon = 0.5;
    project->bounds.max.lat = 0.6;
    project->bounds.max.lon = 0.6;

    assert(project->bounds == pos_area::normalized(project->bounds.min, project->bounds.max));
    assert(project->bounds == pos_area::normalized(project->bounds.max, project->bounds.min));
    assert(static_cast<bool>(osmfile));

    size_t msgs; // number of showNotification() messages this is expected to emit
    switch (i) {
    case 6:
    case 5:
      // these must come first, they expected that no .osm file is present
      msgs = 2;
      break;
    case 4:
    case 3:
      msgs = 2;
      // let it fail because of invalid bounds
      project->bounds.min.lat = 2;
      helper_createOsm(project, tmpdir, osmfile.data(), osmfile.length());
      break;
    case 2:
    case 1:
      msgs = 1;
      helper_createOsm(project, tmpdir, osmfile.data(), osmfile.length());
      break;
    }

    assert(project->save());
    const std::string fn = project_filename(*project);
    fflush(stdout); // output readability

    bool b;
    if(i > loopcnt) {
      // project_read() would have set this, so fill it here, too
      project->osmFile = std::string(proj_name) + ".osm";
      b = project_load(appdata, project);
    } else {
      project.reset();
      b = project_load(appdata, fn);
    }

    // either was empty before or was swapped to appdata
    assert(!project);

    // loading will fail because window is nullptr (and map also)
    assert(!b);

    // if cleared at the beginning, and again if loading failed
    assert(!appdata.project);

    MainUiDummy *uid = static_cast<MainUiDummy *>(appdata.uicontrol.get());
    assert_cmpnum(uid->messages.size(), msgs);
    assert(!appdata.project);

    const std::vector<std::string>::const_iterator itEnd = uid->messages.end();
    // every expected message contains the project name
    for(std::vector<std::string>::const_iterator it = uid->messages.begin(); it != itEnd; it++)
      assert(it->find(proj_name) != std::string::npos);
    uid->messages.clear();
  }

  std::unique_ptr<project_t> tmpproj(std::make_unique<project_t>(proj_name, tmpdir));
  project_delete(tmpproj);
}

void
testRename(const std::string &tmpdir, const char *diff_file)
{
  // wronly flagged as gzip
  const char *not_gzip = "<?xml version='1.0' encoding='UTF-8'?>\n<osm></osm>";

  // run 3 times, with different settings of the global project
  for (int i = 0; i < 3; i++) {
    std::unique_ptr<project_t> project(std::make_unique<project_t>("diff_restore_data", tmpdir));
    assert(project->save());
    project->osmFile = "diff_restore_data.osm.gz";
    const std::string oldpath = project->path;

    {
      fdguard osmfd(openat(project->dirfd, project->osmFile.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR));
      assert_cmpnum_op(static_cast<int>(osmfd), >=, 0);
      write(osmfd, not_gzip, strlen(not_gzip));

      project_t::projectStatus status = project->status(false);
      assert(status.valid);
      assert(!status.errorColor);
      assert_cmpstr(status.compressedMessage, _("Map data:\n(compressed)"));
    }

    {
    fdguard osmfd(openat(project->dirfd, (project->name + ".trk").c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR));
      assert_cmpnum_op(static_cast<int>(osmfd), >=, 0);
    }

    // use an already existing diff
    osm2go_platform::MappedFile mf(diff_file);
    assert(static_cast<bool>(mf));
    {
      appdata_t appdata;
      assert(!project->activeOrDirty(appdata));

      fdguard osmfd(openat(project->dirfd, (project->name + ".diff").c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR));
      assert_cmpnum_op(static_cast<int>(osmfd), >=, 0);
      assert_cmpnum(write(osmfd, mf.data(), mf.length()), mf.length());

      assert(project->diff_file_present());
      // any project with a diff file is dirty
      assert(project->activeOrDirty(appdata));
    }

    std::unique_ptr<project_t> global;
    switch (i) {
    case 0:
      // empty global
      break;
    case 1:
      // different global project
      global.reset(new project_t("unrelated", tmpdir));
      assert(global->save());
      break;
    case 2:
      // referencing the same project
      global.reset(new project_t(project->name, tmpdir));
      break;
    default:
      assert_unreachable();
    }

    assert(project->rename("newproj", global));

    // verify what it has done to the global project
    switch (i) {
    case 0:
      assert(!global);
      break;
    case 1:
      // different global project
      assert_cmpstr(global->name, "unrelated");
      project_delete(global);
      assert(!global);
      break;
    case 2:
      // global project should also be renamed
      assert_cmpstr(project->name, global->name);
      // descriptor was reopened to point to the same directory, but must be distinct
      assert_cmpnum_op(project->dirfd.fd, !=, global->dirfd.fd);
      assert_cmpstr(project->path, global->path);
      global.reset();
      break;
    default:
      assert_unreachable();
    }

    // the non-gzip file should have been properly renamed
    assert(ends_with(project->osmFile, ".osm"));

    struct stat st;
    assert_cmpnum(stat(oldpath.c_str(), &st), -1);
    assert_cmpnum(errno, ENOENT);

    // dir exists
    assert_cmpnum(stat(project->path.c_str(), &st), 0);
    // project file exists and is not empty
    assert_cmpnum(stat((project->path + project->name + ".proj").c_str(), &st), 0);
    assert_cmpnum_op(st.st_size, >, project->name.size() + 20);
    // OSM file exists
    assert_cmpnum(stat((project->path + project->osmFile).c_str(), &st), 0);
    assert_cmpnum(st.st_size, strlen(not_gzip));
    // track file exists
    assert_cmpnum(stat((project->path + project->name + ".trk").c_str(), &st), 0);
    assert_cmpnum(st.st_size, 0);
    // diff exists
    const std::string ndiffname = project->path + project->name + ".diff";
    osm2go_platform::MappedFile ndiff(ndiffname);
    assert(static_cast<bool>(ndiff));
    const char *dnold = strstr(mf.data(), "diff_restore_data");
    const char *dnnew = strstr(ndiff.data(), project->name.c_str());
    assert_cmpmem(mf.data(), dnold - mf.data(), ndiff.data(), dnnew - ndiff.data());
    // only compare the next few bytes. The rest of the file may be differently formatted
    // (e.g. ' vs ", spaces before /> or not.
    assert_cmpmem(dnold + strlen("diff_restore_data"), 60, dnnew + project->name.size(), 60);
    dnnew = nullptr; // sanity
    ndiff.reset();

    bool b = project->parse_osm();
    assert(b);

    unsigned int u = project->diff_restore();
    assert_cmpnum(u, DIFF_ELEMENTS_IGNORED);

    // remove diff and check it's really gone
    project->diff_remove_file();
    assert_cmpnum(stat(ndiffname.c_str(), &st), -1);
    assert_cmpnum(errno, ENOENT);
    assert(!project->diff_file_present());

    project_t::projectStatus status = project->status(false);
    assert(status.valid);
    assert(!status.errorColor);

    // recreate it with the unmodified diff
    {
      fdguard osmfd(open(ndiffname.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR));
      assert_cmpnum_op(static_cast<int>(osmfd), >=, 0);
      assert_cmpnum(write(osmfd, mf.data(), mf.length()), mf.length());
    }
    assert(project->diff_file_present());

    // throw away all changes
    b = project->parse_osm();
    assert(b);

    // this should warn
    u = project->diff_restore();
    assert_cmpnum(u, DIFF_PROJECT_MISMATCH | DIFF_ELEMENTS_IGNORED);

    project_delete(project);
  }
}

void
testCreate(const std::string &tmpdir, const std::string &readonly)
{
  std::unique_ptr<project_t> project(project_t::create("newly_created", tmpdir, nullptr));
  assert(project);
  project_delete(project);

  project.reset(project_t::create("foobar", readonly, nullptr));
  assert(!project);
}

void
testLoadSave(const std::string &tmpdir)
{
  const std::string defaultserver = "https://example.com/default";
  const std::string otherserver = "https://example.com/other";
  const std::string wmsserver = "https://example.org/wms_base/";
  const char *nserver;

  std::srand(reinterpret_cast<uintptr_t>(&nserver)); // random because of ASLR

  for (unsigned int j = 0; j < 10; j++) {
    unsigned int i = std::rand();
    // these 2 are mutually exclusive
    switch (i & ((1 << 4) | (1 << 5))) {
    case 0:
      nserver = nullptr;
      break;
    case (1 << 4):
      nserver = defaultserver.c_str();
      break;
    case (1 << 5):
      nserver = otherserver.c_str();
      break;
    default:
      j--; // try again
      continue;
    }

    const std::string prjname = "load_save_" + std::to_string(i);
    std::unique_ptr<project_t> project(project_t::create(prjname, tmpdir, nullptr));
    assert(project);

    project->isDemo = i & (1 << 0);
    project->data_dirty = i & (1 << 1);

    project->bounds.min.lat = (std::rand() % 360) - 180;
    project->bounds.min.lon = ((std::rand() % (4 *360)) - 4 * 180) / 4.0;

    project->bounds.max.lat = ((std::rand() % (8 *360)) - 8 * 180) / 8.0;
    project->bounds.max.lon = (std::rand() % 360) - 180;

    if (i & (1 << 2))
      project->desc = "has description" + std::to_string(i);
    if (nserver != nullptr)
      project->adjustServer(nserver, defaultserver);
    if (i & (1 << 6))
      project->wms_server = wmsserver;

    assert(project->save());

    const std::unique_ptr<project_t> rproject(std::make_unique<project_t>(prjname, tmpdir));
    const std::string pfile = tmpdir + '/' + prjname + '/' + prjname + ".proj";

    assert(project_read(pfile, rproject, std::string(), -1));

    assert_cmpnum(project->isDemo ? 1 : 0, rproject->isDemo ? 1 : 0);
    assert_cmpnum(project->data_dirty ? 1 : 0, rproject->data_dirty ? 1 : 0);
    assert_cmpstr(project->desc, rproject->desc);
    assert_cmpstr(project->server(defaultserver), rproject->server(defaultserver));
    assert_cmpstr(project->wms_server, rproject->wms_server);
    // newly created projects will use .osm.gz, but if that file not found on project start
    // the code will fall back to the old name
    assert_cmpstr(project->osmFile, rproject->osmFile + ".gz");

    assert_cmpnum(project->bounds.min.lat, rproject->bounds.min.lat);
    assert_cmpnum(project->bounds.min.lon, rproject->bounds.min.lon);
    assert_cmpnum(project->bounds.max.lat, rproject->bounds.max.lat);
    assert_cmpnum(project->bounds.max.lon, rproject->bounds.max.lon);
    assert_cmpnum(project->bounds.valid() ? 1 : 0, rproject->bounds.valid() ? 1 : 0);

    project_delete(project);
  }
}

struct scan_project_creator {
  const std::string &tmpdir;
  const std::string &defaultserver;
  scan_project_creator(const std::string &dir, const std::string &srv) : tmpdir(dir), defaultserver(srv) {}

  void operator()(const std::string &prjname)
  {
    std::unique_ptr<project_t> p(project_t::create(prjname, tmpdir, nullptr));
    assert(p);
  }
};

struct scan_project_verifier {
  scan_project_verifier(std::vector<std::string> &n) : names(n) {}
  std::vector<std::string> &names;

  void operator()(project_t *prj)
  {
    std::unique_ptr<project_t> p(prj);
    const std::vector<std::string>::iterator it = std::find(names.begin(), names.end(), p->name);
    assert(it != names.end());
    names.erase(it); // make sure there is only one match, simplifies later check if all expected names were found
    project_delete(p);
  }
};

void
helper_testScan(const std::string &tmpdir, const dirguard &dir, std::vector<std::string> names)
{
  const std::string defaultserver = "https://example.com/default";

  std::for_each(names.begin(), names.end(), scan_project_creator(tmpdir, defaultserver));

  std::vector<project_t *> scan = project_scan(tmpdir, dir.dirfd(), defaultserver);
  assert_cmpnum(scan.size(), names.size());

  std::for_each(scan.begin(), scan.end(), scan_project_verifier(names));
  assert_cmpnum(names.size(), 0);
}

void
testScan(const std::string &tmpdir)
{
  const std::string defaultserver = "https://example.com/default";
  dirguard dir(tmpdir);

  std::vector<project_t *> scan = project_scan(tmpdir, dir.dirfd(), defaultserver);
  assert_cmpnum(scan.size(), 0);

  // empty directories should be ignored
  assert_cmpnum(mkdirat(dir.dirfd(), "emptydir", 0755), 0);

  scan = project_scan(tmpdir, dir.dirfd(), defaultserver);
  assert_cmpnum(scan.size(), 0);

  // same as files
  const char *junkfilename = "unrelated file";
  fdguard junkfile(dir.dirfd(), junkfilename, O_CREAT | O_EXCL);
  assert(junkfile);

  scan = project_scan(tmpdir, dir.dirfd(), defaultserver);
  assert_cmpnum(scan.size(), 0);

  // a correctly named project file, but can't be opened as it's empty
  assert_cmpnum(mkdirat(dir.dirfd(), "emptyproj", 0755), 0);
  const char *emptyprojname = "emptyproj/emptyproj.proj";
  fdguard projfile(dir.dirfd(), emptyprojname, O_CREAT | O_EXCL);
  assert(projfile);

  scan = project_scan(tmpdir, dir.dirfd(), defaultserver);
  assert_cmpnum(scan.size(), 0);

  std::vector<std::string> names;
  names.push_back("first");
  helper_testScan(tmpdir, dir, names);
  names.push_back("second");
  helper_testScan(tmpdir, dir, names);
  names.insert(names.begin(), "third");
  helper_testScan(tmpdir, dir, names);

  assert_cmpnum(unlinkat(dir.dirfd(), emptyprojname, 0), 0);
  assert_cmpnum(unlinkat(dir.dirfd(), "emptyproj", AT_REMOVEDIR), 0);
  assert_cmpnum(unlinkat(dir.dirfd(), "emptydir", AT_REMOVEDIR), 0);
  assert_cmpnum(unlinkat(dir.dirfd(), junkfilename, 0), 0);
}

} // namespace

appdata_t::appdata_t()
  : uicontrol(new MainUiDummy())
  , map(nullptr)
  , icons(icon_t::instance())
{
}

int main(int argc, char **argv)
{
  OSM2GO_TEST_INIT(argc, argv);

  xmlInitParser();

  if(argc != 4)
    return 1;

  char tmpdir[] = "/tmp/osm2go-project-XXXXXX";

  if(mkdtemp(tmpdir) == nullptr) {
    std::cerr << "cannot create temporary directory" << std::endl;
    return 1;
  }

  std::string osm_path = tmpdir;
  osm_path += '/';

  std::string readonly = osm_path + "readonly/";
  int r = mkdir(readonly.c_str(), S_IRUSR | S_IXUSR);
  if (r != 0) {
    std::cerr << "cannot create non-writable directory";
    return 1;
  }

  osm2go_platform::MappedFile osmfile(argv[2]);
  assert(osmfile);

  testNoFiles(osm_path);
  testSave(osm_path, readonly, argv[1]);
  testNoData(osm_path);
  testServer(osm_path);
  testLoad(osm_path, osmfile);
  testRename(osm_path, argv[3]);
  testCreate(osm_path, readonly);
  testLoadSave(osm_path);
  testScan(osm_path);

  assert_cmpnum(rmdir(readonly.c_str()), 0);
  assert_cmpnum(rmdir(tmpdir), 0);

  xmlCleanupParser();

  return 0;
}

#include "dummy_appdata.h"
