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

#include <math.h>

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

typedef struct bounds_s {
  pos_t ll_min, ll_max;
  lpos_t min, max;
  lpos_t center;
  float scale;
} bounds_t;

typedef struct user_s {
  char *name;
  struct user_s *next;
} user_t;

typedef struct tag_s {
  char *key, *value;
  struct tag_s *next;
} tag_t;

typedef struct {
  item_id_t id;
  item_id_t version;
  user_t *user;
  time_t time;
  gboolean visible;
  int flags;
  tag_t *tag;

} base_object_t;

typedef struct node_s {
  base_object_t base;

  pos_t pos;
  lpos_t lpos;
  int ways;
  float zoom_max;

  /* icon */
  GdkPixbuf *icon_buf;

  /* a link to the visual representation on screen */
  struct map_item_chain_s *map_item_chain;

  struct node_s *next;
} node_t;

typedef enum {
  ILLEGAL=0, NODE, WAY, RELATION, NODE_ID, WAY_ID, RELATION_ID
} type_t;

typedef struct item_id_chain_s {
  type_t type;
  item_id_t id;
  struct item_id_chain_s *next;
} item_id_chain_t;

typedef struct node_chain {
  node_t *node;
  struct node_chain *next;
} node_chain_t;

#define OSM_DRAW_FLAG_AREA  (1<<0)
#define OSM_DRAW_FLAG_BG    (1<<1)

typedef struct way_s {
  base_object_t base;

  node_chain_t *node_chain;

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
  struct map_item_chain_s *map_item_chain;

  struct way_s *next;
} way_t;

typedef struct way_chain {
  way_t *way;
  struct way_chain *next;
} way_chain_t;

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

typedef struct relation_s {
  base_object_t base;

  struct member_s *member;
  struct relation_s *next;
} relation_t;

typedef struct relation_chain_s {
  relation_t *relation;
  struct relation_chain_s *next;
} relation_chain_t;

/* two of these hash tables are used, one for nodes and one for ways */
/* currently relations aren't used often enough to justify the use */
/* of a hash table */

/* the current hash table uses 16 bits. each table thus is */
/* 256 kbytes (2^16 * sizeof(void*)) in size */
#define ID2HASH(a) ((unsigned short)(a) ^ (unsigned short)((a)>>16))
typedef struct hash_item_s {
  union {
    node_t *node;
    way_t *way;
    relation_t *relation;
  } data;

  struct hash_item_s *next;
} hash_item_t;

typedef struct {
  hash_item_t *hash[65536];
} hash_table_t;

typedef struct {
  type_t type;
  union {
    node_t *node;
    way_t *way;
    relation_t *relation;
    item_id_t id;
    void *ptr;
  };
} object_t;

typedef struct member_s {
  object_t object;
  char   *role;
  struct member_s *next;
} member_t;

typedef struct osm_s {
  bounds_t *bounds;   // original bounds as they appear in the file
  user_t *user;

  node_t *node;
  hash_table_t *node_hash;

  way_t  *way;
  hash_table_t *way_hash;

  // hashing relations doesn't yet make much sense as relations are quite rare
  relation_t  *relation;

} osm_t;

#include <libxml/parser.h>
#include <libxml/tree.h>

osm_t *osm_parse(char *path, char *filename);
gboolean osm_sanity_check(GtkWidget *parent, osm_t *osm);
tag_t *osm_parse_osm_tag(osm_t *osm, xmlDocPtr doc, xmlNode *a_node);
node_chain_t *osm_parse_osm_way_nd(osm_t *osm, xmlDocPtr doc, xmlNode *a_node);
member_t *osm_parse_osm_relation_member(osm_t *osm, xmlDocPtr doc, xmlNode *a_node);
void osm_free(struct icon_s **icon, osm_t *osm);

char *osm_node_get_value(node_t *node, char *key);
gboolean osm_node_has_tag(node_t *node);

void osm_way_free(hash_table_t *hash_table, way_t *way);
char *osm_way_get_value(way_t *way, char *key);
gboolean osm_node_has_value(node_t *node, char *str);
gboolean osm_way_has_value(way_t *way, char *str);
void osm_way_append_node(way_t *way, node_t *node);

gboolean osm_node_in_way(way_t *way, node_t *node);
gboolean osm_node_in_other_way(osm_t *osm, way_t *way, node_t *node);

