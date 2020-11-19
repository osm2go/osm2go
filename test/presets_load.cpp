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
#include <osm2go_test.h>

static std::vector<std::string> basedirs;

namespace {

struct check_icon {
  const presets_item_named * const vis;
  explicit __attribute__((nonnull(2))) check_icon(const presets_item_named *v) : vis(v) {}
  bool operator()(const std::string &dir);
};

std::set<std::string> missingIcons;

bool check_icon::operator()(const std::string &dir)
{
  if(vis->icon[0] == '/')
    return std::filesystem::is_regular_file(vis->icon);

  const std::array<const char *, 4> icon_exts = { { ".svg", ".gif", ".png", ".jpg" } };
  const std::string dirname = dir + "/icons";
  fdguard dirfd(dirname.c_str());
  if(unlikely(!dirfd.valid()))
    return false;

  std::string name = vis->icon;

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
  std::map<presets_element_type_t, unsigned int> &element_counts;
  std::map<presets_item_t::item_type, unsigned int> &item_counter;
  unsigned int &list_entries;
  unsigned int &roles;
  counter(std::map<presets_element_type_t, unsigned int> &ec, std::map<presets_item_t::item_type, unsigned int> &ic,
          unsigned int &ce, unsigned int &rl)
    : element_counts(ec), item_counter(ic), list_entries(ce), roles(rl) {}
  void operator()(const presets_item_t *p);
  void operator()(const presets_element_t *w);
};

void counter::operator()(const presets_item_t *p)
{
  if(p->type & presets_item_t::TY_GROUP) {
    const presets_item_group * const gr = static_cast<const presets_item_group *>(p);
    std::for_each(gr->items.begin(), gr->items.end(), *this);
    item_counter[presets_item_t::TY_GROUP]++;
    return;
  } else if (p->type == presets_item_t::TY_SEPARATOR) {
    item_counter[presets_item_t::TY_SEPARATOR]++;
  } else {
    assert(p->isItem());
    item_counter[presets_item_t::TY_ALL]++;
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
    element_counts[WIDGET_TYPE_LABEL]++;
    break;
  case WIDGET_TYPE_SEPARATOR:
  case WIDGET_TYPE_SPACE:
  case WIDGET_TYPE_TEXT:
    break;
  case WIDGET_TYPE_COMBO: {
    element_counts[WIDGET_TYPE_COMBO]++;
    const presets_element_selectable *sel = static_cast<const presets_element_selectable *>(w);
    list_entries += sel->values.size();
    std::for_each(sel->display_values.begin(), sel->display_values.end(), display_value_verifactor);
    break;
  }
  case WIDGET_TYPE_MULTISELECT: {
    element_counts[WIDGET_TYPE_MULTISELECT]++;
    const presets_element_selectable *sel = static_cast<const presets_element_selectable *>(w);
    list_entries += sel->values.size();
    std::for_each(sel->display_values.begin(), sel->display_values.end(), display_value_verifactor);
    break;
  }
  case WIDGET_TYPE_CHECK:
  case WIDGET_TYPE_KEY:
  case WIDGET_TYPE_REFERENCE:
  case WIDGET_TYPE_LINK:
  case WIDGET_TYPE_CHUNK_LIST_ENTRIES:
  case WIDGET_TYPE_CHUNK_ROLE_ENTRIES:
    element_counts[w->type]++;
    break;
  case WIDGET_TYPE_CHUNK_CONTAINER:
    assert_unreachable();
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

  if(!vis->icon.empty() && std::none_of(basedirs.begin(), basedirs.end(), check_icon(vis)))
    missingIcons.insert(vis->icon);

  const presets_item_group * const group = dynamic_cast<const presets_item_group *>(vis);
  if(group != nullptr)
    std::for_each(group->items.begin(), group->items.end(), checkItem);
}

void
test_mp_member_roles(const relation_t &mp, way_t &w, way_t &cw, const presets_items *presets)
{
  assert(mp.is_multipolygon());
  assert(!w.is_closed());
  assert(cw.is_closed());

  std::array<way_t *, 2> wp = {{ &w, &cw }};

  for (unsigned int i = 0; i < wp.size(); i++) {
    const std::set<std::string> roles = presets->roles(&mp, object_t(wp.at(i)));
    assert_cmpnum(roles.size(), 2);
    assert(roles.find("inner") != roles.end());
    assert(roles.find("outer") != roles.end());
  }

  // there should be no roles for a node
  const std::set<std::string> roles = presets->roles(&mp, object_t(cw.node_chain.front()));
  assert_cmpnum(roles.size(), 0);
}

void
test_roles(const presets_items *presets)
{
  relation_t mp;
  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("type", "multipolygon"));
  mp.tags.replace(tags);

  // object type does not match
  node_t n(base_attributes(), lpos_t(0, 0), pos_t(0, 0));
  node_t n2(base_attributes(), lpos_t(1, 0), pos_t(1, 0));

  way_t w;
  // closed way
  way_t cw;
  cw.node_chain.push_back(&n);
  cw.node_chain.push_back(&n2);
  cw.node_chain.push_back(&n);

  test_mp_member_roles(mp, w, cw, presets);

  // make sure that even with more tags the relation is still handled as multipolygon
  tags.insert(osm_t::TagMap::value_type("landuse", "commercial"));
  mp.tags.replace(tags);

  test_mp_member_roles(mp, w, cw, presets);

  std::set<std::string> roles = presets->roles(&mp, object_t(&cw));
  assert_cmpnum(roles.size(), 2);
  assert(roles.find("inner") != roles.end());
  assert(roles.find("outer") != roles.end());

  // check count restriction
  relation_t r;
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("type", "boundary"));
  r.tags.replace(tags);

