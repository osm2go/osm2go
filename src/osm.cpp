/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
 *
 * This file is part of OSM2Go.
 *
 * OSM2Go is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OSM2Go is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _DEFAULT_SOURCE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "osm.h"

#include "cache_set.h"
#include "icon.h"
#include "map.h"
#include "misc.h"
#include "osm2go_platform.h"
#include "pos.h"
#include "xml_helpers.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <numeric>
#include <string>
#include <strings.h>
#include <utility>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

static_assert(sizeof(tag_list_t) == sizeof(tag_t *), "tag_list_t is not exactly as big as a pointer");

static cache_set value_cache; ///< the cache for key, value, and role strings

bool object_t::operator==(const object_t &other) const noexcept
{
  if (type != other.type) {
    if ((type | _REF_FLAG) != (other.type | _REF_FLAG))
      return false;
    // we only handle the other case
    if(type & _REF_FLAG)
      return other == *this;
    switch(type) {
    case NODE:
    case WAY:
    case RELATION:
      return obj->id == other.id;
    default:
      assert_unreachable();
    }
  }

  switch(type) {
  case NODE:
  case WAY:
  case RELATION:
    return obj == other.obj;
  case NODE_ID:
  case WAY_ID:
  case RELATION_ID:
    return id == other.id;
  case ILLEGAL:
    return true;
  default:
    assert_unreachable();
  }
}

bool object_t::operator==(const node_t *n) const noexcept {
  return type == NODE && node == n;
}

bool object_t::operator==(const way_t *w) const noexcept {
  return type == WAY && way == w;
}

bool object_t::operator==(const relation_t *r) const noexcept {
  return type == RELATION && relation == r;
}

bool object_t::is_real() const noexcept {
  return (type == NODE) ||
         (type == WAY)  ||
         (type == RELATION);
}

/* return plain text of type */
static std::map<object_t::type_t, const char *> type_string_init()
{
  std::map<object_t::type_t, const char *> types;

  types[object_t::ILLEGAL] =     "illegal";
  types[object_t::NODE] =        "node";
  types[object_t::WAY] =         "way/area";
  types[object_t::RELATION] =    "relation";
  types[object_t::NODE_ID] =     "node id";
  types[object_t::WAY_ID] =      "way/area id";
  types[object_t::RELATION_ID] = "relation id";

  return types;
}

const char *object_t::type_string() const {
  static std::map<type_t, const char *> types = type_string_init();

  const std::map<type_t, const char *>::const_iterator it = types.find(type);

  if(likely(it != types.end()))
    return it->second;

  return nullptr;
}

std::string object_t::id_string() const {
  assert_cmpnum_op(type, !=, ILLEGAL);

  return std::to_string(get_id());
}

item_id_t object_t::get_id() const noexcept {
  if(unlikely(type == ILLEGAL))
    return ID_ILLEGAL;
  if(is_real())
    return obj->id;
  return id;
}

/* ------------------------- user handling --------------------- */

struct cmp_user {
  const char * const uname;
  explicit cmp_user(const char *u) : uname(u) {}
  bool operator()(const std::string &s) {
    return (strcasecmp(s.c_str(), uname) == 0);
  }
};

/**
 * @brief insert a username into osm_t::users if needed
 * @param osm the osm object
 * @param name the username
 * @param uid the user id as returned by the server
 * @returns the id in the user map
 *
 * In case no userid is given a temporary one will be created.
 */
static int osm_user_insert(osm_t::ref osm, const char *name, int uid) {
  if(unlikely(!name)) {
    osm->users[0] = std::string();
    return 0;
  }

  const std::map<int, std::string>::const_iterator itEnd = osm->users.end();
  /* search through user list */
  if(likely(uid > 0)) {
    const std::map<int, std::string>::const_iterator it = osm->users.find(uid);
    if(unlikely(it == itEnd))
      osm->users[uid] = name;

    return uid;
  } else {
    // no virtual user found
    if(osm->users.empty() || osm->users.begin()->first > 0) {
      osm->users[-1] = name;
      return -1;
    }
    /* check if ay of the temporary ids already matches the name */
    std::map<int, std::string>::const_iterator it = osm->users.begin();
    for(; it != itEnd && it->first < 0; it++)
      if(it->second == name)
        return it->first;
    // generate a new temporary id
    // it is already one in there, so use one less as the lowest existing id
    int id = osm->users.begin()->first - 1;
    osm->users[id] = name;
    return id;
  }
}

static
time_t __attribute__((nonnull(1))) convert_iso8601(const char *str) {
  struct tm ctime;
  memset(&ctime, 0, sizeof(ctime));
  strptime(str, "%FT%T%z", &ctime);

  long gmtoff = ctime.tm_gmtoff;

  return timegm(&ctime) - gmtoff;
}

/* -------------------- tag handling ----------------------- */

/**
 * @brief fill tag_t from XML values
 * @param k the key found in XML
 * @param v the value found in XML
 * @param tags the vector where the new tag will be added
 *
 * k and v will be freed.
 */
static void tag_from_xml(xmlChar *k, xmlChar *v, std::vector<tag_t> &tags) {
  const char *key = reinterpret_cast<char *>(k);
  const char *value = reinterpret_cast<char *>(v);

  if(likely(key != nullptr && value != nullptr && *key != '\0' && *value != '\0'))
    tags.push_back(tag_t(key, value));
  else
    printf("incomplete tag key/value %s/%s\n", k, v);

  xmlFree(k);
  xmlFree(v);
}

bool osm_t::parse_tag(xmlNode *a_node, TagMap &tags) {
  xmlString key(xmlGetProp(a_node, BAD_CAST "k"));
  xmlString value(xmlGetProp(a_node, BAD_CAST "v"));

  if(unlikely(!key || !value || strlen(reinterpret_cast<char *>(key.get())) == 0 ||
                                  strlen(reinterpret_cast<char *>(value.get())) == 0)) {
    return false;
  }

  std::string k = reinterpret_cast<char *>(key.get());
  std::string v = reinterpret_cast<char *>(value.get());

  if(unlikely(tags.findTag(k, v) != tags.end()))
    return false;

  tags.insert(TagMap::value_type(k, v));

  return true;
}

struct map_value_match_functor {
  const std::string &value;
  explicit map_value_match_functor(const std::string &v) : value(v) {}
  bool operator()(const osm_t::TagMap::value_type &pair) {
    return pair.second == value;
  }
};

osm_t::TagMap::iterator osm_t::TagMap::findTag(const std::string &key, const std::string &value)
{
  std::pair<osm_t::TagMap::iterator, osm_t::TagMap::iterator> matches = equal_range(key);
  if(matches.first == matches.second)
    return end();
  osm_t::TagMap::iterator it = std::find_if(matches.first, matches.second, map_value_match_functor(value));
  return it == matches.second ? end() : it;
}

bool osm_t::tagSubset(const TagMap &sub, const TagMap &super)
{
  const TagMap::const_iterator superEnd = super.end();
  const TagMap::const_iterator itEnd = sub.end();
  for(TagMap::const_iterator it = sub.begin(); it != itEnd; it++)
    if(super.findTag(it->first, it->second) == superEnd)
      return false;
  return true;
}

struct relation_object_replacer {
  const object_t &old;
  const object_t &replace;
  relation_object_replacer(const object_t &t, const object_t &n) : old(t), replace(n) {}
  inline void operator()(std::pair<item_id_t, relation_t *> pair)
  { operator()(pair.second); }
  void operator()(relation_t *r);
};

void relation_object_replacer::operator()(relation_t *r)
{
  const std::vector<member_t>::iterator itBegin = r->members.begin();
  std::vector<member_t>::iterator itEnd = r->members.end();

  for(std::vector<member_t>::iterator it = itBegin; it != itEnd; it++) {
    if(it->object != old)
      continue;

    printf("  found %s #" ITEM_ID_FORMAT " in relation #" ITEM_ID_FORMAT "\n",
          old.type_string(), old.get_id(), r->id);

    it->object = replace;

    // check if this member now is the same as the next or previous one
    if((it != itBegin && *(it - 1) == *it) || (it + 1 != itEnd && *it == *(it + 1))) {
      it = r->members.erase(it);
      // this is now the next element, go one back so this is actually checked
      // as the for loop increments the iterator again
      if(likely(it != itBegin))
        it--;

      // end iterator changed because container was modified, update it
      itEnd = r->members.end();
    }

    r->flags |= OSM_FLAG_DIRTY;
  }
}

struct relation_membership_functor {
  std::vector<relation_t *> &arels, &brels;
  const object_t &a, &b;
  explicit relation_membership_functor(const object_t &first, const object_t &second,
                                       std::vector<relation_t *> &firstRels,
                                       std::vector<relation_t *> &secondRels)
    : arels(firstRels), brels(secondRels), a(first), b(second) {}
  void operator()(const std::pair<item_id_t, relation_t *> &p);
};

void relation_membership_functor::operator()(const std::pair<item_id_t, relation_t *>& p)
{
  relation_t * const rel = p.second;
  const std::vector<member_t>::const_iterator itEnd = rel->members.end();
  bool aFound = false, bFound = false;
  for(std::vector<member_t>::const_iterator it = rel->members.begin();
      it != itEnd && (!aFound || !bFound); it++) {
    if(*it == a) {
      if(!aFound) {
        arels.push_back(rel);
        aFound = true;
      }
    } else if(*it == b) {
      if(!bFound) {
        brels.push_back(rel);
        bFound = true;
      }
    }
  }
}

