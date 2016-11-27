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

#define _DEFAULT_SOURCE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "osm.h"

#include "appdata.h"
#include "banner.h"
#include "icon.h"
#include "map.h"
#include "misc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif
#include <time.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

#define ID2HASH(a) ((unsigned short)(a) ^ (unsigned short)((a)>>16))

/* the current hash table uses 16 bits. each table thus is */
/* 256 kbytes (2^16 * sizeof(void*)) in size */
typedef struct hash_item_s {
  struct hash_item_s *next;

  union {
    node_t *node;
    way_t *way;
  } data;
} hash_item_t;

struct hash_table_s {
  hash_item_t *hash[65536];
};

/* ------------------------- user handling --------------------- */

static void osm_users_free(user_t *user) {
  while(user) {
    user_t *next = user->next;

    g_free(user);

    user = next;
  }
}

static user_t *osm_user(osm_t *osm, const char *name, int uid) {
  if(!name) return NULL;

  /* search through user list */
  user_t **user = &osm->user;

  if(uid >= 0) {
    /* match using the uid */
    for (; *user; user = &(*user)->next)
      if((*user)->uid >= uid)
        break;
  } else {
    /* match with the name, but only against users without uid */
    for (; *user; user = &(*user)->next)
      if((*user)->uid >= 0 || strcasecmp((*user)->name, name) >= 0)
        break;
  }

  /* end of list or inexact match? create new user entry! */
  if(!*user || strcasecmp((*user)->name, name)) {
    const size_t nlen = strlen(name) + 1;
    user_t *newu = (user_t*)g_malloc(sizeof(**user) + nlen);
    memcpy(newu->name, name, nlen);
    newu->uid = uid;
    newu->next = *user;
    *user = newu;

    return newu;
  }

  return *user;
}

static
time_t convert_iso8601(const char *str) {
  if(!str) return 0;

  struct tm ctime = { 0 };
  strptime(str, "%FT%T%z", &ctime);

  long gmtoff = ctime.tm_gmtoff;

  return timegm(&ctime) - gmtoff;
}

/* -------------------- tag handling ----------------------- */

void osm_tag_free(tag_t *tag) {
  g_free(tag->key);
  g_free(tag->value);
  g_free(tag);
}

void osm_tags_free(tag_t *tag) {
  while(tag) {
    tag_t *next = tag->next;
    osm_tag_free(tag);
    tag = next;
  }
}

tag_t *osm_parse_osm_tag(xmlNode *a_node) {
  /* allocate a new tag structure */
  tag_t *tag = g_new0(tag_t, 1);

  char *prop;
  if((prop = (char*)xmlGetProp(a_node, BAD_CAST "k"))) {
    if(strlen(prop) > 0) tag->key = g_strdup(prop);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, BAD_CAST "v"))) {
    if(strlen(prop) > 0) tag->value = g_strdup(prop);
    xmlFree(prop);
  }

  if(!tag->key || !tag->value) {
    printf("incomplete tag key/value %s/%s\n", tag->key, tag->value);
    osm_tags_free(tag);
    return NULL;
  }

  const xmlNode *cur_node = NULL;
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next)
    if (cur_node->type == XML_ELEMENT_NODE)
      printf("found unhandled osm/node/tag/%s\n", cur_node->name);

  return tag;
}

gboolean osm_is_creator_tag(const tag_t *tag) {
  if(strcasecmp(tag->key, "created_by") == 0) return TRUE;

  return FALSE;
}

gboolean osm_tag_key_and_value_present(const tag_t *haystack, const tag_t *tag) {
  for(; haystack; haystack = haystack->next) {
    if((strcasecmp(haystack->key, tag->key) == 0) &&
       (strcasecmp(haystack->value, tag->value) == 0))
      return TRUE;
  }
  return FALSE;
}

gboolean osm_tag_key_other_value_present(const tag_t *haystack, const tag_t *tag) {
  for(; haystack; haystack = haystack->next) {
    if((strcasecmp(haystack->key, tag->key) == 0) &&
       (strcasecmp(haystack->value, tag->value) != 0))
      return TRUE;
  }
  return FALSE;
}

/**
 * @brief compare 2 tag lists
 * @param t1 first list
 * @param t2 second list
 * @return if the lists differ
 */
gboolean osm_tag_lists_diff(const tag_t *t1, const tag_t *t2) {
  unsigned int ocnt = 0, ncnt = 0;
  const tag_t *ntag;
  const tag_t *t1creator = NULL, *t2creator = NULL;

  /* first check list length, otherwise deleted tags are hard to detect */
  for(ntag = t1; ntag != NULL; ntag = ntag->next) {
    if(osm_is_creator_tag(ntag))
      t1creator = ntag;
    else
      ncnt++;
  }
  for(ntag = t2; ntag != NULL; ntag = ntag->next) {
    if(osm_is_creator_tag(ntag))
      t2creator = ntag;
    else
      ocnt++;
  }

  if (ncnt != ocnt)
    return TRUE;

  for (ntag = t1; ntag != NULL; ntag = ntag->next) {
    if (ntag == t1creator)
      continue;

    const tag_t *otag;
    for (otag = t2; otag != NULL; otag = otag->next) {
      if(otag == t2creator)
        continue;

      if (strcmp(otag->key, ntag->key) == 0) {
        if (strcmp(otag->value, ntag->value) != 0)
          return TRUE;
        break;
      }
    }
  }

  return FALSE;
}

/**
 * @brief update the key and value of a tag
 * @param tag the tag struct to update
 * @param key the new key
 * @param value the new value
 * @return if tag was actually changed
 *
 * This will update the key and value, but will avoid memory allocations
 * in case key or value have not changed.
 *
 * This would be a no-op:
 * \code
 * osm_tag_update(tag, tag->key, tag->value);
 * \endcode
 */
gboolean osm_tag_update(tag_t *tag, const char *key, const char *value)
{
  gboolean ret = FALSE;
  if(strcmp(tag->key, key) != 0) {
    osm_tag_update_key(tag, key);
    ret = TRUE;
  }
  if(strcmp(tag->value, value) != 0) {
    osm_tag_update_value(tag, value);
    ret = TRUE;
  }
  return ret;
}

/**
 * @brief replace the key of a tag
 */
void osm_tag_update_key(tag_t *tag, const char *key)
{
  const size_t nlen = strlen(key) + 1;
  tag->key = g_realloc(tag->key, nlen);
  memcpy(tag->key, key, nlen);
}

/**
 * @brief replace the value of a tag
 */
void osm_tag_update_value(tag_t *tag, const char *value)
{
  const size_t nlen = strlen(value) + 1;
  tag->value = g_realloc(tag->value, nlen);
  memcpy(tag->value, value, nlen);
}

gboolean osm_way_ends_with_node(const way_t *way, const node_t *node) {
  /* a deleted way may even not contain any nodes at all */
  /* so ignore it */
  if(OSM_FLAGS(way) & OSM_FLAG_DELETED)
    return FALSE;

  /* any valid way must have at least two nodes */
  g_assert(way->node_chain);
  g_assert(way->node_chain->next);

  const node_chain_t *chain = way->node_chain;
  if(chain->node == node) return TRUE;

  while(chain->next) chain = chain->next;
  if(chain->node == node) return TRUE;

  return FALSE;
}

/* ------------------- node handling ------------------- */

void osm_node_free(osm_t *osm, node_t *node) {
  hash_table_t *hash_table = osm->node_hash;
  item_id_t id = OSM_ID(node);

  if(node->icon_buf)
    icon_free(osm->icons, node->icon_buf);

  /* there must not be anything left in this chain */
  g_assert(!node->map_item_chain);

  osm_tags_free(OSM_TAG(node));

  g_free(node);

  /* also remove node from hash table */
  if(id > 0 && hash_table) {
    // use hash table if present
    hash_item_t **item = &(hash_table->hash[ID2HASH(id)]);
    for(; *item; item = &(*item)->next) {
      if(OSM_ID((*item)->data.node) == id) {
	hash_item_t *cur = *item;
	*item = (*item)->next;
	g_free(cur);
	return;
      }
    }
  }
}

static void osm_nodes_free(osm_t *osm, node_t *node) {
  while(node) {
    node_t *next = node->next;
    osm_node_free(osm, node);
    node = next;
  }
}

/* ------------------- way handling ------------------- */

void osm_node_chain_free(node_chain_t *node_chain) {
  while(node_chain) {
    node_t *node = node_chain->node;
    g_assert_cmpint(node->ways, >, 0);

    node_chain_t *next = node_chain->next;
    node->ways--;
    g_free(node_chain);
    node_chain = next;
  }
}

void osm_way_free(osm_t *osm, way_t *way) {
  hash_table_t *hash_table = osm->way_hash;
  item_id_t id = OSM_ID(way);

  //  printf("freeing way #" ITEM_ID_FORMAT "\n", OSM_ID(way));

  osm_node_chain_free(way->node_chain);
  osm_tags_free(OSM_TAG(way));

  /* there must not be anything left in this chain */
  g_assert(!way->map_item_chain);

  g_free(way);

  /* also remove way from hash table */
  if(id > 0 && hash_table) {
    // use hash table if present
    hash_item_t **item = &(hash_table->hash[ID2HASH(id)]);
    for(; *item; item = &(*item)->next) {
      if(OSM_ID((*item)->data.way) == id) {
	hash_item_t *cur = *item;
	*item = (*item)->next;
	g_free(cur);
	return;
      }
    }
  }
}

