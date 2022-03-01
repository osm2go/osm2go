#include "api_limits.h"

#include <fdguard.h>

#include <cstdio>
#include <errno.h>
#include <fcntl.h>
#include <optional>

#include <osm2go_test.h>

namespace {

class api_limits_test : public api_limits {
  api_limits_test() : api_limits() {}
public:
  static api_limits parseExisting(const fdguard &basedir, const char *filename);
};

api_limits api_limits_test::parseExisting(const fdguard &basedir, const char *filename)
{
  fdguard difffd(basedir, filename, O_RDONLY);

  if (!difffd) {
    fprintf(stderr, "can not open %s\n", filename);
    abort();
  }

  api_limits_test ret;

  /* parse the file and get the DOM */
  xmlDocGuard doc(xmlReadFd(difffd, nullptr, nullptr, XML_PARSE_NONET));
  if(!doc) {
    fprintf(stderr, "can not parse XML of %s\n", filename);
    abort();
  }

  ret.parseXml(doc);

  return ret;
}

int verifyUInt(const char *descr, unsigned int value, unsigned int expected)
{
  if (value != expected) {
    fprintf(stderr, "expected value %u for %s, but got %u\n", expected, descr, value);
    return 1;
  }

  return 0;
}

struct limits {
  limits(const char *fn, api_limits::ApiVersions api, float area, unsigned int waynodes, unsigned int relmembers,
         unsigned int cselements, unsigned int timeout)
   : filename(fn), minApiVersion(api), maxAreaSize(area), nodesPerWay(waynodes)
   , membersPerRelation(relmembers), elementsPerChangeset(cselements), apiTimeout(timeout)
  {
  }

  const char * const filename;
  const api_limits::ApiVersions minApiVersion;
  const float maxAreaSize;
  const unsigned int nodesPerWay;
  const unsigned int membersPerRelation;
  const unsigned int elementsPerChangeset;
  const unsigned int apiTimeout;
};

int verifyExisting(const fdguard &basedir, const limits &limits)
{
  const api_limits limit = api_limits_test::parseExisting(basedir, limits.filename);

  if (!limit.initialized())
    return false;

  int ret = 0;

  ret += verifyUInt("min API version", limit.minApiVersion(), limits.minApiVersion);
  ret += verifyUInt("nodes per way", limit.nodesPerWay(), limits.nodesPerWay);
  ret += verifyUInt("relation members", limit.membersPerRelation(), limits.membersPerRelation);
  ret += verifyUInt("changeset elements", limit.elementsPerChangeset(), limits.elementsPerChangeset);
  ret += verifyUInt("API timeout", limit.apiTimeout(), limits.apiTimeout);

  float areaSize;
  if ((areaSize = limit.maxAreaSize()) != limits.maxAreaSize) {
    fprintf(stderr, "expected value %f for area size, but got %f\n", limits.maxAreaSize, areaSize);
    ret = 1;
  }

  return ret;
}

int verifyDevXml()
{
  const char *devServer = "https://master.apis.dev.openstreetmap.org";
  const api_limits &deflimits = api_limits::offlineInstance(devServer);

  if (deflimits.initialized())
    return 1;

  if (deflimits.minApiVersion() != api_limits::ApiVersion_0_6)
    return 1;

  if (deflimits.nodesPerWay() != static_cast<unsigned int>(-1))
    return 1;

  const api_limits &limits = api_limits::instance(devServer);

  if (!limits.initialized())
    return 1;

  if (limits.minApiVersion() != api_limits::ApiVersion_0_6)
    return 1;

  return 0;
}

int verifyInvalid()
{
  const api_limits &limits = api_limits::instance("http://invalid.invalid");

  if (limits.initialized())
    return 1;

  if (limits.minApiVersion() != api_limits::ApiVersion_0_6)
    return 1;

  return 0;
}

} // namespace

int main(int argc, char **argv)
{
  int result = 0;

  OSM2GO_TEST_INIT(argc, argv);

  if (argc < 2 || argc > 3)
    return EINVAL;

  int fnindex = 1;
  bool online = false;
  if (strcmp(argv[1], "--online") == 0) {
    online = true;
    fnindex++;
  }

  fdguard basedir(argv[fnindex]);
  if (!basedir) {
    int err = errno;
    fprintf(stderr, "can not open base directory %s: error %i\n", argv[1], err);
    return err;
  }

  xmlInitParser();

  limits limits_20220227("api_limits_20220227.xml", api_limits::ApiVersion_0_6, 0.25, 2000, 32000, 10000, 300);
  result += verifyExisting(basedir, limits_20220227);

  limits limits_crazy("api_limits_crazy.xml", api_limits::ApiVersion_Unsupported, 8.5, 22222, 111, 3, 86400);
  result += verifyExisting(basedir, limits_crazy);

  if (online) {
    result += verifyDevXml();

    result += verifyInvalid();
  }

  xmlCleanupParser();

  return result;
}

#include "dummy_appdata.h"
