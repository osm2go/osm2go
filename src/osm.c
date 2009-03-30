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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define __USE_XOPEN
#include <time.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "appdata.h"
#include "banner.h"

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

/* determine where a node/way/relation read from the osm file */
/* is inserted into the internal database */
// #define OSM_SORT_ID
#define OSM_SORT_LAST
// #define OSM_SORT_FIRST

/* ------------------------- bounds handling --------------------- */

static void osm_bounds_free(bounds_t *bounds) {
  free(bounds);
}

static void osm_bounds_dump(bounds_t *bounds) {
  printf("\nBounds: %f->%f %f->%f\n", 
	 bounds->ll_min.lat, bounds->ll_max.lat, 
	 bounds->ll_min.lon, bounds->ll_max.lon);
}

/* ------------------------- user handling --------------------- */

void osm_users_free(user_t *user) {
  while(user) {
    user_t *next = user->next;

    if(user->name) g_free(user->name);
    g_free(user);

    user = next;
  }
}

void osm_users_dump(user_t *user) {
  printf("\nUser list:\n");
  while(user) {
    printf("Name: %s\n", user->name);
    user = user->next;
  }
}

static user_t *osm_user(osm_t *osm, char *name) {
  if(!name) return NULL;

  /* search through user list */
  user_t **user = &osm->user;
  while(*user && strcasecmp((*user)->name, name) < 0) 
    user = &(*user)->next;

  /* end of list or inexact match? create new user entry! */
  if(!*user || strcasecmp((*user)->name, name)) {
    user_t *new = g_new0(user_t, 1);
    new->name = g_strdup(name);
    new->next = *user;
    *user = new;

    return new;
  }

  return *user;
}

static
time_t convert_iso8601(const char *str) {
  if(!str) return 0;

  tzset();

  struct tm ctime;
  memset(&ctime, 0, sizeof(struct tm));
  strptime(str, "%FT%T%z", &ctime);
 
  return mktime(&ctime) - timezone;
}

/* -------------------- tag handling ----------------------- */

void osm_tag_free(tag_t *tag) {
  if(tag->key)   g_free(tag->key);
  if(tag->value) g_free(tag->value);
  g_free(tag);
}

void osm_tags_free(tag_t *tag) {
  while(tag) {
    tag_t *next = tag->next;
    osm_tag_free(tag);
    tag = next;
  }
}

static void osm_tags_dump(tag_t *tag) {
  while(tag) {
    printf("Key/Val: %s/%s\n", tag->key, tag->value);
    tag = tag->next;
  }
}

tag_t *osm_parse_osm_tag(osm_t *osm, xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;

  /* allocate a new tag structure */
  tag_t *tag = g_new0(tag_t, 1);

  char *prop;
  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"k"))) {
    if(strlen(prop) > 0) tag->key = g_strdup(prop);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"v"))) {
    if(strlen(prop) > 0) tag->value = g_strdup(prop);
    xmlFree(prop);
  }

  if(!tag->key || !tag->value) {
    printf("incomplete tag key/value %s/%s\n", tag->key, tag->value);
    osm_tags_free(tag);
    return NULL;
  }

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) 
    if (cur_node->type == XML_ELEMENT_NODE) 
      printf("found unhandled osm/node/tag/%s\n", cur_node->name);

  return tag;
}

gboolean osm_is_creator_tag(tag_t *tag) {
  if(strcasecmp(tag->key, "created_by") == 0) return TRUE;
  if(strcasecmp(tag->key, "source") == 0) return TRUE;

  return FALSE;
}

gboolean osm_tag_key_and_value_present(tag_t *haystack, tag_t *tag) {
  while(haystack) {
    if((strcasecmp(haystack->key, tag->key) == 0) &&
       (strcasecmp(haystack->value, tag->value) == 0))
      return TRUE;

    haystack = haystack->next;
  }
  return FALSE;
}

gboolean osm_tag_key_other_value_present(tag_t *haystack, tag_t *tag) {
  while(haystack) {
    if((strcasecmp(haystack->key, tag->key) == 0) &&
       (strcasecmp(haystack->value, tag->value) != 0)) 
      return TRUE;
    
    haystack = haystack->next;
  }
  return FALSE;
}

gboolean osm_way_ends_with_node(way_t *way, node_t *node) {
  /* and deleted way may even not contain any nodes at all */
  /* so ignore it */
  if(way->flags & OSM_FLAG_DELETED)
    return FALSE;

  /* any valid way must have at least two nodes */
  g_assert(way->node_chain && way->node_chain->next);

  node_chain_t *chain = way->node_chain;
  if(chain->node == node) return TRUE;

  while(chain->next) chain = chain->next;
  if(chain->node == node) return TRUE;

  return FALSE;
}

/* ------------------- node handling ------------------- */

void osm_node_free(icon_t **icon, node_t *node) {
  if(node->icon_buf) 
    icon_free(icon, node->icon_buf);

  /* there must not be anything left in this chain */
  g_assert(!node->map_item_chain);

  osm_tags_free(node->tag);
  g_free(node);
}

static void osm_nodes_free(icon_t **icon, node_t *node) {
  while(node) {
    node_t *next = node->next;
    osm_node_free(icon, node);
    node = next;
  }
}

void osm_node_dump(node_t *node) {
  char buf[64];
  struct tm tm;
    
  printf("Id:      %lu\n", node->id);
  printf("User:    %s\n", node->user?node->user->name:"<unspecified>");
  printf("Visible: %s\n", node->visible?"yes":"no");
  
  localtime_r(&node->time, &tm);
  strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
  printf("Time:    %s\n", buf);
  osm_tags_dump(node->tag);
}

void osm_nodes_dump(node_t *node) {
  printf("\nNode list:\n");
  while(node) {
    osm_node_dump(node);
    printf("\n");
    node = node->next;
  }
}

/* ------------------- way handling ------------------- */

void osm_node_chain_free(node_chain_t *node_chain) {
  while(node_chain) {
    g_assert(node_chain->node->ways);

    node_chain_t *next = node_chain->next;
    node_chain->node->ways--;
    g_free(node_chain);
    node_chain = next;
  }
}