static void osm_ways_free(osm_t *osm, way_t *way) {
  while(way) {
    way_t *next = way->next;
    osm_way_free(osm, way);
    way = next;
  }
}

void osm_way_append_node(way_t *way, node_t *node) {
  node_chain_t **node_chain = &way->node_chain;

  while(*node_chain)
    node_chain = &((*node_chain)->next);

  *node_chain = g_new0(node_chain_t, 1);
  (*node_chain)->node = node;

  node->ways++;
}

/**
 * @brief check if 2 node chains differ
 * @param n1 first chain
 * @param n2 second chain
 * @retval if the chains differ
 */
gboolean osm_node_chain_diff(const node_chain_t *n1, const node_chain_t *n2) {
  while(n1) {
    if (n2 == NULL)
      return TRUE;

    if (OSM_ID(n1->node) != OSM_ID(n2->node))
      return TRUE;

    n1 = n1->next;
    n2 = n2->next;
  }

  return (n2 != NULL) ? TRUE : FALSE;
}

/**
 * @brief check if 2 member lists differ
 * @param n1 first list
 * @param n2 second list
 * @retval if the lists differ
 */
gboolean osm_members_diff(const member_t *n1, const member_t *n2) {
  while(n1) {
    if (n2 == NULL)
      return TRUE;

    if (n1->object.id != n2->object.id)
      return TRUE;

    if (n1->object.type != n2->object.type)
      return TRUE;

    if (g_strcmp0(n1->role, n2->role) != 0)
      return TRUE;

    n1 = n1->next;
    n2 = n2->next;
  }

  return (n2 != NULL) ? TRUE : FALSE;
}

node_chain_t *osm_parse_osm_way_nd(osm_t *osm, xmlNode *a_node) {
  xmlChar *prop;

  if((prop = xmlGetProp(a_node, BAD_CAST "ref"))) {
    item_id_t id = strtoll((char*)prop, NULL, 10);
    node_chain_t *node_chain = g_new0(node_chain_t, 1);

    /* search matching node */
    node_chain->node = osm_get_node_by_id(osm, id);
    if(!node_chain->node) {
      printf("Node id " ITEM_ID_FORMAT " not found\n", id);
      g_free(node_chain);
      node_chain = NULL;
    } else
      node_chain->node->ways++;

    xmlFree(prop);

    return node_chain;
  }

  return NULL;
}

/* ------------------- relation handling ------------------- */

void osm_member_free(member_t *member) {
  g_free(member->role);
  g_free(member);
}

void osm_members_free(member_t *member) {
  while(member) {
    member_t *next = member->next;
    osm_member_free(member);
    member = next;
  }
}

void osm_relation_free(relation_t *relation) {
  osm_tags_free(OSM_TAG(relation));
  osm_members_free(relation->member);

  g_free(relation);
}

static void osm_relations_free(relation_t *relation) {
  while(relation) {
    relation_t *next = relation->next;
    osm_relation_free(relation);
    relation = next;
  }
}

member_t *osm_parse_osm_relation_member(osm_t *osm, xmlNode *a_node) {
  char *prop;
  member_t *member = g_new0(member_t, 1);
  member->object.type = ILLEGAL;

  if((prop = (char*)xmlGetProp(a_node, BAD_CAST "type"))) {
    if(strcmp(prop, "way") == 0)           member->object.type = WAY;
    else if(strcmp(prop, "node") == 0)     member->object.type = NODE;
    else if(strcmp(prop, "relation") == 0) member->object.type = RELATION;
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, BAD_CAST "ref"))) {
    item_id_t id = strtoll(prop, NULL, 10);

    switch(member->object.type) {
    case ILLEGAL:
      g_free(member);
      printf("Unable to store illegal type\n");
      return NULL;

    case WAY:
      /* search matching way */
      member->object.way = osm_get_way_by_id(osm, id);
      if(!member->object.way) {
	member->object.type = WAY_ID;
	member->object.id = id;
      }
      break;

    case NODE:
      /* search matching node */
      member->object.node = osm_get_node_by_id(osm, id);
      if(!member->object.node) {
	member->object.type = NODE_ID;
	member->object.id = id;
      }
      break;

    case RELATION:
      /* search matching relation */
      member->object.relation = osm_get_relation_by_id(osm, id);
      if(!member->object.relation) {
	member->object.type = NODE_ID;
	member->object.id = id;
      }
      break;

    case WAY_ID:
    case NODE_ID:
    case RELATION_ID:
      break;
    }

    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, BAD_CAST "role"))) {
    if(strlen(prop) > 0) member->role = g_strdup(prop);
    xmlFree(prop);
  }

  return member;
}

/* try to find something descriptive */
gchar *relation_get_descriptive_name(const relation_t *relation) {
  const char *name = NULL;
  const char *keys[] = { "ref", "name", "description", "note", "fix" "me", NULL};
  unsigned int i;
  for (i = 0; (keys[i] != NULL) && (name == NULL); i++)
    name = osm_tag_get_by_key(OSM_TAG(relation), keys[i]);

  if(!name)
    return g_strdup_printf("<ID #" ITEM_ID_FORMAT ">", OSM_ID(relation));
  else
    return g_strdup(name);
}

/* ------------------ osm handling ----------------- */

/* the two hash tables eat over 512kBytes memory and may thus be */
/* freed at any time. osm2go can work without them (albeit slower) */
static void hash_table_free(hash_table_t *table) {
  if(!table) return;

  int i;
  for(i=0;i<65536;i++) {
    hash_item_t *item = table->hash[i];
    while(item) {
      hash_item_t *next = item->next;
      g_free(item);
      item = next;
    }
  }
  g_free(table);
}

static void osm_hash_tables_free(osm_t *osm) {
  hash_table_free(osm->node_hash);
  osm->node_hash = NULL;
  hash_table_free(osm->way_hash);
  osm->way_hash = NULL;
}

void osm_free(osm_t *osm) {
  if(!osm) return;

  osm_hash_tables_free(osm);

  osm_users_free(osm->user);
  osm_ways_free(osm, osm->way);
  osm_nodes_free(osm, osm->node);
  osm_relations_free(osm->relation);
  g_free(osm);
}

/* -------------------------- stream parser ------------------- */

#include <libxml/xmlreader.h>

static gint my_strcmp(const xmlChar *a, const xmlChar *b) {
  if(!a && !b) return 0;
  if(!a) return -1;
  if(!b) return +1;
  return strcmp((char*)a,(char*)b);
}

/* skip current element incl. everything below (mainly for testing) */
/* returns FALSE if something failed */
static gboolean skip_element(xmlTextReaderPtr reader) {
  g_assert(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT);
  const xmlChar *name = xmlTextReaderConstName(reader);
  g_assert(name);
  int depth = xmlTextReaderDepth(reader);

  if(xmlTextReaderIsEmptyElement(reader))
    return TRUE;

  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) > depth) ||
	 (my_strcmp(xmlTextReaderConstName(reader), name) != 0))) {
    ret = xmlTextReaderRead(reader);
  }
  return(ret == 1);
}

/* parse bounds */
static gboolean process_bounds(xmlTextReaderPtr reader, bounds_t *bounds) {
  char *prop = NULL;

  bounds->ll_min.lat = bounds->ll_min.lon = NAN;
  bounds->ll_max.lat = bounds->ll_max.lon = NAN;

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "minlat"))) {
    bounds->ll_min.lat = g_ascii_strtod(prop, NULL);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "maxlat"))) {
    bounds->ll_max.lat = g_ascii_strtod(prop, NULL);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "minlon"))) {
    bounds->ll_min.lon = g_ascii_strtod(prop, NULL);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "maxlon"))) {
    bounds->ll_max.lon = g_ascii_strtod(prop, NULL);
    xmlFree(prop);
  }

  if(isnan(bounds->ll_min.lat) || isnan(bounds->ll_min.lon) ||
     isnan(bounds->ll_max.lat) || isnan(bounds->ll_max.lon)) {
    errorf(NULL, "Invalid coordinate in bounds (%f/%f/%f/%f)",
	   bounds->ll_min.lat, bounds->ll_min.lon,
	   bounds->ll_max.lat, bounds->ll_max.lon);

    return FALSE;
  }

  /* skip everything below */
  skip_element(reader);

  /* calculate map zone which will be used as a reference for all */
  /* drawing/projection later on */
  pos_t center = { (bounds->ll_max.lat + bounds->ll_min.lat)/2,
		   (bounds->ll_max.lon + bounds->ll_min.lon)/2 };

  pos2lpos_center(&center, &bounds->center);

  /* the scale is needed to accomodate for "streching" */
  /* by the mercartor projection */
  bounds->scale = cos(DEG2RAD(center.lat));

  pos2lpos_center(&bounds->ll_min, &bounds->min);
  bounds->min.x -= bounds->center.x;
  bounds->min.y -= bounds->center.y;
  bounds->min.x *= bounds->scale;
  bounds->min.y *= bounds->scale;

  pos2lpos_center(&bounds->ll_max, &bounds->max);
  bounds->max.x -= bounds->center.x;
  bounds->max.y -= bounds->center.y;
  bounds->max.x *= bounds->scale;
  bounds->max.y *= bounds->scale;

  return TRUE;
}

