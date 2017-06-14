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
#include "pos.h"

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

#include <osm2go_cpp.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

bool object_t::operator==(const object_t &other) const
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

  return O2G_NULLPTR;
}

std::string object_t::id_string() const {
  // long enough for every int64
  char buf[32] = { 0 };

  switch(type) {
  case ILLEGAL:
    break;
  case NODE:
  case WAY:
  case RELATION:
    snprintf(buf, sizeof(buf), ITEM_ID_FORMAT, obj->id);
    break;
  case NODE_ID:
  case WAY_ID:
  case RELATION_ID:
    snprintf(buf, sizeof(buf), ITEM_ID_FORMAT, id);
    break;
  default:
    g_assert_not_reached();
    break;
  }

  return buf;
}

const char *object_t::get_tag_value(const char *key) const {
  if(!is_real())
    return O2G_NULLPTR;
  return obj->tags.get_value(key);
}

bool object_t::has_tags() const {
  if(!is_real())
    return false;
  return obj->tags.hasRealTags();
}

item_id_t object_t::get_id() const {
  if(G_UNLIKELY(type == ILLEGAL))
    return ID_ILLEGAL;
  if(is_real())
    return obj->id;
  return id;
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
    return O2G_NULLPTR;

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
    std::vector<std::string>::const_iterator it = std::find_if(cbegin(osm->anonusers),
                                                               itEnd, cmp_user(name));
    if(it != itEnd)
      return it->c_str();
    osm->anonusers.push_back(name);
    return osm->anonusers.back().c_str();
  }
}

static
time_t convert_iso8601(const char *str) {
  if(!str) return 0;

  struct tm ctime;
  memset(&ctime, 0, sizeof(ctime));
  strptime(str, "%FT%T%z", &ctime);

  long gmtoff = ctime.tm_gmtoff;

  return timegm(&ctime) - gmtoff;
}

/* -------------------- tag handling ----------------------- */

void osm_tag_free(tag_t &tag) {
  g_free(tag.key);
  g_free(tag.value);
}

/**
 * @brief fill tag_t from XML values
 * @param k the key found in XML
 * @param v the value found in XML
 * @param tags the vector where the new tag will be added
 * @return if a new tag was added
 * @retval false if k and v were not empty
 *
 * k and v will be freed.
 */
static bool tag_from_xml(xmlChar *k, xmlChar *v, std::vector<tag_t> &tags) {
  bool ret = false;
  const char *key = reinterpret_cast<char *>(k);
  const char *value = reinterpret_cast<char *>(v);

  if(G_LIKELY(key && value && strlen(key) > 0 &&
                              strlen(value) > 0)) {
    ret = true;
    tags.push_back(tag_t(g_strdup(key), g_strdup(value)));
  } else {
    printf("incomplete tag key/value %s/%s\n", k, v);
  }

  xmlFree(k);
  xmlFree(v);

  return ret;
}

bool osm_t::parse_tag(xmlNode *a_node, TagMap &tags) {
  xmlChar *key = xmlGetProp(a_node, BAD_CAST "k");
  xmlChar *value = xmlGetProp(a_node, BAD_CAST "v");

  if(G_UNLIKELY(!key || !value || strlen(reinterpret_cast<char *>(key)) == 0 ||
                                  strlen(reinterpret_cast<char *>(value)) == 0)) {
    xmlFree(key);
    xmlFree(value);
    return false;
  }

  std::string k = reinterpret_cast<char *>(key);
  std::string v = reinterpret_cast<char *>(value);

  xmlFree(key);
  xmlFree(value);

  if(G_UNLIKELY(findTag(tags, k, v) != tags.end()))
    return false;

  tags.insert(TagMap::value_type(k, v));

  return true;
}

struct map_value_match_functor {
  const std::string &value;
  map_value_match_functor(const std::string &v) : value(v) {}
  bool operator()(const osm_t::TagMap::value_type &pair) {
    return pair.second == value;
  }
};

osm_t::TagMap::iterator osm_t::findTag(TagMap &map, const std::string &key, const std::string &value)
{
  std::pair<osm_t::TagMap::iterator, osm_t::TagMap::iterator> matches = map.equal_range(key);
  if(matches.first == matches.second)
    return map.end();
  osm_t::TagMap::iterator it = std::find_if(matches.first, matches.second, map_value_match_functor(value));
  return it == matches.second ? map.end() : it;
}

bool osm_t::tagSubset(const TagMap &sub, const TagMap &super)
{
  const TagMap::const_iterator superEnd = super.end();
  const TagMap::const_iterator itEnd = sub.end();
  for(TagMap::const_iterator it = sub.begin(); it != itEnd; it++)
    if(osm_t::findTag(super, it->first, it->second) == superEnd)
      return false;
  return true;
}

struct tag_match_functor {
  const tag_t &other;
  const bool same_values;
  tag_match_functor(const tag_t &o, bool s) : other(o), same_values(s) {}
  bool operator()(const tag_t &tag) {
    return (strcasecmp(other.key, tag.key) == 0) &&
           ((strcasecmp(other.value, tag.value) == 0) == same_values);
  }
};

bool tag_list_t::merge(tag_list_t &other)
{
  if(other.empty())
    return false;

  if(empty()) {
    delete contents; // just to be sure not to leak if an empty vector is around
    contents = other.contents;
    other.contents = 0;
    return false;
  }

  bool conflict = false;

  /* ---------- transfer tags from way[1] to way[0] ----------- */
  const std::vector<tag_t>::const_iterator itEnd = other.contents->end();
  for(std::vector<tag_t>::iterator srcIt = other.contents->begin();
      srcIt != itEnd; srcIt++) {
    tag_t &src = *srcIt;
    /* don't copy "created_by" tag or tags that already */
    /* exist in identical form */
    if(src.is_creator_tag() || contains(tag_match_functor(src, true))) {
      osm_tag_free(src);
    } else {
      /* check if same key but with different value is present */
      if(!conflict)
        conflict = contains(tag_match_functor(src, false));
      contents->push_back(src);
    }
  }

  delete other.contents;
  other.contents = O2G_NULLPTR;

  return conflict;
}

