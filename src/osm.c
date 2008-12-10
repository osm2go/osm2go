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

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

/* determine where a node/way/relation read from the osm file */
/* is inserted into the internal database */
// #define OSM_SORT_ID
#define OSM_SORT_LAST
// #define OSM_SORT_FIRST

/* ------------------------- user handling --------------------- */

static void osm_bounds_free(bounds_t *bounds) {
  free(bounds);
}

static void osm_bounds_dump(bounds_t *bounds) {
  printf("\nBounds: %f->%f %f->%f\n", 
	 bounds->ll_min.lat, bounds->ll_max.lat, 
	 bounds->ll_min.lon, bounds->ll_max.lon);
}

static bounds_t *osm_parse_osm_bounds(osm_t *osm, 
		     xmlDocPtr doc, xmlNode *a_node) {
  char *prop;

  if(osm->bounds) {
    errorf(NULL, "Doubly defined bounds");
    return NULL;
  }

  bounds_t *bounds = g_new0(bounds_t, 1);

  bounds->ll_min.lat = bounds->ll_min.lon = NAN;
  bounds->ll_max.lat = bounds->ll_max.lon = NAN;

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"minlat"))) {
    bounds->ll_min.lat = g_ascii_strtod(prop, NULL);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"maxlat"))) {
    bounds->ll_max.lat = g_ascii_strtod(prop, NULL);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"minlon"))) {
    bounds->ll_min.lon = g_ascii_strtod(prop, NULL);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"maxlon"))) { 
    bounds->ll_max.lon = g_ascii_strtod(prop, NULL); 
    xmlFree(prop); 
  }

  if(isnan(bounds->ll_min.lat) || isnan(bounds->ll_min.lon) || 
     isnan(bounds->ll_max.lat) || isnan(bounds->ll_max.lon)) {
    errorf(NULL, "Invalid coordinate in bounds (%f/%f/%f/%f)",
	   bounds->ll_min.lat, bounds->ll_min.lon, 
	   bounds->ll_max.lat, bounds->ll_max.lon);

    g_free(bounds);
    return NULL;
  }


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

static node_t *osm_parse_osm_node(osm_t *osm, 
			  xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;

  /* allocate a new node structure */
  node_t *node = g_new0(node_t, 1);
  node->pos.lat = node->pos.lon = NAN;

  char *prop;
  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"id"))) {
    node->id = strtoul(prop, NULL, 10);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"lat"))) {
    node->pos.lat = g_ascii_strtod(prop, NULL);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"lon"))) {
    node->pos.lon = g_ascii_strtod(prop, NULL);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"user"))) {
    node->user = osm_user(osm, prop);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"visible"))) {
    node->visible = (strcasecmp(prop, "true") == 0);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"timestamp"))) {
    node->time = convert_iso8601(prop);
    xmlFree(prop);
  }

  /* scan for tags and attach a list of tags */
  tag_t **tag = &node->tag;
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "tag") == 0) {
	/* attach tag to node */
      	*tag = osm_parse_osm_tag(osm, doc, cur_node);
	if(*tag) tag = &((*tag)->next);
      } else
	printf("found unhandled osm/node/%s\n", cur_node->name);
    }
  }

  pos2lpos(osm->bounds, &node->pos, &node->lpos);

  return node;
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
    node_chain->node = osm->node;
    while(node_chain->node && node_chain->node->id != id)
      node_chain->node = node_chain->node->next;

    if(!node_chain->node) printf("Node id %lu not found\n", id);

    if(node_chain->node) 
      node_chain->node->ways++;

    xmlFree(prop);

    return node_chain;
  }

  return NULL;
}

