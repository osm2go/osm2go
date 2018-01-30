#include <net_io.h>

#include <misc.h>
#include <osm2go_annotations.h>
#include <osm2go_cpp.h>

#include <cassert>
#include <curl/curl.h>
#include <cstdlib>
#include <glib.h>
#include <gtk/gtk.h>
#include <string>

static void do_mem(g_mapped_file &lic)
{
  char *mem;
  size_t len;
  assert(net_io_download_mem(O2G_NULLPTR, "https://raw.githubusercontent.com/osm2go/osm2go/master/data/COPYING", &mem, len, "dummy"));
  g_string memguard(mem);

  assert_cmpmem(g_mapped_file_get_contents(lic.get()), g_mapped_file_get_length(lic.get()), mem, len);
}

static void do_mem_fail()
{
  char *mem;
  size_t len;
  assert(!net_io_download_mem(O2G_NULLPTR, "https://raw.githubusercontent.com/osm2go/osm2go/master/data/this_file_does_not_exist", &mem, len, "dummy"));
}

static void do_file(g_mapped_file &lic)
{
  char tmpdir[32] = "/tmp/osm2go_net_XXXXXX";

  assert(mkdtemp(tmpdir) != O2G_NULLPTR);

  std::string fname = tmpdir;
  fname += "/lic";
  assert(net_io_download_file(O2G_NULLPTR, "https://raw.githubusercontent.com/osm2go/osm2go/master/data/COPYING", fname, O2G_NULLPTR, false));

  g_mapped_file download(g_mapped_file_new(fname.c_str(), FALSE, O2G_NULLPTR));
  assert(download);
  assert_cmpmem(g_mapped_file_get_contents(lic.get()), g_mapped_file_get_length(lic.get()),
                g_mapped_file_get_contents(download.get()), g_mapped_file_get_length(download.get()));

  assert_cmpnum(unlink(fname.c_str()), 0);

  fname += ".gz";
  assert(net_io_download_file(O2G_NULLPTR, "https://raw.githubusercontent.com/osm2go/osm2go/master/data/COPYING", fname, O2G_NULLPTR, true));
  download.reset(g_mapped_file_new(fname.c_str(), FALSE, O2G_NULLPTR));

  assert(check_gzip(g_mapped_file_get_contents(download.get()), g_mapped_file_get_length(download.get())));

  assert_cmpnum(unlink(fname.c_str()), 0);
  assert_cmpnum(rmdir(tmpdir), 0);
}

static void do_file_fail()
{
  char tmpdir[32] = "/tmp/osm2go_net_XXXXXX";

  assert(mkdtemp(tmpdir) != O2G_NULLPTR);

  std::string fname = tmpdir;
  fname += "/empty";
  assert(!net_io_download_file(O2G_NULLPTR, "https://raw.githubusercontent.com/osm2go/osm2go/master/data/this_file_does_not_exist", fname, O2G_NULLPTR, false));
  assert_cmpnum(rmdir(tmpdir), 0);
}

int main(int argc, char **argv)
{
  if(argc != 2)
    return EINVAL;

  g_mapped_file lic(g_mapped_file_new(argv[1], FALSE, O2G_NULLPTR));

  assert(lic);

  curl_global_init(CURL_GLOBAL_ALL);

  do_mem(lic);
  do_mem_fail();
  do_file(lic);
  do_file_fail();

  curl_global_cleanup();

  return 0;
}
