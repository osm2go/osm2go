#include <josm_presets.h>
#include <josm_presets_p.h>

#include <algorithm>
#include <cstdlib>
#include <cerrno>
#include <iostream>
#include <libxml/parser.h>
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

struct counter {
  unsigned int &groups;
  unsigned int &items;
  unsigned int &separators;
  unsigned int &combos;
  unsigned int &combo_entries;
  unsigned int &labels;
  unsigned int &keys;
  unsigned int &checks;
  unsigned int &refs;
  counter(unsigned int &gr, unsigned int &it, unsigned int &sep, unsigned int &c,
          unsigned int &ce, unsigned int &lb, unsigned int &ky,  unsigned int &chk,
          unsigned int &ref)
    : groups(gr), items(it), separators(sep), combos(c), combo_entries(ce),
      labels(lb), keys(ky), checks(chk), refs(ref) {}
  void operator()(const presets_item_t *p);
  void operator()(const presets_widget_t *w);
};

void counter::operator()(const presets_item_t *p)
{
  if(p->type & presets_item_t::TY_GROUP) {
    const presets_item_group * const gr = static_cast<const presets_item_group *>(p);
    std::for_each(gr->items.begin(), gr->items.end(), *this);
    groups++;
    return;
  } else if (p->type == presets_item_t::TY_SEPARATOR) {
    separators++;
  } else {
    items++;
    std::for_each(p->widgets.begin(), p->widgets.end(), *this);
  }
}

void counter::operator()(const presets_widget_t *w)
{
  switch(w->type) {
  case WIDGET_TYPE_LABEL:
    labels++;
    break;
  case WIDGET_TYPE_SEPARATOR:
  case WIDGET_TYPE_SPACE:
  case WIDGET_TYPE_TEXT:
    break;
  case WIDGET_TYPE_COMBO:
    combos++;
    combo_entries += static_cast<const presets_widget_combo *>(w)->values.size();
    break;
  case WIDGET_TYPE_CHECK:
    checks++;
    break;
  case WIDGET_TYPE_KEY:
    keys++;
    break;
  case WIDGET_TYPE_REFERENCE:
    refs++;
    break;
  }
}

/**
 * @brief check icons of the given item
 * @param item the item to check
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

  unsigned int groups = 0;
  unsigned int items = 0;
  unsigned int separators = 0;
  unsigned int combos = 0;
  unsigned int combo_entries = 0;
  unsigned int labels = 0;
  unsigned int keys = 0;
  unsigned int checks = 0;
  unsigned int refs = 0;
  counter cnt(groups, items, separators, combos, combo_entries, labels, keys, checks, refs);

  std::for_each(presets->items.begin(), presets->items.end(), cnt);
  std::for_each(presets->chunks.begin(), presets->chunks.end(), cnt);

  std::cout
    << "chunks found: " << presets->chunks.size() << std::endl
    << "top level items found: " << presets->items.size() << std::endl
    << "groups: " << groups << std::endl
    << "items: " << items << std::endl
    << "separators: " << separators << std::endl
    << "combos: " << combos << std::endl
    << "combo_entries: " << combo_entries << std::endl
    << "labels: " << labels << std::endl
    << "keys: " << keys << std::endl
    << "checks: " << checks << std::endl
    << "refs: " << refs << std::endl;

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