static way_t *osm_parse_osm_way(osm_t *osm, 
			  xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;

  /* allocate a new way structure */
  way_t *way = g_new0(way_t, 1);

  char *prop;
  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"id"))) {
    way->id = strtoul(prop, NULL, 10);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"user"))) {
    way->user = osm_user(osm, prop);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"visible"))) {
    way->visible = (strcasecmp(prop, "true") == 0);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"timestamp"))) {
    way->time = convert_iso8601(prop);
    xmlFree(prop);
  }

  /* scan for tags/nodes and attach their lists */
  tag_t **tag = &way->tag;
  node_chain_t **node_chain = &way->node_chain;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "tag") == 0) {
	/* attach tag to node */
      	*tag = osm_parse_osm_tag(osm, doc, cur_node);
	if(*tag) tag = &((*tag)->next);
      } else if(strcasecmp((char*)cur_node->name, "nd") == 0) {
	*node_chain = osm_parse_osm_way_nd(osm, doc, cur_node);
	if(*node_chain) 
	  node_chain = &((*node_chain)->next);
      } else
	printf("found unhandled osm/node/%s\n", cur_node->name);
    }
  }

  return way;
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

static void osm_relations_free(relation_t *relation) {
  while(relation) {
    relation_t *next = relation->next;

    osm_tags_free(relation->tag);
    osm_members_free(relation->member);

    g_free(relation);
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
      member->way = osm->way;
      while(member->way && member->way->id != id)
	member->way = member->way->next;

      if(!member->way) {
	member->type = WAY_ID;
	member->id = id;
      }
      break;

    case NODE:
      /* search matching node */
      member->node = osm->node;
      while(member->node && member->node->id != id)
	member->node = member->node->next;

      if(!member->node) {
	member->type = NODE_ID;
	member->id = id;
      }
      break;

    case RELATION:
      /* search matching relation */
      member->relation = osm->relation;
      while(member->relation && member->relation->id != id)
	member->relation = member->relation->next;

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

static relation_t *osm_parse_osm_relation(osm_t *osm, 
			  xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;

  /* allocate a new relation structure */
  relation_t *relation = g_new0(relation_t, 1);

  char *prop;
  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"id"))) {
    relation->id = strtoul(prop, NULL, 10);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"user"))) {
    relation->user = osm_user(osm, prop);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"visible"))) {
    relation->visible = (strcasecmp(prop, "true") == 0);
    xmlFree(prop);
  }

  if((prop = (char*)xmlGetProp(a_node, (unsigned char*)"timestamp"))) {
    relation->time = convert_iso8601(prop);
    xmlFree(prop);
  }

  /* scan for tags and attach a list of tags */
  tag_t **tag = &relation->tag;
  member_t **member = &relation->member;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "tag") == 0) {
	/* attach tag to node */
      	*tag = osm_parse_osm_tag(osm, doc, cur_node);
	if(*tag) tag = &((*tag)->next);
      } else if(strcasecmp((char*)cur_node->name, "member") == 0) {
	*member = osm_parse_osm_relation_member(osm, doc, cur_node);
	if(*member) member = &((*member)->next);
      } else
	printf("found unhandled osm/node/%s\n", cur_node->name);
    }
  }

  return relation;
}

/* ----------------------- generic xml handling -------------------------- */

