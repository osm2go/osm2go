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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OSM_H
#define OSM_H

#include "misc.h"
#include "pos.h"

#ifdef __cplusplus
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <osm2go_cpp.h>

#include <math.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

extern "C" {
#endif

#define OSM_FLAG_DIRTY    (1<<0)
#define OSM_FLAG_DELETED  (1<<1)
#define OSM_FLAG_NEW      (1<<2)
#define OSM_FLAG_HIDDEN   (1<<3)

/* item_id_t needs to be signed as osm2go uses negative ids for items */
/* not yet registered with the main osm database */
typedef gint64 item_id_t;
#define G_TYPE_ITEM_ID_T G_TYPE_INT64
#define ITEM_ID_FORMAT  "%" G_GINT64_FORMAT

#define ID_ILLEGAL  ((item_id_t)0)

/* icon stuff is required since nodes may held a icon reference */
struct icon_s;

typedef struct osm_t osm_t;

typedef struct bounds_t {
  pos_t ll_min, ll_max;
  lpos_t min, max;
  lpos_t center;
  float scale;
} bounds_t;

#ifdef __cplusplus
class base_object_t;
struct item_id_chain_t;
class node_t;
class relation_t;
class way_t;
struct tag_t;
typedef std::vector<relation_t *> relation_chain_t;
typedef std::vector<way_t *> way_chain_t;
#else
struct base_object_t;
typedef struct base_object_t base_object_t;
typedef struct node_s node_t;
typedef struct relation_s relation_t;
typedef struct way_s way_t;
#endif

typedef enum {
  ILLEGAL=0, NODE, WAY, RELATION, NODE_ID, WAY_ID, RELATION_ID
} type_t;

typedef struct object_t {
  type_t type;
  union {
    node_t *node;
    way_t *way;
    relation_t *relation;
    item_id_t id;
    base_object_t *obj;
  };

#ifdef __cplusplus
  explicit inline object_t(type_t t = ILLEGAL)
    : type(t), obj(O2G_NULLPTR) {}
  explicit inline object_t(node_t *n)
    : type(NODE), node(n) { }
  explicit inline object_t(way_t *w)
    : type(WAY), way(w) { }
  explicit inline object_t(relation_t *r)
    : type(RELATION), relation(r) { }

  inline object_t &operator=(node_t *n)
  { type = NODE; node = n; return *this; }
  inline object_t &operator=(way_t *w)
  { type = WAY; way = w; return *this; }
  inline object_t &operator=(relation_t *r)
  { type = RELATION; relation = r; return *this; }

  bool operator==(const object_t &other) const;
  inline bool operator!=(const object_t &other) const
  { return !operator==(other); }
  inline bool operator==(const object_t *other) const
  { return operator==(*other); }
  inline bool operator!=(const object_t *other) const
  { return !operator==(*other); }
  bool operator==(const node_t *n) const;
  bool operator!=(const node_t *n) const
  { return !operator==(n); }
  bool operator==(const way_t *w) const;
  bool operator!=(const way_t *w) const
  { return !operator==(w); }
  bool operator==(const relation_t *r) const;
  bool operator!=(const relation_t *r) const
  { return !operator==(r); }

  bool is_real() const;
  const char *type_string() const;
  std::string id_string() const;
  const char *get_tag_value(const char *key) const;
  bool has_tags() const;
  item_id_t get_id() const;
  std::string get_name() const;
#endif
} object_t;

#ifdef __cplusplus
}

struct member_t {
  explicit member_t(type_t t);
  explicit member_t(const object_t &o, char *r = O2G_NULLPTR);

  object_t object;
  char   *role;

  bool operator==(const member_t &other) const;
  inline bool operator==(const object_t &other) const
  { return object == other; }
};

struct osm_t {
  typedef std::multimap<std::string, std::string> TagMap;

  ~osm_t();

  bounds_t *bounds;   // original bounds as they appear in the file