void osm_node_chain_free(node_chain_t *node_chain);
unsigned int osm_node_chain_length(const node_chain_t *node_chain);
gboolean osm_node_chain_diff(const node_chain_t *n1, const node_chain_t *n2);
void osm_node_free(hash_table_t *hash, struct icon_s **icon, node_t *node);

void osm_members_free(member_t *member);
void osm_member_free(member_t *member);

void osm_tag_free(tag_t *tag);
void osm_tags_free(tag_t *tag);
char *osm_tag_get_by_key(tag_t *tag, char *key);
gboolean osm_is_creator_tag(tag_t *tag);
gboolean osm_tag_key_and_value_present(tag_t *haystack, tag_t *tag);
gboolean osm_tag_key_other_value_present(tag_t *haystack, tag_t *tag);
gboolean osm_tag_lists_diff(const tag_t *t1, const tag_t *t2);

char *osm_generate_xml_changeset(osm_t *osm, char *comment);
char *osm_generate_xml_node(osm_t *osm, item_id_t changeset, node_t *node);
char *osm_generate_xml_way(osm_t *osm, item_id_t changeset, way_t *way);
char *osm_generate_xml_relation(osm_t *osm, item_id_t changeset,
				relation_t *relation);

node_t *osm_get_node_by_id(osm_t *osm, item_id_t id);
way_t *osm_get_way_by_id(osm_t *osm, item_id_t id);
relation_t *osm_get_relation_by_id(osm_t *osm, item_id_t id);

guint osm_way_number_of_nodes(way_t *way);
relation_chain_t *osm_node_to_relation(osm_t *osm, node_t *node,
				       gboolean via_way);
relation_chain_t *osm_way_to_relation(osm_t *osm, way_t *way);
relation_chain_t *osm_relation_to_relation(osm_t *osm, relation_t *relation);
relation_chain_t *osm_object_to_relation(osm_t *osm, object_t *object);
void osm_relation_chain_free(relation_chain_t *relation_chain);
way_chain_t *osm_node_to_way(osm_t *osm, node_t *node);

/* ----------- edit functions ----------- */
node_t *osm_node_new(osm_t *osm, gint x, gint y);
void osm_node_attach(osm_t *osm, node_t *node);
void osm_node_restore(osm_t *osm, node_t *node);
way_chain_t *osm_node_delete(osm_t *osm, struct icon_s **icon, node_t *node,
			     gboolean permanently, gboolean affect_ways);
void osm_way_delete(osm_t *osm, struct icon_s **icon, way_t *way,
		    gboolean perm);
void osm_way_restore(osm_t *osm, way_t *way, item_id_chain_t *id_chain);

way_t *osm_way_new(void);
void osm_way_attach(osm_t *osm, way_t *way);

gboolean osm_position_within_bounds(osm_t *osm, gint x, gint y);
item_id_t osm_new_way_id(osm_t *osm);
gboolean osm_way_ends_with_node(way_t *way, node_t *node);

void osm_way_reverse(way_t *way);
guint osm_way_reverse_direction_sensitive_tags(way_t *way);
guint osm_way_reverse_direction_sensitive_roles(osm_t *osm, way_t *way);

void osm_node_remove_from_relation(osm_t *osm, node_t *node);
void osm_way_remove_from_relation(osm_t *osm, way_t *way);

node_t *osm_way_get_last_node(way_t *way);
node_t *osm_way_get_first_node(way_t *way);
void osm_way_rotate(way_t *way, gint offset);

tag_t *osm_tags_copy(tag_t *tag);

relation_t *osm_relation_new(void);
void osm_relation_free(relation_t *relation);
void osm_relation_attach(osm_t *osm, relation_t *relation);
void osm_relation_delete(osm_t *osm, relation_t *relation,
			 gboolean permanently);
gint osm_relation_members_num(relation_t *relation);

gboolean osm_object_is_real(object_t *object);
char *osm_object_type_string(object_t *object);
char *osm_object_id_string(object_t *object);
char *osm_object_string(object_t *object);
tag_t *osm_object_get_tags(object_t *object);
item_id_t osm_object_get_id(object_t *object);
void osm_object_set_flags(object_t *map_item, int set, int clr);
char *osm_object_get_name(object_t *object);
gboolean osm_object_is_same(object_t *obj1, object_t *obj2);

#endif /* OSM_H */

// vim:et:ts=8:sw=2:sts=2:ai