static tag_t *process_tag(xmlTextReaderPtr reader) {
  /* allocate a new tag structure */
  tag_t *tag = g_new0(tag_t, 1);

  char *prop;
  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "k"))) {
    if(strlen(prop) > 0) tag->key = g_strdup(prop);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "v"))) {
    if(strlen(prop) > 0) tag->value = g_strdup(prop);
    xmlFree(prop);
  }

  if(!tag->key || !tag->value) {
    printf("incomplete tag key/value %s/%s\n", tag->key, tag->value);
    osm_tags_free(tag);
    tag = NULL;
  }

  skip_element(reader);
  return tag;
}

static void process_base_attributes(base_object_t *obj, xmlTextReaderPtr reader, osm_t *osm)
{
  xmlChar *prop;
  if((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "id"))) {
    OSM_ID(obj) = strtoll((char*)prop, NULL, 10);
    xmlFree(prop);
  }

  /* new in api 0.6: */
  if((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "version"))) {
    OSM_VERSION(obj) = strtoul((char*)prop, NULL, 10);
    xmlFree(prop);
  }

  if((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "user"))) {
    int uid = -1;
    xmlChar *puid = xmlTextReaderGetAttribute(reader, BAD_CAST "uid");
    if(puid) {
      char *endp;
      uid = strtol((char*)puid, &endp, 10);
      if(*endp) {
        printf("WARNING: cannot parse uid '%s' for user '%s'\n", puid, prop);
        uid = -1;
      }
      xmlFree(puid);
    }
    OSM_USER(obj) = osm_user(osm, (char*)prop, uid);
    xmlFree(prop);
  }

  if((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "visible"))) {
    OSM_VISIBLE(obj) = (strcasecmp((char*)prop, "true") == 0);
    xmlFree(prop);
  }

  if((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "timestamp"))) {
    OSM_TIME(obj) = convert_iso8601((char*)prop);
    xmlFree(prop);
  }
}

static node_t *process_node(xmlTextReaderPtr reader, osm_t *osm) {

  /* allocate a new node structure */
  node_t *node = g_new0(node_t, 1);
  node->pos.lat = node->pos.lon = NAN;

  process_base_attributes(&node->base, reader, osm);

  xmlChar *prop;
  if((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "lat"))) {
    node->pos.lat = g_ascii_strtod((char*)prop, NULL);
    xmlFree(prop);
  }

  if((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "lon"))) {
    node->pos.lon = g_ascii_strtod((char*)prop, NULL);
    xmlFree(prop);
  }

  pos2lpos(osm->bounds, &node->pos, &node->lpos);

  /* append node to end of hash table if present */
  if(osm->node_hash) {
    hash_item_t **item = &osm->node_hash->hash[ID2HASH(OSM_ID(node))];
    while(*item) item = &(*item)->next;

    *item = g_new0(hash_item_t, 1);
    (*item)->data.node = node;
  }

  /* just an empty element? then return the node as it is */
  if(xmlTextReaderIsEmptyElement(reader))
    return node;

  /* parse tags if present */
  int depth = xmlTextReaderDepth(reader);

  /* scan all elements on same level or its children */
  tag_t **tag = &OSM_TAG(node);
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const char *subname = (const char*)xmlTextReaderConstName(reader);
      if(strcmp(subname, "tag") == 0) {
	*tag = process_tag(reader);
	if(*tag) tag = &(*tag)->next;
      } else
	skip_element(reader);
    }

    ret = xmlTextReaderRead(reader);
  }

  return node;
}

static node_chain_t *process_nd(xmlTextReaderPtr reader, osm_t *osm) {
  xmlChar *prop;

  if((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "ref"))) {
    item_id_t id = strtoll((char*)prop, NULL, 10);
    node_chain_t *node_chain = g_new0(node_chain_t, 1);

    /* search matching node */
    node_chain->node = osm_get_node_by_id(osm, id);
    if(!node_chain->node) printf("Node id " ITEM_ID_FORMAT " not found\n", id);
    else                  node_chain->node->ways++;

    xmlFree(prop);

    skip_element(reader);
    return node_chain;
  }

  skip_element(reader);
  return NULL;
}

static way_t *process_way(xmlTextReaderPtr reader, osm_t *osm) {
  /* allocate a new way structure */
  way_t *way = g_new0(way_t, 1);

  process_base_attributes(&way->base, reader, osm);

  /* append way to end of hash table if present */
  if(osm->way_hash) {
    hash_item_t **item = &osm->way_hash->hash[ID2HASH(OSM_ID(way))];
    while(*item) item = &(*item)->next;

    *item = g_new0(hash_item_t, 1);
    (*item)->data.way = way;
  }

  /* just an empty element? then return the way as it is */
  /* (this should in fact never happen as this would be a way without nodes) */
  if(xmlTextReaderIsEmptyElement(reader))
    return way;

  /* parse tags/nodes if present */
  int depth = xmlTextReaderDepth(reader);

  /* scan all elements on same level or its children */
  tag_t **tag = &OSM_TAG(way);
  node_chain_t **node_chain = &way->node_chain;
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const char *subname = (const char*)xmlTextReaderConstName(reader);
      if(strcmp(subname, "nd") == 0) {
	*node_chain = process_nd(reader, osm);
	if(*node_chain) node_chain = &(*node_chain)->next;
      } else if(strcmp(subname, "tag") == 0) {
	*tag = process_tag(reader);
	if(*tag) tag = &(*tag)->next;
      } else
	skip_element(reader);
    }
    ret = xmlTextReaderRead(reader);
  }

  return way;
}

static member_t *process_member(xmlTextReaderPtr reader, osm_t *osm) {
  char *prop;
  member_t *member = g_new0(member_t, 1);
  member->object.type = ILLEGAL;

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "type"))) {
    if(strcmp(prop, "way") == 0)           member->object.type = WAY;
    else if(strcmp(prop, "node") == 0)     member->object.type = NODE;
    else if(strcmp(prop, "relation") == 0) member->object.type = RELATION;
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "ref"))) {
    item_id_t id = strtoll(prop, NULL, 10);

    switch(member->object.type) {
    case ILLEGAL:
      g_free(member);
      printf("Unable to store illegal type\n");
      return NULL;

    case WAY:
      /* search matching way */
      member->object.way = osm_get_way_by_id(osm, id);
      if(!member->object.way) {
	member->object.type = WAY_ID;
	member->object.id = id;
      }
      break;

    case NODE:
      /* search matching node */
      member->object.node = osm_get_node_by_id(osm, id);
      if(!member->object.node) {
	member->object.type = NODE_ID;
	member->object.id = id;
      }
      break;

    case RELATION:
      /* search matching relation */
      member->object.relation = osm_get_relation_by_id(osm, id);
      if(!member->object.relation) {
	member->object.type = NODE_ID;
	member->object.id = id;
      }
      break;

    case WAY_ID:
    case NODE_ID:
    case RELATION_ID:
      break;
    }

    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "role"))) {
    if(strlen(prop) > 0) member->role = g_strdup(prop);
    xmlFree(prop);
  }

  return member;
}

static relation_t *process_relation(xmlTextReaderPtr reader, osm_t *osm) {
  /* allocate a new relation structure */
  relation_t *relation = g_new0(relation_t, 1);

  process_base_attributes(&relation->base, reader, osm);

  /* just an empty element? then return the relation as it is */
  /* (this should in fact never happen as this would be a relation */
  /* without members) */
  if(xmlTextReaderIsEmptyElement(reader))
    return relation;

  /* parse tags/member if present */
  int depth = xmlTextReaderDepth(reader);

  /* scan all elements on same level or its children */
  tag_t **tag = &OSM_TAG(relation);
  member_t **member = &relation->member;
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const char *subname = (const char*)xmlTextReaderConstName(reader);
      if(strcmp(subname, "member") == 0) {
	*member = process_member(reader, osm);
	if(*member) member = &(*member)->next;
      } else if(strcmp(subname, "tag") == 0) {
	*tag = process_tag(reader);
	if(*tag) tag = &(*tag)->next;
      } else
	skip_element(reader);
    }
    ret = xmlTextReaderRead(reader);
  }

  return relation;
}

