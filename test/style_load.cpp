#include <josm_elemstyles.h>
#include <josm_elemstyles_p.h>

#include <icon.h>

#include <osm2go_cpp.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

static struct counter {
  counter() : conditions(0) {}

  std::map<elemstyle_type_t, unsigned int> ruletypes;
  std::vector<elemstyle_condition_t>::size_type conditions;
} counter;

static void checkItem(const elemstyle_t *item)
{
  counter.ruletypes[item->type]++;
  counter.conditions += item->conditions.size();
}

static void show_rule_count(const std::pair<elemstyle_type_t, unsigned int> &p)
{
  std::cout << "rule type " << p.first << ": " << p.second << std::endl;
}

static void usage(const char *bin)
{
  std::cerr << "Usage: " << bin << " style.xml #rules #conditions [path_prefix]" << std::endl;
}

static const char *path_prefix;
static bool error = false;

static void icon_check(const elemstyle_t *item)
{
  if(item->icon.filename.empty())
    return;

  std::string name = "styles/";
  name += path_prefix;
  // the final size is now known, avoid too big allocations
  name.reserve(name.size() + 1 + item->icon.filename.size());
  name += '/';
  name += item->icon.filename;

  icon_t *iconP = O2G_NULLPTR;
  GdkPixbuf *buf = icon_load(&iconP, name);
  if(!buf) {
    std::cout << "icon missing: " << item->icon.filename << std::endl;
    if(strcmp(path_prefix, "standard") == 0)
      error = true;
  } else {
    icon_free(&iconP, buf);
  }
  icon_free_all(iconP);
}

int main(int argc, char **argv)
{
  if (argc < 4 || argc > 5) {
    usage(argv[0]);
    return 1;
  }

  char *endp;
  unsigned long rules = strtoul(argv[2], &endp, 10);
  if (endp == O2G_NULLPTR || *endp != 0) {
    usage(argv[0]);
    return 1;
  }
  unsigned long conditions = strtoul(argv[3], &endp, 10);
  if (endp == O2G_NULLPTR || *endp != 0) {
    usage(argv[0]);
    return 1;
  }

  if(argc == 5)
    path_prefix = argv[4];

#if !GLIB_CHECK_VERSION(2,36,0)
  g_type_init();
#endif

  xmlInitParser();

  std::vector<elemstyle_t *> styles = josm_elemstyles_load(argv[1]);

  if(styles.empty()) {
    std::cerr << "failed to load styles" << std::endl;
    return 1;
  }

  std::cout << styles.size() << " top level items found";
  if (styles.size() != rules) {
    std::cout << ", but " << rules << " expected";
    error = true;
  }
  std::cout << std::endl;

  std::for_each(styles.begin(), styles.end(), checkItem);

  if (counter.ruletypes.size() > 4) {
    std::cerr << "too many rule types found" << std::endl;
    error = true;
  }

  std::for_each(styles.begin(), styles.end(), icon_check);

  std::for_each(counter.ruletypes.begin(), counter.ruletypes.end(), show_rule_count);

  std::cout << counter.conditions << " conditions found";
  if (counter.conditions != conditions) {
    std::cout << ", but " << conditions << " expected";
    error = true;
  }
  std::cout << std::endl;

  josm_elemstyles_free(styles);
  xmlCleanupParser();

  return error ? 1 : 0;
}