static inline bool is_creator_tag(const tag_t &tag) {
  return tag.is_creator_tag();
}

struct tag_find_functor {
  const char * const needle;
  tag_find_functor(const char *n) : needle(n) {}
  bool operator()(const tag_t &tag) {
    return (strcmp(needle, tag.key) == 0);
  }
};

bool tag_list_t::operator!=(const std::vector<tag_t> &t2) const {
  if(empty() && t2.empty())
    return false;

  // Special case for an empty list as contents is not set in this case and
  // must not be dereferenced. Check if t2 only consists of a creator tag, in
  // which case both lists would still be considered the same, or not. Not
  // further checks need to be done for the end result.
  const std::vector<tag_t>::const_iterator t2start = t2.begin();
  const std::vector<tag_t>::const_iterator t2End = t2.end();
  bool t2HasCreator = (std::find_if(t2start, t2End, is_creator_tag) != t2End);
  if(empty())
    return t2HasCreator ? (t2.size() != 1) : !t2.empty();

  /* first check list length, otherwise deleted tags are hard to detect */
  std::vector<tag_t>::size_type ocnt = contents->size();
  std::vector<tag_t>::const_iterator t1it = contents->begin();
  const std::vector<tag_t>::const_iterator t1End = contents->end();
  const std::vector<tag_t>::const_iterator t1cit = std::find_if(t1it, t1End, is_creator_tag);

  if(t2HasCreator)
    ocnt++;

  // ocnt can't become negative here as it was checked before that contents is not empty
  if(t1cit != t1End)
    ocnt--;

  if (t2.size() != ocnt)
    return true;

  for (; t1it != t1End; t1it++) {
    if (t1it == t1cit)
      continue;
    const tag_t &ntag = *t1it;

    const std::vector<tag_t>::const_iterator it = std::find_if(t2start, t2End,
                                                               tag_find_functor(ntag.key));

    // key not found
    if(it == t2End)
      return true;
    // different value
    if(strcmp(ntag.value, it->value) != 0)
      return true;
  }

  return false;
}

bool tag_list_t::operator!=(const osm_t::TagMap &t2) const {
  if(empty() && t2.empty())
    return false;

  // Special case for an empty list as contents is not set in this case and
  // must not be dereferenced. Check if t2 only consists of a creator tag, in
  // which case both lists would still be considered the same, or not. Not
  // further checks need to be done for the end result.
  const osm_t::TagMap::const_iterator t2End = t2.end();
  bool t2HasCreator = (t2.find("created_by") != t2End);
  if(empty())
    return t2HasCreator ? (t2.size() != 1) : !t2.empty();

  /* first check list length, otherwise deleted tags are hard to detect */
  std::vector<tag_t>::size_type ocnt = contents->size();
  std::vector<tag_t>::const_iterator t1it = contents->begin();
  const std::vector<tag_t>::const_iterator t1End = contents->end();
  const std::vector<tag_t>::const_iterator t1cit = std::find_if(t1it, t1End, is_creator_tag);

  if(t2HasCreator)
    ocnt++;

  // ocnt can't become negative here as it was checked before that contents is not empty
  if(t1cit != t1End)
    ocnt--;

  if (t2.size() != ocnt)
    return true;

  for (; t1it != t1End; t1it++) {
    if (t1it == t1cit)
      continue;
    const tag_t &ntag = *t1it;

    std::pair<osm_t::TagMap::const_iterator, osm_t::TagMap::const_iterator> its = t2.equal_range(ntag.key);

    // key not found
    if(its.first == its.second)
      return true;
    // check different values
    for(; its.first != its.second; its.first++)
      if(its.first->second == ntag.value)
        break;
    // none of the values matched
    if(its.first == its.second)
      return true;
  }

  return false;
}

struct collision_functor {
  const tag_t &tag;
  collision_functor(const tag_t &t) : tag(t) { }
  bool operator()(const tag_t &t) {
    return (strcasecmp(t.key, tag.key) == 0);
  }
};

bool tag_list_t::hasTagCollisions() const
{
  if(empty())
    return false;

  const std::vector<tag_t>::const_iterator itEnd = contents->end();
  for(std::vector<tag_t>::const_iterator it = contents->begin();
      it + 1 != itEnd; it++) {
    if (std::find_if(it + 1, itEnd, collision_functor(*it)) != itEnd)
      return true;
  }
  return false;
}

void tag_t::update_value(const char *nvalue)
{
  const size_t nlen = strlen(nvalue) + 1;
  value = static_cast<char *>(g_realloc(value, nlen));
  memcpy(value, nvalue, nlen);
}

/* ------------------- node handling ------------------- */

void osm_t::node_free(node_t *node) {
  nodes.erase(node->id);

  /* there must not be anything left in this chain */
  g_assert_null(node->map_item_chain);

  delete node;
}

static inline void nodefree(std::pair<item_id_t, node_t *> pair) {
  delete pair.second;
}

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

