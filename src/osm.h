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

#ifdef __cplusplus
#include <map>
#include <vector>

extern "C" {
#endif

#include "pos.h"

#include <math.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifdef __cplusplus
#include <vector>
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

typedef struct bounds_t {
  pos_t ll_min, ll_max;
  lpos_t min, max;
  lpos_t center;
  float scale;
} bounds_t;

typedef struct user_t {
  int uid;
  char name[];
} user_t;

typedef struct tag_t {
  struct tag_t *next;
  char *key, *value;
#ifdef __cplusplus
  tag_t(char *k, char *v)
    : next(0), key(k), value(v)
  { }
#endif
} tag_t;

typedef struct {
  item_id_t id;
  item_id_t version;
  user_t *user;
  tag_t *tag;
  time_t time;
  gboolean visible:8;
  int flags:24;
} base_object_t;

typedef struct node_t {
  base_object_t base;

  struct node_t *next;

  pos_t pos;
  lpos_t lpos;
  int ways;
  float zoom_max;

  /* icon */
  GdkPixbuf *icon_buf;

  /* a link to the visual representation on screen */
  struct map_item_chain_t *map_item_chain;
} node_t;

typedef enum {
  ILLEGAL=0, NODE, WAY, RELATION, NODE_ID, WAY_ID, RELATION_ID
} type_t;

#ifdef __cplusplus
struct item_id_chain_t {
  item_id_chain_t(type_t t, item_id_t i)
    : type(t), id(i) {}
  type_t type;
  item_id_t id;
};
#endif

#ifdef __cplusplus
typedef std::vector<node_t *> node_chain_t;
#endif

#define OSM_DRAW_FLAG_AREA  (1<<0)
#define OSM_DRAW_FLAG_BG    (1<<1)

typedef struct way_t {
  base_object_t base;

  struct way_t *next;

  /* visual representation from elemstyle */
  struct {
    guint flags;
    gulong color;
    gint width;
    float zoom_max;
    gboolean dashed;
    guint dash_length;

    union {
      struct {
	gulong color;
	gint width;
      } bg;

      struct {
	gulong color;
      } area;
    };
  } draw;

  /* a link to the visual representation on screen */
  struct map_item_chain_t *map_item_chain;

#ifdef __cplusplus
  node_chain_t *node_chain;
#else
  void *node_chain;
#endif
} way_t;

#ifdef __cplusplus
typedef std::vector<way_t *> way_chain_t;
#endif

/* return a pointer to the "base" object of an object */
#define OBJECT_BASE(a)     ((base_object_t*)((a).ptr))
#define OBJECT_ID(a)       (OBJECT_BASE(a)->id)
#define OBJECT_VERSION(a)  (OBJECT_BASE(a)->version)
#define OBJECT_USER(a)     (OBJECT_BASE(a)->user)
#define OBJECT_TIME(a)     (OBJECT_BASE(a)->time)
#define OBJECT_TAG(a)      (OBJECT_BASE(a)->tag)
#define OBJECT_VISIBLE(a)  (OBJECT_BASE(a)->visible)
#define OBJECT_FLAGS(a)    (OBJECT_BASE(a)->flags)

/* osm base type access macros */
#define OSM_BASE(a)        ((base_object_t*)(a))
#define OSM_ID(a)          (OSM_BASE(a)->id)
#define OSM_VERSION(a)     (OSM_BASE(a)->version)
#define OSM_USER(a)        (OSM_BASE(a)->user)
#define OSM_TIME(a)        (OSM_BASE(a)->time)
#define OSM_TAG(a)         (OSM_BASE(a)->tag)
#define OSM_VISIBLE(a)     (OSM_BASE(a)->visible)
#define OSM_FLAGS(a)       (OSM_BASE(a)->flags)

typedef struct relation_t relation_t;

/* two of these hash tables are used, one for nodes and one for ways */
/* currently relations aren't used often enough to justify the use */
/* of a hash table */

typedef struct hash_table_t hash_table_t;

typedef struct object_s {
  type_t type;
  union {
    node_t *node;
    way_t *way;
    relation_t *relation;
    item_id_t id;
    void *ptr;
  };

#ifdef __cplusplus
  inline object_s()
    : type(ILLEGAL), ptr(0) {}
  explicit inline object_s(node_t *n)
    : type(NODE), node(n) { }
  explicit inline object_s(way_t *w)
    : type(WAY), way(w) { }
  explicit inline object_s(relation_t *r)
    : type(RELATION), relation(r) { }

  inline object_s &operator=(node_t *n)
  { type = NODE; node = n; return *this; }
  inline object_s &operator=(way_t *w)
  { type = WAY; way = w; return *this; }
  inline object_s &operator=(relation_t *r)
  { type = RELATION; relation = r; return *this; }

  bool operator==(const object_s &other) const;
  inline bool operator!=(const object_s &other) const
  { return !operator==(other); }
  inline bool operator==(const object_s *other) const
  { return operator==(*other); }
  inline bool operator!=(const object_s *other) const
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
#endif
} object_t;

#ifdef __cplusplus
struct member_t {
  member_t(type_t t = ILLEGAL);

  object_t object;
  char   *role;

  operator bool() const;
  bool operator==(const member_t &other) const;
  inline bool operator==(const object_t &other) const
  { return object == other; }
};
#endif

struct relation_t {
#ifdef __cplusplus
  relation_t();
#endif
  base_object_t base;

  struct relation_t *next;
#ifdef __cplusplus
  std::vector<member_t> members;
  std::vector<member_t>::iterator find_member_object(const object_t &o);
  std::vector<member_t>::const_iterator find_member_object(const object_t &o) const;

  void members_by_type(guint *nodes, guint *ways, guint *relations) const;
#endif
};

typedef struct osm_t {
  bounds_t *bounds;   // original bounds as they appear in the file
  user_t *user;

  node_t *node;

  way_t  *way;

  relation_t  *relation;

  struct icon_t **icons;

  bounds_t rbounds;

#ifdef __cplusplus
  std::map<item_id_t, node_t *> node_hash;
  std::map<item_id_t, way_t *> way_hash;
  // hashing relations doesn't yet make much sense as relations are quite rare
  std::map<int, user_t *> users;   //< users where uid is given in XML
  std::vector<user_t *> anonusers; //< users without uid
#endif
} osm_t;

osm_t *osm_parse(const char *path, const char *filename, struct icon_t **icons);
gboolean osm_sanity_check(GtkWidget *parent, const osm_t *osm);
tag_t *osm_parse_osm_tag(xmlNode* a_node);
void osm_free(osm_t *osm);

const char *osm_node_get_value(node_t *node, const char *key);
gboolean osm_node_has_tag(const node_t *node);

void osm_way_free(osm_t *osm, way_t *way);
const char *osm_way_get_value(way_t *way, const char *key);
gboolean osm_node_has_value(const node_t* node, const char* str);
gboolean osm_way_has_value(const way_t* way, const char* str);
void osm_way_append_node(way_t *way, node_t *node);

gboolean osm_node_in_way(const way_t *way, const node_t *node);
gboolean osm_node_in_other_way(const osm_t *osm, const way_t *way, const node_t *node);

void osm_node_free(osm_t *osm, node_t *node);

void osm_tag_free(tag_t *tag);
void osm_tags_free(tag_t *tag);
tag_t *osm_tag_find(tag_t* tag, const char* key);
const char *osm_tag_get_by_key(const tag_t *tag, const char *key);
gboolean osm_is_creator_tag(const tag_t *tag);
gboolean osm_tag_key_and_value_present(const tag_t *haystack, const tag_t *tag);
gboolean osm_tag_key_other_value_present(const tag_t *haystack, const tag_t *tag);
gboolean osm_tag_lists_diff(const tag_t *t1, const tag_t *t2);
gboolean osm_tag_update(tag_t *tag, const char *key, const char *value);
void osm_tag_update_key(tag_t *tag, const char *key);
void osm_tag_update_value(tag_t *tag, const char *value);

char *osm_generate_xml_changeset(char* comment);
char *osm_generate_xml_node(item_id_t changeset, const node_t *node);
char *osm_generate_xml_way(item_id_t changeset, const way_t *way);
void osm_write_node_chain(xmlNodePtr way_node, const way_t *node_chain);
char *osm_generate_xml_relation(item_id_t changeset,
                                const relation_t *relation);

node_t *osm_get_node_by_id(osm_t *osm, item_id_t id);
way_t *osm_get_way_by_id(osm_t *osm, item_id_t id);
relation_t *osm_get_relation_by_id(osm_t *osm, item_id_t id);

gboolean osm_way_min_length(const way_t *way, guint len);
guint osm_way_number_of_nodes(const way_t *way);

/* ----------- edit functions ----------- */
node_t *osm_node_new(osm_t *osm, gint x, gint y);
node_t *osm_node_new_pos(osm_t *osm, const pos_t *pos);
void osm_node_attach(osm_t *osm, node_t *node);
void osm_node_restore(osm_t *osm, node_t *node);
void osm_way_delete(osm_t *osm, way_t *way, gboolean perm);

way_t *osm_way_new(void);
void osm_way_attach(osm_t *osm, way_t *way);

gboolean osm_position_within_bounds(const osm_t *osm, gint x, gint y);
gboolean osm_position_within_bounds_ll(const pos_t *ll_min, const pos_t *ll_max, const pos_t *pos);
item_id_t osm_new_way_id(osm_t *osm);
gboolean osm_way_ends_with_node(const way_t *way, const node_t *node);

void osm_way_reverse(way_t *way);
guint osm_way_reverse_direction_sensitive_tags(way_t *way);
guint osm_way_reverse_direction_sensitive_roles(osm_t *osm, way_t *way);

void osm_node_remove_from_relation(osm_t *osm, node_t *node);
void osm_way_remove_from_relation(osm_t *osm, way_t *way);

const node_t *osm_way_get_last_node(const way_t *way);
const node_t *osm_way_get_first_node(const way_t *way);
gboolean osm_way_is_closed(const way_t *way);

tag_t *osm_tags_copy(const tag_t *tag);

relation_t *osm_relation_new(void);
void osm_relation_free(relation_t *relation);
void osm_relation_attach(osm_t *osm, relation_t *relation);
void osm_relation_delete(osm_t *osm, relation_t *relation,
			 gboolean permanently);
gchar *relation_get_descriptive_name(const relation_t *relation);

gboolean osm_object_is_real(const object_t *object);
const char *osm_object_type_string(const object_t *object);
gchar *osm_object_id_string(const object_t *object);
char *osm_object_string(const object_t *object);
const tag_t *osm_object_get_tags(const object_t *object);
item_id_t osm_object_get_id(const object_t *object);
void osm_object_set_flags(object_t *map_item, int set, int clr);
char *osm_object_get_name(const object_t *object);
gboolean osm_object_is_same(const object_t *obj1, const object_t *obj2);

#ifdef __cplusplus
}

typedef std::vector<relation_t *> relation_chain_t;

member_t osm_parse_osm_relation_member(osm_t *osm, xmlNode *a_node);

node_t *osm_parse_osm_way_nd(osm_t *osm, xmlNode *a_node);
void osm_node_chain_free(node_chain_t &node_chain);
way_chain_t osm_node_to_way(const osm_t *osm, const node_t *node);
way_chain_t osm_node_delete(osm_t *osm, node_t *node,
                            bool permanently, bool affect_ways);
void osm_way_rotate(way_t *way, node_chain_t::iterator nfirst);
relation_chain_t osm_way_to_relation(osm_t *osm, const way_t *way);
relation_chain_t osm_object_to_relation(osm_t *osm, const object_t *object);
void osm_way_restore(osm_t *osm, way_t *way, const std::vector<item_id_chain_t> &id_chain);

void osm_member_free(member_t &member);
void osm_members_free(std::vector<member_t> &members);

#endif

#endif /* OSM_H */

// vim:et:ts=8:sw=2:sts=2:ai