static osm_t *process_osm(xmlTextReaderPtr reader) {
  /* alloc osm structure */
  osm_t *osm = g_new0(osm_t, 1);
  osm->node_hash = g_new0(hash_table_t, 1);
  osm->way_hash = g_new0(hash_table_t, 1);

  node_t **node = &osm->node;
  way_t **way = &osm->way;
  relation_t **relation = &osm->relation;

  /* no attributes of interest */

  const xmlChar *name = xmlTextReaderConstName(reader);
  g_assert(name);

  /* read next node */
  int num_elems = 0;

  /* the objects come in exactly this order, so some parsing time can be
   * saved as it is clear that e.g. no node can show up if the first way
   * was seen. */
  enum blocks {
    BLOCK_OSM = 0,
    BLOCK_BOUNDS,
    BLOCK_NODES,
    BLOCK_WAYS,
    BLOCK_RELATIONS
  };
  enum blocks block = BLOCK_OSM;

  const int tick_every = 50; // Balance responsive appearance with performance.
  int ret = xmlTextReaderRead(reader);
  while(ret == 1) {

    switch(xmlTextReaderNodeType(reader)) {
    case XML_READER_TYPE_ELEMENT:

      g_assert_cmpint(xmlTextReaderDepth(reader), ==, 1);
      const char *name = (const char*)xmlTextReaderConstName(reader);
      if(block <= BLOCK_BOUNDS && strcmp(name, "bounds") == 0) {
        if(process_bounds(reader, &osm->rbounds))
          osm->bounds = &osm->rbounds;
	block = BLOCK_BOUNDS;
      } else if(block <= BLOCK_NODES && strcmp(name, "node") == 0) {
	*node = process_node(reader, osm);
	if(*node) node = &(*node)->next;
	block = BLOCK_NODES;
      } else if(block <= BLOCK_WAYS && strcmp(name, "way") == 0) {
	*way = process_way(reader, osm);
	if(*way) way = &(*way)->next;
	block = BLOCK_WAYS;
      } else if(block <= BLOCK_RELATIONS && strcmp(name, "relation") == 0) {
	*relation = process_relation(reader, osm);
	if(*relation) relation = &(*relation)->next;
	block = BLOCK_RELATIONS;
      } else {
	printf("something unknown found: %s\n", name);
	g_assert_not_reached();
	skip_element(reader);
      }
      break;

    case XML_READER_TYPE_END_ELEMENT:
      /* end element must be for the current element */
      g_assert_cmpint(xmlTextReaderDepth(reader), ==, 0);
      return osm;
      break;

    default:
      break;
    }
    ret = xmlTextReaderRead(reader);

    if (num_elems++ > tick_every) {
      num_elems = 0;
      banner_busy_tick();
    }
  }

  g_assert_not_reached();
  return NULL;
}

static osm_t *process_file(const char *filename) {
  osm_t *osm = NULL;
  xmlTextReaderPtr reader;
  int ret;

  reader = xmlReaderForFile(filename, NULL, 0);
  if (reader != NULL) {
    ret = xmlTextReaderRead(reader);
    if(ret == 1) {
      const char *name = (const char*)xmlTextReaderConstName(reader);
      if(name && strcmp(name, "osm") == 0)
	osm = process_osm(reader);
    } else
      printf("file empty\n");

    xmlFreeTextReader(reader);
  } else {
    fprintf(stderr, "Unable to open %s\n", filename);
  }
  return osm;
}

/* ----------------------- end of stream parser ------------------- */

#include <sys/time.h>

osm_t *osm_parse(const char *path, const char *filename, icon_t **icon) {

  struct timeval start;
  gettimeofday(&start, NULL);

  // use stream parser
  osm_t *osm = NULL;
  if(filename[0] == '/')
    osm = process_file(filename);
  else {
    char *full = g_strconcat(path, filename, NULL);
    osm = process_file(full);
    g_free(full);
  }

  struct timeval end;
  gettimeofday(&end, NULL);

  osm->icons = icon;

  printf("total parse time: %ldms\n",
	 (end.tv_usec - start.tv_usec)/1000 +
	 (end.tv_sec - start.tv_sec)*1000);

  return osm;
}

gboolean osm_sanity_check(GtkWidget *parent, const osm_t *osm) {
  if(!osm->bounds) {
    errorf(parent, _("Invalid data in OSM file:\n"
		     "Boundary box missing!"));
    return FALSE;
  }
  if(!osm->node) {
    errorf(parent, _("Invalid data in OSM file:\n"
		     "No drawable content found!"));
    return FALSE;
  }
  return TRUE;
}

/* ------------------------- misc access functions -------------- */

tag_t *osm_tag_find(tag_t* tag, const char* key) {
  if(!key) return NULL;

  while(tag) {
    if(strcasecmp(tag->key, key) == 0)
      return tag;

    tag = tag->next;
  }

  return NULL;
}

const char *osm_tag_get_by_key(const tag_t* tag, const char* key) {
  const tag_t *t = osm_tag_find((tag_t*)tag, key);

  if (t)
    return t->value;

  return NULL;
}

const char *osm_way_get_value(way_t* way, const char* key) {
  return osm_tag_get_by_key(OSM_TAG(way), key);
}

const char *osm_node_get_value(node_t *node, const char *key) {
  return osm_tag_get_by_key(OSM_TAG(node), key);
}

gboolean osm_way_has_value(const way_t *way, const char *str) {
  tag_t *tag = OSM_TAG(way);

  while(tag) {
    if(tag->value && strcasecmp(tag->value, str) == 0)
      return TRUE;

    tag = tag->next;
  }
  return FALSE;
}

gboolean osm_node_has_value(const node_t *node, const char *str) {
  tag_t *tag = OSM_TAG(node);

  while(tag) {
    if(tag->value && strcasecmp(tag->value, str) == 0)
      return TRUE;

    tag = tag->next;
  }
  return FALSE;
}

gboolean osm_node_has_tag(const node_t *node) {
  tag_t *tag = OSM_TAG(node);

  /* created_by tags don't count as real tags */
  if(tag && osm_is_creator_tag(tag))
    tag = tag->next;

  return tag != NULL;
}

/* return true if node is part of way */
gboolean osm_node_in_way(const way_t *way, const node_t *node) {
  const node_chain_t *node_chain = way->node_chain;
  while(node_chain) {
    if(node_chain->node == node)
      return TRUE;

    node_chain = node_chain->next;
  }
  return FALSE;
}

/* return true if node is part of other way than this one */
gboolean osm_node_in_other_way(const osm_t *osm, const way_t *way, const node_t *node) {
  const way_t *it = osm->way;
  for(it = osm->way; it; it = it->next) {
    if(it == way)
      continue;
    if(osm_node_in_way(it, node))
      return TRUE;
  }

  return FALSE;
}

static void osm_generate_tags(const tag_t *tag, xmlNodePtr node) {
  while(tag) {
    /* skip "created_by" tags as they aren't needed anymore with api 0.6 */
    if(!osm_is_creator_tag(tag)) {
      xmlNodePtr tag_node = xmlNewChild(node, NULL, BAD_CAST "tag", NULL);
      xmlNewProp(tag_node, BAD_CAST "k", BAD_CAST tag->key);
      xmlNewProp(tag_node, BAD_CAST "v", BAD_CAST tag->value);
    }
    tag = tag->next;
  }
}

static xmlDocPtr
osm_generate_xml_init(xmlNodePtr *node, const char *node_name)
{
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "osm");
  xmlDocSetRootElement(doc, root_node);

  *node = xmlNewChild(root_node, NULL, BAD_CAST node_name, NULL);

  return doc;
}

static char *
osm_generate_xml_finish(xmlDocPtr doc)
{
  xmlChar *result = NULL;
  int len = 0;

  xmlDocDumpFormatMemoryEnc(doc, &result, &len, "UTF-8", 1);
  xmlFreeDoc(doc);

  return (char*)result;

}

/* build xml representation for a node */
char *osm_generate_xml_node(item_id_t changeset, const node_t *node) {
  char str[32];

  xmlNodePtr xml_node;
  xmlDocPtr doc = osm_generate_xml_init(&xml_node, "node");

  /* new nodes don't have an id, but get one after the upload */
  if(!(OSM_FLAGS(node) & OSM_FLAG_NEW)) {
    snprintf(str, sizeof(str), ITEM_ID_FORMAT, OSM_ID(node));
    xmlNewProp(xml_node, BAD_CAST "id", BAD_CAST str);
  }
  snprintf(str, sizeof(str), ITEM_ID_FORMAT, OSM_VERSION(node));
  xmlNewProp(xml_node, BAD_CAST "version", BAD_CAST str);
  snprintf(str, sizeof(str), "%u", (unsigned)changeset);
  xmlNewProp(xml_node, BAD_CAST "changeset", BAD_CAST str);
  g_ascii_formatd(str, sizeof(str), LL_FORMAT, node->pos.lat);
  xmlNewProp(xml_node, BAD_CAST "lat", BAD_CAST str);
  g_ascii_formatd(str, sizeof(str), LL_FORMAT, node->pos.lon);
  xmlNewProp(xml_node, BAD_CAST "lon", BAD_CAST str);
  osm_generate_tags(OSM_TAG(node), xml_node);

  return osm_generate_xml_finish(doc);
}

/**
 * @brief write the referenced nodes of a way to XML
 * @param way_node the XML node of the way to append to
 * @param way the way to walk
 */
void osm_write_node_chain(xmlNodePtr way_node, const way_t *way) {
  const node_chain_t *node_chain;
  for(node_chain = way->node_chain; node_chain; node_chain = node_chain->next) {
    xmlNodePtr nd_node = xmlNewChild(way_node, NULL, BAD_CAST "nd", NULL);
    gchar str[G_ASCII_DTOSTR_BUF_SIZE];
    g_snprintf(str, sizeof(str), ITEM_ID_FORMAT, OSM_ID(node_chain->node));
    xmlNewProp(nd_node, BAD_CAST "ref", BAD_CAST str);
  }
}

