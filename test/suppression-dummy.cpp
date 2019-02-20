#include <curl/curl.h>
#include <libxml/parser.h>

int main()
{
  xmlInitParser();
  if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK)
    return 1;

  curl_global_cleanup();
  xmlCleanupParser();

  return 0;
}