void osm_way_free(way_t *way) {
  //  printf("freeing way #%ld\n", way->id);

  osm_node_chain_free(way->node_chain);
  osm_tags_free(way->tag);

  /* there must not be anything left in this chain */
  g_assert(!way->map_item_chain);

  g_free(way);
}

static void osm_ways_free(way_t *way) {
  while(way) {
    way_t *next = way->next;
    osm_way_free(way);
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

int osm_node_chain_length(node_chain_t *node_chain) {
  int cnt = 0;
  while(node_chain) {
    cnt++;
    node_chain = node_chain->next;
  }

  return cnt;
}

void osm_way_dump(way_t *way) {
  char buf[64];
  struct tm tm;

  printf("Id:      %lu\n", way->id);
  printf("User:    %s\n", way->user?way->user->name:"<unspecified>");
  printf("Visible: %s\n", way->visible?"yes":"no");
  node_chain_t *node_chain = way->node_chain;
  while(node_chain) {
    printf("  Node:  %lu\n", node_chain->node->id);
    node_chain = node_chain->next;
  }
  
  localtime_r(&way->time, &tm);
  strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
  printf("Time:    %s\n", buf);
  osm_tags_dump(way->tag);
}

void osm_ways_dump(way_t *way) {
  printf("\nWay list:\n");
  while(way) {
    osm_way_dump(way);
    printf("\n");
    way = way->next;
  }
}

node_chain_t *osm_parse_osm_way_nd(osm_t *osm, 
			  xmlDocPtr doc, xmlNode *a_node) {
  char *prop;

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"ref"))) {
    item_id_t id = strtoul(prop, NULL, 10);
    node_chain_t *node_chain = g_new0(node_chain_t, 1);

    /* search matching node */
    node_chain->node = osm_get_node_by_id(osm, id);
    if(!node_chain->node) printf("Node id %lu not found\n", id);
    else                  node_chain->node->ways++;

    xmlFree(prop);

    return node_chain;
  }

  return NULL;
}

/* ------------------- relation handling ------------------- */

void osm_member_free(member_t *member) {
  if(member->role) g_free(member->role);
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
  osm_tags_free(relation->tag);
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

void osm_relations_dump(relation_t *relation) {
  printf("\nRelation list:\n");
  while(relation) {
    char buf[64];
    struct tm tm;

    printf("Id:      %lu\n", relation->id);
    printf("User:    %s\n", 
	   relation->user?relation->user->name:"<unspecified>");
    printf("Visible: %s\n", relation->visible?"yes":"no");

    member_t *member = relation->member;
    while(member) {
      switch(member->type) {
      case ILLEGAL:
      case NODE_ID:
      case WAY_ID:
      case RELATION_ID:
	break;

      case NODE:
	if(member->node)
	  printf(" Member: Node, id = %lu, role = %s\n", 
		 member->node->id, member->role);
	break;

      case WAY:
	if(member->way)
	printf(" Member: Way, id = %lu, role = %s\n", 
	       member->way->id, member->role);
	break;

      case RELATION:
	if(member->relation)
	printf(" Member: Relation, id = %lu, role = %s\n", 
	       member->relation->id, member->role);
	break;
      }

      member = member->next;
    }
    
    localtime_r(&relation->time, &tm);
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
    printf("Time:    %s\n", buf);
    osm_tags_dump(relation->tag);
    
    printf("\n");
    relation = relation->next;
  }
}

member_t *osm_parse_osm_relation_member(osm_t *osm, 
			  xmlDocPtr doc, xmlNode *a_node) {
  char *prop;
  member_t *member = g_new0(member_t, 1);
  member->type = ILLEGAL;

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"type"))) {
    if(strcasecmp(prop, "way") == 0)           member->type = WAY;
    else if(strcasecmp(prop, "node") == 0)     member->type = NODE;
    else if(strcasecmp(prop, "relation") == 0) member->type = RELATION;
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"ref"))) {
    item_id_t id = strtoul(prop, NULL, 10);

    switch(member->type) {
    case ILLEGAL:
      printf("Unable to store illegal type\n");
      break;

    case WAY:
      /* search matching way */
      member->way = osm_get_way_by_id(osm, id);
      if(!member->way) {
	member->type = WAY_ID;
	member->id = id;
      }
      break;

    case NODE:
      /* search matching node */
      member->node = osm_get_node_by_id(osm, id);
      if(!member->node) {
	member->type = NODE_ID;
	member->id = id;
      }
      break;

    case RELATION:
      /* search matching relation */
      member->relation = osm_get_relation_by_id(osm, id);
      if(!member->relation) {
	member->type = NODE_ID;
	member->id = id;
      }
      break;

    case WAY_ID:
    case NODE_ID:
    case RELATION_ID:
      break;
    }

    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"role"))) {
    if(strlen(prop) > 0) member->role = g_strdup(prop);
    xmlFree(prop);
  }

  return member;
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
}

void osm_hash_tables_free(osm_t *osm) {
  hash_table_free(osm->node_hash);
  osm->node_hash = NULL;
  hash_table_free(osm->way_hash);
  osm->way_hash = NULL;
}

void osm_free(icon_t **icon, osm_t *osm) {
  if(!osm) return;

  osm_hash_tables_free(osm);

  if(osm->bounds)   osm_bounds_free(osm->bounds);
  if(osm->user)     osm_users_free(osm->user);
  if(osm->way)      osm_ways_free(osm->way);
  if(osm->node)     osm_nodes_free(icon, osm->node);
  if(osm->relation) osm_relations_free(osm->relation);
  g_free(osm);
}

