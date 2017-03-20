#include <josm_presets.h>
#include <josm_presets_p.h>

#include <algorithm>
#include <cstdlib>
#include <cerrno>
#include <iostream>
#include <set>
#include <string>

static std::vector<std::string> basedirs;

struct check_icon {
  const std::string &filename;
  check_icon(const std::string &fn) : filename(fn) {}
  bool operator()(const std::string &dir);
};

static std::set<std::string> missingIcons;

bool check_icon::operator()(const std::string &dir)
{
  const char *icon_exts[] = { ".svg", ".gif", ".png", ".jpg", NULL };
  std::string path = dir + "/icons/" + filename;
  std::string name = path + icon_exts[0];

  if(g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR) == TRUE)
    return true;

  for (gint idx = 0; icon_exts[idx]; idx++) {
    name = path + icon_exts[idx];

    if(g_file_test(name.c_str(), G_FILE_TEST_IS_REGULAR) == TRUE)
      return true;
  }
  return false;
}

static void err(const std::string &filename)
{
  std::cerr << filename << std::endl;
}

/**
 * @brief check icons of the given item
 * @param item the item to check
 * @returns if a missing icon was detected
 * @retval false all icons below this nodes were found (if any)
 */
static void checkItem(const presets_item_t *item)
{
  const presets_item_visible * const vis = dynamic_cast<const presets_item_visible *>(item);
  if(!vis)
    return;

  if(vis->icon) {
    const std::string filename(reinterpret_cast<const char *>(vis->icon));
    const std::vector<std::string>::const_iterator it = std::find_if(
                      basedirs.begin(), basedirs.end(), check_icon(filename));
    if(it == basedirs.end())
      missingIcons.insert(filename);
  }

  const presets_item_group * const group = dynamic_cast<const presets_item_group *>(vis);
  if(group) {
    std::for_each(group->items.begin(), group->items.end(), checkItem);
  }
}

int main(int argc, char **argv)
{
  struct presets_items *presets;

  xmlInitParser();

  for(int i = 1; i < argc; i++)
    basedirs.push_back(argv[i]);

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

  std::for_each(presets->items.begin(), presets->items.end(), checkItem);

  josm_presets_free(presets);
  xmlCleanupParser();

  if(!missingIcons.empty()) {
    std::cerr << missingIcons.size() << " icons missing" << std::endl;
    std::for_each(missingIcons.begin(), missingIcons.end(), err);
    return 1;
  }

  return 0;
}
