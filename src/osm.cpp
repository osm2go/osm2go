/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _DEFAULT_SOURCE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "osm.h"
#include "osm_p.h"

#include "cache_set.h"
#include "map.h"
#include "misc.h"
#include "osm_objects.h"
#include "pos.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <numeric>
#include <optional>
#include <string>
#include <strings.h>
#include <utility>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_platform.h>

cache_set value_cache;

bool object_t::operator==(const object_t &other) const noexcept
{
  // the base types must be identical
  if ((type & ~_REF_FLAG) != (other.type & ~_REF_FLAG))
    return false;

  return get_id() == other.get_id();
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

  types[object_t::ILLEGAL] =     tr_noop("illegal");
  types[object_t::NODE] =        tr_noop("node");
  types[object_t::WAY] =         tr_noop("way/area");
  types[object_t::RELATION] =    tr_noop("relation");
  types[object_t::NODE_ID] =     tr_noop("node id");
  types[object_t::WAY_ID] =      tr_noop("way/area id");
  types[object_t::RELATION_ID] = tr_noop("relation id");

  return types;
}

trstring::native_type
object_t::type_string() const
{
  static std::map<type_t, const char *> types = type_string_init();

  if(type == WAY) {
    if(!way->is_closed())
      return _("way");
    else if(way->is_area())
      return _("area");
  }

  const std::map<type_t, const char *>::const_iterator it = types.find(type);

  if(likely(it != types.end()))
    return _(it->second);

  assert_unreachable();
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

/* -------------------- tag handling ----------------------- */

class map_value_match_functor {
  const std::string &value;
public:
  explicit inline map_value_match_functor(const std::string &v) : value(v) {}
  inline bool operator()(const osm_t::TagMap::value_type &pair) const {
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

class check_subset {
  const osm_t::TagMap &super;
  const osm_t::TagMap::const_iterator superEnd;
public:
  explicit inline check_subset(const osm_t::TagMap &s) : super(s), superEnd(s.end()) {}
  inline bool operator()(const osm_t::TagMap::value_type &v) const
  {
    return super.findTag(v.first, v.second) == superEnd;
  }
};

bool osm_t::tagSubset(const TagMap &sub, const TagMap &super)
{
  return std::none_of(sub.begin(), sub.end(), check_subset(super));
}

void relation_object_replacer::operator()(relation_t *r)
{
  const std::vector<member_t>::iterator itBegin = r->members.begin();
  std::vector<member_t>::iterator itEnd = r->members.end();

  for(std::vector<member_t>::iterator it = itBegin; it != itEnd; it++) {
    if(it->object != old)
      continue;

    osm->mark_dirty(r);

    it->object = replace;

    // check if this member now is the same as the next or previous one
    if((it != itBegin && *std::prev(it) == *it) || (std::next(it) != itEnd && *it == *std::next(it))) {
      it = r->members.erase(it);
      // this is now the next element, go one back so this is actually checked
      // as the for loop increments the iterator again
      if(likely(it != itBegin))
        it--;

      // end iterator changed because container was modified, update it
      itEnd = r->members.end();
    }
  }
}

class relation_membership_functor {
  std::vector<relation_t *> &arels, &brels;
  const object_t &a, &b;
public:
  explicit inline relation_membership_functor(const object_t &first, const object_t &second,
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
  assert(first.type == second.type);
  assert(first.type == object_t::NODE || first.type == object_t::WAY);

  std::vector<relation_t *> removeRels, keepRels;

  std::for_each(relations.begin(), relations.end(), relation_membership_functor(remove, keep, removeRels, keepRels));
  const base_object_t * const keepObj = static_cast<base_object_t *>(keep);
  const base_object_t * const removeObj = static_cast<base_object_t *>(remove);

  // find out which node to keep
  bool nret =
              // if one is new: keep the other one
              (keepObj->isNew() && !removeObj->isNew()) ||
              // or keep the one with most relations
              removeRels.size() > keepRels.size() ||
              // or the one with most ways (if nodes)
              (keep.type == object_t::NODE &&
#if 0
                                              remove.type == keep.type &&
#endif
               static_cast<node_t *>(remove)->ways > static_cast<node_t *>(keep)->ways) ||
              // or the one with most nodes (if ways)
              (keep.type == object_t::WAY &&
#if 0
                                             remove.type == keep.type &&
#endif
               static_cast<way_t *>(remove)->node_chain.size() > static_cast<way_t *>(keep)->node_chain.size()) ||
#if 0
              // or the one with most members (if relations)
              (keep.type == object_t::RELATION && remove.type == keep.type &&
               remove.relation->members.size() > keep.relation->members.size()) ||
#endif
              // or the one with the longest history
              removeObj->version > keepObj->version ||
              // or simply the older one
              (removeObj->id > 0 && removeObj->id < keepObj->id);

  if(nret)
    rels.swap(keepRels);
  else
    rels.swap(removeRels);

  return !nret;
}

class find_way_ends {
  const node_t * const node;
public:
  explicit find_way_ends(const node_t *n) : node(n) {}
  bool operator()(const std::pair<item_id_t, way_t *> &p) const {
    return p.second->ends_with_node(node);
  }
};

osm_t::mergeResult<node_t> osm_t::mergeNodes(node_t *first, node_t *second, std::array<way_t *, 2> &mergeways)
{
  node_t *keep = first, *remove = second;

  std::vector<relation_t *> rels;
  if(!checkObjectPersistence(object_t(keep), object_t(remove), rels))
    std::swap(keep, remove);

  mark_dirty(keep);
  mark_dirty(remove);

  /* use "second" position as that was the target */
  keep->lpos = second->lpos;
  keep->pos = second->pos;

#if O2G_COMPILER_IS_GNU && ((__GNUC__ * 100 + __GNUC_MINOR__) < 403)
  mergeways.assign(nullptr);
#else
  mergeways.fill(nullptr);
#endif
  bool mayMerge = keep->ways == 1 && remove->ways == 1; // if there could be mergeable ways

  const std::map<item_id_t, way_t *>::iterator witEnd = ways.end();
  const std::map<item_id_t, way_t *>::iterator witBegin = ways.begin();
  std::map<item_id_t, way_t *>::iterator wit;
  if(mayMerge) {
    // only ways ending in that node are considered
    wit = std::find_if(witBegin, witEnd, find_way_ends(keep));
    if(wit != witEnd)
      mergeways[0] = wit->second;
    else
      mayMerge = false;
  }

  for(wit = witBegin; remove->ways > 0 && wit != witEnd; wit++) {
    way_t * const way = wit->second;
    const node_chain_t::iterator itBegin = way->node_chain.begin();
    node_chain_t::iterator it = itBegin;
    node_chain_t::iterator itEnd = way->node_chain.end();

    while(remove->ways > 0 && (it = std::find(it, itEnd, remove)) != itEnd) {
      printf("  found node in way #" ITEM_ID_FORMAT "\n", way->id);

      mark_dirty(way);
      // check if this node is the same as the neighbor
      if((it != itBegin && *std::prev(it) == keep) || (std::next(it) != itEnd && *std::next(it) == keep)) {
        // this node would now be twice in the way at adjacent positions
        it = way->node_chain.erase(it);
        itEnd = way->node_chain.end();
      } else {
        if(mayMerge) {
          if(way != mergeways[0] && way->ends_with_node(remove)) {
            mergeways[1] = way;
          } else {
            mergeways[0] = nullptr;
            mayMerge = false; // unused from now on, but to be sure
          }
        }
        /* replace by keep */
        *it = keep;
        // no need to check this one again
        it++;
        keep->ways++;
      }

      /* and adjust way references of remove */
      assert_cmpnum_op(remove->ways, >, 0);
      remove->ways--;
    }
  }
  assert_cmpnum(remove->ways, 0);

  /* replace "remove" in relations */
  std::for_each(rels.begin(), rels.end(),
                relation_object_replacer(this, object_t(remove), object_t(keep)));

  /* transfer tags from "remove" to "keep" */
  bool conflict = keep->tags.merge(remove->tags);

  /* remove must not have any references to ways anymore */
  assert_cmpnum(remove->ways, 0);

  node_delete(remove, osm_t::NodeDeleteKeepRefs);

  return mergeResult<node_t>(keep, conflict);
}

osm_t::mergeResult<way_t> osm_t::mergeWays(way_t *first, way_t *second, map_t *map)
{
  assert(first != second);
  std::vector<relation_t *> rels;
  if(!checkObjectPersistence(object_t(first), object_t(second), rels))
    std::swap(first, second);

  /* ---------- transfer tags from second to first ----------- */

  return mergeResult<way_t>(first, first->merge(second, this, map, rels));
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

  if(honor_hidden_flags && !hiddenWays.empty())
    return false;

  // now check all objects for modifications
  return original.nodes.empty() &&
         original.ways.empty() &&
         original.relations.empty();
}

class tag_match_functor {
  const tag_t &other;
  const bool same_values;
public:
  inline tag_match_functor(const tag_t &o, bool s) : other(o), same_values(s) {}
  bool operator()(const tag_t &tag) const
  {
    return (strcasecmp(other.key, tag.key) == 0) &&
           ((strcasecmp(other.value, tag.value) == 0) == same_values);
  }
};

bool tag_list_t::merge(tag_list_t &other)
{
  if(other.empty())
    return false;

  if(empty()) {
    contents.swap(other.contents);
    return false;
  }

  bool conflict = false;

  /* ---------- transfer tags from way[1] to way[0] ----------- */
  const std::vector<tag_t>::const_iterator itEnd = other.contents->end();
  for(std::vector<tag_t>::const_iterator srcIt = std::cbegin(*other.contents); srcIt != itEnd; srcIt++) {
    const tag_t &src = *srcIt;
    /* don't copy discardable tags or tags that already
     * exist in identical form */
    if(!src.is_discardable() && !contains(tag_match_functor(src, true))) {
      /* check if same key but with different value is present */
      if(!conflict)
        conflict = contains(tag_match_functor(src, false));
      contents->push_back(src);
    }
  }

  other.contents.reset();

  return conflict;
}

struct check_discardable_tag {
  inline bool operator()(const tag_t tag) {
    return tag.is_discardable();
  }
  inline bool operator()(const osm_t::TagMap::value_type tag)
  {
    return tag_t::is_discardable(tag.first.c_str());
  }
};

class tag_find_functor {
  const char * const needle;
public:
  explicit inline tag_find_functor(const char *n) : needle(n) {}
  inline bool operator()(const tag_t &tag) const {
    return (strcmp(needle, tag.key) == 0);
  }
};

/**
 * @brief do the common check to compare a tag_list with another set of tags
 * @returns if the end result is fixed and the result if it is
 * @retval true the compare has finished, result hold the decision
 * @retval false further checks have to be done
 */
template<typename T>
static std::optional<bool> tag_list_compare_base(const tag_list_t &list,
                                                 const std::unique_ptr<std::vector<tag_t> > &contents,
                                                 const T &other, unsigned int &t1discardables)
{
  if(list.empty() && other.empty())
    return false;

  // Special case for an empty list as contents is not set in this case and
  // must not be dereferenced. Check if t2 only consists of a creator tag, in
  // which case both lists would still be considered the same, or not. Not
  // further checks need to be done for the end result.
  const typename T::const_iterator t2start = other.begin();
  const typename T::const_iterator t2End = other.end();
  unsigned int t2discardables = std::count_if(t2start, t2End, check_discardable_tag());
  if(list.empty())
    return (other.size() != t2discardables);

  /* first check list length, otherwise deleted tags are hard to detect */
  std::vector<tag_t>::size_type ocnt = contents->size();
  const std::vector<tag_t>::const_iterator t1End = contents->end();
  t1discardables = std::count_if(std::cbegin(*contents), t1End, check_discardable_tag());

  // the result can't become negative here as it was checked before that contents is not empty
  if (other.size() - t2discardables != ocnt - t1discardables)
    return true;

  return std::optional<bool>();
}

bool tag_list_t::operator!=(const std::vector<tag_t> &t2) const {
  unsigned int t1discardables;
  std::optional<bool> r = tag_list_compare_base(*this, contents, t2, t1discardables);
  if(r)
    return *r;

  const std::vector<tag_t>::const_iterator t2End = t2.end();
  const std::vector<tag_t>::const_iterator t2start = t2.begin();

  std::vector<tag_t>::const_iterator t1it = contents->begin();
  const std::vector<tag_t>::const_iterator t1End = contents->end();

  for (; t1it != t1End; t1it++) {
    if (t1discardables && t1it->is_discardable()) {
      t1discardables--; // do a countdown to avoid needless string compares
      continue;
    }
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
  unsigned int t1discardables;
  std::optional<bool> r = tag_list_compare_base(*this, contents, t2, t1discardables);
  if(r)
    return *r;

  std::vector<tag_t>::const_iterator t1it = contents->begin();
  const std::vector<tag_t>::const_iterator t1End = contents->end();

  for (; t1it != t1End; t1it++) {
    if (t1discardables && t1it->is_discardable()) {
      t1discardables--; // do a countdown to avoid needless string compares
      continue;
    }
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

namespace {

class collision_functor {
  const char *key;
public:
  explicit inline collision_functor(const tag_t &t) : key(t.key) { }
  inline bool operator()(const tag_t &t) const {
    return (strcasecmp(t.key, key) == 0);
  }
};

} // namespace

bool tag_list_t::hasTagCollisions() const
{
  if(empty())
    return false;

  const std::vector<tag_t>::const_iterator itEnd = contents->end();
  for(std::vector<tag_t>::const_iterator it = contents->begin();
      std::next(it) != itEnd; it++) {
    if (std::any_of(std::next(it), itEnd, collision_functor(*it)))
      return true;
  }
  return false;
}

template<typename T>
void osm_t::wipeImpl(T *obj)
{
  if (likely(obj->id != ID_ILLEGAL)) {
    size_t rcnt = objects<T>().erase(obj->id);
    assert_cmpnum(rcnt, 1);

    rcnt = originalObjects<T>().erase(obj->id);
    assert_cmpnum_op(rcnt, <=, 1);
  }

  delete obj;
}

/* ------------------- node handling ------------------- */

void osm_t::wipe(node_t *node)
{
  /* there must not be anything left in this chain */
  assert_null(node->map_item);

  wipeImpl(node);
}

/* ------------------- way handling ------------------- */
static void osm_unref_node(node_t* node)
{
  assert_cmpnum_op(node->ways, >, 0);
  node->ways--;
}

void osm_node_chain_unref(node_chain_t &node_chain)
{
  std::for_each(node_chain.begin(), node_chain.end(), osm_unref_node);
}

void osm_t::wipe(way_t *way)
{
  /* there must not be anything left in this chain */
  assert_null(way->map_item);

  wipeImpl(way);
}

/* ------------------- relation handling ------------------- */

bool relation_t::is_multipolygon() const {
  const char *tp = tags.get_value("type");
  return tp != nullptr && (strcmp(tp, "multipolygon") == 0);
}

class gen_xml_relation_functor {
  xmlNodePtr const xml_node;
public:
  explicit inline gen_xml_relation_functor(xmlNodePtr n) : xml_node(n) {}
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

  if(member.role != nullptr)
    xmlNewProp(m_node, BAD_CAST "role", BAD_CAST member.role);
}

void relation_t::generate_member_xml(xmlNodePtr xml_node) const
{
  std::for_each(members.begin(), members.end(), gen_xml_relation_functor(xml_node));
}

void osm_t::wipe(relation_t *relation)
{
  wipeImpl(relation);
}

trstring::native_type osm_t::sanity_check() const
{
  if(unlikely(!bounds.ll.valid()))
    return _("Invalid data in OSM file:\nBoundary box invalid!");

  if(unlikely(nodes.empty()))
    return _("Invalid data in OSM file:\nNo drawable content found!");

  return trstring::native_type();
}

/* ------------------------- misc access functions -------------- */

class tag_to_xml {
  xmlNodePtr const node;
  const bool keep_created;
public:
  explicit inline tag_to_xml(xmlNodePtr n, bool k = false) : node(n), keep_created(k) {}
  void operator()(const tag_t &tag) {
    /* skip discardable tags, see https://wiki.openstreetmap.org/wiki/Discardable_tags */
    if(likely(!tag.is_discardable() ||
       /* allow to explicitely keep "created_by", which will be used in changeset tags */
       (keep_created && strcmp(tag.key, "created_by") == 0))) {
      xmlNodePtr tag_node = xmlNewChild(node, nullptr, BAD_CAST "tag", nullptr);
      xmlNewProp(tag_node, BAD_CAST "k", BAD_CAST tag.key);
      xmlNewProp(tag_node, BAD_CAST "v", BAD_CAST tag.value);
    }
  }
};

xmlChar *base_object_t::generate_xml(const std::string &changeset) const
{
  char str[32];
  xmlDocGuard doc(xmlNewDoc(BAD_CAST "1.0"));
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

class add_xml_node_refs {
  xmlNodePtr const way_node;
public:
  explicit inline add_xml_node_refs(xmlNodePtr n) : way_node(n) {}
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

  xmlDocGuard doc(xmlNewDoc(BAD_CAST "1.0"));
  xmlNodePtr root_node = xmlNewNode(nullptr, BAD_CAST "osm");
  xmlDocSetRootElement(doc.get(), root_node);

  xmlNodePtr cs_node = xmlNewChild(root_node, nullptr, BAD_CAST "changeset", nullptr);

  tag_to_xml fc(cs_node, true);
  fc(tag_creator);
  fc(tag_comment);
  if(!src.empty())
    fc(tag_t::uncached("source", src.c_str()));

  xmlDocDumpFormatMemoryEnc(doc.get(), &result, &len, "UTF-8", 1);

  return result;
}

/* ---------- edit functions ------------- */

template<typename T> void osm_t::attachObject(T *obj)
{
  std::map<item_id_t, T *> &map = objects<T>();
#ifndef NDEBUG
  // the variables are needed to avoid the need for "typename" because these are templates in templates
  item_id_t id = obj->id;
  assert_cmpnum(id, ID_ILLEGAL);
  unsigned int version = obj->version;
  assert_cmpnum(version, 0);
#endif
  if(map.empty()) {
    obj->id = -1;
  } else {
    // map is sorted, so use one less the first id in the container if it is negative,
    // or -1 if it is positive
    const typename std::map<item_id_t, T *>::const_iterator it = map.begin();
    if(it->first >= 0)
      obj->id = -1;
    else
      obj->id = it->first - 1;
  }
  printf("Attaching %s " ITEM_ID_FORMAT "\n", obj->apiString(), obj->id);
  map[obj->id] = obj;
}

node_t *osm_t::node_new(const lpos_t lpos) {
  /* convert screen position back to ll */
  return new node_t(base_attributes(), lpos, lpos.toPos(bounds));
}

node_t *osm_t::node_new(const pos_t &pos, const base_attributes &attr)
{
  /* convert ll position to screen */
  return new node_t(attr, pos.toLpos(bounds), pos);
}

void osm_t::attach(node_t *node) {
  attachObject(node);
}

way_t *osm_t::attach(way_t *way)
{
  attachObject(way);
  return way;
}

namespace {

template<typename T ENABLE_IF_CONVERTIBLE(T *, base_object_t *)> bool
unmarkedDirty(T *obj, const base_object_t *orig, osm_t *osm)
{
  if (*obj == *static_cast<const T *>(orig)) {
    osm->unmark_dirty(obj);
    return true;
  }

  return false;
}

} // namespace

void
osm_t::updateTags(object_t o, const TagMap &ntags)
{
  // when no tags have changed at this point nothing has to be updated
  if (static_cast<base_object_t *>(o)->tags == ntags)
    return;

  const base_object_t * const origobj = originalObject(o);
  bool tagsUpdated = false;

  if (origobj != nullptr) {
    static_cast<base_object_t *>(o)->tags.replace(ntags);
    tagsUpdated = true;

    // reset the objects to being unmodified if possible
    switch (o.type) {
    case object_t::NODE:
      if (unmarkedDirty(static_cast<node_t *>(o), origobj, this))
        return;
      break;
    case object_t::WAY:
      if (unmarkedDirty(static_cast<way_t *>(o), origobj, this))
        return;
      break;
    case object_t::RELATION:
      if (unmarkedDirty(static_cast<relation_t *>(o), origobj, this))
        return;
      break;
    default:
      assert_unreachable();
    }
  }

  // only mark dirty if this will actually change something
  if (static_cast<base_object_t *>(o)->tags != ntags) {
    switch (o.type) {
    case object_t::NODE:
      mark_dirty(static_cast<node_t *>(o));
      break;
    case object_t::WAY:
      mark_dirty(static_cast<way_t *>(o));
      break;
    case object_t::RELATION:
      mark_dirty(static_cast<relation_t *>(o));
      break;
    default:
      assert_unreachable();
    }
  }

  if (!tagsUpdated)
    static_cast<base_object_t *>(o)->tags.replace(ntags);
}

namespace {

class node_chain_delete_functor {
  osm_t * const osm;
  const node_t * const node;
  way_chain_t &way_chain;
public:
  inline node_chain_delete_functor(osm_t *o, const node_t *n, way_chain_t &w)
    : osm(o), node(n), way_chain(w) {}
  void operator()(std::pair<item_id_t, way_t *> p);
};

void node_chain_delete_functor::operator()(std::pair<item_id_t, way_t *> p)
{
  way_t * const way = p.second;
  node_chain_t &chain = way->node_chain;
  bool modified = false;

  node_chain_t::iterator it = chain.begin();
  // special case closed ways where the closing node is deleted
  bool needsClose = way->is_closed() && way->ends_with_node(node);

  while((it = std::find(it, chain.end(), node)) != chain.end()) {
    if (!modified) {
      modified = true;
      osm->mark_dirty(way);

      // and add the way to the list of affected ways
      way_chain.push_back(way);
    }

    // remove node from chain
    it = chain.erase(it);
  }

  // the way was formerly closed and the end node was deleted: use the
  // remaining front node to close the way again.
  if (needsClose && chain.size() > 1 && chain.front() != chain.back())
    way->append_node(chain.front());
}

class node_deleted_from_ways {
  map_t * const map;
  osm_t * const osm;
public:
  explicit inline node_deleted_from_ways(map_t *m, osm_t *o) : map(m), osm(o) { }
  void operator()(way_t *way);
};

/* redraw all affected ways */
void node_deleted_from_ways::operator()(way_t *way)
{
  if(way->node_chain.size() <= 1 ||
    // closed way that consists only of the same node
    (way->node_chain.size() == 2 && way->node_chain.front() == way->node_chain.back())) {
    /* this way now only contains one node and thus isn't a valid */
    /* way anymore. So it'll also get deleted (which in turn may */
    /* cause other nodes to be deleted as well) */
    osm->way_delete(way, map);
  } else {
    // just redraw, this will filter out deleted and hidden objects itself
    map->redraw_item(way);
  }
}

node_t *
cloneForDeletion(node_t &o)
{
  return new node_t(o);
}

way_t *
cloneForDeletion(way_t &o)
{
  node_chain_t nodes;
  nodes.swap(o.node_chain);
  way_t *ret = new way_t(o);
  ret->node_chain.swap(nodes);
  return ret;
}

relation_t *
cloneForDeletion(relation_t &o)
{
  std::vector<member_t> members;
  members.swap(o.members);
  relation_t *ret = new relation_t(o);
  ret->members.swap(members);
  return ret;
}

} // namespace

template<typename T>
void osm_t::markDeleted(T &obj)
{
  // new objects should simply be deleted
  if (obj.isNew()) {
    printf("permanently delete %s #" ITEM_ID_FORMAT "\n", obj.apiString(), obj.id);

    assert(originalObjects<T>().find(obj.id) == originalObjects<T>().end());
    wipe(&obj);
    return;
  }

  std::unordered_map<item_id_t, const T *> &orig = originalObjects<T>();

  printf("mark %s #" ITEM_ID_FORMAT " as deleted\n", obj.apiString(), obj.id);

  // no need to keep anything, it was already in the dirty map
  if (obj.flags & OSM_FLAG_DIRTY) {
    assert(orig.find(obj.id) != orig.end());
    obj.tags.clear();
  } else {
    // A previously unmodified object is about to be deleted, put it in the originalObjects
    // map along the way, but with less allocations than mark_dirty() would do.

    assert(orig.find(obj.id) == orig.end());

    tag_list_t tags;
    tags.swap(obj.tags);
    T *n = cloneForDeletion(obj);
    n->tags.swap(tags);
    cleanupOriginalObject(n);
    orig[obj.id] = n;
  }

  obj.flags = OSM_FLAG_DELETED;
}

void
osm_t::node_delete(node_t *node, NodeDeleteFlags flags, map_t *map)
{
  way_chain_t way_chain;

  // no need to iterate all ways if we already know in advance that none references this node
  if (node->ways > 0 && flags != NodeDeleteKeepRefs) {
    /* first remove node from all ways using it */
    std::for_each(ways.begin(), ways.end(),
                  node_chain_delete_functor(this, node, way_chain));
  }

  if(flags != NodeDeleteKeepRefs)
    remove_from_relations(object_t(node));

  /* remove that nodes map representations */
  node->item_chain_destroy(nullptr);

  markDeleted(*node);

  if (flags == NodeDeleteShortWays)
    std::for_each(way_chain.begin(), way_chain.end(), node_deleted_from_ways(map, this));
}

class remove_member_functor {
  osm_t * const osm;
  const object_t obj;
public:
  explicit inline remove_member_functor(osm_t *o, object_t ob) : osm(o), obj(ob) {}
  void operator()(std::pair<item_id_t, relation_t *> pair);
};

void remove_member_functor::operator()(std::pair<item_id_t, relation_t *> pair)
{
  relation_t * const relation = pair.second;
  std::vector<member_t>::iterator itEnd = relation->members.end();
  std::vector<member_t>::iterator it = relation->members.begin();

  while((it = std::find(it, itEnd, obj)) != itEnd) {
    printf("  from relation #" ITEM_ID_FORMAT "\n", relation->id);

    osm->mark_dirty(relation);
    it = relation->members.erase(it);
    // refresh end iterator as the vector was modified
    itEnd = relation->members.end();
  }
}

/* remove the given object from all relations. used if the object is to */
/* be deleted */
void osm_t::remove_from_relations(object_t obj) {
  printf("removing %s #" ITEM_ID_FORMAT " from all relations:\n", static_cast<base_object_t *>(obj)->apiString(), obj.get_id());

  std::for_each(relations.begin(), relations.end(),
                remove_member_functor(this, obj));
}

relation_t *osm_t::attach(relation_t *relation)
{
  attachObject(relation);
  return relation;
}

class find_relation_members {
  const object_t obj;
public:
  explicit inline find_relation_members(const object_t o) : obj(o) {}
  bool operator()(const std::pair<item_id_t, relation_t *> &pair) const {
    const std::vector<member_t>::const_iterator itEnd = pair.second->members.end();
    return std::find(std::cbegin(pair.second->members), itEnd, obj) != itEnd;
  }
};

class osm_unref_way_free {
  osm_t * const osm;
public:
  inline osm_unref_way_free(osm_t *o) : osm(o) {}
  void operator()(node_t *node);
};

void osm_unref_way_free::operator()(node_t *node)
{
  printf("checking node #" ITEM_ID_FORMAT " (still used by %u)\n",
         node->id, node->ways);
  assert_cmpnum_op(node->ways, >, 0);
  node->ways--;

  /* this node must only be part of this way */
  if(!node->ways && !node->tags.hasNonDiscardableTags()) {
    /* delete this node, but don't let this actually affect the */
    /* associated ways as the only such way is the one we are currently */
    /* deleting */
    if(osm->find_relation(find_relation_members(object_t(node))) == nullptr)
      osm->node_delete(node, osm_t::NodeDeleteKeepRefs);
  }
}

void osm_t::way_delete(way_t *way, map_t *map, void (*unref)(node_t *))
{
  if(likely(way->id != ID_ILLEGAL))
    remove_from_relations(object_t(way));

  /* remove it visually from the screen */
  way->item_chain_destroy(map);

  /* delete all nodes that aren't in other use now */
  node_chain_t &chain = way->node_chain;
  if(unref == nullptr)
    std::for_each(chain.begin(), chain.end(), osm_unref_way_free(this));
  else
    std::for_each(chain.begin(), chain.end(), unref);

  /* there must not be anything left in this chain */
  assert_null(way->map_item);

  if(!way->isNew()) {
    // this is already in the original list, so no need to keep the vector around
    if (way->flags & OSM_FLAG_DIRTY)
      chain.clear();
  }

  markDeleted(*way);
}

void osm_t::relation_delete(relation_t *relation) {
  remove_from_relations(object_t(relation));

  /* the deletion of a relation doesn't affect the members as they */
  /* don't have any reference to the relation they are part of */

  markDeleted(*relation);
}

/* Reverse direction-sensitive tags like "oneway". Marks the way as dirty if
 * anything is changed, and returns the number of flipped tags. */

class reverse_direction_sensitive_tags_functor {
  unsigned int &n_tags_altered;
public:
  explicit inline reverse_direction_sensitive_tags_functor(unsigned int &c) : n_tags_altered(c) {}
  void operator()(tag_t &etag);
};

#if __cplusplus >= 201703L
#include <string_view>
typedef std::vector<std::pair<std::string_view, std::string_view>> rtable_type;
#else
typedef std::vector<std::pair<std::string, std::string> > rtable_type;
#endif

static rtable_type rtable_init()
{
  rtable_type rtable(4);

  rtable[0] = rtable_type::value_type("left", "right");
  rtable[1] = rtable_type::value_type("right", "left");
  rtable[2] = rtable_type::value_type("forward", "backward");
  rtable[3] = rtable_type::value_type("backward", "forward");

  return rtable;
}

void reverse_direction_sensitive_tags_functor::operator()(tag_t &etag)
{
  static const char *oneway = value_cache.insert("oneway");
  static const char *sidewalk = value_cache.insert("sidewalk");
  static const char *DS_ONEWAY_FWD = value_cache.insert("yes");
  static const char *DS_ONEWAY_REV = value_cache.insert("-1");
  static const char *left = value_cache.insert("left");
  static const char *right = value_cache.insert("right");

  if (etag.key_compare(oneway)) {
    // oneway={yes/true/1/-1} is unusual.
    // Favour "yes" and "-1".
    if (etag.value_compare_ci(DS_ONEWAY_FWD) ||
        strcasecmp("true", etag.value) == 0 || strcmp(etag.value, "1") == 0) {
      etag = tag_t::uncached(oneway, DS_ONEWAY_REV);
      n_tags_altered++;
    } else if (etag.value_compare(DS_ONEWAY_REV)) {
      etag = tag_t::uncached(oneway, DS_ONEWAY_FWD);
      n_tags_altered++;
    } else {
      printf("warning: unknown oneway value: %s\n", etag.value);
    }
  } else if (etag.key_compare(sidewalk)) {
    if (etag.value_compare_ci(right)) {
      etag = tag_t::uncached(sidewalk, left);
      n_tags_altered++;
    } else if (etag.value_compare_ci(left)) {
      etag = tag_t::uncached(sidewalk, right);
      n_tags_altered++;
    }
  } else {
    // suffixes
    const char *lastcolon = strrchr(etag.key, ':');

    if (lastcolon != nullptr) {
      static const rtable_type rtable = rtable_init();

      for (unsigned int i = 0; i < rtable.size(); i++) {
        if (rtable[i].first == (lastcolon + 1)) {
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

class reverse_roles {
  osm_t::ref osm;
  const object_t way;
  unsigned int &n_roles_flipped;
public:
  inline reverse_roles(osm_t::ref o, way_t *w, unsigned int &n) : osm(o), way(w), n_roles_flipped(n) {}
  void operator()(const std::pair<item_id_t, relation_t *> &pair);
};

void reverse_roles::operator()(const std::pair<item_id_t, relation_t *> &pair)
{
  static const char *DS_ROUTE_FORWARD = value_cache.insert("forward");
  static const char *DS_ROUTE_REVERSE = value_cache.insert("backward");

  relation_t * const relation = pair.second;
  const char *type = relation->tags.get_value("type");

  // Route relations; https://wiki.openstreetmap.org/wiki/Relation:route
  if (type == nullptr || strcasecmp(type, "route") != 0)
    return;

  // First find the member corresponding to our way:
  const std::vector<member_t>::iterator mitEnd = relation->members.end();
  std::vector<member_t>::iterator member = std::find(relation->members.begin(), mitEnd, way);
  if(member == relation->members.end())
    return;

  // Then flip its role if it's one of the direction-sensitive ones
  if (member->role == nullptr) {
    printf("null role in route relation -> ignore\n");
  } else if (member->role == DS_ROUTE_FORWARD || strcasecmp(member->role, DS_ROUTE_FORWARD) == 0) {
    member->role = DS_ROUTE_REVERSE;
    osm->mark_dirty(relation);
    ++n_roles_flipped;
  } else if (member->role == DS_ROUTE_REVERSE || strcasecmp(member->role, DS_ROUTE_REVERSE) == 0) {
    member->role = DS_ROUTE_FORWARD;
    osm->mark_dirty(relation);
    ++n_roles_flipped;
  }

  // TODO: what about numbered stops? Guess we ignore them; there's no
  // consensus about whether they should be placed on the way or to one side
  // of it.
}

std::pair<unsigned int, unsigned int>
way_t::reverse(osm_t::ref osm)
{
  std::pair<unsigned int, unsigned int> ret = std::make_pair<unsigned int, unsigned int>(0, 0);

  osm->mark_dirty(this);
  tags.for_each(reverse_direction_sensitive_tags_functor(ret.first));

  std::reverse(node_chain.begin(), node_chain.end());

  reverse_roles context(osm, this, ret.second);
  std::for_each(osm->relations.begin(), osm->relations.end(), context);

  return ret;
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

namespace {

bool implicit_area(const tag_t &tg)
{
  std::array<const char *, 5> keys = { {
    "building", "landuse", "leisure", "natural", "aeroway"
  } };

  // this can be checked faster than the keys, so do it first
  if(strcmp(tg.value, "no") == 0)
    return false;

  for(unsigned int i = 0; i < keys.size(); i++)
    if(strcmp(tg.key, keys.at(i)) == 0)
      return true;

  return false;
}

} // namespace

bool way_t::is_area() const
{
  if(!is_closed())
    return false;

  const char *area = tags.get_value("area");
  if(area != nullptr)
    return strcmp(area, "yes") == 0;

  return tags.contains(implicit_area);
}

namespace {

class relation_transfer {
  osm_t::ref osm;
  way_t * const dst;
  const way_t * const src;
public:
  inline relation_transfer(osm_t::ref o, way_t *d, const way_t *s) : osm(o), dst(d), src(s) {}
  void operator()(const std::pair<item_id_t, relation_t *> &pair) const;
};

void relation_transfer::operator()(const std::pair<item_id_t, relation_t *> &pair) const
{
  relation_t * const relation = pair.second;
  /* walk member chain. save role of way if its being found. */
  const object_t osrc(const_cast<way_t *>(src));
  find_member_object_functor fc(osrc);
  const std::vector<member_t>::iterator itBegin = relation->members.begin();
  std::vector<member_t>::iterator itEnd = relation->members.end();
  std::vector<member_t>::iterator it = std::find_if(itBegin, itEnd, fc);

  if (it == itEnd)
    return;

  osm->mark_dirty(relation);

  for(; it != itEnd; it = std::find_if(std::next(it), itEnd, fc)) {
    printf("way #" ITEM_ID_FORMAT " is part of relation #" ITEM_ID_FORMAT " at position %zu, adding way #" ITEM_ID_FORMAT "\n",
           src->id, relation->id, std::distance(relation->members.begin(), it), dst->id);

    member_t m(object_t(dst), *it);

    // find out if the relation members are ordered ways, so the split parts should
    // be inserted in a sensible order to keep the relation intact
    bool insertBefore = false;
    if(it != itBegin && std::prev(it)->object.type == object_t::WAY) {
      const way_t *prev_way = static_cast<way_t *>(std::prev(it)->object);

      insertBefore = prev_way->ends_with_node(dst->node_chain.front()) ||
                     prev_way->ends_with_node(dst->node_chain.back());
    } else if (std::next(it) != itEnd && std::next(it)->object.type == object_t::WAY) {
      const way_t *next_way = static_cast<way_t *>(std::next(it)->object);

      insertBefore = next_way->ends_with_node(src->node_chain.front()) ||
                     next_way->ends_with_node(src->node_chain.back());
    } // if this is both itEnd and itBegin it is the only member, so the ordering is irrelevant

    // make dst member of the same relation
    if(insertBefore) {
      printf("\tinserting before way #" ITEM_ID_FORMAT " to keep relation ordering\n", src->id);
      it = relation->members.insert(it, m);
      // skip this object when calling fc again, it can't be the searched one
      it++;
    } else {
      it = relation->members.insert(std::next(it), m);
    }
    // refresh the end iterator as the container was modified
    itEnd = relation->members.end();
  }
}

} // namespace

way_t *way_t::split(osm_t::ref osm, node_chain_t::iterator cut_at, bool cut_at_node)
{
  assert_cmpnum_op(node_chain.size(), >, 2);

  /* remember that the way needs to be uploaded */
  osm->mark_dirty(this);

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
  std::unique_ptr<way_t> neww(std::make_unique<way_t>());

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
    return nullptr;
  }

  /* ------------  copy all tags ------------- */
  neww->tags.copy(tags);

  // keep the history with the longer way
  // this must be before the relation transfer, as that needs to know the
  // contained nodes to determine proper ordering in the relations
  if(node_chain.size() < neww->node_chain.size())
    node_chain.swap(neww->node_chain);

  // now move the way itself into the main data structure
  // do it before transferring the relation membership to get meaningful ids in debug output
  way_t *ret = osm->attach(neww.release());

  /* ---- transfer relation membership from way to new ----- */
  std::for_each(osm->relations.begin(), osm->relations.end(), relation_transfer(osm, ret, this));

  return ret;
}

class tag_map_functor {
  osm_t::TagMap &tags;
public:
  explicit inline tag_map_functor(osm_t::TagMap &t) : tags(t) {}
  inline void operator()(const tag_t &otag) {
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

void tag_list_t::copy(const tag_list_t &other)
{
  assert(!contents);

  if(other.empty())
    return;

  contents.reset(new std::vector<tag_t>());
  contents->reserve(other.contents->size());

  std::remove_copy_if(other.contents->begin(), other.contents->end(), std::back_inserter(*contents), tag_t::isDiscardable);
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
  if((role == nullptr) ^ (other.role == nullptr))
    return false;

  return role == nullptr || role == other.role || strcmp(role, other.role) == 0;
}

template<typename T> T *osm_t::object_by_id(item_id_t id) const
{
  const std::map<item_id_t, T *> &map = objects<T>();
  const typename std::map<item_id_t, T *>::const_iterator it = map.find(id);
  if(it != map.end())
    return it->second;

  return nullptr;
}

template node_t *osm_t::object_by_id(item_id_t id) const;
template way_t *osm_t::object_by_id(item_id_t id) const;
template relation_t *osm_t::object_by_id(item_id_t id) const;

template<typename T> const T *osm_t::findOriginalById(item_id_t id) const
{
  const std::unordered_map<item_id_t, const T *> &map = originalObjects<T>();
  const typename std::unordered_map<item_id_t, const T *>::const_iterator it = map.find(id);
  if(it != map.end())
    return it->second;

  return nullptr;
}

osm_t::osm_t()
  : uploadPolicy(Upload_Normal)
{
  bounds.ll = pos_area(pos_t(NAN, NAN), pos_t(NAN, NAN));
}

namespace {

template<typename T> inline void
pairfree(std::pair<item_id_t, T *> pair)
{
  delete pair.second;
}

} // namespace

osm_t::~osm_t()
{
  std::for_each(relations.begin(), relations.end(), pairfree<relation_t>);
  std::for_each(ways.begin(), ways.end(), pairfree<way_t>);
  std::for_each(nodes.begin(), nodes.end(), pairfree<node_t>);
  std::for_each(original.ways.begin(), original.ways.end(), pairfree<const way_t>);
  std::for_each(original.nodes.begin(), original.nodes.end(), pairfree<const node_t>);
  std::for_each(original.relations.begin(), original.relations.end(), pairfree<const relation_t>);
}

osm_t::dirty_t::dirty_t(const osm_t &osm)
  : nodes(osm)
  , ways(osm)
  , relations(osm)
{
}

namespace {

template<typename T ENABLE_IF_CONVERTIBLE(T *, base_object_t *)>
class object_counter {
  osm_t::dirty_t::counter<T> &dirty;
  const std::map<item_id_t, T *> &map;
public:
  explicit inline object_counter(osm_t::dirty_t::counter<T> &d, const std::map<item_id_t, T *> &m) : dirty(d), map(m) {}
  void operator()(std::pair<item_id_t, const T *> pair)
  {
    const T * const origObj = pair.second;
    const typename std::map<item_id_t, T *>::const_iterator mit = map.find(origObj->id);
    assert(mit != map.end());
    T * const obj = mit->second;

    if(obj->isDeleted()) {
      dirty.deleted.push_back(obj);
    } else {
      assert(obj->flags & OSM_FLAG_DIRTY);
      dirty.changed.push_back(obj);
    }
  }
};

inline bool objectCompare(const base_object_t *a, const base_object_t *b)
{
  return a->id < b->id;
}

} // namespace

template<typename T>
osm_t::dirty_t::counter<T>::counter(const osm_t &osm)
  : total(osm.objects<T>().size())
{
  const std::map<item_id_t, T *> &map = osm.objects<T>();
  typename std::map<item_id_t, T *>::const_iterator it = map.begin();
  typename std::map<item_id_t, T *>::const_iterator itEnd = map.end();

  while (it != itEnd && it->second->isNew()) {
    added.push_back(it->second);
    it++;
  }

  std::for_each(osm.originalObjects<T>().begin(), osm.originalObjects<T>().end(), object_counter<T>(*this, osm.objects<T>()));

  // the originalObjects maps are unordered
  std::sort(deleted.begin(), deleted.end(), objectCompare);
  std::sort(changed.begin(), changed.end(), objectCompare);
}

namespace {

template<typename T>
inline void object_insert(std::map<item_id_t, T *> &map, T *o)
{
  bool b = map.insert(std::make_pair(o->id, o)).second;
  assert(b); (void)b;
}

} // namespace

void osm_t::insert(node_t *node)
{
  object_insert(nodes, node);
}

void osm_t::insert(way_t *way)
{
  object_insert(ways, way);
}

void osm_t::insert(relation_t *relation)
{
  object_insert(relations, relation);
}

const base_object_t *
osm_t::originalObject(object_t o) const
{
  switch (o.type) {
  case object_t::NODE:
  case object_t::NODE_ID:
    return findOriginalById<node_t>(o.get_id());
  case object_t::WAY:
  case object_t::WAY_ID:
    return findOriginalById<way_t>(o.get_id());
  case object_t::RELATION:
  case object_t::RELATION_ID:
    return findOriginalById<relation_t>(o.get_id());
  default:
    assert_unreachable();
  }
}

void osm_t::cleanupOriginalObject(node_t *o)
{
  o->map_item = nullptr;
  o->ways = 0;
}

void osm_t::cleanupOriginalObject(way_t *o)
{
  o->map_item = nullptr;
}

// explicit instantiation is needed at least for release builds with gcc 4.2
template const node_t *osm_t::findOriginalById<node_t>(item_id_t id) const;
template const way_t *osm_t::findOriginalById<way_t>(item_id_t id) const;
template const relation_t *osm_t::findOriginalById<relation_t>(item_id_t id) const;