void osm_dump(osm_t *osm) {
  osm_bounds_dump(osm->bounds);
  osm_users_dump(osm->user);
  osm_nodes_dump(osm->node);
  osm_ways_dump(osm->way);
  osm_relations_dump(osm->relation);
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
static bounds_t *process_bounds(xmlTextReaderPtr reader) {
  char *prop = NULL;
  bounds_t *bounds = g_new0(bounds_t, 1);

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

    osm_bounds_free(bounds);
    return NULL;
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

  return bounds;
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

static node_t *process_node(xmlTextReaderPtr reader, osm_t *osm) {

  /* allocate a new node structure */
  node_t *node = g_new0(node_t, 1);
  node->pos.lat = node->pos.lon = NAN;

  char *prop;
  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "id"))) {
    node->id = strtoul(prop, NULL, 10);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "lat"))) {
    node->pos.lat = g_ascii_strtod(prop, NULL);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "lon"))) {
    node->pos.lon = g_ascii_strtod(prop, NULL);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "user"))) {
    node->user = osm_user(osm, prop);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "visible"))) {
    node->visible = (strcasecmp(prop, "true") == 0);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "timestamp"))) {
    node->time = convert_iso8601(prop);
    xmlFree(prop);
  }

  pos2lpos(osm->bounds, &node->pos, &node->lpos);

  /* append node to end of hash table if present */
  if(osm->node_hash) {
    hash_item_t **item = &osm->node_hash->hash[ID2HASH(node->id)];
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
  tag_t **tag = &node->tag;
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) && 
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      char *subname = (char*)xmlTextReaderConstName(reader);
      if(strcasecmp(subname, "tag") == 0) {
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
  char *prop;

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "ref"))) {
    item_id_t id = strtoul(prop, NULL, 10);
    node_chain_t *node_chain = g_new0(node_chain_t, 1);
    
    /* search matching node */
    node_chain->node = osm_get_node_by_id(osm, id);
    if(!node_chain->node) printf("Node id %lu not found\n", id);
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

  char *prop;
  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "id"))) {
    way->id = strtoul(prop, NULL, 10);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "user"))) {
    way->user = osm_user(osm, prop);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "visible"))) {
    way->visible = (strcasecmp(prop, "true") == 0);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "timestamp"))) {
    way->time = convert_iso8601(prop);
    xmlFree(prop);
  }

  /* append way to end of hash table if present */
  if(osm->way_hash) {
    hash_item_t **item = &osm->way_hash->hash[ID2HASH(way->id)];
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
  tag_t **tag = &way->tag;
  node_chain_t **node_chain = &way->node_chain;
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) && 
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      char *subname = (char*)xmlTextReaderConstName(reader);
      if(strcasecmp(subname, "nd") == 0) {
	*node_chain = process_nd(reader, osm);
	if(*node_chain) node_chain = &(*node_chain)->next;
      } else if(strcasecmp(subname, "tag") == 0) {
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
  member->type = ILLEGAL;

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "type"))) {
    if(strcasecmp(prop, "way") == 0)           member->type = WAY;
    else if(strcasecmp(prop, "node") == 0)     member->type = NODE;
    else if(strcasecmp(prop, "relation") == 0) member->type = RELATION;
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "ref"))) {
    item_id_t id = strtoul(prop, NULL, 10);

    switch(member->type) {
    case ILLEGAL:
      printf("Unable to store illegal type\n");
      break;

    case WAY:
      /* search matching way */
      member->way = osm_get_way_by_id(osm, id);
      if(!member->way) {
	member->type = WAY_ID;
	member->id = id;
      }
      break;

    case NODE:
      /* search matching node */
      member->node = osm_get_node_by_id(osm, id);
      if(!member->node) {
	member->type = NODE_ID;
	member->id = id;
      }
      break;

    case RELATION:
      /* search matching relation */
      member->relation = osm_get_relation_by_id(osm, id);
      if(!member->relation) {
	member->type = NODE_ID;
	member->id = id;
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

  char *prop;
  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "id"))) {
    relation->id = strtoul(prop, NULL, 10);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "user"))) {
    relation->user = osm_user(osm, prop);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "visible"))) {
    relation->visible = (strcasecmp(prop, "true") == 0);
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "timestamp"))) {
    relation->time = convert_iso8601(prop);
    xmlFree(prop);
  }

  /* just an empty element? then return the relation as it is */
  /* (this should in fact never happen as this would be a relation */
  /* without members) */
  if(xmlTextReaderIsEmptyElement(reader))
    return relation;

  /* parse tags/member if present */  
  int depth = xmlTextReaderDepth(reader);

  /* scan all elements on same level or its children */
  tag_t **tag = &relation->tag;
  member_t **member = &relation->member;
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) && 
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      char *subname = (char*)xmlTextReaderConstName(reader);
      if(strcasecmp(subname, "member") == 0) {
	*member = process_member(reader, osm);
	if(*member) member = &(*member)->next;
      } else if(strcasecmp(subname, "tag") == 0) {
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
  const int tick_every = 50; // Balance responsive appearance with performance.
  int ret = xmlTextReaderRead(reader);
  while(ret == 1) {

    switch(xmlTextReaderNodeType(reader)) {
    case XML_READER_TYPE_ELEMENT:

      g_assert(xmlTextReaderDepth(reader) == 1);
      char *name = (char*)xmlTextReaderConstName(reader);
      if(strcasecmp(name, "bounds") == 0) {
	osm->bounds = process_bounds(reader);
      } else if(strcasecmp(name, "node") == 0) {
	*node = process_node(reader, osm);
	if(*node) node = &(*node)->next;
      } else if(strcasecmp(name, "way") == 0) {
	*way = process_way(reader, osm);
	if(*way) way = &(*way)->next;
      } else if(strcasecmp(name, "relation") == 0) {
	*relation = process_relation(reader, osm);
	if(*relation) relation = &(*relation)->next;
      } else {
	printf("something unknown found\n");
	g_assert(0);
	skip_element(reader);
      }
      break;
      
    case XML_READER_TYPE_END_ELEMENT:
      /* end element must be for the current element */
      g_assert(xmlTextReaderDepth(reader) == 0);
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

  g_assert(0);
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
      char *name = (char*)xmlTextReaderConstName(reader);
      if(name && strcasecmp(name, "osm") == 0)
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

osm_t *osm_parse(char *filename) {

  struct timeval start;
  gettimeofday(&start, NULL);

  LIBXML_TEST_VERSION;

  // use stream parser
  osm_t *osm = process_file(filename);
  xmlCleanupParser();

  struct timeval end;
  gettimeofday(&end, NULL);

  printf("total parse time: %ldms\n", 
	 (end.tv_usec - start.tv_usec)/1000 +
	 (end.tv_sec - start.tv_sec)*1000);

  return osm;
}

gboolean osm_sanity_check(GtkWidget *parent, osm_t *osm) {
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

char *osm_tag_get_by_key(tag_t *tag, char *key) {
  if(!tag || !key) return NULL;

  while(tag) {
    if(strcasecmp(tag->key, key) == 0)
      return tag->value;

    tag = tag->next;
  }

  return NULL;
}

char *osm_way_get_value(way_t *way, char *key) {
  tag_t *tag = way->tag;

  while(tag) {
    if(strcasecmp(tag->key, key) == 0)
      return tag->value;

    tag = tag->next;
  }

  return NULL;
}

char *osm_node_get_value(node_t *node, char *key) {
  tag_t *tag = node->tag;

  while(tag) {
    if(strcasecmp(tag->key, key) == 0)
      return tag->value;

    tag = tag->next;
  }

  return NULL;
}

gboolean osm_way_has_value(way_t *way, char *str) {
  tag_t *tag = way->tag;

  while(tag) {
    if(tag->value && strcasecmp(tag->value, str) == 0)
      return TRUE;

    tag = tag->next;
  }
  return FALSE;
}

gboolean osm_node_has_value(node_t *node, char *str) {
  tag_t *tag = node->tag;

  while(tag) {
    if(tag->value && strcasecmp(tag->value, str) == 0)
      return TRUE;

    tag = tag->next;
  }
  return FALSE;
}

gboolean osm_node_has_tag(node_t *node) {
  tag_t *tag = node->tag;

  if(tag && strcasecmp(tag->key, "created_by") == 0)
    tag = tag->next;

  return tag != NULL;
}

/* return true if node is part of way */
gboolean osm_node_in_way(way_t *way, node_t *node) {
  node_chain_t *node_chain = way->node_chain;
  while(node_chain) {
    if(node_chain->node == node)
      return TRUE;

    node_chain = node_chain->next;
  }
  return FALSE;
}

static void osm_generate_tags(tag_t *tag, xmlNodePtr node) {
  while(tag) {
    /* make sure "created_by" tag contains our id */
    if(strcasecmp(tag->key, "created_by") == 0) {
      g_free(tag->value);
      tag->value = g_strdup(PACKAGE " v" VERSION);
    }

    xmlNodePtr tag_node = xmlNewChild(node, NULL, BAD_CAST "tag", NULL);
    xmlNewProp(tag_node, BAD_CAST "k", BAD_CAST tag->key);
    xmlNewProp(tag_node, BAD_CAST "v", BAD_CAST tag->value);
    tag = tag->next;
  }
}

/* build xml representation for a way */
char *osm_generate_xml(osm_t *osm, type_t type, void *item) {
  char str[32];
  xmlChar *result = NULL;
  int len = 0;

  LIBXML_TEST_VERSION;

  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "osm");
  xmlNewProp(root_node, BAD_CAST "version", BAD_CAST "0.5");
  xmlNewProp(root_node, BAD_CAST "generator", BAD_CAST PACKAGE " V" VERSION);
  xmlDocSetRootElement(doc, root_node);

  switch(type) {
  case NODE: 
    {
      node_t *node = (node_t*)item;
      xmlNodePtr node_node = xmlNewChild(root_node, NULL, 
					 BAD_CAST "node", NULL);
      /* new nodes don't have an id, but get one after the upload */
      if(!(node->flags & OSM_FLAG_NEW)) {
	snprintf(str, sizeof(str), "%u", (unsigned)node->id);
	xmlNewProp(node_node, BAD_CAST "id", BAD_CAST str);
      }
      g_ascii_dtostr(str, sizeof(str), node->pos.lat);
      xmlNewProp(node_node, BAD_CAST "lat", BAD_CAST str);
      g_ascii_dtostr(str, sizeof(str), node->pos.lon);
      xmlNewProp(node_node, BAD_CAST "lon", BAD_CAST str);    
      osm_generate_tags(node->tag, node_node);
    }
    break;

  case WAY: 
    {
      way_t *way = (way_t*)item;
      xmlNodePtr way_node = xmlNewChild(root_node, NULL, BAD_CAST "way", NULL);
      snprintf(str, sizeof(str), "%u", (unsigned)way->id);
      xmlNewProp(way_node, BAD_CAST "id", BAD_CAST str);
      
      node_chain_t *node_chain = way->node_chain;
      while(node_chain) {
	xmlNodePtr nd_node = xmlNewChild(way_node, NULL, BAD_CAST "nd", NULL);
	char *str = g_strdup_printf("%ld", node_chain->node->id);
	xmlNewProp(nd_node, BAD_CAST "ref", BAD_CAST str);
	g_free(str);
	node_chain = node_chain->next;
      }
      
      osm_generate_tags(way->tag, way_node);
    } 
    break;

  case RELATION:
    {
      relation_t *relation = (relation_t*)item;
      xmlNodePtr rel_node = xmlNewChild(root_node, NULL, 
					BAD_CAST "relation", NULL);
      snprintf(str, sizeof(str), "%u", (unsigned)relation->id);
      xmlNewProp(rel_node, BAD_CAST "id", BAD_CAST str);
      
      member_t *member = relation->member;
      while(member) {
	xmlNodePtr m_node = xmlNewChild(rel_node,NULL,BAD_CAST "member", NULL);
	char *str = NULL;

	switch(member->type) {
	case NODE:
	  xmlNewProp(m_node, BAD_CAST "type", BAD_CAST "node");
	  str = g_strdup_printf("%ld", member->node->id);
	  break;

	case WAY:
	  xmlNewProp(m_node, BAD_CAST "type", BAD_CAST "way");
	  str = g_strdup_printf("%ld", member->way->id);
	  break;

	case RELATION:
	  xmlNewProp(m_node, BAD_CAST "type", BAD_CAST "relation");
	  str = g_strdup_printf("%ld", member->relation->id);
	  break;

	default:
	  break;
	}

	if(str) {
	  xmlNewProp(m_node, BAD_CAST "ref", BAD_CAST str);
	  g_free(str);
	}

	if(member->role)
	  xmlNewProp(m_node, BAD_CAST "role", BAD_CAST member->role);
	else
	  xmlNewProp(m_node, BAD_CAST "role", BAD_CAST "");

	member = member->next;
      }
      osm_generate_tags(relation->tag, rel_node);
    }
    break;

  default:
    printf("neither NODE nor WAY nor RELATION\n");
    g_assert(0);
    break;
  }

  xmlDocDumpFormatMemoryEnc(doc, &result, &len, "UTF-8", 1);
  xmlFreeDoc(doc);
  xmlCleanupParser();

  //  puts("xml encoding result:");
  //  puts((char*)result);

  return (char*)result;
}

/* build xml representation for a node */
char *osm_generate_xml_node(osm_t *osm, node_t *node) {
  return osm_generate_xml(osm, NODE, node);
}

/* build xml representation for a way */
char *osm_generate_xml_way(osm_t *osm, way_t *way) {
  return osm_generate_xml(osm, WAY, way);
}

/* build xml representation for a relation */
char *osm_generate_xml_relation(osm_t *osm, relation_t *relation) {
  return osm_generate_xml(osm, RELATION, relation);
}

/* the following three functions are eating much CPU power */
/* as they search the objects lists. Hashing is supposed to help */
node_t *osm_get_node_by_id(osm_t *osm, item_id_t id) {
  if(id > 0 && osm->node_hash) {
    // use hash table if present
    hash_item_t *item = osm->node_hash->hash[ID2HASH(id)];
    while(item) {
      if(item->data.node->id == id)
	return item->data.node;
      
      item = item->next;
    }
  }

  /* use linear search if no hash tables are present or search in hash table failed */
  node_t *node = osm->node;
  while(node) {
    if(node->id == id)
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
      if(item->data.way->id == id)
	return item->data.way;
      
      item = item->next;
    }
  }

  /* use linear search if no hash tables are present or search on hash table failed */
  way_t *way = osm->way;
  while(way) {
    if(way->id == id)
      return way;
    
    way = way->next;
  }

  return NULL;
}

relation_t *osm_get_relation_by_id(osm_t *osm, item_id_t id) {
  // use linear search
  relation_t *relation = osm->relation;
  while(relation) {
    if(relation->id == id)
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
    way_t *way = osm->way;
    while(way) {
      if(way->id == id)
	found = TRUE;

      way = way->next;
    }

    /* no such id so far -> use it */
    if(!found) return id;

    id--;
  }
  g_assert(0);
  return 0;
}

item_id_t osm_new_node_id(osm_t *osm) {
  item_id_t id = -1;

  while(TRUE) {
    gboolean found = FALSE;
    node_t *node = osm->node;
    while(node) {
      if(node->id == id)
	found = TRUE;

      node = node->next;
    }

    /* no such id so far -> use it */
    if(!found) return id;

    id--;
  }
  g_assert(0);
  return 0;
}

item_id_t osm_new_relation_id(osm_t *osm) {
  item_id_t id = -1;

  while(TRUE) {
    gboolean found = FALSE;
    relation_t *relation = osm->relation;
    while(relation) {
      if(relation->id == id)
	found = TRUE;

      relation = relation->next;
    }

    /* no such id so far -> use it */
    if(!found) return id;

    id--;
  }
  g_assert(0);
  return 0;
}

node_t *osm_node_new(osm_t *osm, gint x, gint y) {
  printf("Creating new node\n");

  node_t *node = g_new0(node_t, 1);
  node->lpos.x = x; 
  node->lpos.y = y;
  node->visible = TRUE;
  node->time = time(NULL);

  /* add created_by tag */
  node->tag = g_new0(tag_t, 1);
  node->tag->key = g_strdup("created_by");
  node->tag->value = g_strdup(PACKAGE " v" VERSION);

  /* convert screen position back to ll */
  lpos2pos(osm->bounds, &node->lpos, &node->pos);

  printf("  new at %d %d (%f %f)\n", 
	 node->lpos.x, node->lpos.y, node->pos.lat, node->pos.lon);

  return node;
}


void osm_node_attach(osm_t *osm, node_t *node) {
  printf("Attaching node\n");

  node->id = osm_new_node_id(osm);
  node->flags = OSM_FLAG_NEW;

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
  way->visible = TRUE;
  way->flags = OSM_FLAG_NEW;
  way->time = time(NULL);

  /* add created_by tag */
  way->tag = g_new0(tag_t, 1);
  way->tag->key = g_strdup("created_by");
  way->tag->value = g_strdup(PACKAGE " v" VERSION);

  return way;
}

void osm_way_attach(osm_t *osm, way_t *way) {
  printf("Attaching way\n");

  way->id = osm_new_way_id(osm);
  way->flags = OSM_FLAG_NEW;

  /* attach to end of way list */
  way_t **lway = &osm->way;
  while(*lway) lway = &(*lway)->next;  
  *lway = way;
}

/* returns pointer to chain of ways affected by this deletion */
way_chain_t *osm_node_delete(osm_t *osm, icon_t **icon, 
			     node_t *node, gboolean permanently,
			     gboolean affect_ways) {
  way_chain_t *way_chain = NULL, **cur_way_chain = &way_chain;

  /* new nodes aren't stored on the server and are just deleted permanently */
  if(node->flags & OSM_FLAG_NEW) {
    printf("About to delete NEW node #%ld -> force permanent delete\n",
	   node->id);
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
	  chain = &((*chain)->next);
      } else
	chain = &((*chain)->next);
    }

    if(modified) {
      way->flags |= OSM_FLAG_DIRTY;
      
      /* and add the way to the list of affected ways */
      *cur_way_chain = g_new0(way_chain_t, 1);
      (*cur_way_chain)->way = way;
      cur_way_chain = &((*cur_way_chain)->next);
    }

    way = way->next;
  }

  if(!permanently) {
    printf("mark node #%ld as deleted\n", node->id);
    node->flags |= OSM_FLAG_DELETED;
  } else {
    printf("permanently delete node #%ld\n", node->id);

    /* remove it from the chain */
    node_t **cnode = &osm->node;
    int found = 0;

    while(*cnode) {
      if(*cnode == node) {
	found++;
	*cnode = (*cnode)->next;

	osm_node_free(icon, node);
      } else
	cnode = &((*cnode)->next);
    }
    g_assert(found == 1);
  }

  return way_chain;
}

