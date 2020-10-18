/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "osm.h"
#include "pos.h"

#include <algorithm>
#include <string>
#include <vector>

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

  inline bool is_discardable() const noexcept
  { return is_discardable(key); }
  static bool is_discardable(const char *key) noexcept;
  // also inversion to be able to use it directly as predicate
  static inline bool is_non_discardable(const tag_t &tag) noexcept
  { return !tag.is_discardable(); }

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

  /**
   * @brief compare if the values are identical
   *
   * This is intended to compare to values already mapped to the value cache
   * so a simple pointer compare is a fast start. Afterwards a case insensitive
   * compare of the values is done.
   */
  bool value_compare_ci(const char *v) const
  {
    return value_compare(v) || strcasecmp(v, value) == 0;
  }
};

class tag_list_t {
public:
  inline tag_list_t() noexcept : contents(nullptr) {}
  ~tag_list_t();

  bool operator==(const tag_list_t &other) const;
  inline bool operator!=(const tag_list_t &other) const
  { return !operator==(other); }

  /**
   * @brief check if any tags are present
   */
  bool empty() const noexcept;

  /**
   * @brief check if any tag that is not "created_by" is present
   */
  bool hasNonDiscardableTags() const noexcept;

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

  inline void swap(tag_list_t &other)
  { std::swap(contents, other.contents); }

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

class base_object_t : public base_attributes {
  friend class osm_t;

protected:
  explicit base_object_t(const base_attributes &attr) noexcept;
  base_object_t(const base_object_t &other);

  // it makes no sense do directly compare base_object_t instances as that will miss half of the picture
  bool operator==(const base_object_t &other) const
  {
    // flags are just a marker for runtime processing so are ignored here
    return base_attributes::operator==(other) && tags == other.tags;
  }

public:
  unsigned int flags;
  tag_list_t tags;

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

protected:
  virtual void generate_xml_custom(xmlNodePtr xml_node) const = 0;
};

class visible_item_t : public base_object_t {
protected:
  inline visible_item_t(const base_attributes &attr) noexcept
    : base_object_t(attr), map_item(nullptr), zoom_max(0.0f) {}

  inline bool operator==(const visible_item_t &other) const
  {
    // explicitely ignore the local members which are just visual representation
    return base_object_t::operator==(other);
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
  node_t(const base_attributes &attr, const lpos_t lp = lpos_t(), const pos_t &p = pos_t()) noexcept
    : visible_item_t(attr) , ways(0) , pos(p) , lpos(lp) {}

  virtual ~node_t() {}

  inline bool operator==(const node_t &other) const
  {
    // the other members are only about visual representation and can be ignored
    return visible_item_t::operator==(other) &&
           pos == other.pos;
  }
  inline bool operator!=(const node_t &other) const
  { return !operator==(other); }

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
  void generate_xml_custom(xmlNodePtr xml_node) const override;
};

typedef std::vector<node_t *> node_chain_t;

#define OSM_DRAW_FLAG_AREA  (1<<0)
#define OSM_DRAW_FLAG_BG    (1<<1)

class way_t : public visible_item_t {
  friend class osm_t;
public:
  explicit way_t(const base_attributes &attr = base_attributes())
    : visible_item_t(attr)
  {
    memset(&draw, 0, sizeof(draw));
  }
  virtual ~way_t() {}
  bool operator==(const way_t &other) const;
  inline bool operator!=(const way_t &other) const
  { return !operator==(other); }

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
  void generate_xml_custom(xmlNodePtr xml_node) const override {
    write_node_chain(xml_node);
  }
};

class relation_t : public base_object_t {
public:
  explicit relation_t(const base_attributes &attr = base_attributes())
    : base_object_t(attr) {}
  virtual ~relation_t() {}
  inline bool operator==(const relation_t &other) const
  {
    return base_object_t::operator==(other) &&
           members == other.members;
  }
  inline bool operator!=(const relation_t &other) const
  { return !operator==(other); }

  std::vector<member_t> members;

  std::vector<member_t>::iterator find_member_object(const object_t &o);
  inline std::vector<member_t>::const_iterator find_member_object(const object_t &o) const
  { return const_cast<relation_t *>(this)->find_member_object(o); }

  void members_by_type(unsigned int &nodes, unsigned int &ways, unsigned int &relations) const;
  std::string descriptive_name() const;
  void generate_member_xml(xmlNodePtr xml_node) const;

  bool is_multipolygon() const;

  const char *apiString() const noexcept override {
    return api_string();
  }
  static const char *api_string() noexcept {
    return "relation";
  }
protected:
  void generate_xml_custom(xmlNodePtr xml_node) const override {
    generate_member_xml(xml_node);
  }
};

/**
 * @brief drop the reference to all nodes in the given chain
 *
 * This does not modify the node_chain itself!
 */
void osm_node_chain_unref(node_chain_t &node_chain);
