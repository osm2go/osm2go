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

#define _DEFAULT_SOURCE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "osm.h"

#include "osm2go_platform.h"
#include "pos.h"
#include "xml_helpers.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <strings.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

/* ------------------------- user handling --------------------- */

struct cmp_user {
  const char * const uname;
  explicit cmp_user(const char *u) : uname(u) {}
  bool operator()(const std::string &s) {
    return (strcasecmp(s.c_str(), uname) == 0);
  }
};

/**
 * @brief insert a username into osm_t::users if needed
 * @param osm the osm object
 * @param name the username
 * @param uid the user id as returned by the server
 * @returns the id in the user map
 *
 * In case no userid is given a temporary one will be created.
 */
static int osm_user_insert(osm_t::ref osm, const char *name, int uid) {
  if(unlikely(!name)) {
    osm->users[0] = std::string();
    return 0;
  }

  const std::map<int, std::string>::const_iterator itEnd = osm->users.end();
  /* search through user list */
  if(likely(uid > 0)) {
    const std::map<int, std::string>::const_iterator it = osm->users.find(uid);
    if(unlikely(it == itEnd))
      osm->users[uid] = name;

    return uid;
  } else {
    // no virtual user found
    if(osm->users.empty() || osm->users.begin()->first > 0) {
      osm->users[-1] = name;
      return -1;
    }
    /* check if ay of the temporary ids already matches the name */
    std::map<int, std::string>::const_iterator it = osm->users.begin();
    for(; it != itEnd && it->first < 0; it++)
      if(it->second == name)
        return it->first;
    // generate a new temporary id
    // it is already one in there, so use one less as the lowest existing id
    int id = osm->users.begin()->first - 1;
    osm->users[id] = name;
    return id;
  }
}

static
time_t __attribute__((nonnull(1))) convert_iso8601(const char *str) {
  struct tm ctime;
  memset(&ctime, 0, sizeof(ctime));
  strptime(str, "%FT%T%z", &ctime);

  long gmtoff = ctime.tm_gmtoff;

  return timegm(&ctime) - gmtoff;
}

/* -------------------- tag handling ----------------------- */

bool osm_t::parse_tag(xmlNode *a_node, TagMap &tags) {
  xmlString key(xmlGetProp(a_node, BAD_CAST "k"));
  xmlString value(xmlGetProp(a_node, BAD_CAST "v"));

  if(unlikely(key.empty() || value.empty()))
    return false;

  const std::string k = reinterpret_cast<char *>(key.get());
  const std::string v = reinterpret_cast<char *>(value.get());

  if(unlikely(tags.findTag(k, v) != tags.end()))
    return false;

  tags.insert(TagMap::value_type(k, v));

  return true;
}

/* ------------------- way handling ------------------- */

node_t *osm_t::parse_way_nd(xmlNode *a_node) const {
  xmlString prop(xmlGetProp(a_node, BAD_CAST "ref"));
  node_t *node = nullptr;

  if(prop) {
    item_id_t id = strtoll(prop, nullptr, 10);

    /* search matching node */
    node = node_by_id(id);
    if(node == nullptr)
      printf("Node id " ITEM_ID_FORMAT " not found\n", id);
    else
      node->ways++;
  }

  return node;
}

/* ------------------- relation handling ------------------- */

bool osm_t::parse_relation_member(const xmlString &tp, const xmlString &refstr, const xmlString &role, std::vector<member_t> &members) {
  if(unlikely(tp.empty())) {
    printf("missing type for relation member\n");
    return false;
  }
  if(unlikely(refstr.empty())) {
    printf("missing ref for relation member\n");
    return false;
  }

  object_t::type_t type;
  if(strcmp(tp, way_t::api_string()) == 0)
    type = object_t::WAY;
  else if(strcmp(tp, node_t::api_string()) == 0)
    type = object_t::NODE;
  else if(likely(strcmp(tp, relation_t::api_string()) == 0))
    type = object_t::RELATION;
  else {
    printf("Unable to store illegal type '%s'\n", tp.get());
    return false;
  }

  char *endp;
  item_id_t id = strtoll(refstr, &endp, 10);
  if(unlikely(*endp != '\0')) {
    printf("Illegal ref '%s' for relation member\n", refstr.get());
    return false;
  }

  object_t obj(type);

  switch(type) {
  case object_t::WAY:
    /* search matching way */
    obj.way = way_by_id(id);
    break;

  case object_t::NODE:
    /* search matching node */
    obj.node = node_by_id(id);
    break;

  case object_t::RELATION:
    /* search matching relation */
    obj.relation = relation_by_id(id);
    break;
  default:
    assert_unreachable();
  }

  if(obj.obj == nullptr) {
    obj.type = static_cast<object_t::type_t>(type | object_t::_REF_FLAG);
    obj.id = id;
  }

  const char *rstr = role.empty() ? nullptr : static_cast<const char *>(role);
  members.push_back(member_t(obj, rstr));
  return true;
}

