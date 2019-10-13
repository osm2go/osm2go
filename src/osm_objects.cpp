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

#include "osm_objects.h"

#include "osm_p.h"

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_platform.h>

static_assert(sizeof(tag_list_t) == sizeof(tag_t *), "tag_list_t is not exactly as big as a pointer");

const char *tag_t::mapToCache(const char *v)
{
  return value_cache.insert(v);
}

tag_t::tag_t(const char *k, const char *v)
  : key(mapToCache(k))
  , value(mapToCache(v))
{
}

bool tag_t::is_creator_tag(const char *key) noexcept
{
  return (strcasecmp(key, "created_by") == 0);
}

tag_list_t::~tag_list_t()
{
  clear();
}

bool tag_list_t::empty() const noexcept
{
  return contents == nullptr || contents->empty();
}

bool tag_list_t::hasNonCreatorTags() const noexcept
{
  if(empty())
    return false;

  const std::vector<tag_t>::const_iterator itEnd = contents->end();
  return std::find_if(std::cbegin(*contents), itEnd, tag_t::is_no_creator) != itEnd;
}

static bool isRealTag(const tag_t &tag)
{
  return !tag.is_creator_tag() && strcmp(tag.key, "source") != 0;
}

bool tag_list_t::hasRealTags() const noexcept
{
  if(empty())
    return false;

  const std::vector<tag_t>::const_iterator itEnd = contents->end();
  return std::find_if(std::cbegin(*contents), itEnd, isRealTag) != itEnd;
}

const tag_t *tag_list_t::singleTag() const noexcept
{
  if(unlikely(empty()))
    return nullptr;

  const std::vector<tag_t>::const_iterator itEnd = contents->end();
  const std::vector<tag_t>::const_iterator it = std::find_if(std::cbegin(*contents), itEnd, isRealTag);
  if(unlikely(it == itEnd))
    return nullptr;
  if (std::find_if(std::next(it), itEnd, isRealTag) != itEnd)
    return nullptr;

  return &(*it);
}

namespace {

class key_match_functor {
  const char * const key;
public:
  explicit inline key_match_functor(const char *k) : key(k) {}
  inline bool operator()(const tag_t &tag) const {
    return tag.key_compare(key);
  }
};

} // namespace

const char* tag_list_t::get_value(const char *key) const
{
  if(empty())
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

#if __cplusplus < 201103L
// workaround for the fact that the default constructor is unavailable
template<>
inline void shrink_to_fit(std::vector<tag_t> &v)
{
  size_t sz = v.size();
  if(v.capacity() == sz)
    return;
  std::vector<tag_t> tmp(sz, tag_t::uncached(nullptr, nullptr));
  tmp = v;
  tmp.swap(v);
}

void tag_list_t::replace(std::vector<tag_t> &ntags)
#else
void tag_list_t::replace(std::vector<tag_t> &&ntags)
#endif
{
  if(ntags.empty()) {
    clear();
    return;
  }

  if(contents == nullptr) {
#if __cplusplus >= 201103L
    contents = new std::vector<tag_t>(std::move(ntags));
#else
    contents = new std::vector<tag_t>(ntags.size(), tag_t::uncached(nullptr, nullptr));
    contents->swap(ntags);
#endif
  } else {
#if __cplusplus >= 201103L
    *contents = std::move(ntags);
#else
    contents->swap(ntags);
#endif
  }

  shrink_to_fit(*contents);
}

namespace {

class tag_fill_functor {
  std::vector<tag_t> &tags;
public:
  explicit inline tag_fill_functor(std::vector<tag_t> &t) : tags(t) {}
  void operator()(const osm_t::TagMap::value_type &p) {
    if(unlikely(tag_t::is_creator_tag(p.first.c_str())))
      return;

    tags.push_back(tag_t(p.first.c_str(), p.second.c_str()));
  }
};

} // namespace

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
  assert(isDeleted());

  xmlNodePtr obj_node = xmlNewChild(parent_node, nullptr, BAD_CAST apiString(), nullptr);

  xmlNewProp(obj_node, BAD_CAST "id", BAD_CAST id_string().c_str());

  char buf[32];
  snprintf(buf, sizeof(buf), "%u", version);

  xmlNewProp(obj_node, BAD_CAST "version", BAD_CAST buf);
  xmlNewProp(obj_node, BAD_CAST "changeset", BAD_CAST changeset);
}

void base_object_t::markDeleted()
{
  printf("mark %s #" ITEM_ID_FORMAT " as deleted\n", apiString(), id);
  flags = OSM_FLAG_DELETED;
  tags.clear();
}

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
  if(isDeleted())
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
  assert_null(map_item);
}

node_t *way_t::insert_node(osm_t::ref osm, int position, lpos_t coords)
{
  node_t *node = osm->node_new(coords);
  osm->node_attach(node);

  /* search correct position */
  node_chain.insert(std::next(node_chain.begin(), position), node);

  /* remember that this node is contained in one way */
  node->ways = 1;

  /* and that the way needs to be uploaded */
  flags |= OSM_FLAG_DIRTY;

  return node;
}

bool way_t::merge(way_t *other, osm_t *osm, map_t *map, const std::vector<relation_t *> &rels)
{
  printf("  request to extend way #" ITEM_ID_FORMAT "\n", other->id);

  // drop the visible items
  other->item_chain_destroy(map);

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
    node_chain.insert(node_chain.begin(), other->node_chain.rbegin(), std::prev(other->node_chain.rend()));

    other->node_chain.resize(1);
  } else if(other->node_chain.back() == node_chain.front()) {
    printf("  need to prepend\n");
    node_chain.insert(node_chain.begin(), other->node_chain.begin(), std::prev(other->node_chain.end()));

    other->node_chain.erase(other->node_chain.begin(), other->node_chain.end() - 1);
  } else if(other->node_chain.back() == node_chain.back()) {
    printf("  need to append\n");
    node_chain.insert(node_chain.end(), std::next(other->node_chain.rbegin()), other->node_chain.rend());

    other->node_chain.erase(other->node_chain.begin(), other->node_chain.end() - 1);
  } else {
    printf("  need to append\n");
    node_chain.insert(node_chain.end(), std::next(other->node_chain.begin()), other->node_chain.end());

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

namespace {

class member_counter {
  unsigned int &nodes, &ways, &relations;
public:
  inline member_counter(unsigned int &n, unsigned int &w, unsigned int &r) : nodes(n), ways(w), relations(r) {}
  void operator()(const member_t &member) noexcept;
};

} // namespace

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

node_t::node_t(unsigned int ver, const lpos_t lp, const pos_t &p) noexcept
  : visible_item_t(ver, ID_ILLEGAL)
  , ways(0)
  , pos(p)
  , lpos(lp)
{
}

node_t::node_t(unsigned int ver, const pos_t &p, item_id_t i) noexcept
  : visible_item_t(ver, i)
  , ways(0)
  , pos(p)
  , lpos(0, 0)
{
}
