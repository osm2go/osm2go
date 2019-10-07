#include <josm_presets.h>
#include <josm_presets_p.h>

#include <fdguard.h>
#include <osm.h>
#include <osm_objects.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <libxml/parser.h>
#include <set>
#include <string>
#include <sys/stat.h>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>

static std::vector<std::string> basedirs;

namespace {

struct check_icon {
  const std::string &filename;
  explicit check_icon(const std::string &fn) : filename(fn) {}
  bool operator()(const std::string &dir);
};

std::set<std::string> missingIcons;

bool check_icon::operator()(const std::string &dir)
{
  if(filename[0] == '/')
    return std::filesystem::is_regular_file(filename);

  const std::array<const char *, 4> icon_exts = { { ".svg", ".gif", ".png", ".jpg" } };
  const std::string dirname = dir + "/icons";
  fdguard dirfd(dirname.c_str());
  if(unlikely(!dirfd.valid()))
    return false;

  std::string name = filename;

  for(unsigned int i = 0; i < icon_exts.size(); i++) {
    struct stat st;

    name += icon_exts[i];

    if(fstatat(dirfd, name.c_str(), &st, 0) == 0 && S_ISREG(st.st_mode))
      return true;
    name.erase(name.size() - strlen(icon_exts[i]));
  }
  return false;
}

void
err(const std::string &filename)
{
  std::cerr << filename << std::endl;
}

struct counter {
  unsigned int &groups;
  unsigned int &items;
  unsigned int &separators;
  unsigned int &combos;
  unsigned int &multis;
  unsigned int &list_entries;
  unsigned int &labels;
  unsigned int &keys;
  unsigned int &checks;
  unsigned int &refs;
  unsigned int &plinks;
  unsigned int &roles;
  unsigned int &list_entry_chunks;
  counter(unsigned int &gr,  unsigned int &it, unsigned int &sep, unsigned int &c,
          unsigned int &mu,  unsigned int &ce, unsigned int &lb,  unsigned int &ky,
          unsigned int &chk, unsigned int &rf, unsigned int &pl,  unsigned int &rl,
          unsigned int &lec)
    : groups(gr), items(it), separators(sep), combos(c), multis(mu), list_entries(ce),
      labels(lb), keys(ky), checks(chk), refs(rf), plinks(pl), roles(rl),
      list_entry_chunks(lec) {}
  void operator()(const presets_item_t *p);
  void operator()(const presets_element_t *w);
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
    assert(p->isItem());
    items++;
    const presets_item * const item = static_cast<const presets_item *>(p);
    std::for_each(item->widgets.begin(), item->widgets.end(), *this);
    roles += item->roles.size();
  }
}

void display_value_verifactor(const std::string &value) {
  assert(!value.empty());
}

void counter::operator()(const presets_element_t *w)
{
  switch(w->type) {
  case WIDGET_TYPE_LABEL:
    labels++;
    break;
  case WIDGET_TYPE_SEPARATOR:
  case WIDGET_TYPE_SPACE:
  case WIDGET_TYPE_TEXT:
    break;
  case WIDGET_TYPE_COMBO: {
    combos++;
    const presets_element_selectable *sel = static_cast<const presets_element_selectable *>(w);
    list_entries += sel->values.size();
    std::for_each(sel->display_values.begin(), sel->display_values.end(), display_value_verifactor);
    break;
  }
  case WIDGET_TYPE_MULTISELECT: {
    multis++;
    const presets_element_selectable *sel = static_cast<const presets_element_selectable *>(w);
    list_entries += sel->values.size();
    std::for_each(sel->display_values.begin(), sel->display_values.end(), display_value_verifactor);
    break;
  }
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
  case WIDGET_TYPE_CHUNK_LIST_ENTRIES:
    list_entry_chunks++;
    break;
  }
}

/**
 * @brief check icons of the given item
 * @param item the item to check
 */
void
checkItem(const presets_item_t *item)
{
  const presets_item_named * const vis = dynamic_cast<const presets_item_named *>(item);
  if(vis == nullptr)
    return;

  if(!vis->icon.empty()) {
    const std::vector<std::string>::const_iterator it = std::find_if(
                      basedirs.begin(), basedirs.end(), check_icon(vis->icon));
    if(it == basedirs.end())
      missingIcons.insert(vis->icon);
  }

  const presets_item_group * const group = dynamic_cast<const presets_item_group *>(vis);
  if(group != nullptr)
    std::for_each(group->items.begin(), group->items.end(), checkItem);
}

