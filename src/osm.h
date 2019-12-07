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

#pragma once

#include "color.h"
#include "pos.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include "osm2go_stl.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#define OSM_FLAG_DIRTY    (1<<0)
#define OSM_FLAG_DELETED  (1<<1)

/* item_id_t needs to be signed as osm2go uses negative ids for items */
/* not yet registered with the main osm database */
typedef int64_t item_id_t;
#define ITEM_ID_FORMAT  "%" PRIi64

#define ID_ILLEGAL  (static_cast<item_id_t>(0))

class base_object_t;
class map_t;
class node_t;
class osm_t;
class relation_t;
class way_t;
class tag_t;
typedef std::vector<way_t *> way_chain_t;
class xmlString;

struct object_t {
  enum type_t {
    ILLEGAL = 0,
    NODE = 1,
    WAY = 2,
    RELATION = 3,
    _REF_FLAG = 4,
    NODE_ID = NODE | _REF_FLAG,
    WAY_ID = WAY | _REF_FLAG,
    RELATION_ID = RELATION | _REF_FLAG
  };

  type_t type;
  union {
    node_t *node;
    way_t *way;
    relation_t *relation;
    item_id_t id;
    base_object_t *obj;
  };

  explicit inline object_t(type_t t = ILLEGAL, item_id_t i = ID_ILLEGAL) noexcept
    : type(t), id(i) {}
  explicit inline object_t(node_t *n) noexcept
    : type(NODE), node(n) { }
  explicit inline object_t(way_t *w) noexcept
    : type(WAY), way(w) { }
  explicit inline object_t(relation_t *r) noexcept
    : type(RELATION), relation(r) { }

  inline object_t &operator=(node_t *n) noexcept
  { type = NODE; node = n; return *this; }
  inline object_t &operator=(way_t *w) noexcept
  { type = WAY; way = w; return *this; }
  inline object_t &operator=(relation_t *r) noexcept
  { type = RELATION; relation = r; return *this; }

  bool operator==(const object_t &other) const noexcept;
  inline bool operator!=(const object_t &other) const noexcept
  { return !operator==(other); }
  inline bool operator==(const object_t *other) const noexcept
  { return operator==(*other); }
  inline bool operator!=(const object_t *other) const noexcept
  { return !operator==(*other); }
  bool operator==(const node_t *n) const noexcept;
  bool operator!=(const node_t *n) const noexcept
  { return !operator==(n); }
  bool operator==(const way_t *w) const noexcept;
  bool operator!=(const way_t *w) const noexcept
  { return !operator==(w); }
  bool operator==(const relation_t *r) const noexcept;
  bool operator!=(const relation_t *r) const noexcept
  { return !operator==(r); }

  bool is_real() const noexcept;
  const char *type_string() const;
  std::string id_string() const;
  item_id_t get_id() const noexcept;
  std::string get_name(const osm_t &osm) const;
};

struct member_t {
  explicit member_t(object_t::type_t t) noexcept;
  explicit member_t(const object_t &o, const char *r = nullptr);
  /**
   * @brief constructor
   * @param o the object to reference
   * @param m an existing member to copy the role from
   *
   * This is more efficient than the other constructor as the role is either
   * nullptr or already in the role cache, so no further operations are needed.
   */
  member_t(const object_t &o, const member_t &m) noexcept : object(o), role(m.role) {}

  object_t object;
  const char *role;

  bool operator==(const member_t &other) const noexcept;
  inline bool operator==(const object_t &other) const noexcept
  { return object == other; }

  /**
   * @brief check function for use in std::find_if
   */
  static inline bool has_role(const member_t &member) noexcept {
    return member.role != nullptr;
  }
};

class osm_t {
  template<typename T> inline std::map<item_id_t, T *> &objects();
  template<typename T> inline const std::map<item_id_t, T *> &objects() const;
  template<typename T> void attach(T *obj);
  template<typename T> inline T *find_by_id(item_id_t id) const;
public:
  typedef const std::unique_ptr<osm_t> &ref;

  enum UploadPolicy {
    Upload_Normal,
    Upload_Discouraged,
    Upload_Blocked
  };

  struct dirty_t {
    explicit dirty_t(const osm_t &osm);