/* build xml representation for a way */
char *osm_generate_xml_way(item_id_t changeset, const way_t *way) {
  char str[32];

  xmlNodePtr xml_node;
  xmlDocPtr doc = osm_generate_xml_init(&xml_node, "way");

  snprintf(str, sizeof(str), ITEM_ID_FORMAT, OSM_ID(way));
  xmlNewProp(xml_node, BAD_CAST "id", BAD_CAST str);
  snprintf(str, sizeof(str), ITEM_ID_FORMAT, OSM_VERSION(way));
  xmlNewProp(xml_node, BAD_CAST "version", BAD_CAST str);
  snprintf(str, sizeof(str), "%u", (unsigned)changeset);
  xmlNewProp(xml_node, BAD_CAST "changeset", BAD_CAST str);

  osm_write_node_chain(xml_node, way);
  osm_generate_tags(OSM_TAG(way), xml_node);

  return osm_generate_xml_finish(doc);
}

/* build xml representation for a relation */
char *osm_generate_xml_relation(item_id_t changeset,
				const relation_t *relation) {
  char str[32];

  xmlNodePtr xml_node;
  xmlDocPtr doc = osm_generate_xml_init(&xml_node, "relation");

  snprintf(str, sizeof(str), ITEM_ID_FORMAT, OSM_ID(relation));
  xmlNewProp(xml_node, BAD_CAST "id", BAD_CAST str);
  snprintf(str, sizeof(str), ITEM_ID_FORMAT, OSM_VERSION(relation));
  xmlNewProp(xml_node, BAD_CAST "version", BAD_CAST str);
  snprintf(str, sizeof(str), "%u", (unsigned)changeset);
  xmlNewProp(xml_node, BAD_CAST "changeset", BAD_CAST str);

  member_t *member = relation->member;
  while(member) {
    xmlNodePtr m_node = xmlNewChild(xml_node,NULL,BAD_CAST "member", NULL);
    gchar str[G_ASCII_DTOSTR_BUF_SIZE];
    g_snprintf(str, sizeof(str), ITEM_ID_FORMAT, OBJECT_ID(member->object));

    switch(member->object.type) {
    case NODE:
      xmlNewProp(m_node, BAD_CAST "type", BAD_CAST "node");
      break;

    case WAY:
      xmlNewProp(m_node, BAD_CAST "type", BAD_CAST "way");
      break;

    case RELATION:
      xmlNewProp(m_node, BAD_CAST "type", BAD_CAST "relation");
      break;

    default:
      break;
    }

    xmlNewProp(m_node, BAD_CAST "ref", BAD_CAST str);

    if(member->role)
      xmlNewProp(m_node, BAD_CAST "role", BAD_CAST member->role);
    else
      xmlNewProp(m_node, BAD_CAST "role", BAD_CAST "");

    member = member->next;
  }
  osm_generate_tags(OSM_TAG(relation), xml_node);

  return osm_generate_xml_finish(doc);
}

/* build xml representation for a changeset */
char *osm_generate_xml_changeset(char *comment) {
  xmlChar *result = NULL;
  int len = 0;

  /* tags for this changeset */
  tag_t tag_comment = {
    .key = "comment", .value = comment, .next = NULL };
  const tag_t tag_creator = {
    .key = "created_by", .value = PACKAGE " v" VERSION, .next = &tag_comment };

  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "osm");
  xmlDocSetRootElement(doc, root_node);

  xmlNodePtr cs_node = xmlNewChild(root_node, NULL, BAD_CAST "changeset", NULL);
  osm_generate_tags(&tag_creator, cs_node);

  xmlDocDumpFormatMemoryEnc(doc, &result, &len, "UTF-8", 1);
  xmlFreeDoc(doc);

  return (char*)result;
}


/* the following three functions are eating much CPU power */
/* as they search the objects lists. Hashing is supposed to help */
node_t *osm_get_node_by_id(osm_t *osm, item_id_t id) {
  if(id > 0 && osm->node_hash) {
    // use hash table if present
    hash_item_t *item = osm->node_hash->hash[ID2HASH(id)];
    while(item) {
      if(OSM_ID(item->data.node) == id)
	return item->data.node;

      item = item->next;
    }
  }

  /* use linear search if no hash tables are present or search in hash table failed */
  node_t *node = osm->node;
  while(node) {
    if(OSM_ID(node) == id)
      return node;

    node = node->next;
  }

  return NULL;
}

way_t *osm_get_way_by_id(osm_t *osm, item_id_t id) {
  if(id > 0 && osm->way_hash) {
    // use hash table if present
    hash_item_t *item = osm->way_hash->hash[ID2HASH(id)];
    while(item) {
      if(OSM_ID(item->data.way) == id)
	return item->data.way;

      item = item->next;
    }
  }

  /* use linear search if no hash tables are present or search on hash table failed */
  way_t *way = osm->way;
  while(way) {
    if(OSM_ID(way) == id)
      return way;

    way = way->next;
  }

  return NULL;
}

relation_t *osm_get_relation_by_id(osm_t *osm, item_id_t id) {
  // use linear search
  relation_t *relation = osm->relation;
  while(relation) {
    if(OSM_ID(relation) == id)
      return relation;

    relation = relation->next;
  }

  return NULL;
}

/* ---------- edit functions ------------- */

item_id_t osm_new_way_id(osm_t *osm) {
  item_id_t id = -1;

  while(TRUE) {
    gboolean found = FALSE;
    const way_t *way = osm->way;
    while(way && !found) {
      if(OSM_ID(way) == id)
	found = TRUE;

      way = way->next;
    }

    /* no such id so far -> use it */
    if(!found) return id;

    id--;
  }
}

item_id_t osm_new_node_id(osm_t *osm) {
  item_id_t id = -1;

  while(TRUE) {
    gboolean found = FALSE;
    const node_t *node = osm->node;
    while(node && !found) {
      if(OSM_ID(node) == id)
	found = TRUE;

      node = node->next;
    }

    /* no such id so far -> use it */
    if(!found) return id;

    id--;
  }
}

item_id_t osm_new_relation_id(osm_t *osm) {
  item_id_t id = -1;

  while(TRUE) {
    gboolean found = FALSE;
    const relation_t *relation = osm->relation;
    while(relation && !found) {
      if(OSM_ID(relation) == id)
	found = TRUE;

      relation = relation->next;
    }

    /* no such id so far -> use it */
    if(!found) return id;

    id--;
  }
}

node_t *osm_node_new(osm_t *osm, gint x, gint y) {
  printf("Creating new node\n");

  node_t *node = g_new0(node_t, 1);
  OSM_VERSION(node) = 1;
  node->lpos.x = x;
  node->lpos.y = y;
  OSM_VISIBLE(node) = TRUE;
  OSM_TIME(node) = time(NULL);

  /* convert screen position back to ll */
  lpos2pos(osm->bounds, &node->lpos, &node->pos);

  printf("  new at %d %d (%f %f)\n",
	 node->lpos.x, node->lpos.y, node->pos.lat, node->pos.lon);

  return node;
}

node_t *osm_node_new_pos(osm_t *osm, const pos_t *pos) {
  printf("Creating new node from lat/lon\n");

  node_t *node = g_new0(node_t, 1);
  OSM_VERSION(node) = 1;
  node->pos = *pos;
  OSM_VISIBLE(node) = TRUE;
  OSM_TIME(node) = time(NULL);

  /* convert ll position to screen */
  pos2lpos(osm->bounds, &node->pos, &node->lpos);

  printf("  new at %d %d (%f %f)\n",
	 node->lpos.x, node->lpos.y, node->pos.lat, node->pos.lon);

  return node;
}


void osm_node_attach(osm_t *osm, node_t *node) {
  printf("Attaching node\n");

  OSM_ID(node) = osm_new_node_id(osm);
  OSM_FLAGS(node) = OSM_FLAG_NEW;

  /* attach to end of node list */
  node_t **lnode = &osm->node;
  while(*lnode) lnode = &(*lnode)->next;
  *lnode = node;
}

void osm_node_restore(osm_t *osm, node_t *node) {
  printf("Restoring node\n");

  /* attach to end of node list */
  node_t **lnode = &osm->node;
  while(*lnode) lnode = &(*lnode)->next;
  *lnode = node;
}

way_t *osm_way_new(void) {
  printf("Creating new way\n");

  way_t *way = g_new0(way_t, 1);
  OSM_VERSION(way) = 1;
  OSM_VISIBLE(way) = TRUE;
  OSM_FLAGS(way) = OSM_FLAG_NEW;
  OSM_TIME(way) = time(NULL);

  return way;
}

void osm_way_attach(osm_t *osm, way_t *way) {
  printf("Attaching way\n");

  OSM_ID(way) = osm_new_way_id(osm);
  OSM_FLAGS(way) = OSM_FLAG_NEW;

  /* attach to end of way list */
  way_t **lway = &osm->way;
  while(*lway) lway = &(*lway)->next;
  *lway = way;
}