void osm_t::parse_relation_member(xmlNode *a_node, std::vector<member_t> &members) {
  xmlString tp(xmlGetProp(a_node, BAD_CAST "type"));
  xmlString refstr(xmlGetProp(a_node, BAD_CAST "ref"));
  xmlString role(xmlGetProp(a_node, BAD_CAST "role"));

  parse_relation_member(tp, refstr, role, members);
}

/* -------------------------- stream parser ------------------- */

static inline int __attribute__((nonnull(2))) my_strcmp(const xmlChar *a, const xmlChar *b) {
  if(a == nullptr)
    return -1;
  return strcmp(reinterpret_cast<const char *>(a), reinterpret_cast<const char *>(b));
}

/* skip current element incl. everything below (mainly for testing) */
static void skip_element(xmlTextReaderPtr reader) {
  assert_cmpnum(xmlTextReaderNodeType(reader), XML_READER_TYPE_ELEMENT);
  if(xmlTextReaderIsEmptyElement(reader))
    return;

  int depth = xmlTextReaderDepth(reader);
  const xmlChar *name = xmlTextReaderConstName(reader);
  assert(name != nullptr);

  int ret = xmlTextReaderRead(reader);
  while(ret == 1 &&
        (xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT ||
         xmlTextReaderDepth(reader) > depth ||
         my_strcmp(xmlTextReaderConstName(reader), name) != 0)) {
    ret = xmlTextReaderRead(reader);
  }
}

/* parse bounds */
static bool process_bounds(xmlTextReaderPtr reader, bounds_t &bounds) {
  if(unlikely(!bounds.init(pos_area(pos_t::fromXmlProperties(reader, "minlat", "minlon"),
                                    pos_t::fromXmlProperties(reader, "maxlat", "maxlon"))))) {
    fprintf(stderr, "Invalid coordinate in bounds (%f/%f/%f/%f)\n",
            bounds.ll.min.lat, bounds.ll.min.lon,
            bounds.ll.max.lat, bounds.ll.max.lon);

    return false;
  }

  /* skip everything below */
  skip_element(reader);

  bounds.min = bounds.ll.min.toLpos();
  bounds.min.x -= bounds.center.x;
  bounds.min.y -= bounds.center.y;
  bounds.min.x *= bounds.scale;
  bounds.min.y *= bounds.scale;

  bounds.max = bounds.ll.max.toLpos();
  bounds.max.x -= bounds.center.x;
  bounds.max.y -= bounds.center.y;
  bounds.max.x *= bounds.scale;
  bounds.max.y *= bounds.scale;

  return true;
}

static void process_tag(xmlTextReaderPtr reader, std::vector<tag_t> &tags) {
  xmlString k(xmlTextReaderGetAttribute(reader, BAD_CAST "k"));
  xmlString v(xmlTextReaderGetAttribute(reader, BAD_CAST "v"));

  if(likely(!k.empty() && !v.empty()))
    tags.push_back(tag_t(k, v));
  else
    printf("incomplete tag key/value %s/%s\n", k.get(), v.get());

  skip_element(reader);
}

static void process_base_attributes(base_object_t *obj, xmlTextReaderPtr reader, osm_t::ref osm)
{
  xmlString prop(xmlTextReaderGetAttribute(reader, BAD_CAST "id"));
  if(likely(prop))
    obj->id = strtoll(prop, nullptr, 10);

  /* new in api 0.6: */
  prop.reset(xmlTextReaderGetAttribute(reader, BAD_CAST "version"));
  if(likely(prop))
    obj->version = strtoul(prop, nullptr, 10);

  prop.reset(xmlTextReaderGetAttribute(reader, BAD_CAST "user"));
  if(likely(prop)) {
    int uid = -1;
    xmlString puid(xmlTextReaderGetAttribute(reader, BAD_CAST "uid"));
    if(likely(puid)) {
      char *endp;
      uid = strtol(puid, &endp, 10);
      if(unlikely(*endp)) {
        printf("WARNING: cannot parse uid '%s' for user '%s'\n", puid.get(), prop.get());
        uid = -1;
      }
    }
    obj->user = osm_user_insert(osm, prop, uid);
  }

  prop.reset(xmlTextReaderGetAttribute(reader, BAD_CAST "timestamp"));
  if(likely(prop))
    obj->time = convert_iso8601(prop);
}