node_t *osm_t::parse_way_nd(xmlNode *a_node) const {
  xmlChar *prop;
  node_t *node = O2G_NULLPTR;

  if((prop = xmlGetProp(a_node, BAD_CAST "ref"))) {
    item_id_t id = strtoll((char*)prop, O2G_NULLPTR, 10);

    /* search matching node */
    node = node_by_id(id);
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

bool relation_t::is_multipolygon() const {
  const char *tp = tags.get_value("type");
  return tp && (strcmp(tp, "multipolygon") == 0);
}

void relation_t::cleanup() {
  tags.clear();
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

member_t osm_t::parse_relation_member(xmlNode *a_node) {
  xmlChar *prop;
  member_t member(ILLEGAL);

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
    item_id_t id = strtoll((char*)prop, O2G_NULLPTR, 10);

    switch(member.object.type) {
    case ILLEGAL:
      printf("Unable to store illegal type\n");
      return member;

    case WAY:
      /* search matching way */
      member.object.way = way_by_id(id);
      if(!member.object.way) {
	member.object.type = WAY_ID;
	member.object.id = id;
      }
      break;

    case NODE:
      /* search matching node */
      member.object.node = node_by_id(id);
      if(!member.object.node) {
	member.object.type = NODE_ID;
	member.object.id = id;
      }
      break;

    case RELATION:
      /* search matching relation */
      member.object.relation = relation_by_id(id);
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
std::string relation_t::descriptive_name() const {
  const char *keys[] = { "ref", "name", "description", "note", "fix" "me", O2G_NULLPTR};
  for (unsigned int i = 0; keys[i] != O2G_NULLPTR; i++) {
    const char *name = tags.get_value(keys[i]);
    if(name)
      return name;
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "<ID #" ITEM_ID_FORMAT ">", id);
  return buf;
}

/* -------------------------- stream parser ------------------- */

#include <libxml/xmlreader.h>

static inline gint __attribute__((nonnull(2))) my_strcmp(const xmlChar *a, const xmlChar *b) {
  if(!a) return -1;
  return strcmp((char*)a,(char*)b);
}

/* skip current element incl. everything below (mainly for testing) */
/* returns FALSE if something failed */
static void skip_element(xmlTextReaderPtr reader) {
  g_assert(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT);
  const xmlChar *name = xmlTextReaderConstName(reader);
  g_assert_nonnull(name);
  int depth = xmlTextReaderDepth(reader);

  if(xmlTextReaderIsEmptyElement(reader))
    return;

  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) > depth) ||
	 (my_strcmp(xmlTextReaderConstName(reader), name) != 0))) {
    ret = xmlTextReaderRead(reader);
  }
}

static pos_float_t xml_reader_attr_float(xmlTextReaderPtr reader, const char *name) {
  xmlChar *prop = xmlTextReaderGetAttribute(reader, BAD_CAST name);
  pos_float_t ret;

  if((prop)) {
    ret = g_ascii_strtod((gchar *)prop, O2G_NULLPTR);
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
    errorf(O2G_NULLPTR, "Invalid coordinate in bounds (%f/%f/%f/%f)",
	   bounds->ll_min.lat, bounds->ll_min.lon,
	   bounds->ll_max.lat, bounds->ll_max.lon);

    return FALSE;
  }

  /* skip everything below */
  skip_element(reader);

  /* calculate map zone which will be used as a reference for all */
  /* drawing/projection later on */
  pos_t center((bounds->ll_max.lat + bounds->ll_min.lat)/2,
               (bounds->ll_max.lon + bounds->ll_min.lon)/2);

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

static void process_tag(xmlTextReaderPtr reader, std::vector<tag_t> &tags) {
  if(tag_from_xml(xmlTextReaderGetAttribute(reader, BAD_CAST "k"),
                  xmlTextReaderGetAttribute(reader, BAD_CAST "v"), tags))
    skip_element(reader);
}

static void process_base_attributes(base_object_t *obj, xmlTextReaderPtr reader, osm_t *osm)
{
  xmlChar *prop;
  if(G_LIKELY((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "id")))) {
    obj->id = strtoll((char*)prop, O2G_NULLPTR, 10);
    xmlFree(prop);
  }

  /* new in api 0.6: */
  if(G_LIKELY((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "version")))) {
    obj->version = strtoul((char*)prop, O2G_NULLPTR, 10);
    xmlFree(prop);
  }

  if(G_LIKELY((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "user")))) {
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

  if(G_LIKELY((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "timestamp")))) {
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
  std::vector<tag_t> tags;
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const char *subname = (const char*)xmlTextReaderConstName(reader);
      if(strcmp(subname, "tag") == 0) {
        process_tag(reader, tags);
      } else
	skip_element(reader);
    }

    ret = xmlTextReaderRead(reader);
  }
  node->tags.replace(tags);

  return node;
}

static node_t *process_nd(xmlTextReaderPtr reader, osm_t *osm) {
  xmlChar *prop;

  if((prop = xmlTextReaderGetAttribute(reader, BAD_CAST "ref"))) {
    item_id_t id = strtoll((char*)prop, O2G_NULLPTR, 10);
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
  return O2G_NULLPTR;
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
  std::vector<tag_t> tags;
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
        process_tag(reader, tags);
      } else
	skip_element(reader);
    }
    ret = xmlTextReaderRead(reader);
  }
  way->tags.replace(tags);

  return way;
}

static bool process_member(xmlTextReaderPtr reader, osm_t *osm, std::vector<member_t> &members) {
  char *prop;
  member_t member(ILLEGAL);

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "type"))) {
    if(strcmp(prop, "way") == 0)
      member.object.type = WAY;
    else if(strcmp(prop, "node") == 0)
      member.object.type = NODE;
    else if(strcmp(prop, "relation") == 0)
      member.object.type = RELATION;
    else {
      printf("Unable to store illegal type '%s'\n", prop);
      xmlFree(prop);
      return false;
    }
    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "ref"))) {
    item_id_t id = strtoll(prop, O2G_NULLPTR, 10);

    switch(member.object.type) {
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
    default:
      g_assert_not_reached();
    }

    xmlFree(prop);
  }

  if((prop = (char*)xmlTextReaderGetAttribute(reader, BAD_CAST "role"))) {
    if(strlen(prop) > 0) member.role = g_strdup(prop);
    xmlFree(prop);
  }

  members.push_back(member);
  return true;
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
  std::vector<tag_t> tags;
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const char *subname = (const char*)xmlTextReaderConstName(reader);
      if(strcmp(subname, "member") == 0) {
        process_member(reader, osm, relation->members);
      } else if(strcmp(subname, "tag") == 0) {
        process_tag(reader, tags);
      } else
	skip_element(reader);
    }
    ret = xmlTextReaderRead(reader);
  }
  relation->tags.replace(tags);

  return relation;
}