guint osm_way_number_of_nodes(way_t *way) {
  guint nodes = 0;
  node_chain_t *chain = way->node_chain;
  while(chain) {
    nodes++;
    chain = chain->next;
  }
  return nodes;
}

/* return all relations a node is in */
relation_chain_t *osm_node_to_relation(osm_t *osm, node_t *node) {
  relation_chain_t *rel_chain = NULL, **cur_rel_chain = &rel_chain;

  relation_t *relation = osm->relation;
  while(relation) {
    gboolean is_member = FALSE;

    member_t *member = relation->member;
    while(member) {
      switch(member->type) {
      case NODE:
	/* nodes are checked directly */
	if(member->node == node)
	  is_member = TRUE;
	break;

      case WAY: {
	/* ways have to be checked for the nodes they consist of */
	node_chain_t *chain = member->way->node_chain;
	while(chain && !is_member) {
	  if(chain->node == node)
	    is_member = TRUE;

	  chain = chain->next;
	}
      } break;

      default:
	break;
      }
      member = member->next;
    }

    /* node is a member of this relation, so move it to the member chain */
    if(is_member) {
      *cur_rel_chain = g_new0(relation_chain_t, 1);
      (*cur_rel_chain)->relation = relation;
      cur_rel_chain = &((*cur_rel_chain)->next);
    }

    relation = relation->next;
  }

  return rel_chain;
}

