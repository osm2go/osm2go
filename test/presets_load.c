#include <josm_presets.h>

#include <errno.h>
#include <gtk/gtk.h>
#include <stdlib.h>

int main(void)
{
  struct presets_items *presets;

  xmlInitParser();

  presets = josm_presets_load();

  if(!presets)
    return -1;

  josm_presets_free(presets);
  xmlCleanupParser();
  return 0;
}