void osm_way_restore(osm_t *osm, way_t *way, item_id_chain_t *id_chain) {
  printf("Restoring way\n");

  /* attach to end of node list */
  way_t **lway = &osm->way;
  while(*lway) lway = &(*lway)->next;
  *lway = way;

  /* restore node memberships by converting ids into real pointers */
  g_assert(!way->node_chain);
  node_chain_t **node_chain = &(way->node_chain);
  while(id_chain) {
    item_id_chain_t *id_next = id_chain->next;
    printf("Node " ITEM_ID_FORMAT " is member\n", id_chain->id);

    *node_chain = g_new0(node_chain_t, 1);
    (*node_chain)->node = osm_get_node_by_id(osm, id_chain->id);
    (*node_chain)->node->ways++;

    printf("   -> %p\n", (*node_chain)->node);

    g_free(id_chain);
    id_chain = id_next;
    node_chain = &(*node_chain)->next;
  }

  printf("done\n");
}

/* returns pointer to chain of ways affected by this deletion */
way_chain_t *osm_node_delete(osm_t *osm,
			     node_t *node, gboolean permanently,
			     gboolean affect_ways) {
  way_chain_t *way_chain = NULL;

  /* new nodes aren't stored on the server and are just deleted permanently */
  if(OSM_FLAGS(node) & OSM_FLAG_NEW) {
    printf("About to delete NEW node #" ITEM_ID_FORMAT
	   " -> force permanent delete\n", OSM_ID(node));
    permanently = TRUE;
  }

  /* first remove node from all ways using it */
  way_t *way = osm->way;
  while(way) {
    node_chain_t **chain = &(way->node_chain);
    gboolean modified = FALSE;
    while(*chain) {
      /* remove node from chain */
      if(node == (*chain)->node) {
	modified = TRUE;
	if(affect_ways) {
	  node_chain_t *next = (*chain)->next;
	  g_free(*chain);
	  *chain = next;
	} else
          /* only record that there has been a change */
          break;
      } else
	chain = &((*chain)->next);
    }

    if(modified) {
      OSM_FLAGS(way) |= OSM_FLAG_DIRTY;

      /* and add the way to the list of affected ways */
      way_chain = g_slist_prepend(way_chain, way);
    }

    way = way->next;
  }

  /* remove that nodes map representations */
  if(node->map_item_chain)
    map_item_chain_destroy(&node->map_item_chain);

  if(!permanently) {
    printf("mark node #" ITEM_ID_FORMAT " as deleted\n", OSM_ID(node));
    OSM_FLAGS(node) |= OSM_FLAG_DELETED;
  } else {
    printf("permanently delete node #" ITEM_ID_FORMAT "\n", OSM_ID(node));

    /* remove it from the chain */
    node_t **cnode = &osm->node;
    int found = 0;

    while(*cnode) {
      if(*cnode == node) {
	found++;
	*cnode = (*cnode)->next;

	osm_node_free(osm, node);
      } else
	cnode = &((*cnode)->next);
    }
    g_assert_cmpint(found, ==, 1);
  }

  return way_chain;
}

/**
 * @brief check if a way has at least the given length
 * @param way the way to check
 * @param len the minimum length
 *
 * len must not be 0.
 */
gboolean osm_way_min_length(const way_t *way, guint len) {
  const node_chain_t *chain = way->node_chain;
  for (chain = way->node_chain; chain; chain = chain->next) {
    if (--len == 0)
      return TRUE;
  }
  return FALSE;
}

guint osm_way_number_of_nodes(const way_t *way) {
  guint nodes = 0;
  const node_chain_t *chain = way->node_chain;
  while(chain) {
    nodes++;
    chain = chain->next;
  }
  return nodes;
}

/* return all relations a node is in */
static GSList *osm_node_to_relation(osm_t *osm, const node_t *node,
				       gboolean via_way) {
  GSList *rel_chain = NULL;

  relation_t *relation = osm->relation;
  for(; relation; relation = relation->next) {
    gboolean is_member = FALSE;

    member_t *member = relation->member;
    for(; member && !is_member; member = member->next) {
      switch(member->object.type) {
      case NODE:
	/* nodes are checked directly */
	if(member->object.node == node)
	  is_member = TRUE;
	break;

      case WAY:
	if(via_way)
	  /* ways have to be checked for the nodes they consist of */
	  is_member = osm_node_in_way(member->object.way, node);
	break;

      default:
	break;
      }
    }

    /* node is a member of this relation, so move it to the member chain */
    if(is_member)
      rel_chain = g_slist_prepend(rel_chain, relation);
  }

  return rel_chain;
}

/* return all relations a way is in */
GSList *osm_way_to_relation(osm_t *osm, const way_t *way) {
  GSList *rel_chain = NULL;

  relation_t *relation = osm->relation;
  for(; relation; relation = relation->next) {
    gboolean is_member = FALSE;

    member_t *member = relation->member;
    for(; member && !is_member; member = member->next) {
      switch(member->object.type) {
      case WAY:
	/* ways can be check directly */
	if(member->object.way == way)
	  is_member = TRUE;
        break;

      default:
	break;
      }
    }

    /* way is a member of this relation, so move it to the member chain */
    if(is_member)
      rel_chain = g_slist_prepend(rel_chain, relation);
  }

  return rel_chain;
}

/* return all relations a relation is in */
static GSList *osm_relation_to_relation(osm_t *osm, const relation_t *rel) {
  GSList *rel_chain = NULL;

  relation_t *relation = osm->relation;
  for(; relation; relation = relation->next) {
    gboolean is_member = FALSE;

    member_t *member = relation->member;
    for(; member && !is_member; member = member->next) {
      switch(member->object.type) {
      case RELATION:
	/* relations can be check directly */
	if(member->object.relation == rel)
	  is_member = TRUE;
        break;

      default:
	break;
      }
    }

    /* way is a member of this relation, so move it to the member chain */
    if(is_member)
      rel_chain = g_slist_prepend(rel_chain, relation);
  }

  return rel_chain;
}

/* return all relations an object is in */
GSList *osm_object_to_relation(osm_t *osm, const object_t *object) {
  switch(object->type) {
  case NODE:
    return osm_node_to_relation(osm, object->node, FALSE);

  case WAY:
    return osm_way_to_relation(osm, object->way);

  case RELATION:
    return osm_relation_to_relation(osm, object->relation);

  default:
    return NULL;
  }
}

/* return all ways a node is in */
way_chain_t *osm_node_to_way(const osm_t *osm, const node_t *node) {
  way_chain_t *chain = NULL;

  way_t *way = osm->way;
  for(; way; way = way->next) {
    /* node is a member of this relation, so move it to the member chain */
    if(osm_node_in_way(way, node))
      chain = g_slist_prepend(chain, way);
  }

  return chain;
}

gboolean osm_position_within_bounds(const osm_t *osm, gint x, gint y) {
  if((x < osm->bounds->min.x) || (x > osm->bounds->max.x)) return FALSE;
  if((y < osm->bounds->min.y) || (y > osm->bounds->max.y)) return FALSE;
  return TRUE;
}

gboolean osm_position_within_bounds_ll(const pos_t *ll_min, const pos_t *ll_max, const pos_t *pos) {
  if((pos->lat < ll_min->lat) || (pos->lat > ll_max->lat)) return FALSE;
  if((pos->lon < ll_min->lon) || (pos->lon > ll_max->lon)) return FALSE;
  return TRUE;
}

/* remove the given node from all relations. used if the node is to */
/* be deleted */
void osm_node_remove_from_relation(osm_t *osm, node_t *node) {
  relation_t *relation = osm->relation;
  printf("removing node #" ITEM_ID_FORMAT " from all relations:\n", OSM_ID(node));

  for(; relation; relation = relation->next) {
    member_t **member = &relation->member;
    while(*member) {
      if(((*member)->object.type == NODE) &&
	 ((*member)->object.node == node)) {

	printf("  from relation #" ITEM_ID_FORMAT "\n", OSM_ID(relation));

	member_t *cur = *member;
	*member = (*member)->next;
	osm_member_free(cur);

	OSM_FLAGS(relation) |= OSM_FLAG_DIRTY;
      } else
	member = &(*member)->next;
    }
  }
}

/* remove the given way from all relations */
void osm_way_remove_from_relation(osm_t *osm, way_t *way) {
  relation_t *relation = osm->relation;
  printf("removing way #" ITEM_ID_FORMAT " from all relations:\n", OSM_ID(way));

  for(; relation; relation = relation->next) {
    member_t **member = &relation->member;
    while(*member) {
      if(((*member)->object.type == WAY) &&
	 ((*member)->object.way == way)) {

	printf("  from relation #" ITEM_ID_FORMAT "\n", OSM_ID(relation));

	member_t *cur = *member;
	*member = (*member)->next;
	osm_member_free(cur);

	OSM_FLAGS(relation) |= OSM_FLAG_DIRTY;
      } else
	member = &(*member)->next;
    }
  }
}

relation_t *osm_relation_new(void) {
  printf("Creating new relation\n");

  relation_t *relation = g_new0(relation_t, 1);
  OSM_VERSION(relation) = 1;
  OSM_VISIBLE(relation) = TRUE;
  OSM_FLAGS(relation) = OSM_FLAG_NEW;
  OSM_TIME(relation) = time(NULL);

  return relation;
}

