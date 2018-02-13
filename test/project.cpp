#include <project.h>
#include <project_p.h>

#include <appdata.h>
#include <fdguard.h>
#include <icon.h>
#include <map.h>

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

appdata_t::appdata_t(map_state_t &mstate)
  : uicontrol(O2G_NULLPTR)
  , window(O2G_NULLPTR)
  , statusbar(O2G_NULLPTR)
  , map_state(mstate)
  , settings(O2G_NULLPTR)
  , icons(icon_t::instance())
  , gps_state(O2G_NULLPTR)
{
  track.track = O2G_NULLPTR;
}

appdata_t::~appdata_t()
{
}

static const char *proj_name = "test_proj";

static void testNoFiles(const std::string &tmpdir)
{
  map_state_t dummystate;
  project_t project(dummystate, proj_name, tmpdir);

  appdata_t appdata(dummystate);
  appdata.project = &project;

  assert(!track_restore(appdata));
  assert_null(appdata.track.track);

  const std::string pfile = tmpdir + '/' + std::string(proj_name) + ".proj";
  assert(!project_read(pfile, &project, std::string(), -1));

  int fd = open(pfile.c_str(), O_WRONLY | O_CREAT, 0644);
  assert(fd >= 0);
  const char *xml_minimal = "<a><b/></a>";
  write(fd, xml_minimal, strlen(xml_minimal));
  close(fd);
  fdguard empty(pfile.c_str(), O_RDONLY);
  assert(empty.valid());

  assert(!project_read(pfile, &project, std::string(), -1));

  unlink(pfile.c_str());
}

int main(void)
{
  xmlInitParser();

  char tmpdir[] = "/tmp/osm2go-project-XXXXXX";

  if(mkdtemp(tmpdir) == O2G_NULLPTR) {
    std::cerr << "cannot create temporary directory" << std::endl;
    return 1;
  }

  std::string osm_path = tmpdir;
  osm_path += '/';

  testNoFiles(osm_path);

  rmdir(tmpdir);

  xmlCleanupParser();

  return 0;
}

void appdata_t::main_ui_enable()
{
  assert_unreachable();
}

void appdata_t::track_clear()
{
  assert_unreachable();
}