bool osm_t::checkObjectPersistence(const object_t &first, const object_t &second, std::vector<relation_t *> &rels) const
{
  object_t keep = first, remove = second;

  std::vector<relation_t *> removeRels, keepRels;

  std::for_each(relations.begin(), relations.end(), relation_membership_functor(remove, keep, removeRels, keepRels));

  // find out which node to keep
  bool nret =
              // if one is new: keep the other one
              (keep.obj->isNew() && !remove.obj->isNew()) ||
              // or keep the one with most relations
              removeRels.size() > keepRels.size() ||
              // or the one with most ways (if nodes)
              (keep.type == object_t::NODE && remove.type == keep.type &&
               remove.node->ways > keep.node->ways) ||
              // or the one with most nodes (if ways)
              (keep.type == object_t::WAY && remove.type == keep.type &&
               remove.way->node_chain.size() > keep.way->node_chain.size()) ||
              // or the one with most members (if relations)
              (keep.type == object_t::RELATION && remove.type == keep.type &&
               remove.relation->members.size() > keep.relation->members.size()) ||
              // or the one with the longest history
              remove.obj->version > keep.obj->version ||
              // or simply the older one
              (remove.obj->id > 0 && remove.obj->id < keep.obj->id);

  if(nret)
    rels.swap(keepRels);
  else
    rels.swap(removeRels);

  return !nret;
}

node_t *osm_t::mergeNodes(node_t *first, node_t *second, bool &conflict)
{
  node_t *keep = first, *remove = second;

  std::vector<relation_t *> rels;
  if(!checkObjectPersistence(object_t(keep), object_t(remove), rels))
    std::swap(keep, remove);

  /* use "second" position as that was the target */
  keep->lpos = second->lpos;
  keep->pos = second->pos;

  const std::map<item_id_t, way_t *>::iterator witEnd = ways.end();
  for(std::map<item_id_t, way_t *>::iterator wit = ways.begin();
      remove->ways > 0 && wit != witEnd; wit++) {
    way_t * const way = wit->second;
    const node_chain_t::iterator itBegin = way->node_chain.begin();
    node_chain_t::iterator it = itBegin;
    node_chain_t::iterator itEnd = way->node_chain.end();

    while(remove->ways > 0 && (it = std::find(it, itEnd, remove)) != itEnd) {
      printf("  found node in way #" ITEM_ID_FORMAT "\n", way->id);

      // check if this node is the same as the neighbor
      if((it != itBegin && *(it - 1) == keep) || (it +1 != itEnd && *(it + 1) == keep)) {
        // this node would now be twice in the way at adjacent positions
        it = way->node_chain.erase(it);
        itEnd = way->node_chain.end();
      } else {
        /* replace by keep */
        *it = keep;
        // no need to check this one again
        it++;
        keep->ways++;
      }

      /* and adjust way references of remove */
      assert_cmpnum_op(remove->ways, >, 0);
      remove->ways--;

      way->flags |= OSM_FLAG_DIRTY;
    }
  }
  assert_cmpnum(remove->ways, 0);

  /* replace "remove" in relations */
  std::for_each(rels.begin(), rels.end(),
                relation_object_replacer(object_t(remove), object_t(keep)));

  /* transfer tags from "remove" to "keep" */
  conflict = keep->tags.merge(remove->tags);

  /* remove must not have any references to ways anymore */
  assert_cmpnum(remove->ways, 0);

  node_delete(remove, false);

  keep->flags |= OSM_FLAG_DIRTY;

  return keep;
}

way_t *osm_t::mergeWays(way_t *first, way_t *second, bool &conflict)
{
  std::vector<relation_t *> rels;
  if(!checkObjectPersistence(object_t(first), object_t(second), rels))
    std::swap(first, second);

  /* ---------- transfer tags from second to first ----------- */
  conflict = first->merge(second, this, rels);

  return first;
}

template<typename T> bool map_is_clean(const T &map, int flagmask = ~0) {
  const typename T::const_iterator itEnd = map.end();
  return itEnd == std::find_if(map.begin(), itEnd, osm_t::find_object_by_flags(flagmask));
}

/* return true if no diff needs to be saved */
bool osm_t::is_clean(bool honor_hidden_flags) const
{
  // fast check: if any map contains an object with a negative id something
  // was added, so saving is needed
  if(!nodes.empty() && nodes.begin()->first < 0)
    return false;
  if(!ways.empty() && ways.begin()->first < 0)
    return false;
  if(!relations.empty() && relations.begin()->first < 0)
    return false;

  // now check all objects for modifications
  if(!map_is_clean(nodes))
    return false;
  int flagmask = honor_hidden_flags ? ~0 : ~OSM_FLAG_HIDDEN;
  if(!map_is_clean(ways, flagmask))
    return false;
  return map_is_clean(relations);
}

struct tag_match_functor {
  const tag_t &other;
  const bool same_values;
  tag_match_functor(const tag_t &o, bool s) : other(o), same_values(s) {}
  bool operator()(const tag_t &tag) {
    return (strcasecmp(other.key, tag.key) == 0) &&
           ((strcasecmp(other.value, tag.value) == 0) == same_values);
  }
};

bool tag_list_t::merge(tag_list_t &other)
{
  if(other.empty())
    return false;

  if(empty()) {
    delete contents; // just to be sure not to leak if an empty vector is around
    contents = other.contents;
    other.contents = nullptr;
    return false;
  }

  bool conflict = false;

  /* ---------- transfer tags from way[1] to way[0] ----------- */
  const std::vector<tag_t>::const_iterator itEnd = other.contents->end();
  for(std::vector<tag_t>::iterator srcIt = other.contents->begin();
      srcIt != itEnd; srcIt++) {
    tag_t &src = *srcIt;
    /* don't copy "created_by" tag or tags that already */
    /* exist in identical form */
    if(!src.is_creator_tag() && !contains(tag_match_functor(src, true))) {
      /* check if same key but with different value is present */
      if(!conflict)
        conflict = contains(tag_match_functor(src, false));
      contents->push_back(src);
    }
  }

  delete other.contents;
  other.contents = nullptr;

  return conflict;
}

static inline bool is_creator_tag(const tag_t &tag) {
  return tag.is_creator_tag();
}

struct tag_find_functor {
  const char * const needle;
  explicit tag_find_functor(const char *n) : needle(n) {}
  bool operator()(const tag_t &tag) {
    return (strcmp(needle, tag.key) == 0);
  }
};

bool tag_list_t::operator!=(const std::vector<tag_t> &t2) const {
  if(empty() && t2.empty())
    return false;

  // Special case for an empty list as contents is not set in this case and
  // must not be dereferenced. Check if t2 only consists of a creator tag, in
  // which case both lists would still be considered the same, or not. Not
  // further checks need to be done for the end result.
  const std::vector<tag_t>::const_iterator t2start = t2.begin();
  const std::vector<tag_t>::const_iterator t2End = t2.end();
  bool t2HasCreator = (std::find_if(t2start, t2End, is_creator_tag) != t2End);
  if(empty())
    return t2HasCreator ? (t2.size() != 1) : !t2.empty();

  /* first check list length, otherwise deleted tags are hard to detect */
  std::vector<tag_t>::size_type ocnt = contents->size();
  std::vector<tag_t>::const_iterator t1it = contents->begin();
  const std::vector<tag_t>::const_iterator t1End = contents->end();
  const std::vector<tag_t>::const_iterator t1cit = std::find_if(t1it, t1End, is_creator_tag);

  if(t2HasCreator)
    ocnt++;

  // ocnt can't become negative here as it was checked before that contents is not empty
  if(t1cit != t1End)
    ocnt--;

  if (t2.size() != ocnt)
    return true;

  for (; t1it != t1End; t1it++) {
    if (t1it == t1cit)
      continue;
    const tag_t &ntag = *t1it;

    const std::vector<tag_t>::const_iterator it = std::find_if(t2start, t2End,
                                                               tag_find_functor(ntag.key));

    // key not found
    if(it == t2End)
      return true;
    // different value
    if(strcmp(ntag.value, it->value) != 0)
      return true;
  }

  return false;
}

bool tag_list_t::operator!=(const osm_t::TagMap &t2) const {
  if(empty() && t2.empty())
    return false;

  // Special case for an empty list as contents is not set in this case and
  // must not be dereferenced. Check if t2 only consists of a creator tag, in
  // which case both lists would still be considered the same, or not. No
  // further checks need to be done for the end result.
  const osm_t::TagMap::const_iterator t2End = t2.end();
  bool t2HasCreator = (t2.find("created_by") != t2End);
  if(empty())
    return t2HasCreator ? (t2.size() != 1) : !t2.empty();

  /* first check list length, otherwise deleted tags are hard to detect */
  std::vector<tag_t>::size_type ocnt = contents->size();
  std::vector<tag_t>::const_iterator t1it = contents->begin();
  const std::vector<tag_t>::const_iterator t1End = contents->end();
  const std::vector<tag_t>::const_iterator t1cit = std::find_if(t1it, t1End, is_creator_tag);

  if(t2HasCreator)
    ocnt++;

  // ocnt can't become negative here as it was checked before that contents is not empty
  if(t1cit != t1End)
    ocnt--;

  if (t2.size() != ocnt)
    return true;

  for (; t1it != t1End; t1it++) {
    if (t1it == t1cit)
      continue;
    const tag_t &ntag = *t1it;

    std::pair<osm_t::TagMap::const_iterator, osm_t::TagMap::const_iterator> its = t2.equal_range(ntag.key);

    // key not found
    if(its.first == its.second)
      return true;
    // check different values
    for(; its.first != its.second; its.first++)
      if(its.first->second == ntag.value)
        break;
    // none of the values matched
    if(its.first == its.second)
      return true;
  }

  return false;
}

struct collision_functor {
  const tag_t &tag;
  explicit collision_functor(const tag_t &t) : tag(t) { }
  bool operator()(const tag_t &t) {
    return (strcasecmp(t.key, tag.key) == 0);
  }
};

