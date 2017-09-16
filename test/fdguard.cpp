#include <fdguard.h>

#include <misc.h>

#include <fcntl.h>
#include <glib.h>
#include <unistd.h>
#include <sys/stat.h>

static void check_guard(int openfd, int &dirfd)
{
  fdguard ofd(openfd);
  fdguard rootfd("/");

  dirfd = rootfd;

  g_assert_true(ofd.valid());
  g_assert_cmpint(ofd, >, 0);
  g_assert_cmpint(ofd, ==, openfd);
  g_assert_true(rootfd.valid());
  g_assert_cmpint(rootfd, >, 0);
  g_assert_cmpint(rootfd, ==, dirfd);

  rootfd.swap(ofd);

  g_assert_cmpint(rootfd, ==, openfd);
  g_assert_cmpint(ofd, ==, dirfd);
}

int main()
{
  int openfd = dup(1);
  int dirfd = -1;

  g_assert_cmpint(openfd, >, 0);

  check_guard(openfd, dirfd);

  g_assert_cmpint(dirfd, >, 0);

  struct stat st;

  // the descriptors should be closed not, so fstat() should fail
  g_assert_cmpint(fstat(openfd, &st), ==, -1);
  g_assert_cmpint(fstat(dirfd, &st), ==, -1);

  return 0;
}