void
test_roles(const presets_items *presets)
{
  relation_t r;
  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("type", "multipolygon"));
  r.tags.replace(tags);

  // object type does not match
  node_t n(0, pos_t(0, 0), ID_ILLEGAL);
  std::set<std::string> roles = presets->roles(&r, object_t(&n));
  assert(roles.empty());

  way_t w;
  roles = presets->roles(&r, object_t(&w));
  assert_cmpnum(roles.size(), 2);
  assert(roles.find("inner") != roles.end());
  assert(roles.find("outer") != roles.end());

  // check count restriction
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("type", "boundary"));
  r.tags.replace(tags);

  roles = presets->roles(&r, object_t(&n));
  assert_cmpnum(roles.size(), 2);
  assert(roles.find("admin_centre") != roles.end());
  assert(roles.find("label") != roles.end());

  r.members.push_back(member_t(object_t(&n), "admin_centre"));

  node_t n2(0, pos_t(0, 0), ID_ILLEGAL);
  roles = presets->roles(&r, object_t(&n2));
  assert_cmpnum(roles.size(), 1);
  assert(roles.find("label") != roles.end());

  // check count restriction does not apply if it is 0
  roles = presets->roles(&r, object_t(&w));
  assert_cmpnum(roles.size(), 2);
  assert(roles.find("outer") != roles.end());
  assert(roles.find("inner") != roles.end());

  way_t w2;
  r.members.push_back(member_t(object_t(&w2), "outer"));

  roles = presets->roles(&r, object_t(&w));
  assert_cmpnum(roles.size(), 2);
  assert(roles.find("outer") != roles.end());
  assert(roles.find("inner") != roles.end());

  // check that also non-interactive presets are considered
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("type", "building"));
  r.tags.replace(tags);

  roles = presets->roles(&r, object_t(&n));
  assert_cmpnum(roles.size(), 1);
  assert(roles.find("entrance") != roles.end());

  // check that regexp-roles are not shown
  relation_t r2;
  roles = presets->roles(&r, object_t(&r2));
  assert_cmpnum(roles.size(), 0);

  r.cleanup();
}

}

int main(int argc, char **argv)
{
  xmlInitParser();

  basedirs.reserve(argc - 1);
  for(int i = 1; i < argc; i++)
    basedirs.push_back(argv[i]);

  std::unique_ptr<presets_items_internal> presets(static_cast<presets_items_internal *>(presets_items::load()));

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
  unsigned int multis = 0;
  unsigned int list_entries = 0;
  unsigned int labels = 0;
  unsigned int keys = 0;
  unsigned int checks = 0;
  unsigned int refs = 0;
  unsigned int plinks = 0;
  unsigned int roles = 0;
  unsigned int list_entry_chunks = 0;
  counter cnt(groups, items, separators, combos, multis, list_entries, labels, keys, checks, refs, plinks, roles, list_entry_chunks);

  std::for_each(presets->items.begin(), presets->items.end(), cnt);
  std::for_each(presets->chunks.begin(), presets->chunks.end(), cnt);

  std::cout
    << "chunks found: " << presets->chunks.size() << std::endl
    << "top level items found: " << presets->items.size() << std::endl
    << "groups: " << groups << std::endl
    << "items: " << items << std::endl
    << "separators: " << separators << std::endl
    << "combos: " << combos << std::endl
    << "multis: " << multis << std::endl
    << "list_entries: " << list_entries << std::endl
    << "labels: " << labels << std::endl
    << "keys: " << keys << std::endl
    << "checks: " << checks << std::endl
    << "references: " << refs << std::endl
    << "preset_links: " << plinks << std::endl
    << "roles: " << roles << std::endl
    << "list entry chunks: " << list_entry_chunks << std::endl;

  std::for_each(presets->items.begin(), presets->items.end(), checkItem);

  test_roles(presets.get());

  xmlCleanupParser();

  if(!missingIcons.empty()) {
    std::cerr << missingIcons.size() << " icons missing" << std::endl;
    std::for_each(missingIcons.begin(), missingIcons.end(), err);
    return 1;
  }

  return 0;
}

#include "appdata_dummy.h"