bool tag_list_t::hasTagCollisions() const
{
  if(empty())
    return false;

  const std::vector<tag_t>::const_iterator itEnd = contents->end();
  for(std::vector<tag_t>::const_iterator it = contents->begin();
      it + 1 != itEnd; it++) {
    if (std::find_if(it + 1, itEnd, collision_functor(*it)) != itEnd)
      return true;
  }
  return false;
}

/* ------------------- node handling ------------------- */

void osm_t::node_free(node_t *node) {
  nodes.erase(node->id);

  /* there must not be anything left in this chain */
  assert_null(node->map_item_chain);

  delete node;
}

static inline void nodefree(std::pair<item_id_t, node_t *> pair) {
  delete pair.second;
}

/* ------------------- way handling ------------------- */
static void osm_unref_node(node_t* node)
{
  assert_cmpnum_op(node->ways, >, 0);
  node->ways--;
}

void osm_node_chain_free(node_chain_t &node_chain) {
  std::for_each(node_chain.begin(), node_chain.end(), osm_unref_node);
}

void osm_t::way_free(way_t *way) {
  ways.erase(way->id);
  way->cleanup();
  delete way;
}

static void way_free(std::pair<item_id_t, way_t *> pair) {
  pair.second->cleanup();
  delete pair.second;
}

node_t *osm_t::parse_way_nd(xmlNode *a_node) const {
  xmlString prop(xmlGetProp(a_node, BAD_CAST "ref"));
  node_t *node = nullptr;

  if(prop) {
    item_id_t id = strtoll(reinterpret_cast<char *>(prop.get()), nullptr, 10);

    /* search matching node */
    node = node_by_id(id);
    if(!node)
      printf("Node id " ITEM_ID_FORMAT " not found\n", id);
    else
      node->ways++;
  }

  return node;
}

/* ------------------- relation handling ------------------- */

bool relation_t::is_multipolygon() const {
  const char *tp = tags.get_value("type");
  return tp && (strcmp(tp, "multipolygon") == 0);
}

void relation_t::cleanup() {
  tags.clear();
  members.clear();
}

void relation_t::remove_member(std::vector<member_t>::iterator it)
{
  printf("remove object " ITEM_ID_FORMAT " type %s from relation #" ITEM_ID_FORMAT "\n",
         it->object.get_id(), it->object.type_string(), id);

  assert(it->object.is_real());
  assert(it != members.end());

  members.erase(it);

  flags |= OSM_FLAG_DIRTY;
}

struct gen_xml_relation_functor {
  xmlNodePtr const xml_node;
  explicit gen_xml_relation_functor(xmlNodePtr n) : xml_node(n) {}
  void operator()(const member_t &member);
};

void gen_xml_relation_functor::operator()(const member_t &member)
{
  xmlNodePtr m_node = xmlNewChild(xml_node,nullptr,BAD_CAST "member", nullptr);

  const char *typestr;
  switch(member.object.type) {
  case object_t::NODE:
  case object_t::NODE_ID:
    typestr = node_t::api_string();
    break;
  case object_t::WAY:
  case object_t::WAY_ID:
    typestr = way_t::api_string();
    break;
  case object_t::RELATION:
  case object_t::RELATION_ID:
    typestr = relation_t::api_string();
    break;
  default:
    assert_unreachable();
  }

  xmlNewProp(m_node, BAD_CAST "type", BAD_CAST typestr);
  xmlNewProp(m_node, BAD_CAST "ref", BAD_CAST member.object.id_string().c_str());

  if(member.role)
    xmlNewProp(m_node, BAD_CAST "role", BAD_CAST member.role);
}

void relation_t::generate_member_xml(xmlNodePtr xml_node) const
{
  std::for_each(members.begin(), members.end(), gen_xml_relation_functor(xml_node));
}

void osm_t::relation_free(relation_t *relation) {
  relations.erase(relation->id);
  relation->cleanup();
  delete relation;
}

static void osm_relation_free_pair(std::pair<item_id_t, relation_t *> pair) {
  pair.second->cleanup();
  delete pair.second;
}

bool osm_t::parse_relation_member(const char *tp, const char *refstr, const char *role, std::vector<member_t> &members) {
  if(unlikely(tp == nullptr)) {
    printf("missing type for relation member\n");
    return false;
  }
  if(unlikely(refstr == nullptr)) {
    printf("missing ref for relation member\n");
    return false;
  }

  object_t::type_t type;
  if(strcmp(tp, way_t::api_string()) == 0)
    type = object_t::WAY;
  else if(strcmp(tp, node_t::api_string()) == 0)
    type = object_t::NODE;
  else if(likely(strcmp(tp, relation_t::api_string()) == 0))
    type = object_t::RELATION;
  else {
    printf("Unable to store illegal type '%s'\n", tp);
    return false;
  }

  char *endp;
  item_id_t id = strtoll(refstr, &endp, 10);
  if(unlikely(*endp != '\0')) {
    printf("Illegal ref '%s' for relation member\n", refstr);
    return false;
  }

  object_t obj(type);

  switch(type) {
  case object_t::WAY:
    /* search matching way */
    obj.way = way_by_id(id);
    break;

  case object_t::NODE:
    /* search matching node */
    obj.node = node_by_id(id);
    break;

  case object_t::RELATION:
    /* search matching relation */
    obj.relation = relation_by_id(id);
    break;
  default:
    assert_unreachable();
  }

  if(!obj.obj) {
    obj.type = static_cast<object_t::type_t>(type | object_t::_REF_FLAG);
    obj.id = id;
  }

  if(role != nullptr && strlen(role) == 0)
    role = nullptr;

  members.push_back(member_t(obj, role));
  return true;
}

void osm_t::parse_relation_member(xmlNode *a_node, std::vector<member_t> &members) {
  xmlString tp(xmlGetProp(a_node, BAD_CAST "type"));
  xmlString refstr(xmlGetProp(a_node, BAD_CAST "ref"));
  xmlString role(xmlGetProp(a_node, BAD_CAST "role"));

  parse_relation_member(reinterpret_cast<char *>(tp.get()), reinterpret_cast<char *>(refstr.get()),
                        reinterpret_cast<char *>(role.get()), members);
}

/* try to find something descriptive */
std::string relation_t::descriptive_name() const {
  std::array<const char *, 5> keys = { { "name", "ref", "description", "note", "fix" "me" } };
  for (unsigned int i = 0; i < keys.size(); i++) {
    const char *name = tags.get_value(keys[i]);
    if(name)
      return name;
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "<ID #" ITEM_ID_FORMAT ">", id);
  return buf;
}

/* -------------------------- stream parser ------------------- */

#include <libxml/xmlreader.h>

static inline int __attribute__((nonnull(2))) my_strcmp(const xmlChar *a, const xmlChar *b) {
  if(!a) return -1;
  return strcmp(reinterpret_cast<const char *>(a), reinterpret_cast<const char *>(b));
}

/* skip current element incl. everything below (mainly for testing) */
static void skip_element(xmlTextReaderPtr reader) {
  assert_cmpnum(xmlTextReaderNodeType(reader), XML_READER_TYPE_ELEMENT);
  if(xmlTextReaderIsEmptyElement(reader))
    return;

  int depth = xmlTextReaderDepth(reader);
  const xmlChar *name = xmlTextReaderConstName(reader);
  assert(name != nullptr);

  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) > depth) ||
	 (my_strcmp(xmlTextReaderConstName(reader), name) != 0))) {
    ret = xmlTextReaderRead(reader);
  }
}

/* parse bounds */
static bool process_bounds(xmlTextReaderPtr reader, bounds_t &bounds) {
  if(unlikely(!bounds.init(pos_area(pos_t::fromXmlProperties(reader, "minlat", "minlon"),
                                    pos_t::fromXmlProperties(reader, "maxlat", "maxlon"))))) {
    fprintf(stderr, "Invalid coordinate in bounds (%f/%f/%f/%f)\n",
            bounds.ll.min.lat, bounds.ll.min.lon,
            bounds.ll.max.lat, bounds.ll.max.lon);

    return false;
  }

  /* skip everything below */
  skip_element(reader);

  bounds.min = bounds.ll.min.toLpos();
  bounds.min.x -= bounds.center.x;
  bounds.min.y -= bounds.center.y;
  bounds.min.x *= bounds.scale;
  bounds.min.y *= bounds.scale;

  bounds.max = bounds.ll.max.toLpos();
  bounds.max.x -= bounds.center.x;
  bounds.max.y -= bounds.center.y;
  bounds.max.x *= bounds.scale;
  bounds.max.y *= bounds.scale;

  return true;
}

static void process_tag(xmlTextReaderPtr reader, std::vector<tag_t> &tags) {
  tag_from_xml(xmlTextReaderGetAttribute(reader, BAD_CAST "k"),
               xmlTextReaderGetAttribute(reader, BAD_CAST "v"), tags);
  skip_element(reader);
}

static void process_base_attributes(base_object_t *obj, xmlTextReaderPtr reader, osm_t::ref osm)
{
  xmlString prop(xmlTextReaderGetAttribute(reader, BAD_CAST "id"));
  if(likely(prop))
    obj->id = strtoll(reinterpret_cast<char *>(prop.get()), nullptr, 10);

  /* new in api 0.6: */
  prop.reset(xmlTextReaderGetAttribute(reader, BAD_CAST "version"));
  if(likely(prop))
    obj->version = strtoul(reinterpret_cast<char *>(prop.get()), nullptr, 10);

  prop.reset(xmlTextReaderGetAttribute(reader, BAD_CAST "user"));
  if(likely(prop)) {
    int uid = -1;
    xmlString puid(xmlTextReaderGetAttribute(reader, BAD_CAST "uid"));
    if(likely(puid)) {
      char *endp;
      uid = strtol(reinterpret_cast<char *>(puid.get()), &endp, 10);
      if(unlikely(*endp)) {
        printf("WARNING: cannot parse uid '%s' for user '%s'\n", puid.get(), prop.get());
        uid = -1;
      }
    }
    obj->user = osm_user_insert(osm, reinterpret_cast<char *>(prop.get()), uid);
  }

  prop.reset(xmlTextReaderGetAttribute(reader, BAD_CAST "timestamp"));
  if(likely(prop))
    obj->time = convert_iso8601(reinterpret_cast<char *>(prop.get()));
}