static osm_t *process_osm(xmlTextReaderPtr reader) {
  /* alloc osm structure */
  osm_t *osm = new osm_t();

  /* no attributes of interest */

  const xmlChar *name = xmlTextReaderConstName(reader);
  g_assert_nonnull(name);

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
  return O2G_NULLPTR;
}

static osm_t *process_file(const char *filename) {
  osm_t *osm = O2G_NULLPTR;
  xmlTextReaderPtr reader;

  reader = xmlReaderForFile(filename, O2G_NULLPTR, 0);
  if (G_LIKELY(reader != O2G_NULLPTR)) {
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

osm_t *osm_t::parse(const std::string &path, const std::string &filename, icon_t **icon) {

  struct timeval start;
  gettimeofday(&start, O2G_NULLPTR);

  // use stream parser
  osm_t *osm = O2G_NULLPTR;
  if(filename[0] == '/')
    osm = process_file(filename.c_str());
  else {
    const std::string full = path + filename;
    osm = process_file(full.c_str());
  }

  struct timeval end;
  gettimeofday(&end, O2G_NULLPTR);

  if(osm)
    osm->icons = icon;

  printf("total parse time: %ldms\n",
	 (end.tv_usec - start.tv_usec)/1000 +
	 (end.tv_sec - start.tv_sec)*1000);

  return osm;
}

const char *osm_t::sanity_check() const {
  if(G_UNLIKELY(!bounds))
    return _("Invalid data in OSM file:\nBoundary box missing!");

  if(G_UNLIKELY(nodes.empty()))
    return _("Invalid data in OSM file:\nNo drawable content found!");

  return O2G_NULLPTR;
}

/* ------------------------- misc access functions -------------- */

struct tag_to_xml {
  xmlNodePtr const node;
  const bool keep_created;
  tag_to_xml(xmlNodePtr n, bool k = false) : node(n), keep_created(k) {}
  void operator()(const tag_t &tag) {
    /* skip "created_by" tags as they aren't needed anymore with api 0.6 */
    if(G_LIKELY(keep_created || !tag.is_creator_tag())) {
      xmlNodePtr tag_node = xmlNewChild(node, O2G_NULLPTR, BAD_CAST "tag", O2G_NULLPTR);
      xmlNewProp(tag_node, BAD_CAST "k", BAD_CAST tag.key);
      xmlNewProp(tag_node, BAD_CAST "v", BAD_CAST tag.value);
    }
  }
};

static xmlDocPtr
osm_generate_xml_init(xmlNodePtr *node, const char *node_name)
{
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(O2G_NULLPTR, BAD_CAST "osm");
  xmlDocSetRootElement(doc, root_node);

  *node = xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST node_name, O2G_NULLPTR);

  return doc;
}

static xmlChar *
osm_generate_xml_finish(xmlDocPtr doc)
{
  xmlChar *result = O2G_NULLPTR;
  int len = 0;

  xmlDocDumpFormatMemoryEnc(doc, &result, &len, "UTF-8", 1);
  xmlFreeDoc(doc);

  return result;
}

/* build xml representation for a node */
xmlChar *node_t::generate_xml(item_id_t changeset) const {
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
  tags.for_each(tag_to_xml(xml_node));

  return osm_generate_xml_finish(doc);
}

struct add_xml_node_refs {
  xmlNodePtr const way_node;
  add_xml_node_refs(xmlNodePtr n) : way_node(n) {}
  void operator()(const node_t *node);
};

void add_xml_node_refs::operator()(const node_t* node)
{
  xmlNodePtr nd_node = xmlNewChild(way_node, O2G_NULLPTR, BAD_CAST "nd", O2G_NULLPTR);
  gchar str[G_ASCII_DTOSTR_BUF_SIZE];
  g_snprintf(str, sizeof(str), ITEM_ID_FORMAT, node->id);
  xmlNewProp(nd_node, BAD_CAST "ref", BAD_CAST str);
}

/**
 * @brief write the referenced nodes of a way to XML
 * @param way_node the XML node of the way to append to
 */
void way_t::write_node_chain(xmlNodePtr way_node) const {
  std::for_each(node_chain.begin(), node_chain.end(), add_xml_node_refs(way_node));
}

/* build xml representation for a way */
xmlChar *way_t::generate_xml(item_id_t changeset) const {
  char str[32];

  xmlNodePtr xml_node;
  xmlDocPtr doc = osm_generate_xml_init(&xml_node, "way");

  snprintf(str, sizeof(str), ITEM_ID_FORMAT, id);
  xmlNewProp(xml_node, BAD_CAST "id", BAD_CAST str);
  snprintf(str, sizeof(str), ITEM_ID_FORMAT, version);
  xmlNewProp(xml_node, BAD_CAST "version", BAD_CAST str);
  snprintf(str, sizeof(str), "%u", (unsigned)changeset);
  xmlNewProp(xml_node, BAD_CAST "changeset", BAD_CAST str);

  write_node_chain(xml_node);
  tags.for_each(tag_to_xml(xml_node));

  return osm_generate_xml_finish(doc);
}

struct gen_xml_relation_functor {
  xmlNodePtr const xml_node;
  gen_xml_relation_functor(xmlNodePtr n) : xml_node(n) {}
  void operator()(const member_t &member);
};

void gen_xml_relation_functor::operator()(const member_t &member)
{
  xmlNodePtr m_node = xmlNewChild(xml_node,O2G_NULLPTR,BAD_CAST "member", O2G_NULLPTR);

  const char *typestr;
  switch(member.object.type) {
  case NODE:
  case NODE_ID:
    typestr = "node";
    break;
  case WAY:
  case WAY_ID:
    typestr = "way";
    break;
  case RELATION:
  case RELATION_ID:
    typestr = "relation";
    break;
  default:
    g_assert_not_reached();
    return;
  }

  xmlNewProp(m_node, BAD_CAST "type", BAD_CAST typestr);
  xmlNewProp(m_node, BAD_CAST "ref", BAD_CAST member.object.id_string().c_str());

  if(member.role)
    xmlNewProp(m_node, BAD_CAST "role", BAD_CAST member.role);
  else
    xmlNewProp(m_node, BAD_CAST "role", BAD_CAST "");
}

/* build xml representation for a relation */
xmlChar *relation_t::generate_xml(item_id_t changeset) const {
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
  tags.for_each(tag_to_xml(xml_node));

  return osm_generate_xml_finish(doc);
}

/* build xml representation for a changeset */
xmlChar *osm_generate_xml_changeset(const std::string &comment,
                                    const std::string &src) {
  xmlChar *result = O2G_NULLPTR;
  int len = 0;

  /* tags for this changeset */
  tag_t tag_comment(const_cast<char*>("comment"),
                    const_cast<char *>(comment.c_str()));
  tag_t tag_creator(const_cast<char*>("created_by"),
                    const_cast<char*>(PACKAGE " v" VERSION));
  tag_t tag_source(const_cast<char *>("source"),
                   const_cast<char *>(src.c_str()));

  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(O2G_NULLPTR, BAD_CAST "osm");
  xmlDocSetRootElement(doc, root_node);

  xmlNodePtr cs_node = xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "changeset", O2G_NULLPTR);

  tag_to_xml fc(cs_node, true);
  fc(tag_creator);
  fc(tag_comment);
  if(!src.empty())
    fc(tag_source);

  xmlDocDumpFormatMemoryEnc(doc, &result, &len, "UTF-8", 1);
  xmlFreeDoc(doc);

  return result;
}