/* return all relations a way is in */
relation_chain_t *osm_way_to_relation(osm_t *osm, way_t *way) {
  relation_chain_t *rel_chain = NULL, **cur_rel_chain = &rel_chain;

  relation_t *relation = osm->relation;
  while(relation) {
    gboolean is_member = FALSE;

    member_t *member = relation->member;
    while(member) {
      switch(member->type) {
      case WAY: {
	/* ways can be check directly */
	if(member->way == way)
	  is_member = TRUE;
      } break;
      
      default:
	break;
      }
      member = member->next;
    }

    /* way is a member of this relation, so move it to the member chain */
    if(is_member) {
      *cur_rel_chain = g_new0(relation_chain_t, 1);
      (*cur_rel_chain)->relation = relation;
      cur_rel_chain = &((*cur_rel_chain)->next);
    }

    relation = relation->next;
  }

  return rel_chain;
}

/* return all ways a node is in */
way_chain_t *osm_node_to_way(osm_t *osm, node_t *node) {
  way_chain_t *chain = NULL, **cur_chain = &chain;

  way_t *way = osm->way;
  while(way) {
    gboolean is_member = FALSE;

    node_chain_t *node_chain = way->node_chain;
    while(node_chain) {
      if(node_chain->node == node)
	is_member = TRUE;

      node_chain = node_chain->next;
    }

    /* node is a member of this relation, so move it to the member chain */
    if(is_member) {
      *cur_chain = g_new0(way_chain_t, 1);
      (*cur_chain)->way = way;
      cur_chain = &((*cur_chain)->next);
    }

     way = way->next;
  }

  return chain;
}