static void process_node(xmlTextReaderPtr reader, osm_t::ref osm) {
  const pos_t pos = pos_t::fromXmlProperties(reader);

  /* allocate a new node structure */
  node_t *node = osm->node_new(pos);
  // reset the flags, this object comes from upstream OSM
  node->flags = 0;

  process_base_attributes(node, reader, osm);

  size_t cnt = osm->nodes.size();
  osm->nodes[node->id] = node;
  assert_cmpnum(osm->nodes.size(), cnt + 1);

  /* just an empty element? then return the node as it is */
  if(xmlTextReaderIsEmptyElement(reader))
    return;

  /* parse tags if present */
  int depth = xmlTextReaderDepth(reader);

  /* scan all elements on same level or its children */
  std::vector<tag_t> tags;
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const char *subname = reinterpret_cast<const char *>(xmlTextReaderConstName(reader));
      if(likely(strcmp(subname, "tag") == 0))
        process_tag(reader, tags);
      else
	skip_element(reader);
    }

    ret = xmlTextReaderRead(reader);
  }
  node->tags.replace(tags);
}

static node_t *process_nd(xmlTextReaderPtr reader, osm_t::ref osm) {
  xmlString prop(xmlTextReaderGetAttribute(reader, BAD_CAST "ref"));
  node_t *node = nullptr;

  if(likely(prop)) {
    item_id_t id = strtoll(reinterpret_cast<char *>(prop.get()), nullptr, 10);
    /* search matching node */
    node = osm->node_by_id(id);
    if(unlikely(node == nullptr))
      printf("Node id " ITEM_ID_FORMAT " not found\n", id);
    else
      node->ways++;
  }

  skip_element(reader);
  return node;
}

static void process_way(xmlTextReaderPtr reader, osm_t::ref osm) {
  /* allocate a new way structure */
  way_t *way = new way_t(1);

  process_base_attributes(way, reader, osm);

  size_t cnt = osm->ways.size();
  osm->ways[way->id] = way;
  assert_cmpnum(osm->ways.size(), cnt + 1);

  /* just an empty element? then return the way as it is */
  /* (this should in fact never happen as this would be a way without nodes) */
  if(xmlTextReaderIsEmptyElement(reader))
    return;

  /* parse tags/nodes if present */
  int depth = xmlTextReaderDepth(reader);

  /* scan all elements on same level or its children */
  std::vector<tag_t> tags;
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const char *subname = reinterpret_cast<const char *>(xmlTextReaderConstName(reader));
      if(strcmp(subname, "nd") == 0) {
	node_t *n = process_nd(reader, osm);
        if(likely(n != nullptr))
          way->node_chain.push_back(n);
      } else if(likely(strcmp(subname, "tag") == 0)) {
        process_tag(reader, tags);
      } else
	skip_element(reader);
    }
    ret = xmlTextReaderRead(reader);
  }
  way->tags.replace(tags);
}

static bool process_member(xmlTextReaderPtr reader, osm_t::ref osm, std::vector<member_t> &members) {
  xmlString tp(xmlTextReaderGetAttribute(reader, BAD_CAST "type"));
  xmlString ref(xmlTextReaderGetAttribute(reader, BAD_CAST "ref"));
  xmlString role(xmlTextReaderGetAttribute(reader, BAD_CAST "role"));

  return osm->parse_relation_member(reinterpret_cast<char *>(tp.get()), reinterpret_cast<char *>(ref.get()),
                                    reinterpret_cast<char *>(role.get()), members);
}

static void process_relation(xmlTextReaderPtr reader, osm_t::ref osm) {
  /* allocate a new relation structure */
  relation_t *relation = new relation_t(1);

  process_base_attributes(relation, reader, osm);

  size_t cnt = osm->relations.size();
  osm->relations[relation->id] = relation;
  assert_cmpnum(osm->relations.size(), cnt + 1);

  /* just an empty element? then return the relation as it is */
  /* (this should in fact never happen as this would be a relation */
  /* without members) */
  if(xmlTextReaderIsEmptyElement(reader))
    return;

  /* parse tags/member if present */
  int depth = xmlTextReaderDepth(reader);

  /* scan all elements on same level or its children */
  std::vector<tag_t> tags;
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const char *subname = reinterpret_cast<const char *>(xmlTextReaderConstName(reader));
      if(strcmp(subname, "member") == 0) {
        process_member(reader, osm, relation->members);
      } else if(likely(strcmp(subname, "tag") == 0)) {
        process_tag(reader, tags);
      } else
	skip_element(reader);
    }
    ret = xmlTextReaderRead(reader);
  }
  relation->tags.replace(tags);
}

static osm_t::UploadPolicy parseUploadPolicy(const char *str) {
  if(likely(strcmp(str, "true") == 0))
    return osm_t::Upload_Normal;
  else if(strcmp(str, "false") == 0)
    return osm_t::Upload_Discouraged;
  else if(likely(strcmp(str, "never") == 0))
    return osm_t::Upload_Blocked;

  printf("unknown key for upload found: %s\n", str);

  // just to be cautious
  return osm_t::Upload_Discouraged;
}

static osm_t *process_osm(xmlTextReaderPtr reader) {
  /* alloc osm structure */
  std::unique_ptr<osm_t> osm(new osm_t());

  xmlString prop(xmlTextReaderGetAttribute(reader, BAD_CAST "upload"));
  if(unlikely(prop))
    osm->uploadPolicy = parseUploadPolicy(reinterpret_cast<const char *>(prop.get()));

  /* read next node */
  int num_elems = 0;

  /* the objects come in exactly this order, so some parsing time can be
   * saved as it is clear that e.g. no node can show up if the first way
   * was seen. */
  enum blocks {
    BLOCK_OSM = 0,
//     BLOCK_BOUNDS,
    BLOCK_NODES,
    BLOCK_WAYS,
    BLOCK_RELATIONS
  };
  enum blocks block = BLOCK_OSM;

  const int tick_every = 50; // Balance responsive appearance with performance.
  int ret = xmlTextReaderRead(reader);
  while(ret == 1) {

    switch(xmlTextReaderNodeType(reader)) {
    case XML_READER_TYPE_ELEMENT: {

      assert_cmpnum(xmlTextReaderDepth(reader), 1);
      const char *name = reinterpret_cast<const char *>(xmlTextReaderConstName(reader));
      if(block == BLOCK_OSM && strcmp(name, "bounds") == 0) {
        if(unlikely(!process_bounds(reader, osm->bounds)))
          return nullptr;
        block = BLOCK_NODES; // next must be nodes, there must not be more than one bounds
      } else if(block == BLOCK_NODES && strcmp(name, node_t::api_string()) == 0) {
        process_node(reader, osm);
      } else if(block <= BLOCK_WAYS && strcmp(name, way_t::api_string()) == 0) {
        process_way(reader, osm);
	block = BLOCK_WAYS;
      } else if(likely(block <= BLOCK_RELATIONS && strcmp(name, relation_t::api_string()) == 0)) {
        process_relation(reader, osm);
	block = BLOCK_RELATIONS;
      } else {
	printf("something unknown found: %s\n", name);
	skip_element(reader);
      }
      break;
    }

    case XML_READER_TYPE_END_ELEMENT:
      /* end element must be for the current element */
      assert_cmpnum(xmlTextReaderDepth(reader), 0);
      return osm.release();
      break;

    default:
      break;
    }
    ret = xmlTextReaderRead(reader);

    if (num_elems++ > tick_every) {
      num_elems = 0;
      osm2go_platform::process_events();
    }
  }

  assert_unreachable();
}

struct relation_ref_functor {
  osm_t::ref osm;
  explicit relation_ref_functor(osm_t::ref o) : osm(o) {}
  void operator()(std::pair<item_id_t, relation_t *> p) {
    std::for_each(p.second->members.begin(), p.second->members.end(), *this);
  }
  void operator()(member_t &m) {
    if(m.object.type != object_t::RELATION_ID)
      return;
    std::map<item_id_t, relation_t *>::const_iterator itEnd = osm->relations.end();
    std::map<item_id_t, relation_t *>::const_iterator it = osm->relations.find(m.object.id);
    if(it == itEnd)
      return;
    m.object.relation = it->second;
    m.object.type = object_t::RELATION;
  }
};

static osm_t *process_file(const std::string &filename) {
  std::unique_ptr<osm_t> osm;
  xmlTextReaderPtr reader;

  reader = xmlReaderForFile(filename.c_str(), nullptr, XML_PARSE_NONET);
  if (likely(reader != nullptr)) {
    if(likely(xmlTextReaderRead(reader) == 1)) {
      const char *name = reinterpret_cast<const char *>(xmlTextReaderConstName(reader));
      if(likely(name && strcmp(name, "osm") == 0)) {
        osm.reset(process_osm(reader));
        // relations may have references to other relation, which have greater ids
        // those are not present when the relation itself was created, but may be now
        if(likely(osm))
          std::for_each(osm->relations.begin(), osm->relations.end(), relation_ref_functor(osm));
      }
    } else
      printf("file empty\n");

    xmlFreeTextReader(reader);
  } else {
    fprintf(stderr, "Unable to open %s\n", filename.c_str());
  }
  return osm.release();
}

/* ----------------------- end of stream parser ------------------- */

osm_t *osm_t::parse(const std::string &path, const std::string &filename) {

  // use stream parser
  if(unlikely(filename.find('/') != std::string::npos))
    return process_file(filename);
  else
    return process_file(path + filename);
}