    template<typename T>
    class counter {
      struct object_counter {
        counter<T> &dirty;
        explicit object_counter(counter<T> &d) : dirty(d) {}
        void operator()(std::pair<item_id_t, T *> pair);
      };
    public:
      explicit counter(const std::map<item_id_t, T *> &map);
      const unsigned int total;
      std::vector<T *> added;
      std::vector<T *> changed;
      std::vector<T *> deleted;

      inline bool empty() const
      { return added.empty() && changed.empty() && deleted.empty(); }

      inline void debug(const char *prefix) const
      {
        printf("%s new %2zu, dirty %2zu, deleted %2zu", prefix,
               added.size(), changed.size(), deleted.size());
      }
    };

    counter<node_t> nodes;
    counter<way_t> ways;
    counter<relation_t> relations;
    inline bool empty() const
    { return nodes.empty() && ways.empty() && relations.empty(); }
  };

  class TagMap : public std::multimap<std::string, std::string> {
  public:
    iterator findTag(const std::string &key, const std::string &value);
    inline const_iterator findTag(const std::string &k, const std::string &v) const {
      return const_cast<TagMap *>(this)->findTag(k, v);
    }
  };

  explicit osm_t();
  ~osm_t();

  bounds_t bounds;   // original bounds as they appear in the file

  std::map<item_id_t, node_t *> nodes;
  std::map<item_id_t, way_t *> ways;
  std::map<item_id_t, relation_t *> relations;
  std::map<int, std::string> users;   ///< mapping of user id to username
  UploadPolicy uploadPolicy;

  node_t *node_by_id(item_id_t id) const;
  way_t *way_by_id(item_id_t id) const;
  relation_t *relation_by_id(item_id_t id) const;

  node_t *node_new(const lpos_t lpos);
  node_t *node_new(const pos_t &pos);
  /**
   * @brief insert a node and create a new temporary id
   */
  void node_attach(node_t *node);

  /**
   * @brief insert a node using the id already set
   */
  void node_insert(node_t *node);

  /**
   * @brief mark a way as deleted
   * @param way the way to get rid of
   * @param map the map to delete the visual items
   * @param unref callback function for each node
   *
   * map is only used to delete possibly existing additional map items.
   * If unref is nullptr the way count will be decremented and the node will be deleted
   * if it is not used anymore and has no useful tags.
   *
   * @see visible_item_t::item_chain_destroy
   */
  void way_delete(way_t *way, map_t *map, void (*unref)(node_t *) = nullptr);

  /**
   * @brief insert a way and create a new temporary id
   * @param way the new way
   * @returns the way
   */
  way_t *way_attach(way_t *way);

  /**
   * @brief insert a node using the id already set
   */
  void way_insert(way_t *way);
  void remove_from_relations(object_t obj);
  void way_free(way_t *way);
  void node_free(node_t *node);

  trstring unspecified_name(const object_t &obj) const;

private:
  template<typename T, typename _Predicate> inline
  T *find_object(const std::map<item_id_t, T *> &map, _Predicate pred) const {
    const typename std::map<item_id_t, T *>::const_iterator itEnd = map.end();
    const typename std::map<item_id_t, T *>::const_iterator it = std::find_if(map.begin(), itEnd, pred);
    if(it != itEnd)
      return it->second;
    return nullptr;
  }

public:
  /**
   * @brief find a way matching the given predicate
   */
  template<typename _Predicate>
  way_t *find_way(_Predicate pred) const {
    return find_object(ways, pred);
  }

  /**
   * @brief find a way matching the given predicate, but only if no other one does
   */
  template<typename _Predicate>
  way_t *find_only_way(_Predicate pred) const {
    const typename std::map<item_id_t, way_t *>::const_iterator itEnd = ways.end();
    const typename std::map<item_id_t, way_t *>::const_iterator it = std::find_if(ways.begin(), itEnd, pred);
    if(it == itEnd)
      return nullptr;
    if (std::find_if(std::next(it), itEnd, pred) != itEnd)
      return nullptr;
    return it->second;
  }

  template<typename _Predicate>
  relation_t *find_relation(_Predicate pred) const {
    return find_object(relations, pred);
  }