  roles = presets->roles(&r, object_t(&n));
  assert_cmpnum(roles.size(), 2);
  assert(roles.find("admin_centre") != roles.end());
  assert(roles.find("label") != roles.end());

  r.members.push_back(member_t(object_t(&n), "admin_centre"));

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

  // roles on invalid objects should just be empty
  roles = presets->roles(&r, object_t());
  assert_cmpnum(roles.size(), 0);

  assert_cmpnum(presets->roles(&r, object_t(object_t::NODE_ID, 1234)).size(), 0);
  assert_cmpnum(presets->roles(&r, object_t(object_t::WAY_ID, 1234)).size(), 0);
  assert_cmpnum(presets->roles(&r, object_t(object_t::RELATION_ID, 1234)).size(), 0);

  // check that the roles for some special types are returned correctly
  relation_t site;
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("type", "site"));
  site.tags.replace(tags);

  roles = presets->roles(&site, object_t(&w));
  assert_cmpnum(roles.size(), 1);
  assert_cmpstr(*roles.begin(), std::string());

  roles = presets->roles(&site, object_t(&cw));
  assert_cmpnum(roles.size(), 2);
  assert(std::find(roles.begin(), roles.end(), std::string()) != roles.end());
  assert(std::find(roles.begin(), roles.end(), "perimeter") != roles.end());

  roles = presets->roles(&site, object_t(&r));
  assert_cmpnum(roles.size(), 0);

  roles = presets->roles(&site, object_t(&mp));
  assert_cmpnum(roles.size(), 2);
  assert(std::find(roles.begin(), roles.end(), std::string()) != roles.end());
  assert(std::find(roles.begin(), roles.end(), "perimeter") != roles.end());

  // check that closedway is no way
  relation_t ski;
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("type", "route"));
  tags.insert(osm_t::TagMap::value_type("route", "ski"));
  ski.tags.replace(tags);

  roles = presets->roles(&ski, object_t(&cw));
  assert_cmpnum(roles.size(), 0);

  roles = presets->roles(&ski, object_t(&w));
  assert_cmpnum(roles.size(), 6);
}

} // namespace

int main(int argc, char **argv)
{
  OSM2GO_TEST_INIT(argc, argv);

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

  std::map<presets_element_type_t, unsigned int> element_counts;
  std::map<presets_item_t::item_type, unsigned int> item_counter;
  unsigned int list_entries = 0;
  unsigned int roles = 0;
  counter cnt(element_counts, item_counter, list_entries, roles);

  std::for_each(presets->items.begin(), presets->items.end(), cnt);
  std::for_each(presets->chunks.begin(), presets->chunks.end(), cnt);

  std::cout
    << "chunks found: " << presets->chunks.size() << std::endl
    << "top level items found: " << presets->items.size() << std::endl
    << "groups: " << item_counter[presets_item_t::TY_GROUP] << std::endl
    << "items: " << item_counter[presets_item_t::TY_ALL] << std::endl
    << "separators: " << item_counter[presets_item_t::TY_SEPARATOR] << std::endl
    << "combos: " << element_counts[WIDGET_TYPE_COMBO] << std::endl
    << "multis: " << element_counts[WIDGET_TYPE_MULTISELECT] << std::endl
    << "list_entries: " << list_entries << std::endl
    << "labels: " << element_counts[WIDGET_TYPE_LABEL] << std::endl
    << "keys: " << element_counts[WIDGET_TYPE_KEY] << std::endl
    << "checks: " << element_counts[WIDGET_TYPE_CHECK] << std::endl
    << "references: " << element_counts[WIDGET_TYPE_REFERENCE] << std::endl
    << "preset_links: " << element_counts[WIDGET_TYPE_LINK] << std::endl
    << "roles: " << roles << std::endl
    << "list entry chunks: " << element_counts[WIDGET_TYPE_CHUNK_LIST_ENTRIES] << std::endl
    << "role entry chunks: " << element_counts[WIDGET_TYPE_CHUNK_ROLE_ENTRIES] << std::endl;

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

#include "dummy_appdata.h"