  struct icon_t **icons;

  bounds_t rbounds;

  std::map<item_id_t, node_t *> nodes;
  std::map<item_id_t, way_t *> ways;
  std::map<item_id_t, relation_t *> relations;
  std::map<int, std::string> users;   //< users where uid is given in XML
  std::vector<std::string> anonusers; //< users without uid

  node_t *node_by_id(item_id_t id) const;
  way_t *way_by_id(item_id_t id) const;
  relation_t *relation_by_id(item_id_t id) const;

  node_t *node_new(const lpos_t &pos);
  node_t *node_new(const pos_t &pos);
  void node_attach(node_t *node);
  void node_restore(node_t *node);
  void way_delete(way_t *way, bool permanently);
  void way_attach(way_t *way);
  void remove_from_relations(node_t *node);
  void remove_from_relations(way_t *way);
  void way_restore(way_t *way, const std::vector<item_id_chain_t> &id_chain);
  void way_free(way_t *way);
  void node_free(node_t *node);
  way_chain_t node_to_way(const node_t *node) const;
  way_chain_t node_delete(node_t *node, bool permanently, bool affect_ways);
  void relation_free(relation_t *relation);
  void relation_attach(relation_t *relation);
  void relation_delete(relation_t *relation, bool permanently);
  relation_chain_t to_relation(const way_t *way) const;
  relation_chain_t to_relation(const object_t &object) const;

  bool position_within_bounds(gint x, gint y) const;

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

  member_t parse_relation_member(xmlNode *a_node);

  node_t *parse_way_nd(xmlNode *a_node) const;

  static osm_t *parse(const std::string &path, const std::string &filename, struct icon_t **icons);

  static TagMap::iterator findTag(TagMap &map, const std::string &k, const std::string &v);
  static inline TagMap::const_iterator findTag(const TagMap &map, const std::string &k, const std::string &v) {
    return findTag(const_cast<TagMap &>(map), k, v);
  }
  /**
   * @brief check if a TagMap contains the other
   * @param sub the smaller map
   * @param super the containing map
   * @returns if all elements from sub are in super
   */
  static bool tagSubset(const TagMap &sub, const TagMap &super);
};

xmlChar *osm_generate_xml_changeset(const std::string &comment, const std::string &src);

bool osm_position_within_bounds_ll(const pos_t *ll_min, const pos_t *ll_max, const pos_t *pos);

struct tag_t {
  char *key, *value;
  tag_t(char *k, char *v)
    : key(k), value(v)
  { }

  bool is_creator_tag() const;

  static bool is_creator_tag(const char *key);

  /**
   * @brief replace the value
   */
  void update_value(const char *nvalue);
};

class tag_list_t {
public:
  tag_list_t();
  ~tag_list_t();

  /**
   * @brief check if any tags are present
   */
  bool empty() const;

  /**
   * @brief check if any tag that is not "created_by" is present
   */
  bool hasRealTags() const;

  const char *get_value(const char *key) const;