/* ---------- edit functions ------------- */

template<typename T, typename U> U osm_new_id(const std::map<U, T *> &map) {
  if(map.empty())
    return -1;

  // map is sorted, so use one less the first id in the container if it is negative,
  // or -1 if it is positive
  const typename std::map<U, T *>::const_iterator it = map.begin();
  if(it->first >= 0)
    return -1;
  else
    return it->first - 1;
}

template<typename T> void osm_attach(std::map<item_id_t, T *> &map, T *obj) {
  obj->id = osm_new_id(map);
  map[obj->id] = obj;
}

node_t *osm_t::node_new(const lpos_t &lpos) {
  /* convert screen position back to ll */
  pos_t pos;
  lpos2pos(bounds, &lpos, &pos);

  printf("Creating new node at %d %d (%f %f)\n", lpos.x, lpos.y, pos.lat, pos.lon);

  return new node_t(1, lpos, pos);
}

node_t *osm_t::node_new(const pos_t &pos) {
  /* convert ll position to screen */
  lpos_t lpos;
  pos2lpos(bounds, &pos, &lpos);

  printf("Creating new node from lat/lon at %d %d (%f %f)\n", lpos.x, lpos.y, pos.lat, pos.lon);

  return new node_t(1, lpos, pos);
}

void osm_t::node_attach(node_t *node) {
  printf("Attaching node\n");

  osm_attach(nodes, node);
}

void osm_t::node_restore(node_t *node) {
  printf("Restoring node\n");

  nodes[node->id] = node;
}

void osm_t::way_attach(way_t *way) {
  printf("Attaching way\n");

  osm_attach(ways, way);
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

  ways[way->id] = way;

  /* restore node memberships by converting ids into real pointers */
  g_assert_true(way->node_chain.empty());
  way_member_ref fc(this, way->node_chain);
  std::for_each(id_chain.begin(), id_chain.end(), fc);

  printf("done\n");
}

struct node_chain_delete_functor {
  const node_t * const node;
  way_chain_t &way_chain;
  const bool affect_ways;
  node_chain_delete_functor(const node_t *n, way_chain_t &w, bool a) : node(n), way_chain(w), affect_ways(a) {}
  void operator()(std::pair<item_id_t, way_t *> p);
};

void node_chain_delete_functor::operator()(std::pair<item_id_t, way_t *> p)
{
  way_t * const way = p.second;
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
  std::for_each(ways.begin(), ways.end(),
                node_chain_delete_functor(node, way_chain, affect_ways));

  /* remove that nodes map representations */
  if(node->map_item_chain)
    map_item_chain_destroy(&node->map_item_chain);

  if(!permanently) {
    printf("mark node #" ITEM_ID_FORMAT " as deleted\n", node->id);
    node->flags |= OSM_FLAG_DELETED;
  } else {
    printf("permanently delete node #" ITEM_ID_FORMAT "\n", node->id);

    std::map<item_id_t, node_t *>::iterator it = nodes.find(node->id);
    g_assert(it != nodes.end());

    node_free(it->second);
  }

  return way_chain;
}

struct find_member_object_functor_to_rel {
  const node_t * const node;
  bool via_way;
  find_member_object_functor_to_rel(const node_t *n, bool v) : node(n), via_way(v) {}
  bool operator()(const member_t &member);
};

