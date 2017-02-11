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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <strings.h>

#include <algorithm>
#include <string>
#include <utility>

#include <libxml/parser.h>
#include <libxml/tree.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

bool object_t::operator==(const object_t& other) const
{
  if (type != other.type)
    return false;

  switch(type) {
  case NODE:
  case WAY:
  case RELATION:
    return obj == other.obj;
  case NODE_ID:
  case WAY_ID:
  case RELATION_ID:
    return id == other.id;
  case ILLEGAL:
    return true;
  default:
    g_assert_not_reached();
    return false;
  }
}

bool object_t::operator==(const node_t *n) const {
  return type == NODE && node == n;
}

bool object_t::operator==(const way_t *w) const {
  return type == WAY && way == w;
}

bool object_t::operator==(const relation_t *r) const {
  return type == RELATION && relation == r;
}

bool object_t::is_real() const {
  return (type == NODE) ||
         (type == WAY)  ||
         (type == RELATION);
}

/* return plain text of type */
const char *object_t::type_string() const {
  static std::map<type_t, const char *> types;
  if(types.empty()) {
    types[ILLEGAL] =     "illegal";
    types[NODE] =        "node";
    types[WAY] =         "way/area";
    types[RELATION] =    "relation";
    types[NODE_ID] =     "node id";
    types[WAY_ID] =      "way/area id";
    types[RELATION_ID] = "relation id";
  }

  const std::map<type_t, const char *>::const_iterator it =
        types.find(type);

  if(it != types.end())
    return it->second;

  return NULL;
}

gchar *object_t::id_string() const {
  switch(type) {
  case ILLEGAL:
    return NULL;
  case NODE:
  case WAY:
  case RELATION:
    return g_strdup_printf("#" ITEM_ID_FORMAT, obj->id);
  case NODE_ID:
  case WAY_ID:
  case RELATION_ID:
    return g_strdup_printf("#" ITEM_ID_FORMAT, id);
  default:
    return NULL;
  }
}

gchar *object_t::object_string() const {
  const char *type_str = type_string();

  switch(type) {
  case ILLEGAL:
    return g_strconcat(type_str, " #<unspec>", NULL);
  case NODE:
  case WAY:
  case RELATION:
    g_assert(obj);
    return g_strdup_printf("%s #" ITEM_ID_FORMAT, type_str, obj->id);
  case NODE_ID:
  case WAY_ID:
  case RELATION_ID:
    return g_strdup_printf("%s #" ITEM_ID_FORMAT, type_str, id);
  default:
    return NULL;
  }
}

const tag_t *object_t::get_tags() const {
  if(!is_real())
    return NULL;
  return obj->tag;
}

item_id_t object_t::get_id() const {
  if(type == ILLEGAL)
    return ID_ILLEGAL;
  if(is_real())
    return obj->id;
  return id;
}

void object_t::set_flags(int set, int clr) {
  g_assert(is_real());
  obj->flags |=  set;
  obj->flags &= ~clr;
}

/* ------------------------- user handling --------------------- */

struct cmp_user {
  const char * const uname;
  cmp_user(const char *u) : uname(u) {}
  bool operator()(const std::string &s) {
    return (strcasecmp(s.c_str(), uname) == 0);
  }
};

