#include <fdguard.h>

#include <osm2go_annotations.h>

#include <cassert>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// wrapper function to trigger the copy constructor
static dirguard dguard(const std::string &basepath, const std::string &dirname)
{
  return dirguard(basepath + dirname);
}

static void check_guard(int openfd, int &dirfd, const std::string &exepath)
{
  fdguard infd(0);
  assert(infd);

  fdguard ofd(openfd);
  fdguard rootfd("/");

  dirfd = rootfd;

  assert(ofd.valid());
  assert(ofd);
  assert_cmpnum_op(static_cast<int>(ofd), >, 0);
  assert_cmpnum(static_cast<int>(ofd), openfd);
  assert(rootfd.valid());
  assert(rootfd);
  assert_cmpnum_op(static_cast<int>(rootfd), >, 0);
  assert_cmpnum(static_cast<int>(rootfd), dirfd);

  rootfd.swap(ofd);

  assert_cmpnum(static_cast<int>(rootfd), openfd);
  assert_cmpnum(static_cast<int>(ofd), dirfd);

  std::string::size_type sl = exepath.rfind('/');
  dirguard dg(exepath.substr(0, sl));
  assert(dg.valid());

  dirguard dg2(dg, exepath.substr(sl + 1).c_str());
  assert(dg2.valid());

  dirguard dgchar(exepath.substr(0, sl + 1));
  assert(dg.valid());
  dirguard dgchar2(dgchar, exepath.substr(sl + 1).c_str());
  assert(dgchar2.valid());

  dirguard dgcopy(dguard(exepath.substr(0, sl + 1), exepath.substr(sl + 1)));
  assert(dgcopy.valid());
  dirent *n = dgcopy.next();
  assert(n != nullptr);
}

static void check_notdir(const char *exe, const std::string &exepath)
{
  fdguard dir(exe);
  assert(!dir.valid());
  assert(!dir);
  fdguard file(exe, O_RDONLY);
  assert(file.valid());
  assert(file);

  fdguard exedir(exepath.c_str());
  assert(exedir.valid());
  fdguard exefile(exedir, exe + exepath.length() + 1, O_RDONLY);
  assert(exefile.valid());

  // check with invalid path name (not a directory)
  dirguard dguard_path(exe);
  assert(!dguard_path.valid());

  // check with file descriptor not pointing to a directory
  fdguard zero("/dev/zero", O_RDONLY);
  assert(zero.valid());
  dirguard dguard_fd(zero);
  assert(!dguard_fd.valid());
}

int main(int argc, char **argv)
{
  assert_cmpnum(argc, 2);

  int openfd = dup(1);
  int dirfd = -1;

  assert_cmpnum_op(openfd, >, 0);

  std::string exepath(argv[1]);
  std::string::size_type slpos = exepath.rfind('/');
  exepath.erase(slpos);

  check_guard(openfd, dirfd, exepath);
  check_notdir(argv[1], exepath);

  assert_cmpnum_op(dirfd, >, 0);

  struct stat st;

  // the descriptors should be closed now, so fstat() should fail
  assert_cmpnum(fstat(openfd, &st), -1);
  assert_cmpnum(fstat(dirfd, &st), -1);

  return 0;
}

#include "appdata.h"
void appdata_t::track_clear()
{
  abort();
}