/* parse loc entry */
static void osm_parse_osm(osm_t *osm, xmlDocPtr doc, xmlNode * a_node) {
  xmlNode *cur_node = NULL;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "bounds") == 0) 
      	osm->bounds = osm_parse_osm_bounds(osm, doc, cur_node);
      else if(strcasecmp((char*)cur_node->name, "node") == 0) {
	/* parse node and attach it to chain */
      	node_t *new = osm_parse_osm_node(osm, doc, cur_node);
	if(new) {
	  node_t **node = &osm->node;

#ifdef OSM_SORT_ID
	  /* search chain of nodes */
	  while(*node && ((*node)->id < new->id)) 
	    node = &(*node)->next;
#endif
	  
#ifdef OSM_SORT_LAST
	  while(*node) node = &(*node)->next;
#endif

	  /* insert into chain */
	  new->next = *node;
	  *node = new;
	}
      } else if(strcasecmp((char*)cur_node->name, "way") == 0) {
	/* parse way and attach it to chain */
      	way_t *new = osm_parse_osm_way(osm, doc, cur_node);
	if(new) {
	  way_t **way = &osm->way;

#ifdef OSM_SORT_ID
	  /* insert into chain */
	  while(*way && ((*way)->id < new->id))
	    way = &(*way)->next;
#endif
	  
#ifdef OSM_SORT_LAST
	  while(*way) way = &(*way)->next;
#endif

	  /* insert into chain */
	  new->next = *way;
	  *way = new;
	}
      } else if(strcasecmp((char*)cur_node->name, "relation") == 0) {
	/* parse relation and attach it to chain */
      	relation_t *new = osm_parse_osm_relation(osm, doc, cur_node);
	if(new) {
	  relation_t **relation = &osm->relation;

#ifdef OSM_SORT_ID
	  /* search chain of ways */
	  while(*relation && ((*relation)->id < new->id)) 
	    relation = &(*relation)->next;
#endif
	  
#ifdef OSM_SORT_LAST
	  while(*relation) relation = &(*relation)->next;
#endif

	  /* insert into chain */
	  new->next = *relation;
	  *relation = new;
	}
      } else
	printf("found unhandled osm/%s\n", cur_node->name);
	
    }
  }
}

/* parse root element and search for "osm" */
static osm_t *osm_parse_root(xmlDocPtr doc, xmlNode * a_node) {
  osm_t *osm;
  xmlNode *cur_node = NULL;

  /* allocate memory to hold osm file description */
  osm = g_new0(osm_t, 1);

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      /* parse osm osm file ... */
      if(strcasecmp((char*)cur_node->name, "osm") == 0) 
      	osm_parse_osm(osm, doc, cur_node);
      else 
	printf("found unhandled %s\n", cur_node->name);
    }
  }

  return osm;
}

static osm_t *osm_parse_doc(xmlDocPtr doc) {
  osm_t *osm;

  /* Get the root element node */
  xmlNode *root_element = xmlDocGetRootElement(doc);

  osm = osm_parse_root(doc, root_element);  

  /*free the document */
  xmlFreeDoc(doc);

  /*
   * Free the global variables that may
   * have been allocated by the parser.
   */
  xmlCleanupParser();

  return osm;
}

/* ------------------ osm handling ----------------- */

void osm_free(icon_t **icon, osm_t *osm) {
  if(!osm) return;

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

osm_t *osm_parse(char *filename) {
  xmlDoc *doc = NULL;

  LIBXML_TEST_VERSION;

  /* parse the file and get the DOM */
  if ((doc = xmlReadFile(filename, NULL, 0)) == NULL) {
    xmlErrorPtr	errP = xmlGetLastError();
    errorf(NULL, "While parsing \"%s\":\n\n%s", filename, errP->message);
    return NULL;
  }
  
  return osm_parse_doc(doc); 
}

gboolean osm_sanity_check(GtkWidget *parent, osm_t *osm) {
  if(!osm->bounds) {
    errorf(parent, _("Ivalid data in OSM file:\n"
		     "Boundary box missing!"));
    return FALSE;
  }
  if(!osm->node) {
    errorf(parent, _("Ivalid data in OSM file:\n"
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

node_t *osm_get_node_by_id(osm_t *osm, item_id_t id) {
  node_t *node = osm->node;
  while(node) {
    if(node->id == id)
      return node;

    node = node->next;
  }
  return NULL;
}

way_t *osm_get_way_by_id(osm_t *osm, item_id_t id) {
  way_t *way = osm->way;
  while(way) {
    if(way->id == id)
      return way;

    way = way->next;
  }
  return NULL;
}

relation_t *osm_get_relation_by_id(osm_t *osm, item_id_t id) {
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

void osm_way_revert(way_t *way) {
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