static void process_node(xmlTextReaderPtr reader, osm_t::ref osm) {
  const pos_t pos = pos_t::fromXmlProperties(reader);

  /* allocate a new node structure */
  node_t *node = osm->node_new(pos);
  // reset the flags, this object comes from upstream OSM
  node->flags = 0;

  process_base_attributes(node, reader, osm);

  size_t cnt = osm->nodes.size();
  osm->nodes[node->id] = node;
  assert_cmpnum(osm->nodes.size(), cnt + 1);

  /* just an empty element? then return the node as it is */
  if(xmlTextReaderIsEmptyElement(reader))
    return;

  /* parse tags if present */
  int depth = xmlTextReaderDepth(reader);

  /* scan all elements on same level or its children */
  std::vector<tag_t> tags;
  int ret = xmlTextReaderRead(reader);
  while((ret == 1) &&
	((xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT) ||
	 (xmlTextReaderDepth(reader) != depth))) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const char *subname = reinterpret_cast<const char *>(xmlTextReaderConstName(reader));
      if(likely(strcmp(subname, "tag") == 0))
        process_tag(reader, tags);
      else
        skip_element(reader);
    }

    ret = xmlTextReaderRead(reader);
  }
  node->tags.replace(tags);
}

static node_t *process_nd(xmlTextReaderPtr reader, osm_t::ref osm) {
  xmlString prop(xmlTextReaderGetAttribute(reader, BAD_CAST "ref"));
  node_t *node = nullptr;

  if(likely(!prop.empty())) {
    item_id_t id = strtoll(prop, nullptr, 10);
    /* search matching node */
    node = osm->node_by_id(id);
    if(unlikely(node == nullptr))
      printf("Node id " ITEM_ID_FORMAT " not found\n", id);
    else
      node->ways++;
  }

  skip_element(reader);
  return node;
}

static void process_way(xmlTextReaderPtr reader, osm_t::ref osm) {
  /* allocate a new way structure */
  way_t *way = new way_t(1);

  process_base_attributes(way, reader, osm);

  size_t cnt = osm->ways.size();
  osm->ways[way->id] = way;
  assert_cmpnum(osm->ways.size(), cnt + 1);

  /* just an empty element? then return the way as it is */
  /* (this should in fact never happen as this would be a way without nodes) */
  if(xmlTextReaderIsEmptyElement(reader))
    return;

  /* parse tags/nodes if present */
  int depth = xmlTextReaderDepth(reader);

  /* scan all elements on same level or its children */
  std::vector<tag_t> tags;
  int ret = xmlTextReaderRead(reader);
  while(ret == 1 &&
        (xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT ||
         xmlTextReaderDepth(reader) != depth)) {
    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const char *subname = reinterpret_cast<const char *>(xmlTextReaderConstName(reader));
      if(strcmp(subname, "nd") == 0) {
        node_t *n = process_nd(reader, osm);
        if(likely(n != nullptr))
          way->node_chain.push_back(n);
      } else if(likely(strcmp(subname, "tag") == 0)) {
        process_tag(reader, tags);
      } else
        skip_element(reader);
    }
    ret = xmlTextReaderRead(reader);
  }
  way->tags.replace(tags);
}

static bool process_member(xmlTextReaderPtr reader, osm_t::ref osm, std::vector<member_t> &members) {
  xmlString tp(xmlTextReaderGetAttribute(reader, BAD_CAST "type"));
  xmlString ref(xmlTextReaderGetAttribute(reader, BAD_CAST "ref"));
  xmlString role(xmlTextReaderGetAttribute(reader, BAD_CAST "role"));

  return osm->parse_relation_member(tp, ref, role, members);
}

static void process_relation(xmlTextReaderPtr reader, osm_t::ref osm) {
  /* allocate a new relation structure */
  relation_t *relation = new relation_t(1);

  process_base_attributes(relation, reader, osm);

  size_t cnt = osm->relations.size();
  osm->relations[relation->id] = relation;
  assert_cmpnum(osm->relations.size(), cnt + 1);

  /* just an empty element? then return the relation as it is */
  /* (this should in fact never happen as this would be a relation */
  /* without members) */
  if(xmlTextReaderIsEmptyElement(reader))
    return;

  /* parse tags/member if present */
  int depth = xmlTextReaderDepth(reader);

  /* scan all elements on same level or its children */
  std::vector<tag_t> tags;
  int ret = xmlTextReaderRead(reader);
  while(ret == 1 &&
        (xmlTextReaderNodeType(reader) != XML_READER_TYPE_END_ELEMENT ||
         xmlTextReaderDepth(reader) != depth)) {

    if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const char *subname = reinterpret_cast<const char *>(xmlTextReaderConstName(reader));
      if(strcmp(subname, "member") == 0) {
        process_member(reader, osm, relation->members);
      } else if(likely(strcmp(subname, "tag") == 0)) {
        process_tag(reader, tags);
      } else
        skip_element(reader);
    }
    ret = xmlTextReaderRead(reader);
  }
  relation->tags.replace(tags);
}