const char *osm_t::sanity_check() const {
  if(unlikely(!bounds.ll.valid()))
    return _("Invalid data in OSM file:\nBoundary box invalid!");

  if(unlikely(nodes.empty()))
    return _("Invalid data in OSM file:\nNo drawable content found!");

  return nullptr;
}

/* ------------------------- misc access functions -------------- */

struct tag_to_xml {
  xmlNodePtr const node;
  const bool keep_created;
  explicit tag_to_xml(xmlNodePtr n, bool k = false) : node(n), keep_created(k) {}
  void operator()(const tag_t &tag) {
    /* skip "created_by" tags as they aren't needed anymore with api 0.6 */
    if(likely(keep_created || !tag.is_creator_tag())) {
      xmlNodePtr tag_node = xmlNewChild(node, nullptr, BAD_CAST "tag", nullptr);
      xmlNewProp(tag_node, BAD_CAST "k", BAD_CAST tag.key);
      xmlNewProp(tag_node, BAD_CAST "v", BAD_CAST tag.value);
    }
  }
};

xmlChar *base_object_t::generate_xml(const std::string &changeset) const
{
  char str[32];
  std::unique_ptr<xmlDoc, xmlDocDelete> doc(xmlNewDoc(BAD_CAST "1.0"));
  xmlNodePtr root_node = xmlNewNode(nullptr, BAD_CAST "osm");
  xmlDocSetRootElement(doc.get(), root_node);

  xmlNodePtr xml_node = xmlNewChild(root_node, nullptr, BAD_CAST apiString(), nullptr);

  /* new nodes don't have an id, but get one after the upload */
  if(!isNew()) {
    snprintf(str, sizeof(str), ITEM_ID_FORMAT, id);
    xmlNewProp(xml_node, BAD_CAST "id", BAD_CAST str);
  }
  snprintf(str, sizeof(str), "%u", version);
  xmlNewProp(xml_node, BAD_CAST "version", BAD_CAST str);
  xmlNewProp(xml_node, BAD_CAST "changeset", BAD_CAST changeset.c_str());

  // save the information specific to the given object type
  generate_xml_custom(xml_node);

  // save tags
  tags.for_each(tag_to_xml(xml_node));

  xmlChar *result = nullptr;
  int len = 0;

  xmlDocDumpFormatMemoryEnc(doc.get(), &result, &len, "UTF-8", 1);

  return result;
}

/* build xml representation for a node */
void node_t::generate_xml_custom(xmlNodePtr xml_node) const {
  pos.toXmlProperties(xml_node);
}

struct add_xml_node_refs {
  xmlNodePtr const way_node;
  explicit add_xml_node_refs(xmlNodePtr n) : way_node(n) {}
  void operator()(const node_t *node);
};

void add_xml_node_refs::operator()(const node_t* node)
{
  xmlNodePtr nd_node = xmlNewChild(way_node, nullptr, BAD_CAST "nd", nullptr);
  xmlNewProp(nd_node, BAD_CAST "ref", BAD_CAST node->id_string().c_str());
}

/**
 * @brief write the referenced nodes of a way to XML
 * @param way_node the XML node of the way to append to
 */
void way_t::write_node_chain(xmlNodePtr way_node) const {
  std::for_each(node_chain.begin(), node_chain.end(), add_xml_node_refs(way_node));
}

/* build xml representation for a changeset */
xmlChar *osm_generate_xml_changeset(const std::string &comment,
                                    const std::string &src) {
  xmlChar *result = nullptr;
  int len = 0;

  /* tags for this changeset */
  tag_t tag_comment = tag_t::uncached("comment", comment.c_str());
  tag_t tag_creator = tag_t::uncached("created_by", PACKAGE " v" VERSION);

  std::unique_ptr<xmlDoc, xmlDocDelete> doc(xmlNewDoc(BAD_CAST "1.0"));
  xmlNodePtr root_node = xmlNewNode(nullptr, BAD_CAST "osm");
  xmlDocSetRootElement(doc.get(), root_node);

  xmlNodePtr cs_node = xmlNewChild(root_node, nullptr, BAD_CAST "changeset", nullptr);

  tag_to_xml fc(cs_node, true);
  fc(tag_creator);
  fc(tag_comment);
  if(!src.empty()) {
    tag_t tag_source(const_cast<char *>("source"),
                    const_cast<char *>(src.c_str()));
    fc(tag_source);
  }

  xmlDocDumpFormatMemoryEnc(doc.get(), &result, &len, "UTF-8", 1);

  return result;
}

/* ---------- edit functions ------------- */

template<typename T, typename U> U osm_new_id(const std::map<U, T *> &map) {
  if(map.empty())
    return -1;

  // map is sorted, so use one less the first id in the container if it is negative,
  // or -1 if it is positive
  const typename std::map<U, T *>::const_iterator it = map.begin();
  if(it->first >= 0)
    return -1;
  else
    return it->first - 1;
}

template<typename T> void osm_attach(std::map<item_id_t, T *> &map, T *obj) {
  obj->id = osm_new_id(map);
  printf("Attaching %s " ITEM_ID_FORMAT "\n", obj->apiString(), obj->id);
  map[obj->id] = obj;
}

node_t *osm_t::node_new(const lpos_t lpos) {
  /* convert screen position back to ll */
  return new node_t(0, lpos, lpos.toPos(bounds));
}

node_t *osm_t::node_new(const pos_t &pos) {
  /* convert ll position to screen */
  return new node_t(0, pos.toLpos(bounds), pos);
}

void osm_t::node_attach(node_t *node) {
  osm_attach(nodes, node);
}

void osm_t::way_attach(way_t *way) {
  osm_attach(ways, way);
}

struct node_chain_delete_functor {
  const node_t * const node;
  way_chain_t &way_chain;
  const bool affect_ways;
  node_chain_delete_functor(const node_t *n, way_chain_t &w, bool a) : node(n), way_chain(w), affect_ways(a) {}
  void operator()(std::pair<item_id_t, way_t *> p);
};

void node_chain_delete_functor::operator()(std::pair<item_id_t, way_t *> p)
{
  way_t * const way = p.second;
  node_chain_t &chain = way->node_chain;
  bool modified = false;

  node_chain_t::iterator cit = chain.begin();
  while((cit = std::find(cit, chain.end(), node)) != chain.end()) {
    /* remove node from chain */
    modified = true;
    if(affect_ways)
      cit = chain.erase(cit);
    else
      /* only record that there has been a change */
      break;
  }

  if(modified) {
    way->flags |= OSM_FLAG_DIRTY;

    /* and add the way to the list of affected ways */
    way_chain.push_back(way);
  }
}

way_chain_t osm_t::node_delete(node_t *node, bool remove_refs) {
  way_chain_t way_chain;
  bool permanently = node->isNew();

  /* new nodes aren't stored on the server and are just deleted permanently */
  if(permanently) {
    printf("About to delete NEW node #" ITEM_ID_FORMAT
	   " -> force permanent delete\n", node->id);
  }

  /* first remove node from all ways using it */
  std::for_each(ways.begin(), ways.end(),
                node_chain_delete_functor(node, way_chain, remove_refs));

  if(remove_refs)
    remove_from_relations(object_t(node));

  /* remove that nodes map representations */
  node->item_chain_destroy();

  if(!permanently) {
    printf("mark node #" ITEM_ID_FORMAT " as deleted\n", node->id);
    node->flags |= OSM_FLAG_DELETED;
  } else {
    printf("permanently delete node #" ITEM_ID_FORMAT "\n", node->id);

    std::map<item_id_t, node_t *>::iterator it = nodes.find(node->id);
    assert(it != nodes.end());

    node_free(it->second);
  }

  return way_chain;
}

struct remove_member_functor {
  const object_t obj;
  // the second argument is to distinguish the constructor from operator()
  remove_member_functor(object_t o, bool) : obj(o) {}
  void operator()(std::pair<item_id_t, relation_t *> pair);
};

void remove_member_functor::operator()(std::pair<item_id_t, relation_t *> pair)
{
  relation_t * const relation = pair.second;
  std::vector<member_t>::iterator itEnd = relation->members.end();
  std::vector<member_t>::iterator it = relation->members.begin();

  while((it = std::find(it, itEnd, obj)) != itEnd) {
    printf("  from relation #" ITEM_ID_FORMAT "\n", relation->id);

    it = relation->members.erase(it);
    // refresh end iterator as the vector was modified
    itEnd = relation->members.end();

    relation->flags |= OSM_FLAG_DIRTY;
  }
}

/* remove the given object from all relations. used if the object is to */
/* be deleted */
void osm_t::remove_from_relations(object_t obj) {
  printf("removing %s #" ITEM_ID_FORMAT " from all relations:\n", obj.obj->apiString(), obj.get_id());

  std::for_each(relations.begin(), relations.end(),
                remove_member_functor(obj, false));
}

void osm_t::relation_attach(relation_t *relation) {
  osm_attach(relations, relation);
}

struct find_relation_members {
  const object_t obj;
  explicit find_relation_members(const object_t o) : obj(o) {}
  bool operator()(const std::pair<item_id_t, relation_t *> &pair) {
    const std::vector<member_t>::const_iterator itEnd = pair.second->members.end();
    return std::find(std::cbegin(pair.second->members), itEnd, obj) != itEnd;
  }
};

struct osm_unref_way_free {
  osm_t * const osm;
  const way_t * const way;
  osm_unref_way_free(osm_t *o, const way_t *w) : osm(o), way(w) {}
  void operator()(node_t *node);
};

