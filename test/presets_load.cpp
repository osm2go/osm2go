#include <josm_presets.h>
#include <josm_presets_p.h>

#include <fdguard.h>
#include <misc.h>

#include <osm2go_cpp.h>

#include <algorithm>
#if __cplusplus < 201103L
#include <tr1/array>
namespace std {
  using namespace tr1;
};
#else
#include <array>
#endif
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <libxml/parser.h>
#include <set>
#include <string>
#include <sys/stat.h>

static std::vector<std::string> basedirs;

struct check_icon {
  const std::string &filename;
  explicit check_icon(const std::string &fn) : filename(fn) {}
  bool operator()(const std::string &dir);
};

static std::set<std::string> missingIcons;

bool check_icon::operator()(const std::string &dir)
{
  struct stat st;

  if(filename[0] == '/')
    return (stat(filename.c_str(), &st) == 0 && S_ISREG(st.st_mode));

  const std::array<const char *, 4> icon_exts = { { ".svg", ".gif", ".png", ".jpg" } };
  const std::string dirname = dir + "/icons";
  fdguard dirfd(dirname.c_str());
  if(G_UNLIKELY(!dirfd.valid()))
    return false;

  std::string name = filename;

  for(unsigned int i = 0; i < icon_exts.size(); i++) {
    name += icon_exts[i];

    if(fstatat(dirfd, name.c_str(), &st, 0) == 0 && S_ISREG(st.st_mode))
      return true;
    name.erase(name.size() - strlen(icon_exts[i]));
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
  unsigned int &plinks;
  unsigned int &roles;
  counter(unsigned int &gr, unsigned int &it, unsigned int &sep, unsigned int &c,
          unsigned int &ce, unsigned int &lb, unsigned int &ky,  unsigned int &chk,
          unsigned int &rf, unsigned int &pl, unsigned int &rl)
    : groups(gr), items(it), separators(sep), combos(c), combo_entries(ce),
      labels(lb), keys(ky), checks(chk), refs(rf), plinks(pl), roles(rl) {}
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
    g_assert_true(p->isItem());
    items++;
    const presets_item * const item = static_cast<const presets_item *>(p);
    std::for_each(item->widgets.begin(), item->widgets.end(), *this);
    roles += item->roles.size();
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
  case WIDGET_TYPE_LINK:
    plinks++;
    break;
  }
}

/**
 * @brief check icons of the given item
 * @param item the item to check
 */
static void checkItem(const presets_item_t *item)
{
  const presets_item_named * const vis = dynamic_cast<const presets_item_named *>(item);
  if(!vis)
    return;

  if(!vis->icon.empty()) {
    const std::vector<std::string>::const_iterator it = std::find_if(
                      basedirs.begin(), basedirs.end(), check_icon(vis->icon));
    if(it == basedirs.end())
      missingIcons.insert(vis->icon);
  }

  const presets_item_group * const group = dynamic_cast<const presets_item_group *>(vis);
  if(group) {
    std::for_each(group->items.begin(), group->items.end(), checkItem);
  }
}

static void test_roles(const presets_items *presets)
{
  relation_t r;
  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("type", "multipolygon"));
  r.tags.replace(tags);

  // object type does not match
  node_t n;
  std::set<std::string> roles = preset_roles(&r, object_t(&n), presets);
  g_assert_true(roles.empty());

  way_t w;
  roles = preset_roles(&r, object_t(&w), presets);
  g_assert_cmpuint(roles.size(), ==, 2);
  g_assert(roles.find("inner") != roles.end());
  g_assert(roles.find("outer") != roles.end());

  // check count restriction
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("type", "boundary"));
  r.tags.replace(tags);

  roles = preset_roles(&r, object_t(&n), presets);
  g_assert_cmpuint(roles.size(), ==, 2);
  g_assert(roles.find("admin_centre") != roles.end());
  g_assert(roles.find("label") != roles.end());

  r.members.push_back(member_t(object_t(&n), strdup("admin_centre")));

  node_t n2;
  roles = preset_roles(&r, object_t(&n2), presets);
  g_assert_cmpuint(roles.size(), ==, 1);
  g_assert(roles.find("label") != roles.end());

  // check count restriction does not apply if it is 0
  roles = preset_roles(&r, object_t(&w), presets);
  g_assert_cmpuint(roles.size(), ==, 2);
  g_assert(roles.find("outer") != roles.end());
  g_assert(roles.find("inner") != roles.end());

  way_t w2;
  r.members.push_back(member_t(object_t(&w2), strdup("outer")));

  roles = preset_roles(&r, object_t(&w), presets);
  g_assert_cmpuint(roles.size(), ==, 2);
  g_assert(roles.find("outer") != roles.end());
  g_assert(roles.find("inner") != roles.end());

  // check that also non-interactive presets are considered
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("type", "building"));
  r.tags.replace(tags);

  roles = preset_roles(&r, object_t(&n), presets);
  g_assert_cmpuint(roles.size(), ==, 1);
  g_assert(roles.find("entrance") != roles.end());

  // check that regexp-roles are not shown
  relation_t r2;
  roles = preset_roles(&r, object_t(&r2), presets);
  g_assert_cmpuint(roles.size(), ==, 0);

  r.cleanup();
}

int main(int argc, char **argv)
{
  struct presets_items *presets;

  xmlInitParser();
  misc_init();

  basedirs.reserve(argc - 1);
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
  unsigned int plinks = 0;
  unsigned int roles = 0;
  counter cnt(groups, items, separators, combos, combo_entries, labels, keys, checks, refs, plinks, roles);

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
    << "references: " << refs << std::endl
    << "preset_links: " << plinks << std::endl
    << "roles: " << roles << std::endl;

  std::for_each(presets->items.begin(), presets->items.end(), checkItem);

  test_roles(presets);

  delete presets;
  xmlCleanupParser();

  if(!missingIcons.empty()) {
    std::cerr << missingIcons.size() << " icons missing" << std::endl;
    std::for_each(missingIcons.begin(), missingIcons.end(), err);
    return 1;
  }

  return 0;
}