gboolean osm_position_within_bounds(osm_t *osm, gint x, gint y) {
  if((x < osm->bounds->min.x) || (x > osm->bounds->max.x)) return FALSE;
  if((y < osm->bounds->min.y) || (y > osm->bounds->max.y)) return FALSE;
  return TRUE;
}

/* remove the given node from all relations. used if the node is to */
/* be deleted */
void osm_node_remove_from_relation(osm_t *osm, node_t *node) {
  relation_t *relation = osm->relation;
  printf("removing node #%ld from all relations:\n", node->id);

  while(relation) {
    member_t **member = &relation->member;
    while(*member) {
      if(((*member)->type == NODE) &&
	 ((*member)->node == node)) {

	printf("  from relation #%ld\n", relation->id);
	
	member_t *cur = *member;
	*member = (*member)->next;
	osm_member_free(cur);

	relation->flags |= OSM_FLAG_DIRTY;
      } else
	member = &(*member)->next;
    }
    relation = relation->next;
  }
}

/* remove the given way from all relations */
void osm_way_remove_from_relation(osm_t *osm, way_t *way) {
  relation_t *relation = osm->relation;
  printf("removing way #%ld from all relations:\n", way->id);

  while(relation) {
    member_t **member = &relation->member;
    while(*member) {
      if(((*member)->type == WAY) &&
	 ((*member)->way == way)) {

	printf("  from relation #%ld\n", relation->id);
	
	member_t *cur = *member;
	*member = (*member)->next;
	osm_member_free(cur);

	relation->flags |= OSM_FLAG_DIRTY;
      } else
	member = &(*member)->next;
    }
    relation = relation->next;
  }
}

relation_t *osm_relation_new(void) {
  printf("Creating new relation\n");

  relation_t *relation = g_new0(relation_t, 1);
  relation->visible = TRUE;
  relation->flags = OSM_FLAG_NEW;
  relation->time = time(NULL);

  /* add created_by tag */
  relation->tag = g_new0(tag_t, 1);
  relation->tag->key = g_strdup("created_by");
  relation->tag->value = g_strdup(PACKAGE " v" VERSION);

  return relation;
}

void osm_relation_attach(osm_t *osm, relation_t *relation) {
  printf("Attaching relation\n");

  relation->id = osm_new_relation_id(osm);
  relation->flags = OSM_FLAG_NEW;

  /* attach to end of relation list */
  relation_t **lrelation = &osm->relation;
  while(*lrelation) lrelation = &(*lrelation)->next;  
  *lrelation = relation;
}