void osm_unref_way_free::operator()(node_t* node)
{
  printf("checking node #" ITEM_ID_FORMAT " (still used by %u)\n",
         node->id, node->ways);
  assert_cmpnum_op(node->ways, >, 0);
  node->ways--;

  /* this node must only be part of this way */
  if(!node->ways && !node->tags.hasRealTags()) {
    /* delete this node, but don't let this actually affect the */
    /* associated ways as the only such way is the one we are currently */
    /* deleting */
    const std::map<item_id_t, relation_t *>::const_iterator itEnd = osm->relations.end();
    // do not delete if it is still referenced by a relation
    if(std::find_if(std::cbegin(osm->relations), itEnd, find_relation_members(object_t(node))) == itEnd) {
      const way_chain_t &way_chain = osm->node_delete(node, false);
      assert_cmpnum(way_chain.size(), 1);
      assert(way_chain.front() == way);
    }
  }
}

void osm_t::way_delete(way_t *way) {
  bool permanently = way->isNew();

  /* new ways aren't stored on the server and are just deleted permanently */
  if(permanently) {
    printf("About to delete NEW way #" ITEM_ID_FORMAT
	   " -> force permanent delete\n", way->id);
  }

  remove_from_relations(object_t(way));

  /* remove it visually from the screen */
  way->item_chain_destroy();

  /* delete all nodes that aren't in other use now */
  std::for_each(way->node_chain.begin(), way->node_chain.end(),
                osm_unref_way_free(this, way));
  way->node_chain.clear();

  if(!permanently) {
    printf("mark way #" ITEM_ID_FORMAT " as deleted\n", way->id);
    way->flags |= OSM_FLAG_DELETED;
  } else {
    printf("permanently delete way #" ITEM_ID_FORMAT "\n", way->id);

    std::map<item_id_t, way_t *>::iterator it = ways.find(way->id);
    assert(it != ways.end());

    way_free(it->second);
  }
}

void osm_t::relation_delete(relation_t *relation) {
  bool permanently = relation->isNew();

  /* new relations aren't stored on the server and are just */
  /* deleted permanently */
  if(permanently) {
    printf("About to delete NEW relation #" ITEM_ID_FORMAT
	   " -> force permanent delete\n", relation->id);
  }

  remove_from_relations(object_t(relation));

  /* the deletion of a relation doesn't affect the members as they */
  /* don't have any reference to the relation they are part of */

  if(!permanently) {
    printf("mark relation #" ITEM_ID_FORMAT " as deleted\n", relation->id);
    relation->flags |= OSM_FLAG_DELETED;
  } else {
    printf("permanently delete relation #" ITEM_ID_FORMAT "\n",
	   relation->id);

    relation_free(relation);
  }
}

static const char *DS_ONEWAY_FWD = "yes";
static const char *DS_ONEWAY_REV = "-1";

/* Reverse direction-sensitive tags like "oneway". Marks the way as dirty if
 * anything is changed, and returns the number of flipped tags. */

struct reverse_direction_sensitive_tags_functor {
  unsigned int &n_tags_altered;
  explicit reverse_direction_sensitive_tags_functor(unsigned int &c) : n_tags_altered(c) {}
  void operator()(tag_t &etag);
};

static std::vector<std::pair<std::string, std::string> > rtable_init()
{
  std::vector<std::pair<std::string, std::string> > rtable;

  rtable.push_back(std::pair<std::string, std::string>("left", "right"));
  rtable.push_back(std::pair<std::string, std::string>("right", "left"));
  rtable.push_back(std::pair<std::string, std::string>("forward", "backward"));
  rtable.push_back(std::pair<std::string, std::string>("backward", "forward"));

  return rtable;
}

static char ascii_lower(char ch) {
  if (ch >= 'A' && ch <= 'Z')
    return ch - 'A' + 'a';
  else
    return ch;
}

void reverse_direction_sensitive_tags_functor::operator()(tag_t &etag)
{
  static const char *oneway = value_cache.insert("oneway");
  static const char *sidewalk = value_cache.insert("sidewalk");

  if (etag.key == oneway) {
    std::string lc_value = etag.value;
    std::transform(lc_value.begin(), lc_value.end(), lc_value.begin(), ascii_lower);
    // oneway={yes/true/1/-1} is unusual.
    // Favour "yes" and "-1".
    if (lc_value == DS_ONEWAY_FWD || lc_value == "true" || lc_value == "1") {
      etag = tag_t::uncached(oneway, DS_ONEWAY_REV);
      n_tags_altered++;
    } else if (lc_value == DS_ONEWAY_REV) {
      etag = tag_t::uncached(oneway, DS_ONEWAY_FWD);
      n_tags_altered++;
    } else {
      printf("warning: unknown oneway value: %s\n", etag.value);
    }
  } else if (etag.key == sidewalk) {
    if (strcasecmp(etag.value, "right") == 0) {
      etag = tag_t::uncached(sidewalk, "left");
      n_tags_altered++;
    } else if (strcasecmp(etag.value, "left") == 0) {
      etag = tag_t::uncached(sidewalk, "right");
      n_tags_altered++;
    }
  } else {
    // suffixes
    const char *lastcolon = strrchr(etag.key, ':');

    if (lastcolon != nullptr) {
      static std::vector<std::pair<std::string, std::string> > rtable = rtable_init();

      for (unsigned int i = 0; i < rtable.size(); i++) {
        if (strcmp(lastcolon + 1, rtable[i].first.c_str()) == 0) {
          /* length of key that will persist */
          size_t plen = lastcolon - etag.key;
          /* add length of new suffix */
          std::string nkey(plen + rtable[i].second.size(), 0);
          nkey.assign(etag.key, plen + 1);
          nkey += rtable[i].second;
          etag.key = value_cache.insert(nkey);
          n_tags_altered++;
          break;
        }
      }
    }
  }
}

/* Reverse a way's role within relations where the role is direction-sensitive.
 * Returns the number of roles flipped, and marks any relations changed as
 * dirty. */

static const char *DS_ROUTE_FORWARD = "forward";
static const char *DS_ROUTE_REVERSE = "backward";

struct reverse_roles {
  const object_t way;
  unsigned int &n_roles_flipped;
  reverse_roles(way_t *w, unsigned int &n) : way(w), n_roles_flipped(n) {}
  void operator()(const std::pair<item_id_t, relation_t *> &pair);
};

void reverse_roles::operator()(const std::pair<item_id_t, relation_t *> &pair)
{
  relation_t * const relation = pair.second;
  const char *type = relation->tags.get_value("type");

  // Route relations; https://wiki.openstreetmap.org/wiki/Relation:route
  if (!type || strcasecmp(type, "route") != 0)
    return;

  // First find the member corresponding to our way:
  const std::vector<member_t>::iterator mitEnd = relation->members.end();
  std::vector<member_t>::iterator member = std::find(relation->members.begin(), mitEnd, way);
  if(member == relation->members.end())
    return;

  // Then flip its role if it's one of the direction-sensitive ones
  if (member->role == nullptr) {
    printf("null role in route relation -> ignore\n");
  } else if (strcasecmp(member->role, DS_ROUTE_FORWARD) == 0) {
    member->role = DS_ROUTE_REVERSE;
    relation->flags |= OSM_FLAG_DIRTY;
    ++n_roles_flipped;
  } else if (strcasecmp(member->role, DS_ROUTE_REVERSE) == 0) {
    member->role = DS_ROUTE_FORWARD;
    relation->flags |= OSM_FLAG_DIRTY;
    ++n_roles_flipped;
  }

  // TODO: what about numbered stops? Guess we ignore them; there's no
  // consensus about whether they should be placed on the way or to one side
  // of it.
}

void way_t::reverse(osm_t::ref osm, unsigned int &tags_flipped, unsigned int &roles_flipped) {
  tags_flipped = 0;

  tags.for_each(reverse_direction_sensitive_tags_functor(tags_flipped));

  flags |= OSM_FLAG_DIRTY;

  std::reverse(node_chain.begin(), node_chain.end());

  roles_flipped = 0;
  reverse_roles context(this, roles_flipped);
  std::for_each(osm->relations.begin(), osm->relations.end(), context);
}

const node_t *way_t::first_node() const noexcept {
  if(node_chain.empty())
    return nullptr;

  return node_chain.front();
}

const node_t *way_t::last_node() const noexcept {
  if(node_chain.empty())
    return nullptr;

  return node_chain.back();
}

bool way_t::is_closed() const noexcept {
  if(node_chain.empty())
    return false;
  return node_chain.front() == node_chain.back();
}

struct relation_transfer {
  way_t * const dst;
  const way_t * const src;
  relation_transfer(way_t *d, const way_t *s) : dst(d), src(s) {}
  void operator()(const std::pair<item_id_t, relation_t *> &pair);
};

struct find_member_object_functor {
  const object_t &object;
  explicit find_member_object_functor(const object_t &o) : object(o) {}
  bool operator()(const member_t &member) {
    return member.object == object;
  }
};

