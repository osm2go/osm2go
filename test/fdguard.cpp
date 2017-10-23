#include <fdguard.h>

#include <misc.h>
#include <osm2go_annotations.h>

#include <cassert>
#include <fcntl.h>
#include <glib.h>
#include <unistd.h>
#include <sys/stat.h>

static void check_guard(int openfd, int &dirfd)
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
}

static void check_notdir(const char *exe)
{
  fdguard dir(exe);
  assert(!dir.valid());
  assert(!dir);
  fdguard file(exe, O_RDONLY);
  assert(file.valid());
  assert(file);

  std::string exepath(exe);
  std::string::size_type slpos = exepath.rfind('/');
  assert(slpos != std::string::npos);
  exepath[slpos] = '\0';

  fdguard exedir(exepath.c_str());
  assert(exedir.valid());
  fdguard exefile(exedir, exepath.c_str() + slpos + 1, O_RDONLY);
  assert(exefile.valid());
}

int main(int argc, char **argv)
{
  assert_cmpnum(argc, 2);

  int openfd = dup(1);
  int dirfd = -1;

  assert_cmpnum_op(openfd, >, 0);

  check_guard(openfd, dirfd);
  check_notdir(argv[1]);

  assert_cmpnum_op(dirfd, >, 0);

  struct stat st;

  // the descriptors should be closed now, so fstat() should fail
  assert_cmpnum(fstat(openfd, &st), -1);
  assert_cmpnum(fstat(dirfd, &st), -1);

  return 0;
}
