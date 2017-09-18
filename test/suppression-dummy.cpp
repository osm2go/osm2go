#include <curl/curl.h>
#include <libxml/parser.h>

int main()
{
  xmlInitParser();
  curl_global_init(CURL_GLOBAL_ALL);

  curl_global_cleanup();
  xmlCleanupParser();

  return 0;
}
