#include <fdguard.h>

#include <osm2go_annotations.h>

#include <cassert>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

// wrapper function to trigger the copy constructor
dirguard dguard(const std::string &basepath, const std::string &dirname)
{
  return dirguard(basepath + dirname);
}

void check_guard(int openfd, int &dirfd, const std::string &exepath)
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

void check_notdir(const char *exe, const std::string &exepath)
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

void check_diriter()
{
  char tmpdir[] = "/tmp/osm2go-fdguard-XXXXXX";

  assert(mkdtemp(tmpdir) != nullptr);

  dirguard namedDir(tmpdir);

  for (int i = 0; i < 4; i++) {
    fdguard dummyfile(namedDir.dirfd(), std::to_string(i).c_str(), O_CREAT | O_EXCL);
    assert(dummyfile);
  }

  unsigned int cnt = 0;
  while (namedDir.next() != nullptr)
    cnt++;

  // the 4 created ones + "." + ".."
  assert_cmpnum(cnt, 6);

  // check copy construction only using the filedescriptor
  dirguard otherdir(namedDir.dirfd());

  cnt = 0;
  while (otherdir.next() != nullptr)
    cnt++;

  // the 4 created ones + "." + ".."
  assert_cmpnum(cnt, 6);

  // check normal copy constructor
  dirguard copied(std::move(namedDir));

  cnt = 0;
  while (copied.next() != nullptr)
    cnt++;

  // the move or copy constructors do not rewind
  assert_cmpnum(cnt, 0);

  // path must have been copied over
  assert_cmpstr(copied.path(), tmpdir);

  std::vector<dirguard> vec;
  dirguard dfd(tmpdir);
#if __cplusplus >= 201103L
    vec.emplace_back(std::move(dfd));
#else
    vec.push_back(dfd);
#endif

  cnt = 0;
  while (vec.front().next() != nullptr)
    cnt++;

  // the move or copy constructors do not rewind
  assert_cmpnum(cnt, 6);

  for (int i = 0; i < 4; i++) {
    assert_cmpnum(unlinkat(copied.dirfd(), std::to_string(i).c_str(), 0), 0);
  }
  assert_cmpnum(rmdir(tmpdir), 0);
}

// test copy/move constructors
void check_constructors(const std::string &exepath)
{
  dirguard invalid(-1);
  assert(!invalid.valid());

  fdguard exe(exepath.c_str(), O_RDONLY);
  assert(exe.valid());

  assert_cmpnum(static_cast<int>(lseek(exe.fd, 10240, SEEK_SET)), 10240);

  // copying/moving the descriptor should keep the offset
  fdguard exe2(std::move(exe));
  assert_cmpnum(static_cast<int>(lseek(exe2.fd, 0, SEEK_CUR)), 10240);

  std::vector<fdguard> vec;
#if __cplusplus >= 201103L
    vec.emplace_back(std::move(exe2));
#else
    vec.push_back(exe2);
#endif

  assert_cmpnum(static_cast<int>(lseek(vec.front().fd, 0, SEEK_CUR)), 10240);
}

} // namespace

int main(int argc, char **argv)
{
  assert_cmpnum(argc, 2);

  int openfd = dup(1);
  int dirfd = -1;

  assert_cmpnum_op(openfd, >, 0);

  std::string exepath(argv[1]);
  std::string::size_type slpos = exepath.rfind('/');
  exepath.erase(slpos);

  check_constructors(exepath);
  check_guard(openfd, dirfd, exepath);
  check_notdir(argv[1], exepath);
  check_diriter();

  assert_cmpnum_op(dirfd, >, 0);

  struct stat st;

  // the descriptors should be closed now, so fstat() should fail
  assert_cmpnum(fstat(openfd, &st), -1);
  assert_cmpnum(fstat(dirfd, &st), -1);

  return 0;
}

#include "dummy_appdata.h"