  template<typename _Predicate>
  bool contains(_Predicate pred) const {
    if(!contents)
      return false;
    const std::vector<tag_t>::const_iterator itEnd = contents->end();
    return itEnd != std::find_if(cbegin(*contents), itEnd, pred);
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

G_STATIC_ASSERT(sizeof(tag_list_t) == sizeof(tag_t *));

class base_object_t {
public:
  explicit base_object_t(item_id_t ver = 0, item_id_t i = 0);

  item_id_t id;
  item_id_t version;
  const char *user;
  tag_list_t tags;
  time_t time;
  unsigned int flags;

  /**
   * @brief replace the tags and set dirty flag if they were actually different
   * @param ntags the new tags
   *
   * "created_by" tags are ignored when considering if the list needs to be
   * changed or not.
   */
  void updateTags(const osm_t::TagMap &ntags);

  virtual xmlChar *generate_xml(item_id_t changeset) const = 0;

  /**
   * @brief get the API string for this object type
   * @return the string used for this kind of object in the OSM API
   */
  virtual const char *apiString() const = 0;
};

class node_t : public base_object_t {
public:
  explicit node_t();
  explicit node_t(item_id_t ver, const lpos_t &lp, const pos_t &p, item_id_t i = 0);
  virtual ~node_t() {}

  pos_t pos;
  lpos_t lpos;
  int ways;
  float zoom_max;

  /* a link to the visual representation on screen */
  struct map_item_chain_t *map_item_chain;

  xmlChar *generate_xml(item_id_t changeset) const O2G_OVERRIDE;
  const char *apiString() const O2G_OVERRIDE {
    return api_string();
  }
  static const char *api_string() {
    return "node";
  }
};

struct item_id_chain_t {
  item_id_chain_t(type_t t, item_id_t i)
    : type(t), id(i) {}
  type_t type;
  item_id_t id;
};

typedef std::vector<node_t *> node_chain_t;

#define OSM_DRAW_FLAG_AREA  (1<<0)
#define OSM_DRAW_FLAG_BG    (1<<1)

class way_t: public base_object_t {
public:
  explicit way_t();
  explicit way_t(item_id_t ver, item_id_t i = 0);
  virtual ~way_t() {}

  /* visual representation from elemstyle */
  struct {
    float zoom_max;
    guint color;
    guint flags : 8;
    gint width : 8;
    guint dash_length_on: 8;
    guint dash_length_off: 8;

    union {
      struct {
	guint color;
	gint width;
      } bg;

      struct {
	guint color;
      } area;
    };
  } draw;

  /* a link to the visual representation on screen */
  struct map_item_chain_t *map_item_chain;

  node_chain_t node_chain;

  bool contains_node(const node_t *node) const;
  void append_node(node_t *node);
  bool ends_with_node(const node_t *node) const;
  bool is_closed() const;
  void reverse();

  /**
   * @brief split the way into 2
   * @param osm parent osm object
   * @param cut_at position to split at
   * @param cut_at_node if split should happen between or at nde
   */
  way_t *split(osm_t *osm, node_chain_t::iterator cut_at, bool cut_at_node);
  void transfer_relations(osm_t *osm, const way_t *from);
  void rotate(node_chain_t::iterator nfirst);
  const node_t *last_node() const;
  const node_t *first_node() const;
  unsigned int reverse_direction_sensitive_tags();
  unsigned int reverse_direction_sensitive_roles(osm_t *osm);
  xmlChar *generate_xml(item_id_t changeset) const O2G_OVERRIDE;
  void write_node_chain(xmlNodePtr way_node) const;

  void cleanup();

  const char *apiString() const O2G_OVERRIDE {
    return api_string();
  }
  static const char *api_string() {
    return "way";
  }
};

class relation_t : public base_object_t {
public:
  explicit relation_t();
  explicit relation_t(item_id_t ver, item_id_t i = 0);
  virtual ~relation_t() {}

  std::vector<member_t> members;

  std::vector<member_t>::iterator find_member_object(const object_t &o);
  std::vector<member_t>::const_iterator find_member_object(const object_t &o) const;

  void members_by_type(guint *nodes, guint *ways, guint *relations) const;
  std::string descriptive_name() const;
  xmlChar *generate_xml(item_id_t changeset) const O2G_OVERRIDE;
  void generate_member_xml(xmlNodePtr xml_node) const;

  bool is_multipolygon() const;

  void cleanup();

  const char *apiString() const O2G_OVERRIDE {
    return api_string();
  }
  static const char *api_string() {
    return "relation";
  }
};

void osm_node_chain_free(node_chain_t &node_chain);

void osm_member_free(member_t &member);
void osm_members_free(std::vector<member_t> &members);

void osm_tag_free(tag_t &tag);

#endif

#endif /* OSM_H */

// vim:et:ts=8:sw=2:sts=2:ai
