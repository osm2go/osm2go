#include <osm_api.h>

#include <appdata.h>
#include <map.h>
#include <misc.h>
#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include <pos.h>
#include <project.h>
#include <settings.h>

#include <cassert>
#include <cstdlib>
#include <curl/curl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static char tmpdir[32] = "/tmp/osm2go_api_dl_XXXXXX";
static const char *dev_url  = "https://master.apis.dev.openstreetmap.org/api/0.6";
static map_state_t mapstate;

static project_t *
setup_project(const std::string &projectName, std::string &project_dir)
{
  project_dir = tmpdir;
  project_dir += projectName;
  assert(mkdir(project_dir.c_str(), 0755) == 0);

  project_t *project = new project_t(mapstate, projectName, tmpdir);
  project->bounds = pos_area(pos_t(52.27659, 9.58270), pos_t(52.27738, 9.58426));
  assert(project->bounds.valid());
  project->osmFile = projectName + ".osm";

  return project;
}

static void download_fine()
{
  const std::string projectName = "dl";
  std::string project_dir;

  std::unique_ptr<project_t> project(setup_project(projectName, project_dir));
  project->bounds = pos_area(pos_t(52.27659, 9.58270), pos_t(52.27738, 9.58426));
  assert(project->bounds.valid());
  project->rserver = dev_url;

  assert(osm_download(nullptr, project.get()));

  const std::string osmname = project->path + project->osmFile;
  assert_cmpnum(unlink(osmname.c_str()), 0);
  // the project file has been saved as it was modified (".gz" was added for OSM)
  const std::string projectfile = project_dir + "/" + projectName + ".proj";
  assert_cmpnum(unlink(projectfile.c_str()), 0);

  assert_cmpnum(rmdir(project_dir.c_str()), 0);
}

static void download_fine_was_gz()
{
  const std::string projectName = "dl_gz";
  std::string project_dir;

  std::unique_ptr<project_t> project(setup_project(projectName, project_dir));
  project->bounds = pos_area(pos_t(52.27659, 9.58270), pos_t(52.27738, 9.58426));
  assert(project->bounds.valid());
  project->rserver = dev_url;
  project->osmFile += ".gz";

  assert(osm_download(nullptr, project.get()));

  const std::string osmname = project->path + project->osmFile;
  assert_cmpnum(unlink(osmname.c_str()), 0);

  // the project file is not saved here as it was not modified
  assert_cmpnum(rmdir(project_dir.c_str()), 0);
}

static void download_fine_absolute()
{
  const std::string projectName = "dl_gz";
  std::string project_dir;

  std::unique_ptr<project_t> project(setup_project(projectName, project_dir));
  project->bounds = pos_area(pos_t(52.27659, 9.58270), pos_t(52.27738, 9.58426));
  assert(project->bounds.valid());
  // also trigger URL fixing
  project->rserver = dev_url;
  project->rserver += '/';
  project->osmFile = project_dir.substr(0, project_dir.rfind('/')) + "absolute.osm.gz";

  assert(osm_download(nullptr, project.get()));

  assert_cmpstr(project->rserver, dev_url);

  // the project file is not saved here as it was not modified
  // the file is outside of the directory, so it should be removable
  assert_cmpnum(rmdir(project_dir.c_str()), 0);

  assert_cmpnum(unlink(project->osmFile.c_str()), 0);
}

static void download_bad_server()
{
  const std::string projectName = "bad_server";
  std::string project_dir;

  std::unique_ptr<project_t> project(setup_project(projectName, project_dir));
  project->bounds = pos_area(pos_t(52.27659, 9.58270), pos_t(52.27738, 9.58426));
  assert(project->bounds.valid());

  project->rserver = "https://invalid.invalid";

  assert(!osm_download(nullptr, project.get()));

  assert_cmpnum(rmdir(project_dir.c_str()), 0);
}

static void download_bad_coords()
{
  const std::string projectName = "bad_coords";
  std::string project_dir;

  std::unique_ptr<project_t> project(setup_project(projectName, project_dir));
  project->bounds = pos_area(pos_t(181, 92), pos_t(180, 90));

  project->rserver = dev_url;

  assert(!osm_download(nullptr, project.get()));

  assert_cmpnum(rmdir(project_dir.c_str()), 0);
}

int main()
{
  assert(mkdtemp(tmpdir) != nullptr);
  strcat(tmpdir, "/");

  curl_global_init(CURL_GLOBAL_ALL);
  xmlInitParser();
#if !GLIB_CHECK_VERSION(2,36,0)
  g_type_init();
#endif

  download_fine();
  download_fine_was_gz();
  download_fine_absolute();
  download_bad_server();
  download_bad_coords();

  xmlCleanupParser();
  curl_global_cleanup();

  assert_cmpnum(rmdir(tmpdir), 0);

  return 0;
}

void appdata_t::track_clear()
{
  assert_unreachable();
}
