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
      unsigned int added, dirty;
      std::vector<T *> modified;
      std::vector<T *> deleted;

      inline bool empty() const
      { return modified.empty() && deleted.empty(); }
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
  inline void node_insert(node_t *node);

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
  inline void way_insert(way_t *way);
  void remove_from_relations(object_t obj);
  void way_free(way_t *way);
  void node_free(node_t *node);

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
  template<typename _Predicate>
  way_t *find_way(_Predicate pred) const {
    return find_object(ways, pred);
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
  inline void relation_insert(relation_t *relation);
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
  mergeResult<node_t> mergeNodes(node_t *first, node_t *second, way_t *(&mergeways)[2]);

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

class tag_t {
  friend struct elemstyle_condition_t;

  tag_t() O2G_DELETED_FUNCTION;
  inline explicit tag_t(const char *k, const char *v, bool)
    : key(k), value(v) {}

  static const char *mapToCache(const char *v);
public:
  const char *key, *value;
  tag_t(const char *k, const char *v);

  /**
   * @brief return a tag_t where key and value are not backed by the value cache
   */
  static inline tag_t uncached(const char *k, const char *v)
  {
    return tag_t(k, v, true);
  }

  inline bool is_creator_tag() const noexcept
  { return is_creator_tag(key); }

  static bool is_creator_tag(const char *key) noexcept;
  static inline bool is_no_creator(const tag_t &tag) noexcept
  { return !is_creator_tag(tag.key); }

  /**
   * @brief compare if the keys are identical
   *
   * This is intended to compare to values already mapped to the value cache
   * so a simple pointer compare is enough.
   */
  inline bool key_compare(const char *k) const noexcept
  {
    return key == k;
  }

  /**
   * @brief compare if the values are identical
   *
   * This is intended to compare to values already mapped to the value cache
   * so a simple pointer compare is enough.
   */
  inline bool value_compare(const char *k) const noexcept
  {
    return value == k;
  }
};

class tag_list_t {
public:
  inline tag_list_t() noexcept : contents(nullptr) {}
  ~tag_list_t();

  /**
   * @brief check if any tags are present
   */
  bool empty() const noexcept;

  /**
   * @brief check if any tag that is not "created_by" is present
   */
  bool hasNonCreatorTags() const noexcept;

  /**
   * @brief check if any tag that is not "created_by" or "source" is present
   */
  bool hasRealTags() const noexcept;

  /**
   * @brief scan for the only non-trivial tag of this object
   * @returns tag if there is only one tag present that satisfies hasRealTags()
   * @retval nullptr either hasRealTags() is false or there are multiple tags on this object
   */
  const tag_t *singleTag() const noexcept;

  const char *get_value(const char *key) const;

  template<typename _Predicate>
  bool contains(_Predicate pred) const {
    if(!contents)
      return false;
    const std::vector<tag_t>::const_iterator itEnd = contents->end();
    return itEnd != std::find_if(std::cbegin(*contents), itEnd, pred);
  }

  template<typename _Predicate>
  void for_each(_Predicate pred) const {
    if(contents)
      std::for_each(contents->begin(), contents->end(), pred);
  }

  /**
   * @brief remove all elements and free their memory
   */
  void clear();

  /**
   * @brief copy the contained tags
   */
  osm_t::TagMap asMap() const;

  void copy(const tag_list_t &other);

  /**
   * @brief replace the current tags with the given ones
   * @param ntags array of new tags
   *
   * The contents of ntags are undefined afterwards for C++98.
   */
#if __cplusplus < 201103L
  void replace(std::vector<tag_t> &ntags);
#else
  void replace(std::vector<tag_t> &&ntags);
#endif

  /**
   * @brief replace the current tags with the given ones
   * @param ntags new tags
   */
  void replace(const osm_t::TagMap &ntags);

  /**
   * @brief combine tags from both lists in a useful manner
   * @return if there were any tag collisions
   *
   * other will be empty afterwards.
   */
  bool merge(tag_list_t &other);

  inline bool operator==(const std::vector<tag_t> &t2) const
  { return !operator!=(t2); }
  bool operator!=(const std::vector<tag_t> &t2) const;
  inline bool operator==(const osm_t::TagMap &t2) const
  { return !operator!=(t2); }
  bool operator!=(const osm_t::TagMap &t2) const;

  /**
   * @brief check if 2 tags with the same key exist
   */
  bool hasTagCollisions() const;

private:
  // do not directly use a vector here as many objects do not have
  // any tags and that would waste too much memory
  std::vector<tag_t> *contents;
};

class base_object_t {
public:
  explicit base_object_t(unsigned int ver = 0, item_id_t i = ID_ILLEGAL) noexcept;

  item_id_t id;
  tag_list_t tags;
  time_t time;
  unsigned int flags;
  int user;
  unsigned int version;

  /**
   * @brief replace the tags and set dirty flag if they were actually different
   * @param ntags the new tags
   *
   * "created_by" tags are ignored when considering if the list needs to be
   * changed or not.
   */
  void updateTags(const osm_t::TagMap &ntags);

  xmlChar *generate_xml(const std::string &changeset) const;

  /**
   * @brief get the API string for this object type
   * @return the string used for this kind of object in the OSM API
   */
  virtual const char *apiString() const noexcept = 0;

  std::string id_string() const;

  inline bool isNew() const noexcept
  { return id <= ID_ILLEGAL; }

  inline bool isDirty() const noexcept
  { return flags != 0; }

  inline bool isDeleted() const noexcept
  { return flags & OSM_FLAG_DELETED; }

  /**
   * @brief generate the xml elements for an osmChange delete section
   * @param parent_node the "delete" node of the osmChange document
   * @param changeset a string for the changeset attribute
   *
   * May only be called if this element is marked as deleted
   */
  void osmchange_delete(xmlNodePtr parent_node, const char *changeset) const;

  void markDeleted();
protected:
  virtual void generate_xml_custom(xmlNodePtr xml_node) const = 0;
};

class visible_item_t : public base_object_t {
protected:
  inline visible_item_t(unsigned int ver = 0, item_id_t i = ID_ILLEGAL) noexcept
    : base_object_t(ver, i)
    , map_item(nullptr)
    , zoom_max(0.0f)
  {
  }

public:
  /* a link to the visual representation on screen */
  struct map_item_t *map_item;

  float zoom_max;

  /**
   * @brief destroy the visible items
   * @param map the map pointer needed to release additional items
   *
   * It is known that there are no additional items the map pointer
   * may be nullptr.
   */
  void item_chain_destroy(map_t *map);
};

class node_t : public visible_item_t {
public:
  node_t(unsigned int ver, const lpos_t lp, const pos_t &p) noexcept;
  node_t(unsigned int ver, const pos_t &p, item_id_t i) noexcept;
  virtual ~node_t() {}

  unsigned int ways;
  pos_t pos;
  lpos_t lpos;

  const char *apiString() const noexcept override {
    return api_string();
  }
  static const char *api_string() noexcept {
    return "node";
  }
protected:
  virtual void generate_xml_custom(xmlNodePtr xml_node) const override;
};

typedef std::vector<node_t *> node_chain_t;

#define OSM_DRAW_FLAG_AREA  (1<<0)
#define OSM_DRAW_FLAG_BG    (1<<1)

class way_t : public visible_item_t {
  friend class osm_t;
public:
  explicit way_t();
  explicit way_t(unsigned int ver, item_id_t i = ID_ILLEGAL);
  virtual ~way_t() {}

  /* visual representation from elemstyle */
  struct {
    color_t color;
    unsigned int flags : 8;
    int width : 8;
    unsigned int dash_length_on: 8;
    unsigned int dash_length_off: 8;

    union {
      struct {
        unsigned int color;
        int width;
      } bg;

      struct {
        unsigned int color;
      } area;
    };
  } draw;

  node_chain_t node_chain;

  bool contains_node(const node_t *node) const;
  void append_node(node_t *node);
  bool ends_with_node(const node_t *node) const noexcept;
  bool is_closed() const noexcept;
  bool is_area() const;

  void reverse(osm_t::ref osm, unsigned int &tags_flipped, unsigned int &roles_flipped);

  /**
   * @brief split the way into 2
   * @param osm parent osm object
   * @param cut_at position to split at
   * @param cut_at_node if split should happen before or at the given node
   * @returns the new way
   * @retval nullptr the new way would have only one node
   *
   * The returned way will be the shorter of the 2 new ways.
   *
   * @cut_at denotes the first node that is part of the second way. In case
   * @cut_at_node is true this is also the last node of the first way.
   *
   * In case the way is closed @cut_at denotes the first way of the node
   * after splitting. @cut_at_node has no effect in this case.
   */
  way_t *split(osm_t::ref osm, node_chain_t::iterator cut_at, bool cut_at_node);
  const node_t *last_node() const noexcept;
  const node_t *first_node() const noexcept;
  void write_node_chain(xmlNodePtr way_node) const;

  void cleanup();

  const char *apiString() const noexcept override {
    return api_string();
  }
  static const char *api_string() noexcept {
    return "way";
  }

  /**
   * @brief create a node and insert it into this way
   * @param osm the OSM object database
   * @param position the index in the node chain to insert the node
   * @param coords the coordinates of the new node
   * @returns the new node, already attached to osm
   */
  node_t *insert_node(osm_t::ref osm, int position, lpos_t coords);

private:
  bool merge(way_t *other, osm_t *osm, map_t *map, const std::vector<relation_t *> &rels = std::vector<relation_t *>());
public:
  /**
   * @brief merge this way with the other one
   * @param other the way to take the nodes from
   * @param osm map database
   * @param rels the relations that need to be adjusted
   * @returns if merging the tags caused collisions
   *
   * @other will be removed.
   */
  inline bool merge(way_t *other, osm_t::ref osm, map_t *map, const std::vector<relation_t *> &rels = std::vector<relation_t *>())
  { return merge(other, osm.get(), map, rels); }

protected:
  virtual void generate_xml_custom(xmlNodePtr xml_node) const override {
    write_node_chain(xml_node);
  }
};

class relation_t : public base_object_t {
public:
  explicit relation_t();
  explicit relation_t(unsigned int ver, item_id_t i = ID_ILLEGAL);
  virtual ~relation_t() {}

  std::vector<member_t> members;

  std::vector<member_t>::iterator find_member_object(const object_t &o);
  inline std::vector<member_t>::const_iterator find_member_object(const object_t &o) const
  { return const_cast<relation_t *>(this)->find_member_object(o); }

  void members_by_type(unsigned int &nodes, unsigned int &ways, unsigned int &relations) const;
  std::string descriptive_name() const;
  void generate_member_xml(xmlNodePtr xml_node) const;

  bool is_multipolygon() const;

  void cleanup();

  const char *apiString() const noexcept override {
    return api_string();
  }
  static const char *api_string() noexcept {
    return "relation";
  }
  void remove_member(std::vector<member_t>::iterator it);
protected:
  virtual void generate_xml_custom(xmlNodePtr xml_node) const override {
    generate_member_xml(xml_node);
  }
};

void osm_node_chain_free(node_chain_t &node_chain);

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

void osm_t::node_insert(node_t *node)
{
  bool b = nodes.insert(std::pair<item_id_t, node_t *>(node->id, node)).second;
  assert(b); (void)b;
}

void osm_t::way_insert(way_t *way)
{
  bool b = ways.insert(std::pair<item_id_t, way_t *>(way->id, way)).second;
  assert(b); (void)b;
}

void osm_t::relation_insert(relation_t *relation)
{
  bool b = relations.insert(std::pair<item_id_t, relation_t *>(relation->id, relation)).second;
  assert(b); (void)b;
}