static osm_t::UploadPolicy parseUploadPolicy(const char *str) {
  if(likely(strcmp(str, "true") == 0))
    return osm_t::Upload_Normal;
  else if(strcmp(str, "false") == 0)
    return osm_t::Upload_Discouraged;
  else if(likely(strcmp(str, "never") == 0))
    return osm_t::Upload_Blocked;

  printf("unknown key for upload found: %s\n", str);

  // just to be cautious
  return osm_t::Upload_Discouraged;
}

static osm_t *process_osm(xmlTextReaderPtr reader) {
  /* alloc osm structure */
  std::unique_ptr<osm_t> osm(new osm_t());

  xmlString prop(xmlTextReaderGetAttribute(reader, BAD_CAST "upload"));
  if(unlikely(prop))
    osm->uploadPolicy = parseUploadPolicy(prop);

  /* read next node */
  int num_elems = 0;

  /* the objects come in exactly this order, so some parsing time can be
   * saved as it is clear that e.g. no node can show up if the first way
   * was seen. */
  enum blocks {
    BLOCK_OSM = 0,
//     BLOCK_BOUNDS,
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

      assert_cmpnum(xmlTextReaderDepth(reader), 1);
      const char *name = reinterpret_cast<const char *>(xmlTextReaderConstName(reader));
      if(block == BLOCK_OSM && strcmp(name, "bounds") == 0) {
        if(unlikely(!process_bounds(reader, osm->bounds)))
          return nullptr;
        block = BLOCK_NODES; // next must be nodes, there must not be more than one bounds
      } else if(block == BLOCK_NODES && strcmp(name, node_t::api_string()) == 0) {
        process_node(reader, osm);
      } else if(block <= BLOCK_WAYS && strcmp(name, way_t::api_string()) == 0) {
        process_way(reader, osm);
        block = BLOCK_WAYS;
      } else if(likely(block <= BLOCK_RELATIONS && strcmp(name, relation_t::api_string()) == 0)) {
        process_relation(reader, osm);
        block = BLOCK_RELATIONS;
      } else {
        printf("something unknown found: %s\n", name);
        skip_element(reader);
      }
      break;
    }

    case XML_READER_TYPE_END_ELEMENT:
      /* end element must be for the current element */
      assert_cmpnum(xmlTextReaderDepth(reader), 0);
      return osm.release();
      break;

    default:
      break;
    }
    ret = xmlTextReaderRead(reader);

    if (num_elems++ > tick_every) {
      num_elems = 0;
      osm2go_platform::process_events();
    }
  }

  assert_unreachable();
}

struct relation_ref_functor {
  osm_t::ref osm;
  explicit relation_ref_functor(osm_t::ref o) : osm(o) {}
  void operator()(std::pair<item_id_t, relation_t *> p) {
    std::for_each(p.second->members.begin(), p.second->members.end(), *this);
  }
  void operator()(member_t &m) {
    if(m.object.type != object_t::RELATION_ID)
      return;
    std::map<item_id_t, relation_t *>::const_iterator itEnd = osm->relations.end();
    std::map<item_id_t, relation_t *>::const_iterator it = osm->relations.find(m.object.id);
    if(it == itEnd)
      return;
    m.object.relation = it->second;
    m.object.type = object_t::RELATION;
  }
};

static osm_t *process_file(const std::string &filename) {
  std::unique_ptr<osm_t> osm;
  xmlTextReaderPtr reader;

  reader = xmlReaderForFile(filename.c_str(), nullptr, XML_PARSE_NONET);
  if (likely(reader != nullptr)) {
    if(likely(xmlTextReaderRead(reader) == 1)) {
      const char *name = reinterpret_cast<const char *>(xmlTextReaderConstName(reader));
      if(likely(name && strcmp(name, "osm") == 0)) {
        osm.reset(process_osm(reader));
        // relations may have references to other relation, which have greater ids
        // those are not present when the relation itself was created, but may be now
        if(likely(osm))
          std::for_each(osm->relations.begin(), osm->relations.end(), relation_ref_functor(osm));
      }
    } else
      printf("file empty\n");

    xmlFreeTextReader(reader);
  } else {
    fprintf(stderr, "Unable to open %s\n", filename.c_str());
  }
  return osm.release();
}

/* ----------------------- end of stream parser ------------------- */

osm_t *osm_t::parse(const std::string &path, const std::string &filename) {

  // use stream parser
  if(unlikely(filename.find('/') != std::string::npos))
    return process_file(filename);
  else
    return process_file(path + filename);
}