void osm_relation_attach(osm_t *osm, relation_t *relation) {
  printf("Attaching relation\n");

  OSM_ID(relation) = osm_new_relation_id(osm);
  OSM_FLAGS(relation) = OSM_FLAG_NEW;

  /* attach to end of relation list */
  relation_t **lrelation = &osm->relation;
  while(*lrelation) lrelation = &(*lrelation)->next;
  *lrelation = relation;
}


void osm_way_delete(osm_t *osm, way_t *way, gboolean permanently) {

  /* new ways aren't stored on the server and are just deleted permanently */
  if(OSM_FLAGS(way) & OSM_FLAG_NEW) {
    printf("About to delete NEW way #" ITEM_ID_FORMAT
	   " -> force permanent delete\n", OSM_ID(way));
    permanently = TRUE;
  }

  /* delete all nodes that aren't in other use now */
  node_chain_t **chain = &way->node_chain;
  while(*chain) {
    node_t *node = (*chain)->node;
    node->ways--;
    printf("checking node #" ITEM_ID_FORMAT " (still used by %d)\n",
	   OSM_ID(node), node->ways);

    /* this node must only be part of this way */
    if(!node->ways) {
      /* delete this node, but don't let this actually affect the */
      /* associated ways as the only such way is the one we are currently */
      /* deleting */
      way_chain_t *way_chain = osm_node_delete(osm, node, FALSE, FALSE);
      g_assert(way_chain);
      g_slist_free(way_chain);
    }

    node_chain_t *cur = (*chain);
    *chain = cur->next;
    g_free(cur);
  }

  way->node_chain = NULL;

  if(!permanently) {
    printf("mark way #" ITEM_ID_FORMAT " as deleted\n", OSM_ID(way));
    OSM_FLAGS(way) |= OSM_FLAG_DELETED;
  } else {
    printf("permanently delete way #" ITEM_ID_FORMAT "\n", OSM_ID(way));

    /* remove it from the chain */
    way_t **cway = &osm->way;
    int found = 0;

    while(*cway) {
      if(*cway == way) {
	found++;
	*cway = (*cway)->next;

	g_assert(osm);
	osm_way_free(osm, way);
      } else
	cway = &((*cway)->next);
    }
    g_assert_cmpint(found, ==, 1);
  }
}

void osm_relation_delete(osm_t *osm, relation_t *relation,
			 gboolean permanently) {

  /* new relations aren't stored on the server and are just */
  /* deleted permanently */
  if(OSM_FLAGS(relation) & OSM_FLAG_NEW) {
    printf("About to delete NEW relation #" ITEM_ID_FORMAT
	   " -> force permanent delete\n", OSM_ID(relation));
    permanently = TRUE;
  }

  /* the deletion of a relation doesn't affect the members as they */
  /* don't have any reference to the relation they are part of */

  if(!permanently) {
    printf("mark relation #" ITEM_ID_FORMAT " as deleted\n", OSM_ID(relation));
    OSM_FLAGS(relation) |= OSM_FLAG_DELETED;
  } else {
    printf("permanently delete relation #" ITEM_ID_FORMAT "\n",
	   OSM_ID(relation));

    /* remove it from the chain */
    relation_t **crelation = &osm->relation;
    int found = 0;

    while(*crelation) {
      if(*crelation == relation) {
	found++;
	*crelation = (*crelation)->next;

	osm_relation_free(relation);
      } else
	crelation = &((*crelation)->next);
    }
    g_assert_cmpint(found, ==, 1);
  }
}

void osm_way_reverse(way_t *way) {
  node_chain_t *newn = NULL;

  /* walk old chain first to last */
  node_chain_t *old = way->node_chain;
  while(old) {
    node_chain_t *next = old->next;

    /* and prepend each node to the new chain */
    old->next = newn;
    newn = old;

    old = next;
  }

  way->node_chain = newn;
}

static const char *DS_ONEWAY_FWD = "yes";
static const char *DS_ONEWAY_REV = "-1";

static const struct {
	const char *orig;
	const char *reverse;
} rtable[] = {
	{ .orig = ":left",     .reverse = ":right" },
	{ .orig = ":right",    .reverse = ":left" },
	{ .orig = ":forward",  .reverse = ":backward" },
	{ .orig = ":backward", .reverse = ":forward" },
	{ .orig = NULL,        .reverse = NULL }
};

/* Reverse direction-sensitive tags like "oneway". Marks the way as dirty if
 * anything is changed, and returns the number of flipped tags. */

guint
osm_way_reverse_direction_sensitive_tags (way_t *way) {
  tag_t *tag = OSM_TAG(way);
  guint n_tags_altered = 0;
  for (; tag; tag = tag->next) {
    char *lc_key = g_ascii_strdown(tag->key, -1);

    if (strcmp(lc_key, "oneway") == 0) {
      char *lc_value = g_ascii_strdown(tag->value, -1);
      // oneway={yes/true/1/-1} is unusual.
      // Favour "yes" and "-1".
      if ((strcmp(lc_value, DS_ONEWAY_FWD) == 0) ||
          (strcmp(lc_value, "true") == 0) ||
          (strcmp(lc_value, "1") == 0)) {
        osm_tag_update_value(tag, DS_ONEWAY_REV);
        n_tags_altered++;
      }
      else if (strcmp(lc_value, DS_ONEWAY_REV) == 0) {
        osm_tag_update_value(tag, DS_ONEWAY_FWD);
        n_tags_altered++;
      }
      else {
        printf("warning: unknown tag: %s=%s\n", tag->key, tag->value);
      }
      g_free(lc_value);
    } else if (strcmp(lc_key, "sidewalk") == 0) {
      if (strcasecmp(tag->value, "right") == 0)
        osm_tag_update_value(tag, "left");
      else if (strcasecmp(tag->value, "left") == 0)
        osm_tag_update_value(tag, "right");
    } else {
      // suffixes
      unsigned int i;
      for (i = 0; rtable[i].orig != NULL; i++) {
        if (g_str_has_suffix(lc_key, rtable[i].orig)) {
          /* length of key that will persist */
          size_t plen = strlen(tag->key) - strlen(rtable[i].orig);
          /* add length of new suffix */
          tag->key = g_realloc(tag->key, plen + 1 + strlen(rtable[i].reverse));
          char *lastcolon = tag->key + plen;
          g_assert(*lastcolon == ':');
          /* replace suffix */
          strcpy(lastcolon, rtable[i].reverse);
          n_tags_altered++;
          break;
        }
      }
    }

    g_free(lc_key);
  }
  if (n_tags_altered > 0) {
    OSM_FLAGS(way) |= OSM_FLAG_DIRTY;
  }
  return n_tags_altered;
}

/* Reverse a way's role within relations where the role is direction-sensitive.
 * Returns the number of roles flipped, and marks any relations changed as
 * dirty. */

static const char *DS_ROUTE_FORWARD = "forward";
static const char *DS_ROUTE_REVERSE = "reverse";

struct reverse_roles {
  way_t *way;
  guint n_roles_flipped;
};

void reverse_roles_operator(relation_t* relation, struct reverse_roles *context)
{
  const char *type = osm_tag_get_by_key(OSM_TAG(relation), "type");

  // Route relations; http://wiki.openstreetmap.org/wiki/Relation:route
  if (!type || strcasecmp(type, "route") != 0)
    return;

  // First find the member corresponding to our way:
  member_t *member = relation->member;
  for (; member != NULL; member = member->next) {
    if (member->object.type == WAY) {
      if (member->object.way == context->way)
        break;
    }
    if (member->object.type == WAY_ID) {
      if (member->object.id == OSM_ID(context->way))
        break;
    }
  }
  g_assert(member);  // osm_way_to_relation() broken?

  // Then flip its role if it's one of the direction-sensitive ones
  if (member->role == NULL) {
    printf("null role in route relation -> ignore\n");
  } else if (strcasecmp(member->role, DS_ROUTE_FORWARD) == 0) {
    g_free(member->role);
    member->role = g_strdup(DS_ROUTE_REVERSE);
    OSM_FLAGS(relation) |= OSM_FLAG_DIRTY;
    ++(context->n_roles_flipped);
  } else if (strcasecmp(member->role, DS_ROUTE_REVERSE) == 0) {
    g_free(member->role);
    member->role = g_strdup(DS_ROUTE_FORWARD);
    OSM_FLAGS(relation) |= OSM_FLAG_DIRTY;
    ++(context->n_roles_flipped);
  }

  // TODO: what about numbered stops? Guess we ignore them; there's no
  // consensus about whether they should be placed on the way or to one side
  // of it.
}

guint
osm_way_reverse_direction_sensitive_roles(osm_t *osm, way_t *way) {
  GSList *rel_chain = osm_way_to_relation(osm, way);
  struct reverse_roles context = {
    .way = way,
    .n_roles_flipped = 0
  };

  g_slist_foreach(rel_chain, (GFunc)reverse_roles_operator, &context);
  g_slist_free(rel_chain);
  return context.n_roles_flipped;
}