bool find_member_object_functor_to_rel::operator()(const member_t& member)
{
  switch(member.object.type) {
  case NODE:
    /* nodes are checked directly */
    return member.object.node == node;
  case WAY:
    if(via_way)
      /* ways have to be checked for the nodes they consist of */
      return member.object.way->contains_node(node);
    break;
  default:
    break;
  }

  return false;
}

struct node_to_relation_functor {
  find_member_object_functor_to_rel fc;
  relation_chain_t &rel_chain;
  node_to_relation_functor(relation_chain_t &r, const node_t *n, bool v) : fc(n, v), rel_chain(r) {}
  void operator()(const std::pair<item_id_t, relation_t *> &pair);
};

void node_to_relation_functor::operator()(const std::pair<item_id_t, relation_t *> &pair)
{
  const std::vector<member_t> &members = pair.second->members;
  const std::vector<member_t>::const_iterator mitEnd = members.end();
  /* node is a member of this relation, so move it to the member chain */
  if(std::find_if(members.begin(), mitEnd, fc) != mitEnd)
    rel_chain.push_back(pair.second);
}

/* return all relations a node is in */
static relation_chain_t osm_node_to_relation(const osm_t *osm, const node_t *node,
				       bool via_way) {
  relation_chain_t rel_chain;

  std::for_each(osm->relations.begin(), osm->relations.end(),
                node_to_relation_functor(rel_chain, node, via_way));

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

bool osm_position_within_bounds_ll(const pos_t *ll_min, const pos_t *ll_max, const pos_t *pos) {
  if((pos->lat < ll_min->lat) || (pos->lat > ll_max->lat))
    return false;
  if((pos->lon < ll_min->lon) || (pos->lon > ll_max->lon))
    return false;
  return true;
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

void osm_t::relation_attach(relation_t *relation) {
  printf("Attaching relation\n");

  osm_attach(relations, relation);
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
    g_assert_cmpuint(way_chain.size(), ==, 1);
    g_assert(way_chain.front() == way);
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

struct reverse_direction_sensitive_tags_functor {
  unsigned int &n_tags_altered;
  reverse_direction_sensitive_tags_functor(unsigned int &c) : n_tags_altered(c) {}
  void operator()(tag_t &etag);
};

void reverse_direction_sensitive_tags_functor::operator()(tag_t &etag)
{
  char *lc_key = g_ascii_strdown(etag.key, -1);

  if (strcmp(lc_key, "oneway") == 0) {
    char *lc_value = g_ascii_strdown(etag.value, -1);
    // oneway={yes/true/1/-1} is unusual.
    // Favour "yes" and "-1".
    if ((strcmp(lc_value, DS_ONEWAY_FWD) == 0) ||
        (strcmp(lc_value, "true") == 0) ||
        (strcmp(lc_value, "1") == 0)) {
      etag.update_value(DS_ONEWAY_REV);
      n_tags_altered++;
    } else if (strcmp(lc_value, DS_ONEWAY_REV) == 0) {
      etag.update_value(DS_ONEWAY_FWD);
      n_tags_altered++;
    } else {
      printf("warning: unknown tag: %s=%s\n", etag.key, etag.value);
    }
    g_free(lc_value);
  } else if (strcmp(lc_key, "sidewalk") == 0) {
    if (strcasecmp(etag.value, "right") == 0)
      etag.update_value("left");
    else if (strcasecmp(etag.value, "left") == 0)
      etag.update_value("right");
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
        size_t plen = strlen(etag.key) - rtable[i].first.size();
        /* add length of new suffix */
        etag.key = (char*)g_realloc(etag.key, plen + 1 + rtable[i].second.size());
        char *lastcolon = etag.key + plen;
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

unsigned int
way_t::reverse_direction_sensitive_tags() {
  unsigned int n_tags_altered = 0;

  tags.for_each(reverse_direction_sensitive_tags_functor(n_tags_altered));

  if (n_tags_altered > 0)
    flags |= OSM_FLAG_DIRTY;

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
  const char *type = relation->tags.get_value("type");

  // Route relations; http://wiki.openstreetmap.org/wiki/Relation:route
  if (!type || strcasecmp(type, "route") != 0)
    return;

  // First find the member corresponding to our way:
  const std::vector<member_t>::iterator mitEnd = relation->members.end();
  std::vector<member_t>::iterator member = std::find_if(relation->members.begin(),
                                                        mitEnd, find_way_or_ref(way));
  g_assert(member != relation->members.end());  // osm_way_to_relation() broken?

  // Then flip its role if it's one of the direction-sensitive ones
  if (member->role == O2G_NULLPTR) {
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
    return O2G_NULLPTR;

  return node_chain.front();
}

const node_t *way_t::last_node() const {
  if(node_chain.empty())
    return O2G_NULLPTR;

  return node_chain.back();
}

bool way_t::is_closed() const {
  if(node_chain.empty())
    return false;
  return node_chain.front() == node_chain.back();
}

way_t * way_t::split(osm_t *osm, node_chain_t::iterator cut_at, bool cut_at_node)
{
  /* create a duplicate of the currently selected way */
  way_t *neww = new way_t(1);

  /* ------------  copy all tags ------------- */
  neww->tags.copy(tags);

  /* ---- transfer relation membership from way to new ----- */
  neww->transfer_relations(osm, this);

  /* attach remaining nodes to new way */
  neww->node_chain.insert(neww->node_chain.end(), cut_at, node_chain.end());

  /* if we cut at a node, this node is now part of both ways. so */
  /* keep it in the old way. */
  if(cut_at_node)
    cut_at++;

  /* terminate remainig chain on old way */
  node_chain.erase(cut_at, node_chain.end());

  /* now move the way itself into the main data structure */
  osm->way_attach(neww);

  return neww;
}

struct relation_transfer {
  way_t * const dst;
  const way_t * const src;
  relation_transfer(way_t *d, const way_t *s) : dst(d), src(s) {}
  void operator()(relation_t *relation);
};

void relation_transfer::operator()(relation_t* relation)
{
  printf("way #" ITEM_ID_FORMAT " is part of relation #" ITEM_ID_FORMAT "\n",
         src->id, relation->id);

  /* make new member of the same relation */

  /* walk member chain. save role of way if its being found. */
  std::vector<member_t>::iterator it = relation->find_member_object(object_t(const_cast<way_t *>(src)));

  printf("  adding way #" ITEM_ID_FORMAT " to relation\n", dst->id);
  object_t o(dst);
  member_t member(o);
  member.object = dst;
  if(it != relation->members.end())
    member.role = g_strdup(it->role);
  relation->members.push_back(member);

  relation->flags |= OSM_FLAG_DIRTY;
}

void way_t::transfer_relations(osm_t *osm, const way_t *from) {
  /* transfer relation memberships from the src way to the dst one */
  const relation_chain_t &rchain = osm->to_relation(from);
  std::for_each(rchain.begin(), rchain.end(), relation_transfer(this, from));
}

void way_t::rotate(node_chain_t::iterator nfirst) {
  if(nfirst == node_chain.begin())
    return;

  std::rotate(node_chain.begin(), nfirst, node_chain.end());
}

struct tag_map_functor {
  osm_t::TagMap &tags;
  tag_map_functor(osm_t::TagMap &t) : tags(t) {}
  void operator()(const tag_t &otag) {
    tags.insert(osm_t::TagMap::value_type(otag.key, otag.value));
  }
};

osm_t::TagMap tag_list_t::asMap() const
{
  osm_t::TagMap new_tags;

  if(!empty())
    std::for_each(contents->begin(), contents->end(), tag_map_functor(new_tags));

  return new_tags;
}

struct tag_vector_copy_functor {
  std::vector<tag_t> &tags;
  tag_vector_copy_functor(std::vector<tag_t> &t) : tags(t) {}
  void operator()(const tag_t &otag) {
    if(G_UNLIKELY(otag.is_creator_tag()))
      return;

    tags.push_back(tag_t(g_strdup(otag.key), g_strdup(otag.value)));
  }
};

void tag_list_t::copy(const tag_list_t &other)
{
  g_assert_null(contents);

  if(other.empty())
    return;

  contents = new typeof(*contents);
  contents->reserve(other.contents->size());

  std::for_each(other.contents->begin(), other.contents->end(), tag_vector_copy_functor(*contents));
}

/* try to get an as "speaking" description of the object as possible */
std::string object_t::get_name() const {
  std::string ret;

  /* worst case: we have no tags at all. return techincal info then */
  if(!has_tags())
    return std::string("unspecified ") + type_string();

  /* try to figure out _what_ this is */
  const char *name_tags[] = { "name", "ref", "note", "fix" "me", "sport", O2G_NULLPTR };
  const char *name = O2G_NULLPTR;
  for(unsigned int i = 0; !name && name_tags[i]; i++)
    name = obj->tags.get_value(name_tags[i]);

  /* search for some kind of "type" */
  const char *type_tags[] = { "amenity", "place", "historic", "leisure",
                              "tourism", "landuse", "waterway", "railway",
                              "natural", O2G_NULLPTR };
  const char *type = O2G_NULLPTR;

  for(unsigned int i = 0; !type && type_tags[i]; i++)
    type = obj->tags.get_value(type_tags[i]);

  if(!type && obj->tags.get_value("building")) {
    const char *street = obj->tags.get_value("addr:street");
    const char *hn = obj->tags.get_value("addr:housenumber");

    if(hn) {
      if(street) {
        ret = "building ";
        ret += street;
        ret +=' ';
      } else {
        ret = "building housenumber ";
      }
      ret += hn;
    } else {
      type = "building";
      if(!name)
        name = obj->tags.get_value("addr:housename");
    }
  }
  if(!type && ret.empty())
    type = obj->tags.get_value("emergency");

  /* highways are a little bit difficult */
  const char *highway = obj->tags.get_value("highway");
  if(highway && ret.empty()) {
    if((!strcmp(highway, "primary")) ||
       (!strcmp(highway, "secondary")) ||
       (!strcmp(highway, "tertiary")) ||
       (!strcmp(highway, "unclassified")) ||
       (!strcmp(highway, "residential")) ||
       (!strcmp(highway, "service"))) {
      ret = highway;
      ret += " road";
      type = O2G_NULLPTR;
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

  if(type) {
    g_assert_true(ret.empty());
    ret = type;
  }

  if(name) {
    if(ret.empty())
      ret = type_string();
    ret += ": \"";
    ret += name;
    ret += '"';
  } else if(ret.empty()) {
    ret = "unspecified ";
    ret += type_string();
  }

  /* remove underscores from string and replace them by spaces as this is */
  /* usually nicer */
  std::replace(ret.begin(), ret.end(), '_', ' ');

  return ret;
}

bool tag_t::is_creator_tag() const {
  return is_creator_tag(key);
}

bool tag_t::is_creator_tag(const char* key)
{
  return (strcasecmp(key, "created_by") == 0);
}

tag_list_t::tag_list_t()
  : contents(O2G_NULLPTR)
{
}

tag_list_t::~tag_list_t()
{
  clear();
}

bool tag_list_t::empty() const
{
  return !contents || contents->empty();
}

bool tag_list_t::hasRealTags() const
{
  if(empty())
    return false;

  const std::vector<tag_t>::const_iterator itEnd = contents->end();
  std::vector<tag_t>::const_iterator it = contents->begin();
  while(it != itEnd && it->is_creator_tag())
    it++;

  return it != itEnd;
}

struct key_match_functor {
  const char * const key;
  key_match_functor(const char *k) : key(k) {}
  bool operator()(const tag_t &tag) {
    return (strcasecmp(key, tag.key) == 0);
  }
};

const char* tag_list_t::get_value(const char *key) const
{
  if(!contents)
    return O2G_NULLPTR;
  const std::vector<tag_t>::const_iterator itEnd = contents->end();
  const std::vector<tag_t>::const_iterator it = std::find_if(cbegin(*contents),
                                                             itEnd, key_match_functor(key));
  if(it != itEnd)
    return it->value;

  return O2G_NULLPTR;
}

void tag_list_t::clear()
{
  for_each(osm_tag_free);
  delete contents;
  contents = O2G_NULLPTR;
}

void tag_list_t::replace(std::vector<tag_t> &ntags)
{
  clear();
  if(ntags.empty()) {
    contents = 0;
    return;
  }
#if __cplusplus >= 201103L
  contents = new std::vector<tag_t>(std::move(ntags));
  contents->shrink_to_fit();
#else
  contents = new std::vector<tag_t>();
  contents->reserve(ntags.size());
  contents->swap(ntags);
#endif
}

struct tag_fill_functor {
  std::vector<tag_t> &tags;
  tag_fill_functor(std::vector<tag_t> &t) : tags(t) {}
  void operator()(const osm_t::TagMap::value_type &p) {
    if(G_UNLIKELY(tag_t::is_creator_tag(p.first.c_str())))
      return;

    tags.push_back(tag_t(g_strdup(p.first.c_str()), g_strdup(p.second.c_str())));
  }
};

void tag_list_t::replace(const osm_t::TagMap &ntags)
{
  clear();
  if(ntags.empty()) {
    contents = 0;
    return;
  }
  contents = new std::vector<tag_t>();
  contents->reserve(ntags.size());
  std::for_each(ntags.begin(), ntags.end(), tag_fill_functor(*contents));
}

base_object_t::base_object_t()
{
  memset(this, 0, sizeof(*this));
}

base_object_t::base_object_t(item_id_t ver, item_id_t i)
  : id(i)
  , version(ver)
  , user(O2G_NULLPTR)
  , time(0)
  , flags(version == 1 ? OSM_FLAG_NEW : 0)
{
}

void base_object_t::updateTags(const osm_t::TagMap &ntags)
{
  if (tags == ntags)
    return;

  tags.replace(ntags);

  flags |= OSM_FLAG_DIRTY;
}

struct value_match_functor {
  const char * const value;
  value_match_functor(const char *v) : value(v) {}
  bool operator()(const tag_t *tag) {
    return tag->value && (strcasecmp(tag->value, value) == 0);
  }
};

way_t::way_t()
  : base_object_t()
  , map_item_chain(O2G_NULLPTR)
  , node_chain(0)
{
  memset(&draw, 0, sizeof(draw));
}

way_t::way_t(item_id_t ver, item_id_t i)
  : base_object_t(ver, i)
  , map_item_chain(O2G_NULLPTR)
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
  g_assert_false(node_chain.empty());

  if(node_chain.front() == node)
    return true;

  if(node_chain.back() == node)
    return true;

  return false;
}

void way_t::cleanup() {
  //  printf("freeing way #" ITEM_ID_FORMAT "\n", OSM_ID(way));

  osm_node_chain_free(node_chain);
  tags.clear();

  /* there must not be anything left in this chain */
  g_assert_null(map_item_chain);
}

member_t::member_t(type_t t)
  : role(O2G_NULLPTR)
{
  object.type = t;
}

member_t::member_t(const object_t &o, char *r)
  : object(o)
  , role(r)
{
}

bool member_t::operator==(const member_t &other) const
{
  if(object != other.object)
    return false;

  // check if any of them is 0, strcmp() does not like that
  if(!!role ^ !!other.role)
    return false;

  return !role || strcmp(role, other.role) == 0;
}

relation_t::relation_t()
  : base_object_t()
{
}

relation_t::relation_t(item_id_t ver, item_id_t i)
  : base_object_t(ver, i)
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
  , map_item_chain(O2G_NULLPTR)
{
  memset(&pos, 0, sizeof(pos));
  memset(&lpos, 0, sizeof(lpos));
}

node_t::node_t(item_id_t ver, const lpos_t &lp, const pos_t &p, item_id_t i)
  : base_object_t(ver, i)
  , pos(p)
  , lpos(lp)
  , ways(0)
  , zoom_max(0.0)
  , map_item_chain(O2G_NULLPTR)
{
}

template<typename T> T *osm_find_by_id(const std::map<item_id_t, T *> &map, item_id_t id) {
  const typename std::map<item_id_t, T *>::const_iterator it = map.find(id);
  if(it != map.end())
    return it->second;

  return O2G_NULLPTR;
}

osm_t::~osm_t()
{
  std::for_each(ways.begin(), ways.end(), ::way_free);
  std::for_each(nodes.begin(), nodes.end(), nodefree);
  std::for_each(relations.begin(), relations.end(),
                osm_relation_free_pair);
}

node_t *osm_t::node_by_id(item_id_t id) const {
  return osm_find_by_id<node_t>(nodes, id);
}

way_t *osm_t::way_by_id(item_id_t id) const {
  return osm_find_by_id<way_t>(ways, id);
}

relation_t *osm_t::relation_by_id(item_id_t id) const {
  return osm_find_by_id<relation_t>(relations, id);
}

// vim:et:ts=8:sw=2:sts=2:ai