  /**
   * @brief remove the given node
   * @param node the node to remove
   * @param remove_refs if it should be cleaned also from ways and relations referencing it
   * @return list of ways affected by this deletion
   */
  way_chain_t node_delete(node_t *node, bool remove_refs = true);
  void relation_free(relation_t *relation);
  /**
   * @brief insert a relation and create a new temporary id
   * @param relation the new relation
   * @returns the relation
   */
  relation_t *relation_attach(relation_t *relation);

  /**
   * @brief insert a relation using the id already set
   */
  void relation_insert(relation_t *relation);
  void relation_delete(relation_t *relation);

  /**
   * @brief check if object is in sane state
   * @returns error string or NULL
   * @retval NULL object is sane
   *
   * The error string is a static one and must not be freed by the caller.
   */
  trstring::native_type sanity_check() const;

  /**
   * @brief parse the XML node for tag values
   * @param a_node the XML node to parse
   * @param tags the tag vector the new node is added to
   */
  static void parse_tag(xmlNode* a_node, TagMap &tags);

  bool parse_relation_member(const xmlString &tp, const xmlString &refstr, const xmlString &role, std::vector<member_t> &members);
  void parse_relation_member(xmlNode *a_node, std::vector<member_t> &members);

  node_t *parse_way_nd(xmlNode *a_node) const;

  static osm_t *parse(const std::string &path, const std::string &filename);

  /**
   * @brief check if a TagMap contains the other
   * @param sub the smaller map
   * @param super the containing map
   * @returns if all elements from sub are in super
   */
  static bool tagSubset(const TagMap &sub, const TagMap &super);

protected:
  /**
   * @brief check which of the objects should persist
   * @param first the first object
   * @param second the second object
   * @param rels the relations the object about to be removed is member in
   * @returns if the first object should persist
   *
   * This takes into account the age (object id) and number of affected ways
   * and relationships.
   */
  bool checkObjectPersistence(const object_t &first, const object_t &second, std::vector<relation_t *> &rels) const;

public:
  template<typename T>
  struct mergeResult {
    inline mergeResult(T *o, bool c) : obj(o), conflict(c) {}
    T * const obj;
    const bool conflict;  ///< if any conflicts (e.g. incompatible tags) were detected
  };

  /**
   * @brief merge 2 nodes
   * @param first first node
   * @param second second node
   * @param mergeways adjacent ways that can be merged
   * @return the remaining node (may be any of first and second) and the conflict status
   *
   * This merges the nodes on the position of second, joining the tags together
   * and moving all way and relation memberships to the remaining node.
   *
   * If both nodes are the endpoint of ways mergeways will contain these ways,
   * otherwise it is set to nullptr.
   *
   * The victim node is deleted.
   */
  mergeResult<node_t> mergeNodes(node_t *first, node_t *second, std::array<way_t *, 2> &mergeways);

  /**
   * @brief merge 2 ways
   * @param first first way
   * @param second second way
   * @return the remaining way (may be any of first and second) and the conflict status
   *
   * This merges the ways, assuming that they share one common end node, joining the
   * tags together and moving all relation memberships to the remaining way.
   *
   * The victim way is deleted.
   */
  mergeResult<way_t> mergeWays(way_t *first, way_t *second, map_t *map);

  /**
   * @brief check if there are any modifications
   * @param honor_hidden_flags if setting HIDDEN on ways should be considered a modifications
   */
  bool is_clean(bool honor_hidden_flags) const;

  dirty_t modified() const {
    return dirty_t(*this);
  }

  std::unordered_set<way_t *> hiddenWays;
  inline bool wayIsHidden(const way_t *w) const;
  inline void waySetHidden(way_t *w);
  inline bool hasHiddenWays() const noexcept;
};

xmlChar *osm_generate_xml_changeset(const std::string &comment, const std::string &src);

bool osm_t::wayIsHidden(const way_t *w) const
{
  return hiddenWays.find(const_cast<way_t *>(w)) != hiddenWays.end();
}

void osm_t::waySetHidden(way_t *w)
{
  hiddenWays.insert(w);
}

bool osm_t::hasHiddenWays() const noexcept
{
  return !hiddenWays.empty();
}