void relation_transfer::operator()(const std::pair<item_id_t, relation_t *> &pair)
{
  relation_t * const relation = pair.second;
  /* walk member chain. save role of way if its being found. */
  const object_t osrc(const_cast<way_t *>(src));
  find_member_object_functor fc(osrc);
  std::vector<member_t>::iterator itBegin = relation->members.begin();
  std::vector<member_t>::iterator itEnd = relation->members.end();
  std::vector<member_t>::iterator it = std::find_if(itBegin, itEnd, fc);
  for(; it != itEnd; it = std::find_if(it, itEnd, fc)) {
    printf("way #" ITEM_ID_FORMAT " is part of relation #" ITEM_ID_FORMAT " at position %zu, adding way #" ITEM_ID_FORMAT "\n",
           src->id, relation->id, it - relation->members.begin(), dst->id);

    member_t m(object_t(dst), *it);

    // find out if the relation members are ordered ways, so the split parts should
    // be inserted in a sensible order to keep the relation intact
    bool insertBefore = false;
    if(it != itBegin && (it - 1)->object.type == object_t::WAY) {
      std::vector<member_t>::iterator prev = it - 1;

      insertBefore = prev->object.way->ends_with_node(dst->node_chain.front()) ||
                     prev->object.way->ends_with_node(dst->node_chain.back());
    } else if (it + 1 < itEnd && (it + 1)->object.type == object_t::WAY) {
      std::vector<member_t>::iterator next = it + 1;

      insertBefore = next->object.way->ends_with_node(src->node_chain.front()) ||
                     next->object.way->ends_with_node(src->node_chain.back());
    } // if this is both itEnd and itBegin it is the only member, so the ordering is irrelevant

    // make dst member of the same relation
    if(insertBefore) {
      printf("\tinserting before way #" ITEM_ID_FORMAT " to keep relation ordering\n", src->id);
      it = relation->members.insert(it, m);
      // skip this object when calling fc again, it can't be the searched one
      it++;
    } else {
      it = relation->members.insert(++it, m);
    }
    // skip this object when calling fc again, it can't be the searched one
    it++;
    // refresh the end iterator as the container was modified
    itEnd = relation->members.end();

    relation->flags |= OSM_FLAG_DIRTY;
  }
}

way_t *way_t::split(osm_t::ref osm, node_chain_t::iterator cut_at, bool cut_at_node)
{
  assert_cmpnum_op(node_chain.size(), >, 2);

  /* remember that the way needs to be uploaded */
  flags |= OSM_FLAG_DIRTY;

  /* If this is a closed way, reorder (rotate) it, so the place to cut is
   * adjacent to the begin/end of the way. This prevents a cut polygon to be
   * split into two ways. Splitting closed ways is much less complex as there
   * will be no second way, the only modification done is the node chain. */
  if(is_closed()) {
    printf("CLOSED WAY -> rotate by %zi\n", cut_at - node_chain.begin());

    // un-close the way
    node_chain.back()->ways--;
    node_chain.pop_back();
    // generate the correct layout
    std::rotate(node_chain.begin(), cut_at, node_chain.end());
    return nullptr;
  }

  /* create a duplicate of the currently selected way */
  way_t *neww = new way_t(0);

  /* attach remaining nodes to new way */
  neww->node_chain.insert(neww->node_chain.end(), cut_at, node_chain.end());

  /* if we cut at a node, this node is now part of both ways. so */
  /* keep it in the old way. */
  if(cut_at_node) {
    (*cut_at)->ways++;
    cut_at++;
  }

  /* terminate remainig chain on old way */
  node_chain.erase(cut_at, node_chain.end());

  // This may just split the last node out of the way. The new way is no
  // valid way so it is deleted
  if(neww->node_chain.size() < 2) {
    osm_unref_node(neww->node_chain.front());
    delete neww;
    return nullptr;
  }

  /* ------------  copy all tags ------------- */
  neww->tags.copy(tags);

  // now move the way itself into the main data structure
  // do it before transferring the relation membership to get meaningful ids in debug output
  osm->way_attach(neww);

  // keep the history with the longer way
  // this must be before the relation transfer, as that needs to know the
  // contained nodes to determine proper ordering in the relations
  if(node_chain.size() < neww->node_chain.size())
    node_chain.swap(neww->node_chain);

  /* ---- transfer relation membership from way to new ----- */
  std::for_each(osm->relations.begin(), osm->relations.end(), relation_transfer(neww, this));

  return neww;
}

struct tag_map_functor {
  osm_t::TagMap &tags;
  explicit tag_map_functor(osm_t::TagMap &t) : tags(t) {}
  void operator()(const tag_t &otag) {
    tags.insert(osm_t::TagMap::value_type(otag.key, otag.value));
  }
};

osm_t::TagMap tag_list_t::asMap() const
{
  osm_t::TagMap new_tags;

  if(!empty())
    std::for_each(contents->begin(), contents->end(), tag_map_functor(new_tags));

  return new_tags;
}

struct tag_vector_copy_functor {
  std::vector<tag_t> &tags;
  explicit tag_vector_copy_functor(std::vector<tag_t> &t) : tags(t) {}
  void operator()(const tag_t &otag) {
    if(unlikely(otag.is_creator_tag()))
      return;

    tags.push_back(otag);
  }
};

void tag_list_t::copy(const tag_list_t &other)
{
  assert_null(contents);

  if(other.empty())
    return;

  contents = new typeof(*contents);
  contents->reserve(other.contents->size());

  std::for_each(other.contents->begin(), other.contents->end(), tag_vector_copy_functor(*contents));
}

struct relation_member_functor {
  const member_t member;
  const char * const type;
  relation_member_functor(const char *t, const char *r, const object_t &o)
    : member(o, r), type(value_cache.insert(t)) {}
  bool operator()(const std::pair<item_id_t, relation_t *> &it) const
  { return it.second->tags.get_value("type") == type &&
           std::find(it.second->members.begin(), it.second->members.end(), member) != it.second->members.end(); }
};

/* try to get an as "speaking" description of the object as possible */
std::string object_t::get_name(const osm_t &osm) const {
  std::string ret;

  assert(is_real());

  /* worst case: we have no tags at all. return techincal info then */
  if(!obj->tags.hasRealTags())
    return std::string(_("unspecified ")) + type_string();

  /* try to figure out _what_ this is */
  const std::array<const char *, 5> name_tags = { { "name", "ref", "note", "fix" "me", "sport" } };
  const char *name = nullptr;
  for(unsigned int i = 0; !name && i < name_tags.size(); i++)
    name = obj->tags.get_value(name_tags[i]);

  /* search for some kind of "type" */
  const std::array<const char *, 10> type_tags =
                          { { "amenity", "place", "historic", "leisure",
                              "tourism", "landuse", "waterway", "railway",
                              "natural", "man_made" } };
  const char *typestr = nullptr;

  for(unsigned int i = 0; !typestr && i < type_tags.size(); i++)
    typestr = obj->tags.get_value(type_tags[i]);

  if(!typestr && obj->tags.get_value("building")) {
    const char *street = obj->tags.get_value("addr:street");
    const char *hn = obj->tags.get_value("addr:housenumber");

    if(hn) {
      if(street == nullptr) {
        // check if there is an "associatedStreet" relation where this is a "house" member
        const relation_t *astreet = osm.find_relation(relation_member_functor("associatedStreet", "house", *this));
        if(astreet != nullptr)
          street = astreet->tags.get_value("name");
      }
      if(street) {
        ret = "building ";
        ret += street;
        ret +=' ';
      } else {
        ret = "building housenumber ";
      }
      ret += hn;
    } else {
      typestr = "building";
      if(!name)
        name = obj->tags.get_value("addr:housename");
    }
  }
  if(!typestr && ret.empty())
    typestr = obj->tags.get_value("emergency");

  /* highways are a little bit difficult */
  if(ret.empty()) {
    const char *highway = obj->tags.get_value("highway");
    if(highway != nullptr) {
      if((!strcmp(highway, "primary")) ||
         (!strcmp(highway, "secondary")) ||
         (!strcmp(highway, "tertiary")) ||
         (!strcmp(highway, "unclassified")) ||
         (!strcmp(highway, "residential")) ||
         (!strcmp(highway, "service"))) {
        ret = highway;
        ret += " road";
        typestr = nullptr;
      }

      else if(!strcmp(highway, "pedestrian")) {
        typestr = "pedestrian way/area";
      }

      else if(!strcmp(highway, "construction")) {
        typestr = "road/street under construction";
      }

      else
        typestr = highway;
    }
  }

  if(typestr) {
    assert(ret.empty());
    ret = typestr;
  }

  if(name) {
    if(ret.empty())
      ret = type_string();
    ret += ": \"";
    ret += name;
    ret += '"';
  } else if(ret.empty()) {
    ret = "unspecified ";
    ret += type_string();
  }

  /* remove underscores from string and replace them by spaces as this is */
  /* usually nicer */
  std::replace(ret.begin(), ret.end(), '_', ' ');

  return ret;
}

tag_t::tag_t(const char *k, const char *v)
  : key(value_cache.insert(k))
  , value(value_cache.insert(v))
{
}

bool tag_t::is_creator_tag() const noexcept {
  return is_creator_tag(key);
}

bool tag_t::is_creator_tag(const char* key) noexcept
{
  return (strcasecmp(key, "created_by") == 0);
}

tag_list_t::~tag_list_t()
{
  clear();
}

bool tag_list_t::empty() const noexcept
{
  return !contents || contents->empty();
}

bool tag_list_t::hasRealTags() const noexcept
{
  if(empty())
    return false;

  const std::vector<tag_t>::const_iterator itEnd = contents->end();
  std::vector<tag_t>::const_iterator it = contents->begin();
  while(it != itEnd && it->is_creator_tag())
    it++;

  return it != itEnd;
}

struct key_match_functor {
  const char * const key;
  explicit key_match_functor(const char *k) : key(k) {}
  inline bool operator()(const tag_t &tag) const {
    return key == tag.key;
  }
};

const char* tag_list_t::get_value(const char *key) const
{
  if(!contents)
    return nullptr;

  const char *cacheKey = value_cache.getValue(key);
  // if the key is not in the cache then it is used nowhere
  if(unlikely(cacheKey == nullptr))
    return nullptr;

  const std::vector<tag_t>::const_iterator itEnd = contents->end();
  const std::vector<tag_t>::const_iterator it = std::find_if(std::cbegin(*contents),
                                                             itEnd, key_match_functor(cacheKey));
  if(it != itEnd)
    return it->value;

  return nullptr;
}

void tag_list_t::clear()
{
  delete contents;
  contents = nullptr;
}

