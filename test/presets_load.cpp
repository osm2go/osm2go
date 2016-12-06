#include <josm_presets.h>
#include <josm_presets_p.h>

#include <errno.h>
#include <iostream>
#include <stdlib.h>

int main(void)
{
  struct presets_items *presets;

  xmlInitParser();

  presets = josm_presets_load();

  if(!presets) {
    std::cerr << "failed to load presets" << std::endl;
    return 1;
  }

  if(presets->items.empty()) {
    std::cerr << "no items found" << std::endl;
    return 1;
  }

  std::cout << presets->items.size() << " top level items found" << std::endl;

  josm_presets_free(presets);
  xmlCleanupParser();
  return 0;
}
