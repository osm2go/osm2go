#include <fdguard.h>

#include <misc.h>

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

  g_assert_true(ofd.valid());
  g_assert_true(ofd);
  g_assert_cmpint(static_cast<int>(ofd), >, 0);
  g_assert_cmpint(static_cast<int>(ofd), ==, openfd);
  g_assert_true(rootfd.valid());
  g_assert_true(rootfd);
  g_assert_cmpint(static_cast<int>(rootfd), >, 0);
  g_assert_cmpint(static_cast<int>(rootfd), ==, dirfd);

  rootfd.swap(ofd);

  g_assert_cmpint(static_cast<int>(rootfd), ==, openfd);
  g_assert_cmpint(static_cast<int>(ofd), ==, dirfd);
}

static void check_notdir(const char *exe)
{
  fdguard dir(exe);
  g_assert_false(dir.valid());
  g_assert_false(dir);
  fdguard file(exe, O_RDONLY);
  g_assert_true(file.valid());
  g_assert_true(file);

  std::string exepath(exe);
  std::string::size_type slpos = exepath.rfind('/');
  assert(slpos != std::string::npos);
  exepath[slpos] = '\0';

  fdguard exedir(exepath.c_str());
  g_assert_true(exedir.valid());
  fdguard exefile(exedir, exepath.c_str() + slpos + 1, O_RDONLY);
  g_assert_true(exefile.valid());
}

int main(int argc, char **argv)
{
  g_assert_cmpint(argc, ==, 2);

  int openfd = dup(1);
  int dirfd = -1;

  g_assert_cmpint(openfd, >, 0);

  check_guard(openfd, dirfd);
  check_notdir(argv[1]);

  g_assert_cmpint(dirfd, >, 0);

  struct stat st;

  // the descriptors should be closed now, so fstat() should fail
  g_assert_cmpint(fstat(openfd, &st), ==, -1);
  g_assert_cmpint(fstat(dirfd, &st), ==, -1);

  return 0;
}
