#include <josm_presets.h>

#include <errno.h>
#include <gtk/gtk.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
  presets_item_t * const presets = josm_presets_load();
  if(!presets)
    return -1;  
  josm_presets_free(presets);
  return 0;
}