void osm_way_delete(osm_t *osm, icon_t **icon, 
		    way_t *way, gboolean permanently) {

  /* new ways aren't stored on the server and are just deleted permanently */
  if(way->flags & OSM_FLAG_NEW) {
    printf("About to delete NEW way #%ld -> force permanent delete\n", 
	   way->id);
    permanently = TRUE;
  }

  /* delete all nodes that aren't in other use now */
  node_chain_t **chain = &way->node_chain;
  while(*chain) {

    (*chain)->node->ways--;
    printf("checking node #%ld (still used by %d)\n", 
	   (*chain)->node->id, (*chain)->node->ways);

    /* this node must only be part of this way */
    if(!(*chain)->node->ways) {
      /* delete this node, but don't let this actually affect the */
      /* associated ways as the only such way is the oen we are currently */
      /* deleting */
      way_chain_t *way_chain = 
	osm_node_delete(osm, icon, (*chain)->node, FALSE, FALSE);
      g_assert(way_chain);
      while(way_chain) {
	way_chain_t *way_next = way_chain->next;
	g_assert(way_chain->way == way);
	g_free(way_chain);
	way_chain = way_next;
      }
    }
    
    node_chain_t *cur = (*chain);
    *chain = cur->next;
    g_free(cur);
  }
  way->node_chain = NULL;

  if(!permanently) {
    printf("mark way #%ld as deleted\n", way->id);
    way->flags |= OSM_FLAG_DELETED;
  } else {
    printf("permanently delete way #%ld\n", way->id);

    /* remove it from the chain */
    way_t **cway = &osm->way;
    int found = 0;

    while(*cway) {
      if(*cway == way) {
	found++;
	*cway = (*cway)->next;

	osm_way_free(way);
      } else
	cway = &((*cway)->next);
    }
    g_assert(found == 1);
  }
}

void osm_relation_delete(osm_t *osm, relation_t *relation, 
			 gboolean permanently) {

  /* new relations aren't stored on the server and are just */
  /* deleted permanently */
  if(relation->flags & OSM_FLAG_NEW) {
    printf("About to delete NEW relation #%ld -> force permanent delete\n", 
	   relation->id);
    permanently = TRUE;
  }

  /* the deletion of a relation doesn't affect the members as they */
  /* don't have any reference to the relation they are part of */

  if(!permanently) {
    printf("mark relation #%ld as deleted\n", relation->id);
    relation->flags |= OSM_FLAG_DELETED;
  } else {
    printf("permanently delete relation #%ld\n", relation->id);

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
    g_assert(found == 1);
  }
}

void osm_way_reverse(way_t *way) {
  node_chain_t *new = NULL;

  /* walk old chain first to last */
  node_chain_t *old = way->node_chain;
  while(old) {
    node_chain_t *next = old->next;

    /* and prepend each node to the new chain */
    old->next = new;
    new = old;

    old = next;
  }

  way->node_chain = new;
}

static const char *DS_ONEWAY_FWD = "yes";
static const char *DS_ONEWAY_REV = "-1";
static const char *DS_LEFT_SUFFIX = ":left";
static const char *DS_RIGHT_SUFFIX = ":right";

/* Reverse direction-sensitive tags like "oneway". Marks the way as dirty if
 * anything is changed, and returns the number of flipped tags. */

guint
osm_way_reverse_direction_sensitive_tags (way_t *way) {
  tag_t *tag = way->tag;
  guint n_tags_altered = 0;
  while (tag != NULL) {
    char *lc_key = g_ascii_strdown(tag->key, -1);
    char *lc_value = g_ascii_strdown(tag->value, -1);
    
    if (strcmp(lc_key, "oneway") == 0) {
      // oneway={yes/true/1/-1} is unusual.
      // Favour "yes" and "-1".
      if ((strcmp(lc_value, DS_ONEWAY_FWD) == 0) ||
          (strcmp(lc_value, "true") == 0) ||
          (strcmp(lc_value, "1") == 0)) {
        g_free(tag->value);
        tag->value = g_strdup(DS_ONEWAY_REV);
        n_tags_altered++;
      }
      else if (strcmp(lc_value, DS_ONEWAY_REV) == 0) {
        g_free(tag->value);
        tag->value = g_strdup(DS_ONEWAY_FWD);
        n_tags_altered++;
      }
      else {
        printf("warning: unknown tag: %s=%s\n", tag->key, tag->value);
      }
    }

    // :left and :right suffixes
    else if (g_str_has_suffix(lc_key, DS_LEFT_SUFFIX)) {
      char *key_old = tag->key;
      char *lastcolon = rindex(key_old, ':');
      g_assert(lastcolon != NULL);
      *lastcolon = '\000';
      tag->key = g_strconcat(key_old, DS_RIGHT_SUFFIX, NULL);
      *lastcolon = ':';
      g_free(key_old);
      n_tags_altered++;
    }
    else if (g_str_has_suffix(lc_key, DS_RIGHT_SUFFIX)) {
      char *key_old = tag->key;
      char *lastcolon = rindex(key_old, ':');
      g_assert(lastcolon != NULL);
      *lastcolon = '\000';
      tag->key = g_strconcat(key_old, DS_LEFT_SUFFIX, NULL);
      *lastcolon = ':';
      g_free(key_old);
      n_tags_altered++;
    }

    g_free(lc_key);
    g_free(lc_value);
    tag = tag->next;
  }
  if (n_tags_altered > 0) {
    way->flags |= OSM_FLAG_DIRTY;
  }
  return n_tags_altered;
}

/* Reverse a way's role within relations where the role is direction-sensitive.
 * Returns the number of roles flipped, and marks any relations changed as
 * dirty. */

static const char *DS_ROUTE_FORWARD = "forward";
static const char *DS_ROUTE_REVERSE = "reverse";

