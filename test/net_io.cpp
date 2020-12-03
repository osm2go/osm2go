#include <net_io.h>

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <curl/curl.h>
#include <string>
#include <unistd.h>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include <osm2go_platform.h>
#include <osm2go_test.h>

static void do_mem(osm2go_platform::MappedFile &lic)
{
  std::string mem;
  assert(net_io_download_mem(nullptr, "https://raw.githubusercontent.com/osm2go/osm2go/master/LICENSES/GPL-3.0-or-later", mem, _("dummy")));

  assert_cmpmem(lic.data(), lic.length(), mem.c_str(), mem.size());
}

static void do_mem_fail()
{
  std::string mem;
  assert(!net_io_download_mem(nullptr, "https://raw.githubusercontent.com/osm2go/osm2go/master/data/this_file_does_not_exist", mem, _("dummy")));
}

static void do_file(osm2go_platform::MappedFile &lic)
{
  char tmpdir[32] = "/tmp/osm2go_net_XXXXXX";

  assert(mkdtemp(tmpdir) != nullptr);

  std::string fname = tmpdir;
  fname += "/lic";
  assert(net_io_download_file(nullptr, "https://raw.githubusercontent.com/osm2go/osm2go/master/LICENSES/GPL-3.0-or-later", fname, trstring::native_type(), false));

  osm2go_platform::MappedFile download(fname);
  assert(download);
  assert_cmpmem(lic.data(), lic.length(), download.data(), download.length());

  assert_cmpnum(unlink(fname.c_str()), 0);
  download.reset();

  fname += ".gz";
  assert(net_io_download_file(nullptr, "https://raw.githubusercontent.com/osm2go/osm2go/master/LICENSES/GPL-3.0-or-later", fname, trstring::native_type(), true));
  osm2go_platform::MappedFile downloadgz(fname);

  assert(check_gzip(downloadgz.data(), downloadgz.length()));

  assert_cmpnum(unlink(fname.c_str()), 0);
  assert_cmpnum(rmdir(tmpdir), 0);
}

static void do_file_fail()
{
  char tmpdir[32] = "/tmp/osm2go_net_XXXXXX";

  assert(mkdtemp(tmpdir) != nullptr);

  std::string fname = tmpdir;
  fname += "/empty";
  assert(!net_io_download_file(nullptr, "https://raw.githubusercontent.com/osm2go/osm2go/master/data/this_file_does_not_exist", fname, trstring::native_type(), false));
  assert_cmpnum(rmdir(tmpdir), 0);
}

int main(int argc, char **argv)
{
  OSM2GO_TEST_INIT(argc, argv);

  if(argc != 2)
    return EINVAL;

  if (unlikely(curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK))
    return ENOMEM;

  osm2go_platform::MappedFile lic(argv[1]);

  assert(lic);

  do_mem(lic);
  do_mem_fail();
  do_file(lic);
  do_file_fail();

  curl_global_cleanup();

  return 0;
}

#include "dummy_appdata.h"
