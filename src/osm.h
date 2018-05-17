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

#ifndef OSM_H
#define OSM_H

#include "color.h"
#include "pos.h"

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <osm2go_cpp.h>
#include "osm2go_stl.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#define OSM_FLAG_DIRTY    (1<<0)
#define OSM_FLAG_DELETED  (1<<1)
#define OSM_FLAG_HIDDEN   (1<<3)

/* item_id_t needs to be signed as osm2go uses negative ids for items */
/* not yet registered with the main osm database */
typedef int64_t item_id_t;
#define ITEM_ID_FORMAT  "%" PRIi64

#define ID_ILLEGAL  (static_cast<item_id_t>(0))

class base_object_t;
struct osm_t;
class node_t;
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

struct osm_t {
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
    };

    counter<node_t> nodes;
    counter<way_t> ways;
    counter<relation_t> relations;
  };

  class TagMap : public std::multimap<std::string, std::string> {
  public:
    iterator findTag(const std::string &k, const std::string &v);
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

  node_t *node_new(const lpos_t pos);
  node_t *node_new(const pos_t &pos);
  void node_attach(node_t *node);
  void way_delete(way_t *way);
  void way_attach(way_t *way);
  void remove_from_relations(object_t obj);
  void way_free(way_t *way);
  void node_free(node_t *node);

  template<typename _Predicate>
  way_t *find_way(_Predicate pred) const {
    const std::map<item_id_t, way_t *>::const_iterator itEnd = ways.end();
    const std::map<item_id_t, way_t *>::const_iterator it =
        std::find_if(ways.begin(), itEnd, pred);
    if(it != itEnd)
      return it->second;
    return nullptr;
  }

  template<typename _Predicate>
  relation_t *find_relation(_Predicate pred) const {
    const std::map<item_id_t, relation_t *>::const_iterator itEnd = relations.end();
    const std::map<item_id_t, relation_t *>::const_iterator it =
        std::find_if(relations.begin(), itEnd, pred);
    if(it != itEnd)
      return it->second;
    return nullptr;
  }

  /**
   * @brief remove the given node
   * @param node the node to remove
   * @param remove_refs if it should be cleaned also from ways and relations referencing it
   * @return list of ways affected by this deletion
   */
  way_chain_t node_delete(node_t *node, bool remove_refs = true);
  void relation_free(relation_t *relation);
  void relation_attach(relation_t *relation);
  void relation_delete(relation_t *relation);

  /**
   * @brief check if object is in sane state
   * @returns error string or NULL
   * @retval NULL object is sane
   *
   * The error string is a static one and must not be freed by the caller.
   */
  const char *sanity_check() const;

  /**
   * @brief parse the XML node for tag values
   * @param a_node the XML node to parse
   * @param tags the tag vector the new node is added to
   * @returns if a new tag was added
   */
  static bool parse_tag(xmlNode* a_node, TagMap &tags);

  bool parse_relation_member(const xmlString &tp, const xmlString &ref, const xmlString &role, std::vector<member_t> &members);
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

  /**
   * @brief merge 2 nodes
   * @param first first node
   * @param second second node
   * @param conflict if any conflicts (e.g. incompatible tags) were detected
   * @param mergeways adjacent ways that can be merged
   * @return the remaining node (may be any of first and second)
   *
   * This merges the nodes on the position of second, joining the tags together
   * and moving all way and relation memberships to the remaining node.
   *
   * If both nodes are the endpoint of ways mergeways will contain these ways,
   * otherwise it is set to nullptr.
   *
   * The victim node is deleted.
   */
  node_t *mergeNodes(node_t *first, node_t *second, bool &conflict, way_t *(&mergeways)[2]);

  /**
   * @brief merge 2 ways
   * @param first first way
   * @param second second way
   * @param conflict if any conflicts (e.g. incompatible tags) were detected
   * @return the remaining way (may be any of first and second)
   *
   * This merges the ways, assuming that they share one common end node, joining the
   * tags together and moving all relation memberships to the remaining way.
   *
   * The victim way is deleted.
   */
  way_t *mergeWays(way_t *first, way_t *second, bool &conflict);

  struct find_object_by_flags {
    int flagmask;
    explicit inline find_object_by_flags(int f) : flagmask(f) {}
    inline bool operator()(std::pair<item_id_t, base_object_t *> pair);
  };

  /**
   * @brief check if there are any modifications
   * @param honor_hidden_flags if setting HIDDEN on ways should be considered a modifications
   */
  bool is_clean(bool honor_hidden_flags) const;

  dirty_t modified() const {
    return dirty_t(*this);
  }
};

xmlChar *osm_generate_xml_changeset(const std::string &comment, const std::string &src);

class tag_t {
  tag_t() {}
public:
  const char *key, *value;
  tag_t(const char *k, const char *v);

  /**
   * @brief return a tag_t where key and value are not backed by the value cache
   */
  static inline tag_t uncached(const char *k, const char *v)
  {
    tag_t r;
    r.key = k;
    r.value = v;
    return r;
  }

  bool is_creator_tag() const noexcept;

  static bool is_creator_tag(const char *key) noexcept;
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
  bool hasRealTags() const noexcept;

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
   * The old values will be freed, this object takes ownership of the values
   * in ntags.
   */
  void replace(std::vector<tag_t> &ntags);

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
  { return id < 0; }

  /**
   * @brief generate the xml elements for an osmChange delete section
   * @param parent_node the "delete" node of the osmChange document
   * @param changeset a string for the changeset attribute
   *
   * May only be called if this element is marked as deleted
   */
  void osmchange_delete(xmlNodePtr parent_node, const char *changeset) const;
protected:
  virtual void generate_xml_custom(xmlNodePtr xml_node) const = 0;
};

class visible_item_t : public base_object_t {
protected:
  inline visible_item_t(unsigned int ver = 0, item_id_t i = ID_ILLEGAL) noexcept
    : base_object_t(ver, i)
    , map_item_chain(nullptr)
    , zoom_max(0.0f)
  {
  }

public:
  /* a link to the visual representation on screen */
  struct map_item_chain_t *map_item_chain;

  float zoom_max;

  void item_chain_destroy();
};

class node_t : public visible_item_t {
public:
  explicit node_t() noexcept;
  explicit node_t(unsigned int ver, const lpos_t lp, const pos_t &p, item_id_t i = ID_ILLEGAL) noexcept;
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

class way_t: public visible_item_t {
  friend struct osm_t;
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
   * @brief merge this way with the other one
   * @param other the way to take the nodes from
   * @param osm map database
   * @param rels the relations that need to be adjusted
   * @returns if merging the tags caused collisions
   *
   * @other will be removed.
   */
private:
  bool merge(way_t *other, osm_t *osm, const std::vector<relation_t *> &rels = std::vector<relation_t *>());
public:
  inline bool merge(way_t *other, osm_t::ref osm, const std::vector<relation_t *> &rels = std::vector<relation_t *>())
  { return merge(other, osm.get(), rels); }

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

bool osm_t::find_object_by_flags::operator()(std::pair<item_id_t, base_object_t *> pair) {
  return pair.second->flags & flagmask;
}

#endif /* OSM_H */

// vim:et:ts=8:sw=2:sts=2:ai