const node_t *osm_way_get_first_node(const way_t *way) {
  const node_chain_t *chain = way->node_chain;
  if(!chain) return NULL;
  return chain->node;
}

const node_t *osm_way_get_last_node(const way_t *way) {
  const node_chain_t *chain = way->node_chain;

  if(!chain) return NULL;

  while(chain->next)
    chain = chain->next;

  return chain->node;
}

gboolean osm_way_is_closed(const way_t *way) {
  /* check last first, this already handles the case of
   * an empty way */
  const node_t *last = osm_way_get_last_node(way);
  if(last == NULL)
    return FALSE;
  return (last == way->node_chain->node);
}

void osm_way_rotate(way_t *way, gint offset) {
  if(!offset) return;

  /* needs at least two nodes to work properly */
  g_assert(way->node_chain);
  g_assert(way->node_chain->next);

  while(offset--) {
    node_chain_t *chain = way->node_chain;
    chain->node->ways--; // reduce way count of old start/end node

    /* move all nodes ahead one chain element ... */
    while(chain->next) {
      chain->node = chain->next->node;
      chain = chain->next;
    }

    /* ... and make last one same as first one */
    chain->node = way->node_chain->node;
    chain->node->ways++; // increase way count of new start/end node
  }
}

tag_t *osm_tags_copy(const tag_t *src_tag) {
  tag_t *new_tags = NULL;
  tag_t **dst_tag = &new_tags;

  for(; src_tag; src_tag = src_tag->next) {
    if(!osm_is_creator_tag(src_tag)) {
      *dst_tag = g_new0(tag_t, 1);
      (*dst_tag)->key = g_strdup(src_tag->key);
      (*dst_tag)->value = g_strdup(src_tag->value);
      dst_tag = &(*dst_tag)->next;
    }
  }

  return new_tags;
}

/* return plain text of type */
const char *osm_object_type_string(const object_t *object) {
  const struct { type_t type; const char *name; } types[] = {
    { ILLEGAL,     "illegal" },
    { NODE,        "node" },
    { WAY,         "way/area" },
    { RELATION,    "relation" },
    { NODE_ID,     "node id" },
    { WAY_ID,      "way/area id" },
    { RELATION_ID, "relation id" },
    { 0, NULL }
  };

  int i;
  for(i=0;types[i].name;i++)
    if(object->type == types[i].type)
      return types[i].name;

  return NULL;
}

/* try to get an as "speaking" description of the object as possible */
char *osm_object_get_name(const object_t *object) {
  char *ret = NULL;
  const tag_t *tags = osm_object_get_tags(object);

  /* worst case: we have no tags at all. return techincal info then */
  if(!tags)
    return g_strconcat("unspecified ", osm_object_type_string(object), NULL);

  /* try to figure out _what_ this is */

  const char *name = osm_tag_get_by_key(tags, "name");
  if(!name) name = osm_tag_get_by_key(tags, "ref");
  if(!name) name = osm_tag_get_by_key(tags, "note");
  if(!name) name = osm_tag_get_by_key(tags, "fix" "me");
  if(!name) name = osm_tag_get_by_key(tags, "sport");

  /* search for some kind of "type" */
  const char *type = osm_tag_get_by_key(tags, "amenity");
  gchar *gtype = NULL;
  if(!type) type = osm_tag_get_by_key(tags, "place");
  if(!type) type = osm_tag_get_by_key(tags, "historic");
  if(!type) type = osm_tag_get_by_key(tags, "leisure");
  if(!type) type = osm_tag_get_by_key(tags, "tourism");
  if(!type) type = osm_tag_get_by_key(tags, "landuse");
  if(!type) type = osm_tag_get_by_key(tags, "waterway");
  if(!type) type = osm_tag_get_by_key(tags, "railway");
  if(!type) type = osm_tag_get_by_key(tags, "natural");
  if(!type && osm_tag_get_by_key(tags, "building")) {
    const char *street = osm_tag_get_by_key(tags, "addr:street");
    const char *hn = osm_tag_get_by_key(tags, "addr:housenumber");
    type = "building";

    if(street && hn) {
      if(hn)
        type = gtype = g_strjoin(" ", "building", street, hn, NULL);
    } else if(hn) {
      type = gtype = g_strconcat("building housenumber ", hn, NULL);
    } else if(!name)
      name = osm_tag_get_by_key(tags, "addr:housename");
  }
  if(!type) type = osm_tag_get_by_key(tags, "emergency");

  /* highways are a little bit difficult */
  const char *highway = osm_tag_get_by_key(tags, "highway");
  if(highway && !gtype) {
    if((!strcmp(highway, "primary")) ||
       (!strcmp(highway, "secondary")) ||
       (!strcmp(highway, "tertiary")) ||
       (!strcmp(highway, "unclassified")) ||
       (!strcmp(highway, "residential")) ||
       (!strcmp(highway, "service"))) {
      type = gtype = g_strconcat(highway, " road", NULL);
    }

    else if(!strcmp(highway, "pedestrian")) {
      type = "pedestrian way/area";
    }

    else if(!strcmp(highway, "construction")) {
      type = "road/street under construction";
    }

    else
      type = highway;
  }

  if(type && name)
    ret = g_strconcat(type, ": \"", name, "\"", NULL);
  else if(type && !name) {
    if(gtype) {
      ret = gtype;
      gtype = NULL;
    } else
      ret = g_strdup(type);
  } else if(name && !type)
    ret = g_strconcat(
	  osm_object_type_string(object), ": \"", name, "\"", NULL);
  else
    ret = g_strconcat("unspecified ", osm_object_type_string(object), NULL);

  g_free(gtype);

  /* remove underscores from string and replace them by spaces as this is */
  /* usually nicer */
  char *p = strchr(ret, '_');
  while(p) {
    *p = ' ';
    p = strchr(p + 1, '_');
  }

  return ret;
}

char *osm_object_string(const object_t *object) {
  const char *type_str = osm_object_type_string(object);

  if(!object)
    return g_strconcat(type_str, " #<invalid>", NULL);

  switch(object->type) {
  case ILLEGAL:
    return g_strconcat(type_str, " #<unspec>", NULL);
    break;
  case NODE:
  case WAY:
  case RELATION:
    g_assert(object->ptr);
    return g_strdup_printf("%s #" ITEM_ID_FORMAT, type_str,
			   OBJECT_ID(*object));
    break;
    break;
  case NODE_ID:
  case WAY_ID:
  case RELATION_ID:
    return g_strdup_printf("%s #" ITEM_ID_FORMAT, type_str, object->id);
    break;
  }
  return NULL;
}

gchar *osm_object_id_string(const object_t *object) {
  if(!object) return NULL;

  switch(object->type) {
  case ILLEGAL:
    return NULL;
    break;
  case NODE:
  case WAY:
  case RELATION:
    return g_strdup_printf("#" ITEM_ID_FORMAT, OBJECT_ID(*object));
    break;
  case NODE_ID:
  case WAY_ID:
  case RELATION_ID:
    return g_strdup_printf("#" ITEM_ID_FORMAT, object->id);
    break;
  }
  return NULL;
}


gboolean osm_object_is_real(const object_t *object) {
  return((object->type == NODE) ||
	 (object->type == WAY)  ||
	 (object->type == RELATION));
}

const tag_t *osm_object_get_tags(const object_t *object) {
  if(!object) return NULL;
  if(!osm_object_is_real(object)) return NULL;
  return OBJECT_TAG(*object);
}


item_id_t osm_object_get_id(const object_t *object) {
  if(!object) return ID_ILLEGAL;

  if(object->type == ILLEGAL)     return ID_ILLEGAL;
  if(osm_object_is_real(object))  return OBJECT_ID(*object);
  return object->id;
}

guint osm_relation_members_num(const relation_t *relation) {
  guint n, w, r;
  osm_relation_members_num_by_type(relation, &n, &w, &r);
  return n + w + r;
}

void osm_relation_members_num_by_type(const relation_t* relation,
                                      guint* nodes, guint* ways, guint* relations) {
  *nodes = 0;
  *ways = 0;
  *relations = 0;

  const member_t *member;
  for(member = relation->member; member; member = member->next) {
    switch(member->object.type) {
    case NODE:
    case NODE_ID:
      (*nodes)++;
      break;
    case WAY:
    case WAY_ID:
      (*ways)++;
      break;
    case RELATION:
    case RELATION_ID:
      (*relations)++;
      break;
    default:
      g_assert_not_reached();
      break;
    }
  }
}

void osm_object_set_flags(object_t *object, int set, int clr) {
  g_assert(osm_object_is_real(object));
  OBJECT_FLAGS(*object) |=  set;
  OBJECT_FLAGS(*object) &= ~clr;
}

gboolean osm_object_is_same(const object_t *obj1, const object_t *obj2) {
  item_id_t id1 = osm_object_get_id(obj1);
  item_id_t id2 = osm_object_get_id(obj2);

  if(id1 == ID_ILLEGAL) return FALSE;
  if(id2 == ID_ILLEGAL) return FALSE;
  if(obj1->type != obj2->type) return FALSE;

  return(id1 == id2);
}


// vim:et:ts=8:sw=2:sts=2:ai