static const char *osm_user(osm_t *osm, const char *name, int uid) {
  if(!name)
    return 0;

  /* search through user list */
  if(uid >= 0) {
    const std::map<int, std::string>::const_iterator it = osm->users.find(uid);
    if(it != osm->users.end())
      return it->second.c_str();

    osm->users[uid] = name;
    return osm->users[uid].c_str();
  } else {
    /* match with the name, but only against users without uid */
    const std::vector<std::string>::const_iterator itEnd = osm->anonusers.end();
    std::vector<std::string>::const_iterator it = osm->anonusers.begin();
    it = std::find_if(it, itEnd, cmp_user(name));
    if(it != itEnd)
      return it->c_str();
    osm->anonusers.push_back(name);
    return osm->anonusers.back().c_str();
  }
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

  xmlChar *prop;
  if((prop = xmlGetProp(a_node, BAD_CAST "k"))) {
    if(strlen((char*)prop) > 0)
      tag->key = g_strdup((gchar*)prop);
    xmlFree(prop);
  }

  if((prop = xmlGetProp(a_node, BAD_CAST "v"))) {
    if(strlen((char*)prop) > 0)
      tag->value = g_strdup((gchar*)prop);
    xmlFree(prop);
  }

  if(!tag->key || !tag->value) {
    printf("incomplete tag key/value %s/%s\n", tag->key, tag->value);
    osm_tag_free(tag);
    return NULL;
  }

  const xmlNode *cur_node = NULL;
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next)
    if (cur_node->type == XML_ELEMENT_NODE)
      printf("found unhandled osm/node/tag/%s\n", cur_node->name);

  return tag;
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
    if(ntag->is_creator_tag())
      t1creator = ntag;
    else
      ncnt++;
  }
  for(ntag = t2; ntag != NULL; ntag = ntag->next) {
    if(ntag->is_creator_tag())
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

bool tag_t::update(const char *nkey, const char *nvalue)
{
  bool ret = false;
  if(strcmp(key, nkey) != 0) {
    update_key(nkey);
    ret = true;
  }
  if(strcmp(value, nvalue) != 0) {
    update_value(nvalue);
    ret = true;
  }
  return ret;
}

void tag_t::update_key(const char *nkey)
{
  const size_t nlen = strlen(nkey) + 1;
  key = static_cast<char *>(g_realloc(key, nlen));
  memcpy(key, nkey, nlen);
}

void tag_t::update_value(const char *nvalue)
{
  const size_t nlen = strlen(nvalue) + 1;
  value = static_cast<char *>(g_realloc(value, nlen));
  memcpy(value, nvalue, nlen);
}

/* ------------------- node handling ------------------- */

void node_t::cleanup(osm_t *osm) {
  if(icon_buf)
    icon_free(osm->icons, icon_buf);

  /* there must not be anything left in this chain */
  g_assert(!map_item_chain);

  osm_tags_free(tag);
}

void osm_t::node_free(node_t *node) {
  nodes.erase(node->id);
  node->cleanup(this);
  delete node;
}

struct nodefree {
  osm_t * const osm;
  nodefree(osm_t *o) : osm(o) {}
  void operator()(std::pair<item_id_t, node_t *> pair) {
    pair.second->cleanup(osm);
    delete pair.second;
  }
};

/* ------------------- way handling ------------------- */
static void osm_unref_node(node_t* node)
{
  g_assert_cmpint(node->ways, >, 0);
  node->ways--;
}

void osm_node_chain_free(node_chain_t &node_chain) {
  std::for_each(node_chain.begin(), node_chain.end(), osm_unref_node);
}

void osm_t::way_free(way_t *way) {
  ways.erase(way->id);
  way->cleanup();
  delete way;
}

static void way_free(std::pair<item_id_t, way_t *> pair) {
  pair.second->cleanup();
  delete pair.second;
}

node_t *osm_parse_osm_way_nd(osm_t *osm, xmlNode *a_node) {
  xmlChar *prop;
  node_t *node = NULL;

  if((prop = xmlGetProp(a_node, BAD_CAST "ref"))) {
    item_id_t id = strtoll((char*)prop, NULL, 10);

    /* search matching node */
    node = osm->node_by_id(id);
    if(!node)
      printf("Node id " ITEM_ID_FORMAT " not found\n", id);
    else
      node->ways++;

    xmlFree(prop);
  }

  return node;
}

/* ------------------- relation handling ------------------- */

void osm_member_free(member_t &member) {
  g_free(member.role);
}

void osm_members_free(std::vector<member_t> &members) {
  std::for_each(members.begin(), members.end(), osm_member_free);
  members.clear();
}

void relation_t::cleanup() {
  osm_tags_free(tag);
  osm_members_free(members);
}

void osm_t::relation_free(relation_t *relation) {
  relations.erase(relation->id);
  relation->cleanup();
  delete relation;
}

static void osm_relation_free_pair(std::pair<item_id_t, relation_t *> pair) {
  pair.second->cleanup();
  delete pair.second;
}

member_t osm_parse_osm_relation_member(osm_t *osm, xmlNode *a_node) {
  xmlChar *prop;
  member_t member;

  if((prop = xmlGetProp(a_node, BAD_CAST "type"))) {
    if(strcmp((char*)prop, "way") == 0)
      member.object.type = WAY;
    else if(strcmp((char*)prop, "node") == 0)
      member.object.type = NODE;
    else if(G_LIKELY(strcmp((char*)prop, "relation") == 0))
      member.object.type = RELATION;
    xmlFree(prop);
  }

  if((prop = xmlGetProp(a_node, BAD_CAST "ref"))) {
    item_id_t id = strtoll((char*)prop, NULL, 10);

    switch(member.object.type) {
    case ILLEGAL:
      printf("Unable to store illegal type\n");
      return member;

    case WAY:
      /* search matching way */
      member.object.way = osm->way_by_id(id);
      if(!member.object.way) {
	member.object.type = WAY_ID;
	member.object.id = id;
      }
      break;

    case NODE:
      /* search matching node */
      member.object.node = osm->node_by_id(id);
      if(!member.object.node) {
	member.object.type = NODE_ID;
	member.object.id = id;
      }
      break;

    case RELATION:
      /* search matching relation */
      member.object.relation = osm->relation_by_id(id);
      if(!member.object.relation) {
	member.object.type = NODE_ID;
	member.object.id = id;
      }
      break;

    case WAY_ID:
    case NODE_ID:
    case RELATION_ID:
      break;
    }

    xmlFree(prop);
  }

  if((prop = xmlGetProp(a_node, BAD_CAST "role"))) {
    if(strlen((char*)prop) > 0)
      member.role = g_strdup((char*)prop);
    xmlFree(prop);
  }

  return member;
}

/* try to find something descriptive */
gchar *relation_t::descriptive_name() const {
  const char *keys[] = { "ref", "name", "description", "note", "fix" "me", NULL};
  for (unsigned int i = 0; keys[i] != NULL; i++) {
    const char *name = tag->get_by_key(keys[i]);
    if(name)
      return g_strdup(name);
  }

  return g_strdup_printf("<ID #" ITEM_ID_FORMAT ">", id);
}

/* ------------------ osm handling ----------------- */

void osm_free(osm_t *osm) {
  if(!osm) return;

  std::for_each(osm->ways.begin(), osm->ways.end(), way_free);
  std::for_each(osm->nodes.begin(), osm->nodes.end(), nodefree(osm));
  std::for_each(osm->relations.begin(), osm->relations.end(),
                osm_relation_free_pair);
  delete osm;
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

static pos_float_t xml_reader_attr_float(xmlTextReaderPtr reader, const char *name) {
  xmlChar *prop = xmlTextReaderGetAttribute(reader, BAD_CAST name);
  pos_float_t ret;

  if((prop)) {
    ret = g_ascii_strtod((gchar *)prop, NULL);
    xmlFree(prop);
  } else
    ret = NAN;

  return ret;  
}

/* parse bounds */
static gboolean process_bounds(xmlTextReaderPtr reader, bounds_t *bounds) {
  bounds->ll_min.lat = xml_reader_attr_float(reader, "minlat");
  bounds->ll_min.lon = xml_reader_attr_float(reader, "minlon");
  bounds->ll_max.lat = xml_reader_attr_float(reader, "maxlat");
  bounds->ll_max.lon = xml_reader_attr_float(reader, "maxlon");

  if(G_UNLIKELY(std::isnan(bounds->ll_min.lat) || std::isnan(bounds->ll_min.lon) ||
                std::isnan(bounds->ll_max.lat) || std::isnan(bounds->ll_max.lon))) {
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
  if(G_LIKELY(prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "k"))) {
    if(strlen(prop) > 0) tag->key = g_strdup(prop);
    xmlFree(prop);
  }

  if(G_LIKELY(prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "v"))) {
    if(strlen(prop) > 0) tag->value = g_strdup(prop);
    xmlFree(prop);
  }

  if(G_UNLIKELY(!tag->key || !tag->value)) {
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
  if(G_LIKELY(prop = xmlTextReaderGetAttribute(reader, BAD_CAST "id"))) {
    obj->id = strtoll((char*)prop, NULL, 10);
    xmlFree(prop);
  }

  /* new in api 0.6: */
  if(G_LIKELY(prop = xmlTextReaderGetAttribute(reader, BAD_CAST "version"))) {
    obj->version = strtoul((char*)prop, NULL, 10);
    xmlFree(prop);
  }

  if(G_LIKELY(prop = xmlTextReaderGetAttribute(reader, BAD_CAST "user"))) {
    int uid = -1;
    xmlChar *puid = xmlTextReaderGetAttribute(reader, BAD_CAST "uid");
    if(G_LIKELY(puid)) {
      char *endp;
      uid = strtol((char*)puid, &endp, 10);
      if(G_UNLIKELY(*endp)) {
        printf("WARNING: cannot parse uid '%s' for user '%s'\n", puid, prop);
        uid = -1;
      }
      xmlFree(puid);
    }
    obj->user = osm_user(osm, (char*)prop, uid);
    xmlFree(prop);
  }

  if((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "visible"))) {
    obj->visible = (strcasecmp((char*)prop, "true") == 0);
    xmlFree(prop);
  }

  if(G_LIKELY(prop = xmlTextReaderGetAttribute(reader, BAD_CAST "timestamp"))) {
    obj->time = convert_iso8601((char*)prop);
    xmlFree(prop);
  }
}

static node_t *process_node(xmlTextReaderPtr reader, osm_t *osm) {

  /* allocate a new node structure */
  node_t *node = new node_t();

  process_base_attributes(node, reader, osm);

  node->pos.lat = xml_reader_attr_float(reader, "lat");
  node->pos.lon = xml_reader_attr_float(reader, "lon");

  pos2lpos(osm->bounds, &node->pos, &node->lpos);

  /* append node to end of hash table if present */
  osm->nodes[node->id] = node;

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

static node_t *process_nd(xmlTextReaderPtr reader, osm_t *osm) {
  xmlChar *prop;

  if((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "ref"))) {
    item_id_t id = strtoll((char*)prop, NULL, 10);
    /* search matching node */
    node_t *node = osm->node_by_id(id);
    if(!node)
      printf("Node id " ITEM_ID_FORMAT " not found\n", id);
    else
      node->ways++;

    xmlFree(prop);

    skip_element(reader);
    return node;
  }

  skip_element(reader);
  return NULL;
}

static way_t *process_way(xmlTextReaderPtr reader, osm_t *osm) {
  /* allocate a new way structure */
  way_t *way = new way_t();

  process_base_attributes(way, reader, osm);

  /* append way to end of hash table if present */
  osm->ways[way->id] = way;

  /* just an empty element? then return the way as it is */
  /* (this should in fact never happen as this would be a way without nodes) */
  if(xmlTextReaderIsEmptyElement(reader))
    return way;

  /* parse tags/nodes if present */
  int depth = xmlTextReaderDepth(reader);

  /* scan all elements on same level or its children */
  tag_t **tag = &way->tag;
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const char *subname = (const char*)xmlTextReaderConstName(reader);
      if(strcmp(subname, "nd") == 0) {
	node_t *n = process_nd(reader, osm);
        if(n)
          way->node_chain.push_back(n);
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

static member_t process_member(xmlTextReaderPtr reader, osm_t *osm) {
  char *prop;
  member_t member;

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "type"))) {
    if(strcmp(prop, "way") == 0)           member.object.type = WAY;
    else if(strcmp(prop, "node") == 0)     member.object.type = NODE;
    else if(strcmp(prop, "relation") == 0) member.object.type = RELATION;
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "ref"))) {
    item_id_t id = strtoll(prop, NULL, 10);

    switch(member.object.type) {
    case ILLEGAL:
      printf("Unable to store illegal type\n");
      return member;

    case WAY:
      /* search matching way */
      member.object.way = osm->way_by_id(id);
      if(!member.object.way) {
	member.object.type = WAY_ID;
	member.object.id = id;
      }
      break;

    case NODE:
      /* search matching node */
      member.object.node = osm->node_by_id(id);
      if(!member.object.node) {
	member.object.type = NODE_ID;
	member.object.id = id;
      }
      break;

    case RELATION:
      /* search matching relation */
      member.object.relation = osm->relation_by_id(id);
      if(!member.object.relation) {
	member.object.type = NODE_ID;
	member.object.id = id;
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
    if(strlen(prop) > 0) member.role = g_strdup(prop);
    xmlFree(prop);
  }

  return member;
}

static relation_t *process_relation(xmlTextReaderPtr reader, osm_t *osm) {
  /* allocate a new relation structure */
  relation_t *relation = new relation_t();

  process_base_attributes(relation, reader, osm);

  /* just an empty element? then return the relation as it is */
  /* (this should in fact never happen as this would be a relation */
  /* without members) */
  if(xmlTextReaderIsEmptyElement(reader))
    return relation;

  /* parse tags/member if present */
  int depth = xmlTextReaderDepth(reader);

  /* scan all elements on same level or its children */
  tag_t **tag = &relation->tag;
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const char *subname = (const char*)xmlTextReaderConstName(reader);
      if(strcmp(subname, "member") == 0) {
        member_t member = process_member(reader, osm);
        if(member)
          relation->members.push_back(member);
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
  osm_t *osm = new osm_t();

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
    case XML_READER_TYPE_ELEMENT: {

      g_assert_cmpint(xmlTextReaderDepth(reader), ==, 1);
      const char *name = (const char*)xmlTextReaderConstName(reader);
      if(block <= BLOCK_BOUNDS && strcmp(name, "bounds") == 0) {
        if(process_bounds(reader, &osm->rbounds))
          osm->bounds = &osm->rbounds;
	block = BLOCK_BOUNDS;
      } else if(block <= BLOCK_NODES && strcmp(name, "node") == 0) {
        node_t *node = process_node(reader, osm);
        if(node)
          osm->nodes[node->id] = node;
	block = BLOCK_NODES;
      } else if(block <= BLOCK_WAYS && strcmp(name, "way") == 0) {
        way_t *way = process_way(reader, osm);
        if(way)
          osm->ways[way->id] = way;
	block = BLOCK_WAYS;
      } else if(G_LIKELY(block <= BLOCK_RELATIONS && strcmp(name, "relation") == 0)) {
	relation_t *relation = process_relation(reader, osm);
	if(relation)
	  osm->relations[relation->id] = relation;
	block = BLOCK_RELATIONS;
      } else {
	printf("something unknown found: %s\n", name);
	g_assert_not_reached();
	skip_element(reader);
      }
      break;
    }

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

  reader = xmlReaderForFile(filename, NULL, 0);
  if (G_LIKELY(reader != NULL)) {
    if(G_LIKELY(xmlTextReaderRead(reader) == 1)) {
      const char *name = (const char*)xmlTextReaderConstName(reader);
      if(G_LIKELY(name && strcmp(name, "osm") == 0))
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

  if(osm)
    osm->icons = icon;

  printf("total parse time: %ldms\n",
	 (end.tv_usec - start.tv_usec)/1000 +
	 (end.tv_sec - start.tv_sec)*1000);

  return osm;
}

gboolean osm_sanity_check(GtkWidget *parent, const osm_t *osm) {
  if(G_UNLIKELY(!osm->bounds)) {
    errorf(parent, _("Invalid data in OSM file:\n"
		     "Boundary box missing!"));
    return FALSE;
  }
  if(G_UNLIKELY(osm->nodes.empty())) {
    errorf(parent, _("Invalid data in OSM file:\n"
		     "No drawable content found!"));
    return FALSE;
  }
  return TRUE;
}

/* ------------------------- misc access functions -------------- */

tag_t *tag_t::find(const char* key) {
  if(!key) return NULL;

  tag_t *tag = this;
  while(tag) {
    if(strcasecmp(tag->key, key) == 0)
      return tag;

    tag = tag->next;
  }

  return NULL;
}

const char *tag_t::get_by_key(const char* key) const {
  const tag_t *t = find(key);

  if (t)
    return t->value;

  return NULL;
}

struct node_in_other_way {
  const way_t * const way;
  const node_t * const node;
  node_in_other_way(const way_t *w, const node_t *n) : way(w), node(n) {}
  int operator()(const std::pair<item_id_t, way_t *> &pair) {
    return (pair.second != way) && pair.second->contains_node(node);
  }
};

/* return true if node is part of other way than this one */
bool osm_node_in_other_way(const osm_t *osm, const way_t *way, const node_t *node) {
  const std::map<item_id_t, way_t *>::const_iterator itEnd = osm->ways.end();
  return std::find_if(osm->ways.begin(), itEnd, node_in_other_way(way, node)) != itEnd;
}

static void osm_generate_tags(const tag_t *tag, xmlNodePtr node) {
  while(tag) {
    /* skip "created_by" tags as they aren't needed anymore with api 0.6 */
    if(G_LIKELY(!tag->is_creator_tag())) {
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

static xmlChar *
osm_generate_xml_finish(xmlDocPtr doc)
{
  xmlChar *result = 0;
  int len = 0;

  xmlDocDumpFormatMemoryEnc(doc, &result, &len, "UTF-8", 1);
  xmlFreeDoc(doc);

  return result;
}

/* build xml representation for a node */
xmlChar *node_t::generate_xml(item_id_t changeset) {
  char str[32];

  xmlNodePtr xml_node;
  xmlDocPtr doc = osm_generate_xml_init(&xml_node, "node");

  /* new nodes don't have an id, but get one after the upload */
  if(!(flags & OSM_FLAG_NEW)) {
    snprintf(str, sizeof(str), ITEM_ID_FORMAT, id);
    xmlNewProp(xml_node, BAD_CAST "id", BAD_CAST str);
  }
  snprintf(str, sizeof(str), ITEM_ID_FORMAT, version);
  xmlNewProp(xml_node, BAD_CAST "version", BAD_CAST str);
  snprintf(str, sizeof(str), "%u", (unsigned)changeset);
  xmlNewProp(xml_node, BAD_CAST "changeset", BAD_CAST str);
  xml_set_prop_pos(xml_node, &pos);
  osm_generate_tags(tag, xml_node);

  return osm_generate_xml_finish(doc);
}

struct add_xml_node_refs {
  xmlNodePtr const way_node;
  add_xml_node_refs(xmlNodePtr n) : way_node(n) {}
  void operator()(const node_t *node);
};

void add_xml_node_refs::operator()(const node_t* node)
{
  xmlNodePtr nd_node = xmlNewChild(way_node, NULL, BAD_CAST "nd", NULL);
  gchar str[G_ASCII_DTOSTR_BUF_SIZE];
  g_snprintf(str, sizeof(str), ITEM_ID_FORMAT, node->id);
  xmlNewProp(nd_node, BAD_CAST "ref", BAD_CAST str);
}

/**
 * @brief write the referenced nodes of a way to XML
 * @param way_node the XML node of the way to append to
 * @param way the way to walk
 */
void osm_write_node_chain(xmlNodePtr way_node, const way_t *way) {
  std::for_each(way->node_chain.begin(), way->node_chain.end(), add_xml_node_refs(way_node));
}

/* build xml representation for a way */
xmlChar *way_t::generate_xml(item_id_t changeset) {
  char str[32];

  xmlNodePtr xml_node;
  xmlDocPtr doc = osm_generate_xml_init(&xml_node, "way");

  snprintf(str, sizeof(str), ITEM_ID_FORMAT, id);
  xmlNewProp(xml_node, BAD_CAST "id", BAD_CAST str);
  snprintf(str, sizeof(str), ITEM_ID_FORMAT, version);
  xmlNewProp(xml_node, BAD_CAST "version", BAD_CAST str);
  snprintf(str, sizeof(str), "%u", (unsigned)changeset);
  xmlNewProp(xml_node, BAD_CAST "changeset", BAD_CAST str);

  osm_write_node_chain(xml_node, this);
  osm_generate_tags(tag, xml_node);

  return osm_generate_xml_finish(doc);
}

struct gen_xml_relation_functor {
  xmlNodePtr const xml_node;
  gen_xml_relation_functor(xmlNodePtr n) : xml_node(n) {}
  void operator()(const member_t &member);
};

void gen_xml_relation_functor::operator()(const member_t &member)
{
  xmlNodePtr m_node = xmlNewChild(xml_node,NULL,BAD_CAST "member", NULL);
  gchar str[G_ASCII_DTOSTR_BUF_SIZE];
  g_snprintf(str, sizeof(str), ITEM_ID_FORMAT, member.object.obj->id);

  switch(member.object.type) {
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

  if(member.role)
    xmlNewProp(m_node, BAD_CAST "role", BAD_CAST member.role);
  else
    xmlNewProp(m_node, BAD_CAST "role", BAD_CAST "");
}

/* build xml representation for a relation */
xmlChar *relation_t::generate_xml(item_id_t changeset) {
  char str[32];

  xmlNodePtr xml_node;
  xmlDocPtr doc = osm_generate_xml_init(&xml_node, "relation");

  snprintf(str, sizeof(str), ITEM_ID_FORMAT, id);
  xmlNewProp(xml_node, BAD_CAST "id", BAD_CAST str);
  snprintf(str, sizeof(str), ITEM_ID_FORMAT, version);
  xmlNewProp(xml_node, BAD_CAST "version", BAD_CAST str);
  snprintf(str, sizeof(str), "%u", (unsigned)changeset);
  xmlNewProp(xml_node, BAD_CAST "changeset", BAD_CAST str);

  std::for_each(members.begin(), members.end(), gen_xml_relation_functor(xml_node));
  osm_generate_tags(tag, xml_node);

  return osm_generate_xml_finish(doc);
}

/* build xml representation for a changeset */
xmlChar *osm_generate_xml_changeset(const char *comment) {
  xmlChar *result = NULL;
  int len = 0;

  /* tags for this changeset */
  tag_t tag_comment(const_cast<char*>("comment"), const_cast<char *>(comment));
  tag_t tag_creator(const_cast<char*>("created_by"),
                    const_cast<char*>(PACKAGE " v" VERSION));
  tag_creator.next = &tag_comment;

  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "osm");
  xmlDocSetRootElement(doc, root_node);

  xmlNodePtr cs_node = xmlNewChild(root_node, NULL, BAD_CAST "changeset", NULL);
  osm_generate_tags(&tag_creator, cs_node);

  xmlDocDumpFormatMemoryEnc(doc, &result, &len, "UTF-8", 1);
  xmlFreeDoc(doc);

  return result;
}

/* ---------- edit functions ------------- */

template<typename T> item_id_t osm_new_id(const std::map<item_id_t, T *> &map) {
  item_id_t id = -1;
  const typename std::map<item_id_t, T *>::const_iterator itEnd = map.end();

  while(true) {
    const typename std::map<item_id_t, T *>::const_iterator it = map.find(id);
    /* no such id so far -> use it */
    if(it == itEnd)
      return id;

    id--;
  }
}

static item_id_t osm_new_way_id(const osm_t *osm) {
  return osm_new_id<way_t>(osm->ways);
}

static item_id_t osm_new_node_id(const osm_t *osm) {
  return osm_new_id<node_t>(osm->nodes);
}

static item_id_t osm_new_relation_id(const osm_t *osm) {
  return osm_new_id<relation_t>(osm->relations);
}

node_t *osm_t::node_new(gint x, gint y) {
  printf("Creating new node\n");

  node_t *node = new node_t();
  node->version = 1;
  node->lpos.x = x;
  node->lpos.y = y;
  node->visible = TRUE;
  node->time = time(NULL);

  /* convert screen position back to ll */
  lpos2pos(bounds, &node->lpos, &node->pos);

  printf("  new at %d %d (%f %f)\n",
	 node->lpos.x, node->lpos.y, node->pos.lat, node->pos.lon);

  return node;
}

node_t *osm_t::node_new(const pos_t *pos) {
  printf("Creating new node from lat/lon\n");

  node_t *node = new node_t();
  node->version = 1;
  node->pos = *pos;
  node->visible = TRUE;
  node->time = time(NULL);

  /* convert ll position to screen */
  pos2lpos(bounds, &node->pos, &node->lpos);

  printf("  new at %d %d (%f %f)\n",
	 node->lpos.x, node->lpos.y, node->pos.lat, node->pos.lon);

  return node;
}


void osm_t::node_attach(node_t *node) {
  printf("Attaching node\n");

  node->id = osm_new_node_id(this);
  node->flags = OSM_FLAG_NEW;

  /* attach to end of node list */
  nodes[node->id] = node;
}

void osm_t::node_restore(node_t *node) {
  printf("Restoring node\n");

  /* attach to end of node list */
  nodes[node->id] = node;
}

way_t *osm_way_new(void) {
  printf("Creating new way\n");

  way_t *way = new way_t();
  way->version = 1;
  way->visible = TRUE;
  way->flags = OSM_FLAG_NEW;
  way->time = time(NULL);

  return way;
}

void osm_t::way_attach(way_t *way) {
  printf("Attaching way\n");

  way->id = osm_new_way_id(this);
  way->flags = OSM_FLAG_NEW;

  /* attach to end of way list */
  ways[way->id] = way;
}

struct way_member_ref {
  osm_t * const osm;
  node_chain_t &node_chain;
  way_member_ref(osm_t *o, node_chain_t &n) : osm(o), node_chain(n) {}
  void operator()(const item_id_chain_t &member);
};

void way_member_ref::operator()(const item_id_chain_t &member) {
  printf("Node " ITEM_ID_FORMAT " is member\n", member.id);

  node_t *node = osm->node_by_id(member.id);
  node_chain.push_back(node);
  node->ways++;

  printf("   -> %p\n", node);
}

void osm_t::way_restore(way_t *way, const std::vector<item_id_chain_t> &id_chain) {
  printf("Restoring way\n");

  /* attach to end of node list */
  ways[way->id] = way;

  /* restore node memberships by converting ids into real pointers */
  g_assert(way->node_chain.empty());
  way_member_ref fc(this, way->node_chain);
  std::for_each(id_chain.begin(), id_chain.end(), fc);

  printf("done\n");
}

/* returns pointer to chain of ways affected by this deletion */
way_chain_t osm_t::node_delete(node_t *node, bool permanently,
			    bool affect_ways) {
  way_chain_t way_chain;

  /* new nodes aren't stored on the server and are just deleted permanently */
  if(node->flags & OSM_FLAG_NEW) {
    printf("About to delete NEW node #" ITEM_ID_FORMAT
	   " -> force permanent delete\n", node->id);
    permanently = true;
  }

  /* first remove node from all ways using it */
  const std::map<item_id_t, way_t *>::iterator itEnd = ways.end();
  for (std::map<item_id_t, way_t *>::iterator it = ways.begin(); it != itEnd; it++) {
    way_t * const way = it->second;
    node_chain_t &chain = way->node_chain;
    bool modified = false;

    node_chain_t::iterator cit = chain.begin();
    while((cit = std::find(cit, chain.end(), node)) != chain.end()) {
      /* remove node from chain */
      modified = true;
      if(affect_ways)
        cit = chain.erase(cit);
      else
        /* only record that there has been a change */
        break;
    }

    if(modified) {
      way->flags |= OSM_FLAG_DIRTY;

      /* and add the way to the list of affected ways */
      way_chain.push_back(way);
    }
  }

  /* remove that nodes map representations */
  if(node->map_item_chain)
    map_item_chain_destroy(&node->map_item_chain);

  if(!permanently) {
    printf("mark node #" ITEM_ID_FORMAT " as deleted\n", node->id);
    node->flags |= OSM_FLAG_DELETED;
  } else {
    printf("permanently delete node #" ITEM_ID_FORMAT "\n", node->id);

    /* remove it from the chain */
    std::map<item_id_t, node_t *>::iterator it = nodes.find(node->id);
    g_assert(it != nodes.end());

    node_free(it->second);
  }

  return way_chain;
}

/* return all relations a node is in */
static relation_chain_t osm_node_to_relation(const osm_t *osm, const node_t *node,
				       gboolean via_way) {
  relation_chain_t rel_chain;

  const std::map<item_id_t, relation_t *>::const_iterator ritEnd = osm->relations.end();
  for(std::map<item_id_t, relation_t *>::const_iterator rit = osm->relations.begin(); rit != ritEnd; rit++) {
    bool is_member = false;

    const std::vector<member_t>::const_iterator mitEnd = rit->second->members.end();
    for(std::vector<member_t>::const_iterator member = rit->second->members.begin();
        !is_member && member != mitEnd; member++) {
      switch(member->object.type) {
      case NODE:
	/* nodes are checked directly */
	is_member = member->object.node == node;
	break;

      case WAY:
	if(via_way)
	  /* ways have to be checked for the nodes they consist of */
	  is_member = member->object.way->contains_node(node) == TRUE;
	break;

      default:
	break;
      }
    }

    /* node is a member of this relation, so move it to the member chain */
    if(is_member)
      rel_chain.push_back(rit->second);
  }

  return rel_chain;
}

struct check_member {
  const object_t object;
  check_member(const object_t &o) : object(o) {}
  bool operator()(std::pair<item_id_t, relation_t *> pair) {
    return std::find(pair.second->members.begin(), pair.second->members.end(),
                     object) != pair.second->members.end();
  }
};

/* return all relations a way is in */
relation_chain_t osm_t::to_relation(const way_t *way) const {
  return to_relation(object_t(const_cast<way_t *>(way)));
}

/* return all relations an object is in */
relation_chain_t osm_t::to_relation(const object_t &object) const {
  switch(object.type) {
  case NODE:
    return osm_node_to_relation(this, object.node, false);

  case WAY:
  case RELATION: {
    relation_chain_t rel_chain;
    check_member fc(object);

    const std::map<item_id_t, relation_t *>::const_iterator ritEnd = relations.end();
    std::map<item_id_t, relation_t *>::const_iterator rit = relations.begin();
    while((rit = std::find_if(rit, ritEnd, fc)) != ritEnd)
      /* relation is a member of this relation, so move it to the member chain */
      rel_chain.push_back(rit->second);

    return rel_chain;
  }

  default:
    return relation_chain_t();
  }
}

struct node_collector {
  way_chain_t &chain;
  const node_t * const node;
  node_collector(way_chain_t &c, const node_t *n) : chain(c), node(n) {}
  void operator()(std::pair<item_id_t, way_t *> pair) {
    if(pair.second->contains_node(node))
      chain.push_back(pair.second);
  }
};

/* return all ways a node is in */
way_chain_t osm_t::node_to_way(const node_t *node) const {
  way_chain_t chain;

  std::for_each(ways.begin(), ways.end(), node_collector(chain, node));

  return chain;
}

bool osm_t::position_within_bounds(gint x, gint y) const {
  if((x < bounds->min.x) || (x > bounds->max.x))
    return false;
  if((y < bounds->min.y) || (y > bounds->max.y))
    return false;
  return true;
}

gboolean osm_position_within_bounds_ll(const pos_t *ll_min, const pos_t *ll_max, const pos_t *pos) {
  if((pos->lat < ll_min->lat) || (pos->lat > ll_max->lat)) return FALSE;
  if((pos->lon < ll_min->lon) || (pos->lon > ll_max->lon)) return FALSE;
  return TRUE;
}

struct remove_member_functor {
  const object_t obj;
  // the second argument is to distinguish the constructor from operator()
  remove_member_functor(object_t o, bool) : obj(o) {}
  void operator()(std::pair<item_id_t, relation_t *> pair);
};

void remove_member_functor::operator()(std::pair<item_id_t, relation_t *> pair)
{
  relation_t * const relation = pair.second;
  const std::vector<member_t>::iterator itEnd = relation->members.end();
  std::vector<member_t>::iterator it = relation->members.begin();

  while((it = std::find(it, itEnd, obj)) != itEnd) {
    printf("  from relation #" ITEM_ID_FORMAT "\n", relation->id);

    osm_member_free(*it);
    it = relation->members.erase(it);

    relation->flags |= OSM_FLAG_DIRTY;
  }
}

/* remove the given node from all relations. used if the node is to */
/* be deleted */
void osm_t::remove_from_relations(node_t *node) {
  printf("removing node #" ITEM_ID_FORMAT " from all relations:\n", node->id);

  std::for_each(relations.begin(), relations.end(),
                remove_member_functor(object_t(node), false));
}

/* remove the given way from all relations */
void osm_t::remove_from_relations(way_t *way) {
  printf("removing way #" ITEM_ID_FORMAT " from all relations:\n", way->id);

  std::for_each(relations.begin(), relations.end(),
                remove_member_functor(object_t(way), false));
}

relation_t *osm_relation_new(void) {
  printf("Creating new relation\n");

  relation_t *relation = new relation_t();
  relation->version = 1;
  relation->visible = TRUE;
  relation->flags = OSM_FLAG_NEW;
  relation->time = time(NULL);

  return relation;
}

void osm_t::relation_attach(relation_t *relation) {
  printf("Attaching relation\n");

  relation->id = osm_new_relation_id(this);
  relation->flags = OSM_FLAG_NEW;

  /* attach to end of relation list */
  relations[relation->id] = relation;
}

struct osm_unref_way_free {
  osm_t * const osm;
  const way_t * const way;
  osm_unref_way_free(osm_t *o, const way_t *w) : osm(o), way(w) {}
  void operator()(node_t *node);
};

void osm_unref_way_free::operator()(node_t* node)
{
  g_assert_cmpint(node->ways, >, 0);
  node->ways--;
  printf("checking node #" ITEM_ID_FORMAT " (still used by %d)\n",
         node->id, node->ways);

  /* this node must only be part of this way */
  if(!node->ways) {
    /* delete this node, but don't let this actually affect the */
    /* associated ways as the only such way is the one we are currently */
    /* deleting */
    const way_chain_t &way_chain = osm->node_delete(node, false, false);
    g_assert(!way_chain.empty());
    /* no need in end caching here, there should only be one item in the list */
    for(way_chain_t::const_iterator it = way_chain.begin(); it != way_chain.end(); it++) {
      g_assert(*it == way);
    }
  }
}

void osm_t::way_delete(way_t *way, bool permanently) {

  /* new ways aren't stored on the server and are just deleted permanently */
  if(way->flags & OSM_FLAG_NEW) {
    printf("About to delete NEW way #" ITEM_ID_FORMAT
	   " -> force permanent delete\n", way->id);
    permanently = true;
  }

  /* delete all nodes that aren't in other use now */
  std::for_each(way->node_chain.begin(), way->node_chain.end(),
                osm_unref_way_free(this, way));
  way->node_chain.clear();

  if(!permanently) {
    printf("mark way #" ITEM_ID_FORMAT " as deleted\n", way->id);
    way->flags |= OSM_FLAG_DELETED;
  } else {
    printf("permanently delete way #" ITEM_ID_FORMAT "\n", way->id);

    /* remove it from the chain */
    std::map<item_id_t, way_t *>::iterator it = ways.find(way->id);
    g_assert(it != ways.end());

    way_free(it->second);
  }
}

void osm_t::relation_delete(relation_t *relation, bool permanently) {

  /* new relations aren't stored on the server and are just */
  /* deleted permanently */
  if(relation->flags & OSM_FLAG_NEW) {
    printf("About to delete NEW relation #" ITEM_ID_FORMAT
	   " -> force permanent delete\n", relation->id);
    permanently = TRUE;
  }

  /* the deletion of a relation doesn't affect the members as they */
  /* don't have any reference to the relation they are part of */

  if(!permanently) {
    printf("mark relation #" ITEM_ID_FORMAT " as deleted\n", relation->id);
    relation->flags |= OSM_FLAG_DELETED;
  } else {
    printf("permanently delete relation #" ITEM_ID_FORMAT "\n",
	   relation->id);

    relation_free(relation);
  }
}

void way_t::reverse() {
  std::reverse(node_chain.begin(), node_chain.end());
}

static const char *DS_ONEWAY_FWD = "yes";
static const char *DS_ONEWAY_REV = "-1";

/* Reverse direction-sensitive tags like "oneway". Marks the way as dirty if
 * anything is changed, and returns the number of flipped tags. */

unsigned int
way_t::reverse_direction_sensitive_tags() {
  unsigned int n_tags_altered = 0;
  for (tag_t *etag = tag; etag; etag = etag->next) {
    char *lc_key = g_ascii_strdown(etag->key, -1);

    if (strcmp(lc_key, "oneway") == 0) {
      char *lc_value = g_ascii_strdown(etag->value, -1);
      // oneway={yes/true/1/-1} is unusual.
      // Favour "yes" and "-1".
      if ((strcmp(lc_value, DS_ONEWAY_FWD) == 0) ||
          (strcmp(lc_value, "true") == 0) ||
          (strcmp(lc_value, "1") == 0)) {
        etag->update_value(DS_ONEWAY_REV);
        n_tags_altered++;
      }
      else if (strcmp(lc_value, DS_ONEWAY_REV) == 0) {
        etag->update_value(DS_ONEWAY_FWD);
        n_tags_altered++;
      }
      else {
        printf("warning: unknown tag: %s=%s\n", etag->key, etag->value);
      }
      g_free(lc_value);
    } else if (strcmp(lc_key, "sidewalk") == 0) {
      if (strcasecmp(etag->value, "right") == 0)
        etag->update_value("left");
      else if (strcasecmp(etag->value, "left") == 0)
        etag->update_value("right");
    } else {
      // suffixes
      static std::vector<std::pair<std::string, std::string> > rtable;
      if(rtable.empty()) {
        rtable.push_back(std::pair<std::string, std::string>(":left", ":right"));
        rtable.push_back(std::pair<std::string, std::string>(":right", ":left"));
        rtable.push_back(std::pair<std::string, std::string>(":forward", ":backward"));
        rtable.push_back(std::pair<std::string, std::string>(":backward", ":forward"));
      }

      for (unsigned int i = 0; i < rtable.size(); i++) {
        if (g_str_has_suffix(lc_key, rtable[i].first.c_str())) {
          /* length of key that will persist */
          size_t plen = strlen(etag->key) - rtable[i].first.size();
          /* add length of new suffix */
          etag->key = (char*)g_realloc(etag->key, plen + 1 + rtable[i].second.size());
          char *lastcolon = etag->key + plen;
          g_assert(*lastcolon == ':');
          /* replace suffix */
          strcpy(lastcolon, rtable[i].second.c_str());
          n_tags_altered++;
          break;
        }
      }
    }

    g_free(lc_key);
  }
  if (n_tags_altered > 0) {
    flags |= OSM_FLAG_DIRTY;
  }
  return n_tags_altered;
}

/* Reverse a way's role within relations where the role is direction-sensitive.
 * Returns the number of roles flipped, and marks any relations changed as
 * dirty. */

static const char *DS_ROUTE_FORWARD = "forward";
static const char *DS_ROUTE_REVERSE = "reverse";

struct reverse_roles {
  way_t * const way;
  unsigned int n_roles_flipped;
  reverse_roles(way_t *w) : way(w), n_roles_flipped(0) {}
  void operator()(relation_t *relation);
};

struct find_way_or_ref {
  const object_t way;
  object_t way_ref;
  find_way_or_ref(const way_t *w) : way(const_cast<way_t *>(w)) {
    way_ref.type = WAY_ID;
    way_ref.id = w->id;
  }
  bool operator()(const member_t &member) {
    return member == way || member == way_ref;
  }
};

void reverse_roles::operator()(relation_t* relation)
{
  const char *type = relation->tag->get_by_key("type");

  // Route relations; http://wiki.openstreetmap.org/wiki/Relation:route
  if (!type || strcasecmp(type, "route") != 0)
    return;

  // First find the member corresponding to our way:
  const std::vector<member_t>::iterator mitEnd = relation->members.end();
  std::vector<member_t>::iterator member = std::find_if(relation->members.begin(),
                                                        mitEnd, find_way_or_ref(way));
  g_assert(member != relation->members.end());  // osm_way_to_relation() broken?

  // Then flip its role if it's one of the direction-sensitive ones
  if (member->role == NULL) {
    printf("null role in route relation -> ignore\n");
  } else if (strcasecmp(member->role, DS_ROUTE_FORWARD) == 0) {
    g_free(member->role);
    member->role = g_strdup(DS_ROUTE_REVERSE);
    relation->flags |= OSM_FLAG_DIRTY;
    ++n_roles_flipped;
  } else if (strcasecmp(member->role, DS_ROUTE_REVERSE) == 0) {
    g_free(member->role);
    member->role = g_strdup(DS_ROUTE_FORWARD);
    relation->flags |= OSM_FLAG_DIRTY;
    ++n_roles_flipped;
  }

  // TODO: what about numbered stops? Guess we ignore them; there's no
  // consensus about whether they should be placed on the way or to one side
  // of it.
}

unsigned int
way_t::reverse_direction_sensitive_roles(osm_t *osm) {
  const relation_chain_t &rchain = osm->to_relation(this);

  reverse_roles context(this);
  std::for_each(rchain.begin(), rchain.end(), context);

  return context.n_roles_flipped;
}

const node_t *way_t::first_node() const {
  if(node_chain.empty())
    return 0;

  return node_chain.front();
}

const node_t *way_t::last_node() const {
  if(node_chain.empty())
    return 0;

  return node_chain.back();
}

bool way_t::is_closed() const {
  if(node_chain.empty())
    return false;
  return node_chain.front() == node_chain.back();
}

void way_t::rotate(node_chain_t::iterator nfirst) {
  if(nfirst == node_chain.begin())
    return;

  std::rotate(node_chain.begin(), nfirst, node_chain.end());
}

std::vector<tag_t> osm_tags_list_copy(const tag_t *tag) {
  std::vector<tag_t> new_tags;

  for(const tag_t *src_tag = tag; src_tag; src_tag = src_tag->next) {
    if(!src_tag->is_creator_tag()) {
      tag_t dst_tag(g_strdup(src_tag->key), g_strdup(src_tag->value));
      new_tags.push_back(dst_tag);
    }
  }

  return new_tags;
}

tag_t *osm_tags_list_copy(const std::vector<tag_t> &tags) {
  const std::vector<tag_t>::const_reverse_iterator ritEnd = tags.rend();
  tag_t *new_tags = 0;

  for(std::vector<tag_t>::const_reverse_iterator rit = tags.rbegin();
      rit != ritEnd; rit++) {
    tag_t *n = g_new0(tag_t, 1);
    n->key = g_strdup(rit->key);
    n->value = g_strdup(rit->value);
    n->next = new_tags;
    new_tags = n;
  }

  return new_tags;
}

tag_t *osm_tags_copy(const tag_t *src_tag) {
  tag_t *new_tags = NULL;
  tag_t **dst_tag = &new_tags;

  for(; src_tag; src_tag = src_tag->next) {
    if(!src_tag->is_creator_tag()) {
      *dst_tag = g_new0(tag_t, 1);
      (*dst_tag)->key = g_strdup(src_tag->key);
      (*dst_tag)->value = g_strdup(src_tag->value);
      dst_tag = &(*dst_tag)->next;
    }
  }

  return new_tags;
}

/* try to get an as "speaking" description of the object as possible */
char *object_t::get_name() const {
  char *ret = NULL;
  const tag_t *tags = get_tags();

  /* worst case: we have no tags at all. return techincal info then */
  if(!tags)
    return g_strconcat("unspecified ", type_string(), NULL);

  /* try to figure out _what_ this is */

  const char *name = tags->get_by_key("name");
  if(!name) name = tags->get_by_key("ref");
  if(!name) name = tags->get_by_key("note");
  if(!name) name = tags->get_by_key("fix" "me");
  if(!name) name = tags->get_by_key("sport");

  /* search for some kind of "type" */
  const char *type = tags->get_by_key("amenity");
  gchar *gtype = NULL;
  if(!type) type = tags->get_by_key("place");
  if(!type) type = tags->get_by_key("historic");
  if(!type) type = tags->get_by_key("leisure");
  if(!type) type = tags->get_by_key("tourism");
  if(!type) type = tags->get_by_key("landuse");
  if(!type) type = tags->get_by_key("waterway");
  if(!type) type = tags->get_by_key("railway");
  if(!type) type = tags->get_by_key("natural");
  if(!type && tags->get_by_key("building")) {
    const char *street = tags->get_by_key("addr:street");
    const char *hn = tags->get_by_key("addr:housenumber");
    type = "building";

    if(street && hn) {
      if(hn)
        type = gtype = g_strjoin(" ", "building", street, hn, NULL);
    } else if(hn) {
      type = gtype = g_strconcat("building housenumber ", hn, NULL);
    } else if(!name)
      name = tags->get_by_key("addr:housename");
  }
  if(!type) type = tags->get_by_key("emergency");

  /* highways are a little bit difficult */
  const char *highway = tags->get_by_key("highway");
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
	  type_string(), ": \"", name, "\"", NULL);
  else
    ret = g_strconcat("unspecified ", type_string(), NULL);

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

bool tag_t::is_creator_tag() const {
  return (strcasecmp(key, "created_by") == 0);
}

base_object_t::base_object_t()
{
  memset(this, 0, sizeof(*this));
}

const char* base_object_t::get_value(const char* key) const
{
  return tag->get_by_key(key);
}

bool base_object_t::has_tag() const
{
  tag_t *t = tag;

  /* created_by tags don't count as real tags */
  if(t && t->is_creator_tag())
    t = t->next;

  return t != NULL;
}

bool base_object_t::has_value(const char* str) const
{
  for(const tag_t *t = tag; t; t = t->next) {
    if(t->value && strcasecmp(t->value, str) == 0)
      return true;
  }
  return false;
}

way_t::way_t()
  : base_object_t()
  , map_item_chain(0)
  , node_chain(0)
{
  memset(&draw, 0, sizeof(draw));
}

bool way_t::contains_node(const node_t *node) const
{
  /* return true if node is part of way */
  return std::find(node_chain.begin(), node_chain.end(), node) != node_chain.end();
}

void way_t::append_node(node_t *node) {
  node_chain.push_back(node);
  node->ways++;
}

bool way_t::ends_with_node(const node_t *node) const
{
  /* and deleted way may even not contain any nodes at all */
  /* so ignore it */
  if(flags & OSM_FLAG_DELETED)
    return false;

  /* any valid way must have at least two nodes */
  g_assert(!node_chain.empty());

  if(node_chain.front() == node)
    return true;

  if(node_chain.back() == node)
    return true;

  return false;
}

void way_t::cleanup() {
  //  printf("freeing way #" ITEM_ID_FORMAT "\n", OSM_ID(way));

  osm_node_chain_free(node_chain);
  osm_tags_free(tag);

  /* there must not be anything left in this chain */
  g_assert(!map_item_chain);
}

member_t::member_t(type_t t)
  : role(0)
{
  object.type = t;
}

member_t::operator bool() const
{
  return object.type != ILLEGAL;
}

bool member_t::operator==(const member_t &other) const
{
  if(object != other.object)
    return false;

  return strcmp(role, other.role) == 0;
}

relation_t::relation_t()
  : base_object_t()
{
}

struct find_member_object_functor {
  const object_t &object;
  find_member_object_functor(const object_t &o) : object(o) {}
  bool operator()(const member_t &member) {
    return member.object == object;
  }
};

std::vector<member_t>::iterator relation_t::find_member_object(const object_t &o) {
  return std::find_if(members.begin(), members.end(), find_member_object_functor(o));
}
std::vector<member_t>::const_iterator relation_t::find_member_object(const object_t &o) const {
  return std::find_if(members.begin(), members.end(), find_member_object_functor(o));
}

struct member_counter {
  guint *nodes, *ways, *relations;
  member_counter(guint *n, guint *w, guint *r) : nodes(n), ways(w), relations(r) {
    *n = 0; *w = 0; *r = 0;
  }
  void operator()(const member_t &member);
};

void member_counter::operator()(const member_t &member)
{
  switch(member.object.type) {
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

void relation_t::members_by_type(guint *nodes, guint *ways, guint *relations) const {
  std::for_each(members.begin(), members.end(),
                member_counter(nodes, ways, relations));
}

node_t::node_t()
  : base_object_t()
  , ways(0)
  , zoom_max(0.0)
  , icon_buf(0)
  , map_item_chain(0)
{
  memset(&pos, 0, sizeof(pos));
  memset(&lpos, 0, sizeof(lpos));
}

template<typename T> T *osm_find_by_id(const std::map<item_id_t, T *> &map, item_id_t id) {
  const typename std::map<item_id_t, T *>::const_iterator it = map.find(id);
  if(it != map.end())
    return it->second;

  return 0;
}

node_t *osm_t::node_by_id(item_id_t id) {
  return osm_find_by_id<node_t>(nodes, id);
}

way_t *osm_t::way_by_id(item_id_t id) {
  return osm_find_by_id<way_t>(ways, id);
}

relation_t *osm_t::relation_by_id(item_id_t id) {
  return osm_find_by_id<relation_t>(relations, id);
}

// vim:et:ts=8:sw=2:sts=2:ai