void tag_list_t::replace(std::vector<tag_t> &ntags)
{
  clear();
  if(ntags.empty()) {
    contents = nullptr;
    return;
  }
#if __cplusplus >= 201103L
  contents = new std::vector<tag_t>(std::move(ntags));
  contents->shrink_to_fit();
#else
  contents = new std::vector<tag_t>();
  contents->reserve(ntags.size());
  contents->swap(ntags);
#endif
}

struct tag_fill_functor {
  std::vector<tag_t> &tags;
  explicit tag_fill_functor(std::vector<tag_t> &t) : tags(t) {}
  void operator()(const osm_t::TagMap::value_type &p) {
    if(unlikely(tag_t::is_creator_tag(p.first.c_str())))
      return;

    tags.push_back(tag_t(p.first.c_str(), p.second.c_str()));
  }
};

void tag_list_t::replace(const osm_t::TagMap &ntags)
{
  clear();
  if(ntags.empty())
    return;

  contents = new std::vector<tag_t>();
  contents->reserve(ntags.size());
  std::for_each(ntags.begin(), ntags.end(), tag_fill_functor(*contents));
}

base_object_t::base_object_t(unsigned int ver, item_id_t i) noexcept
  : id(i)
  , time(0)
  , flags(ver == 0 ? OSM_FLAG_DIRTY : 0)
  , user(0)
  , version(ver)
{
}

void base_object_t::updateTags(const osm_t::TagMap &ntags)
{
  if (tags == ntags)
    return;

  tags.replace(ntags);

  flags |= OSM_FLAG_DIRTY;
}

std::string base_object_t::id_string() const {
  return std::to_string(id);
}

void base_object_t::osmchange_delete(xmlNodePtr parent_node, const char *changeset) const
{
  assert(flags & OSM_FLAG_DELETED);

  xmlNodePtr obj_node = xmlNewChild(parent_node, nullptr, BAD_CAST apiString(), nullptr);

  xmlNewProp(obj_node, BAD_CAST "id", BAD_CAST id_string().c_str());

  char buf[32];
  snprintf(buf, sizeof(buf), "%u", version);

  xmlNewProp(obj_node, BAD_CAST "version", BAD_CAST buf);
  xmlNewProp(obj_node, BAD_CAST "changeset", BAD_CAST changeset);
}

struct value_match_functor {
  const char * const value;
  explicit value_match_functor(const char *v) : value(v) {}
  bool operator()(const tag_t *tag) {
    return tag->value && (strcasecmp(tag->value, value) == 0);
  }
};

way_t::way_t()
  : visible_item_t()
{
  memset(&draw, 0, sizeof(draw));
}

way_t::way_t(unsigned int ver, item_id_t i)
  : visible_item_t(ver, i)
{
  memset(&draw, 0, sizeof(draw));
}

bool way_t::contains_node(const node_t *node) const
{
  /* return true if node is part of way */
  return std::find(node_chain.begin(), node_chain.end(), node) != node_chain.end();
}

void way_t::append_node(node_t *node) {
  node_chain.push_back(node);
  node->ways++;
}

bool way_t::ends_with_node(const node_t *node) const noexcept
{
  /* and deleted way may even not contain any nodes at all */
  /* so ignore it */
  if(flags & OSM_FLAG_DELETED)
    return false;

  /* any valid way must have at least two nodes */
  assert(!node_chain.empty());

  if(node_chain.front() == node)
    return true;

  if(node_chain.back() == node)
    return true;

  return false;
}

void way_t::cleanup() {
  osm_node_chain_free(node_chain);
  tags.clear();

  /* there must not be anything left in this chain */
  assert_null(map_item_chain);
}

bool way_t::merge(way_t *other, osm_t *osm, const std::vector<relation_t *> &rels)
{
  printf("  request to extend way #" ITEM_ID_FORMAT "\n", other->id);

  // drop the visible items
  other->item_chain_destroy();

  assert(ends_with_node(other->node_chain.front()) ||
           ends_with_node(other->node_chain.back()));

  const bool collision = tags.merge(other->tags);

  // nothing to do
  if(unlikely(other->node_chain.size() < 2)) {
    osm->way_free(other);
    return collision;
  }

  /* make enough room for all nodes */
  node_chain.reserve(node_chain.size() + other->node_chain.size() - 1);

  if(other->node_chain.front() == node_chain.front()) {
    printf("  need to prepend\n");
    node_chain.insert(node_chain.begin(), other->node_chain.rbegin(), --other->node_chain.rend());

    other->node_chain.resize(1);
  } else if(other->node_chain.back() == node_chain.front()) {
    printf("  need to prepend\n");
    node_chain.insert(node_chain.begin(), other->node_chain.begin(), --other->node_chain.end());

    other->node_chain.erase(other->node_chain.begin(), other->node_chain.end() - 1);
  } else if(other->node_chain.back() == node_chain.back()) {
    printf("  need to append\n");
    node_chain.insert(node_chain.end(), ++other->node_chain.rbegin(), other->node_chain.rend());

    other->node_chain.erase(other->node_chain.begin(), other->node_chain.end() - 1);
  } else {
    printf("  need to append\n");
    node_chain.insert(node_chain.end(), ++other->node_chain.begin(), other->node_chain.end());

    other->node_chain.resize(1);
  }

  /* replace "other" in relations */
  std::for_each(rels.begin(), rels.end(),
                relation_object_replacer(object_t(other), object_t(this)));

  /* erase and free other way (now only containing the overlapping node anymore) */
  osm->way_free(other);

  flags |= OSM_FLAG_DIRTY;

  return collision;
}

member_t::member_t(object_t::type_t t) noexcept
  : role(nullptr)
{
  object.type = t;
}

member_t::member_t(const object_t &o, const char *r)
  : object(o)
  , role(value_cache.insert(r))
{
}

bool member_t::operator==(const member_t &other) const noexcept
{
  if(object != other.object)
    return false;

  // check if any of them is 0, strcmp() does not like that
  if(!!role ^ !!other.role)
    return false;

  return !role || strcmp(role, other.role) == 0;
}

relation_t::relation_t()
  : base_object_t()
{
}

relation_t::relation_t(unsigned int ver, item_id_t i)
  : base_object_t(ver, i)
{
}

std::vector<member_t>::iterator relation_t::find_member_object(const object_t &o) {
  return std::find_if(members.begin(), members.end(), find_member_object_functor(o));
}

struct member_counter {
  unsigned int &nodes, &ways, &relations;
  member_counter(unsigned int &n, unsigned int &w, unsigned int &r) : nodes(n), ways(w), relations(r) {}
  void operator()(const member_t &member) noexcept;
};

void member_counter::operator()(const member_t &member) noexcept
{
  switch(member.object.type) {
  case object_t::NODE:
  case object_t::NODE_ID:
    nodes++;
    break;
  case object_t::WAY:
  case object_t::WAY_ID:
    ways++;
    break;
  case object_t::RELATION:
  case object_t::RELATION_ID:
    relations++;
    break;
  default:
    assert_unreachable();
  }
}

void relation_t::members_by_type(unsigned int &nodes, unsigned int &ways, unsigned int &relations) const {
  std::for_each(members.begin(), members.end(),
                member_counter(nodes, ways, relations));
}

node_t::node_t() noexcept
  : visible_item_t()
  , ways(0)
{
  memset(&pos, 0, sizeof(pos));
  memset(&lpos, 0, sizeof(lpos));
}

node_t::node_t(unsigned int ver, const lpos_t lp, const pos_t &p, item_id_t i) noexcept
  : visible_item_t(ver, i)
  , ways(0)
  , pos(p)
  , lpos(lp)
{
}

template<typename T> T *osm_find_by_id(const std::map<item_id_t, T *> &map, item_id_t id) {
  const typename std::map<item_id_t, T *>::const_iterator it = map.find(id);
  if(it != map.end())
    return it->second;

  return nullptr;
}

osm_t::osm_t()
  : uploadPolicy(Upload_Normal)
{
  bounds.ll = pos_area(pos_t(NAN, NAN), pos_t(NAN, NAN));
}

osm_t::~osm_t()
{
  std::for_each(ways.begin(), ways.end(), ::way_free);
  std::for_each(nodes.begin(), nodes.end(), nodefree);
  std::for_each(relations.begin(), relations.end(),
                osm_relation_free_pair);
}

node_t *osm_t::node_by_id(item_id_t id) const {
  return osm_find_by_id<node_t>(nodes, id);
}

way_t *osm_t::way_by_id(item_id_t id) const {
  return osm_find_by_id<way_t>(ways, id);
}

relation_t *osm_t::relation_by_id(item_id_t id) const {
  return osm_find_by_id<relation_t>(relations, id);
}

osm_t::dirty_t::dirty_t(const osm_t &osm)
  : nodes(osm.nodes)
  , ways(osm.ways)
  , relations(osm.relations)
{
}

template<typename T>
struct object_counter {
  osm_t::dirty_t::counter<T> &dirty;
  explicit object_counter(osm_t::dirty_t::counter<T> &d) : dirty(d) {}
  void operator()(std::pair<item_id_t, T *> pair);
};

template<typename T>
void osm_t::dirty_t::counter<T>::object_counter::operator()(std::pair<item_id_t, T *> pair)
{
  T * const obj = pair.second;
  int flags = obj->flags;
  if(flags & OSM_FLAG_DELETED) {
    dirty.deleted.push_back(obj);
  } else if(obj->isNew()) {
    dirty.added++;
    dirty.modified.push_back(obj);
  } else if(flags & OSM_FLAG_DIRTY) {
    dirty.dirty++;
    dirty.modified.push_back(obj);
  }
}

template<typename T>
osm_t::dirty_t::counter<T>::counter(const std::map<item_id_t, T *> &map)
  : total(map.size())
  , added(0)
  , dirty(0)
{
  std::for_each(map.begin(), map.end(), object_counter(*this));
}

// vim:et:ts=8:sw=2:sts=2:ai