guint
osm_way_reverse_direction_sensitive_roles(osm_t *osm, way_t *way) {
  relation_chain_t *rel_chain0, *rel_chain;
  rel_chain0 = rel_chain = osm_way_to_relation(osm, way);
  guint n_roles_flipped = 0;

  for (; rel_chain != NULL; rel_chain = rel_chain->next) {
    char *type = osm_tag_get_by_key(rel_chain->relation->tag, "type");
    
    // Route relations; http://wiki.openstreetmap.org/wiki/Relation:route
    if (strcasecmp(type, "route") == 0) {
      
      // First find the member corresponding to our way:
      member_t *member = rel_chain->relation->member;
      for (; member != NULL; member = member->next) {
        if (member->type == WAY) {
          if (member->way == way)
            break;
        }
        if (member->type == WAY_ID) {
          if (member->id == way->id)
            break;
        }
      }
      g_assert(member);  // osm_way_to_relation() broken?
      
      // Then flip its role if it's one of the direction-sensitive ones
      if (member->role == NULL) {
        printf("null role in route relation -> ignore\n");
      }
      else if (strcasecmp(member->role, DS_ROUTE_FORWARD) == 0) {
        g_free(member->role);
        member->role = g_strdup(DS_ROUTE_REVERSE);
        rel_chain->relation->flags |= OSM_FLAG_DIRTY;
        ++n_roles_flipped;
      }
      else if (strcasecmp(member->role, DS_ROUTE_REVERSE) == 0) {
        g_free(member->role);
        member->role = g_strdup(DS_ROUTE_FORWARD);
        rel_chain->relation->flags |= OSM_FLAG_DIRTY;
        ++n_roles_flipped;
      }

      // TODO: what about numbered stops? Guess we ignore them; there's no
      // consensus about whether they should be placed on the way or to one side
      // of it.

    }//if-route


  }
  if (rel_chain0) {
    g_free(rel_chain0);
  }
  return n_roles_flipped;
}

node_t *osm_way_get_first_node(way_t *way) {
  node_chain_t *chain = way->node_chain;
  if(!chain) return NULL;
  return chain->node;
}

node_t *osm_way_get_last_node(way_t *way) {
  node_chain_t *chain = way->node_chain;

  while(chain && chain->next) chain=chain->next;

  if(!chain) return NULL;

  return chain->node;
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

tag_t *osm_tags_copy(tag_t *src_tag, gboolean update_creator) {
  tag_t *new_tags = NULL;
  tag_t **dst_tag = &new_tags;

  while(src_tag) {
    *dst_tag = g_new0(tag_t, 1);
    (*dst_tag)->key = g_strdup(src_tag->key);
    if(update_creator && (strcasecmp(src_tag->key, "created_by") == 0))
      (*dst_tag)->value = g_strdup(PACKAGE " v" VERSION);
    else
      (*dst_tag)->value = g_strdup(src_tag->value);

    dst_tag = &(*dst_tag)->next;
    src_tag = src_tag->next;
  }

  return new_tags;
}

/* return plain text of type */
char *osm_type_string(type_t type) {
  const struct { type_t type; char *name; } types[] = {
    { ILLEGAL,     "illegal" },
    { NODE,        "node" },
    { WAY,         "way" },
    { RELATION,    "relation" },
    { NODE_ID,     "node id" },
    { WAY_ID,      "way id" },
    { RELATION_ID, "relation id" },
    { 0, NULL }
  };

  int i;
  for(i=0;types[i].name;i++) 
    if(type == types[i].type)
      return types[i].name;

  return NULL;
}

char *osm_object_string(type_t type, void *object) {
  char *type_str = osm_type_string(type);

  if(!object) 
    return g_strdup_printf("%s #<invalid>", type_str);

  switch(type) {
  case ILLEGAL:
    return g_strdup_printf("%s #<unspec>", type_str);
    break;
  case NODE:
    return g_strdup_printf("%s #%ld", type_str, ((node_t*)object)->id);
    break;
  case WAY:
    return g_strdup_printf("%s #%ld", type_str, ((way_t*)object)->id);
    break;
  case RELATION:
    return g_strdup_printf("%s #%ld", type_str, ((relation_t*)object)->id);
    break;
  case NODE_ID:
  case WAY_ID:
  case RELATION_ID:
    return g_strdup_printf("%s #%ld", type_str, ((item_id_t)object));
    break;
  }
  return NULL;
}

char *osm_id_string(type_t type, void *object) {
  if(!object) return NULL;

  switch(type) {
  case ILLEGAL:
    return NULL;
    break;
  case NODE:
    return g_strdup_printf("#%ld", ((node_t*)object)->id);
    break;
  case WAY:
    return g_strdup_printf("#%ld", ((way_t*)object)->id);
    break;
  case RELATION:
    return g_strdup_printf("#%ld", ((relation_t*)object)->id);
    break;
  case NODE_ID:
  case WAY_ID:
  case RELATION_ID:
    return g_strdup_printf("#%ld", ((item_id_t)object));
    break;
  }
  return NULL;
}

tag_t *osm_object_get_tags(type_t type, void *object) {
  if(!object) return NULL;

  switch(type) {
  case ILLEGAL:
    return NULL;
    break;
  case NODE:
    return ((node_t*)object)->tag;
    break;
  case WAY:
    return ((way_t*)object)->tag;
    break;
  case RELATION:
    return ((relation_t*)object)->tag;
    break;
  case NODE_ID:
  case WAY_ID:
  case RELATION_ID:
    return NULL;
    break;
  }
  return NULL;
}


gint osm_relation_members_num(relation_t *relation) {
  gint num = 0;
  member_t *member = relation->member;
  while(member) {
    num++;
    member = member->next;
  }
  return num;
}

void osm_object_set_flags(object_t *object, int set, int clr) {

  switch(object->type) {
  case NODE:
    object->node->flags |=  set;
    object->node->flags &= ~clr;
    break;

  case WAY:
    object->way->flags |=  set;
    object->way->flags &= ~clr;
    break;

  case RELATION:
    object->relation->flags |=  set;
    object->relation->flags &= ~clr;
    break;

  default:
    g_assert(0);
    break;
  }
}

// vim:et:ts=8:sw=2:sts=2:ai
