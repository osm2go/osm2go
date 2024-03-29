/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _DEFAULT_SOURCE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "osm.h"

#include "osm_objects.h"
#include "misc.h"
#include "pos.h"

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
#include <osm2go_platform.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

/* ------------------------- user handling --------------------- */

namespace {

class cmp_user {
  const std::string &uname;
public:
  explicit inline cmp_user(const std::string &u) : uname(u) {}
  inline bool operator()(const std::pair<int, std::string> &p) const
  {
    return p.second == uname;
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
int __attribute__ ((nonnull (2)))
osm_user_insert(std::map<int, std::string> &users, const char *name, int uid)
{
  const std::map<int, std::string>::const_iterator itEnd = users.end();
  /* search through user list */
  if(likely(uid > 0)) {
    const std::map<int, std::string>::const_iterator it = users.find(uid);
    if(unlikely(it == itEnd))
      users[uid] = name;

    return uid;
  } else {
    // no virtual user found
    if(users.empty() || users.begin()->first > 0) {
      users[-1] = name;
      return -1;
    }
    /* check if any of the temporary ids already matches the name */
    std::map<int, std::string>::const_iterator it = users.begin();
    const std::map<int, std::string>::const_iterator itTemp = std::next(users.find(-1)); ///< the first behind the range of temporary ids

    it = std::find_if(it, itTemp, cmp_user(name));
    if (it != itTemp)
      return it->first;
    // generate a new temporary id
    // it is already one in there, so use one less as the lowest existing id
    int id = users.begin()->first - 1;
    users[id] = name;
    return id;
  }
}

time_t __attribute__((nonnull(1)))
convert_iso8601(const char *str)
{
  struct tm ctime;
  memset(&ctime, 0, sizeof(ctime));
  strptime(str, "%FT%T%z", &ctime);

  long gmtoff = ctime.tm_gmtoff;

  return timegm(&ctime) - gmtoff;
}

} // namespace

/* -------------------- tag handling ----------------------- */

void osm_t::parse_tag(xmlNode *a_node, TagMap &tags)
{
  xmlString key(xmlGetProp(a_node, BAD_CAST "k"));
  xmlString value(xmlGetProp(a_node, BAD_CAST "v"));

  if(unlikely(key.empty() || value.empty())) {
    printf("empty attribute for tag: k='%s' v='%s'\n", static_cast<const char *>(key),
           static_cast<const char *>(value));
    return;
  }

  const std::string k = reinterpret_cast<char *>(key.get());
  const std::string v = reinterpret_cast<char *>(value.get());

  if(unlikely(tags.findTag(k, v) != tags.end())) {
    printf("duplicate tag: k='%s' v='%s'\n", static_cast<const char *>(key),
           static_cast<const char *>(value));
    return;
  }

  tags.insert(TagMap::value_type(k, v));
}

/* ------------------- way handling ------------------- */

namespace {

item_id_t checkReplacedId(const char *type, item_id_t id, const std::unordered_map<item_id_t, item_id_t> *replacedNodeIds)
{
  if (replacedNodeIds != nullptr) {
    const std::unordered_map<item_id_t, item_id_t>::const_iterator it = replacedNodeIds->find(id);
    if (it != replacedNodeIds->end()) {
      printf("Reference to %s id " ITEM_ID_FORMAT " replaced with " ITEM_ID_FORMAT "\n", type, id, it->second);
      id = it->second;
    }
  }

  return id;
}

node_t *parse_node_ref(const xmlString &prop, const osm_t *osm, const std::unordered_map<item_id_t, item_id_t> *replacedNodeIds = nullptr)
{
  node_t *node = nullptr;

  if(likely(!prop.empty())) {
    item_id_t id = checkReplacedId(node_t::api_string(), strtoll(prop, nullptr, 10), replacedNodeIds);

    /* search matching node */
    node = osm->object_by_id<node_t>(id);
    if(unlikely(node == nullptr))
      printf("Node id " ITEM_ID_FORMAT " not found\n", id);
    else
      node->ways++;
  }

  return node;
}

} // namespace

node_t *osm_t::parse_way_nd(xmlNode *a_node, const std::unordered_map<item_id_t, item_id_t> *replacedNodeIds) const
{
  xmlString prop(xmlGetProp(a_node, BAD_CAST "ref"));

  return parse_node_ref(prop, this, replacedNodeIds);
}

/* ------------------- relation handling ------------------- */

void osm_t::parse_relation_member(const xmlString &tp, const xmlString &refstr,
                                  const xmlString &role, std::vector<member_t> &members,
                                  const std::unordered_map<item_id_t, item_id_t> *replacedNodeIds,
                                  const std::unordered_map<item_id_t, item_id_t> *replacedWayIds)
{
  if(unlikely(tp.empty())) {
    printf("missing type for relation member\n");
    return;
  }
  if(unlikely(refstr.empty())) {
    printf("missing ref for relation member\n");
    return;
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
    return;
  }

  char *endp;
  item_id_t id = strtoll(refstr, &endp, 10);
  if(unlikely(*endp != '\0')) {
    printf("Illegal ref '%s' for relation member\n", refstr.get());
    return;
  }

  object_t obj(type);

  switch(type) {
  case object_t::WAY:
    /* search matching way */
    obj = object_by_id<way_t>(checkReplacedId(way_t::api_string(), id, replacedWayIds));
    break;

  case object_t::NODE:
    /* search matching node */
    obj = object_by_id<node_t>(checkReplacedId(node_t::api_string(), id, replacedNodeIds));
    break;

  case object_t::RELATION:
    /* search matching relation */
    obj = object_by_id<relation_t>(id);
    break;
  default:
    assert_unreachable();
  }

  if(static_cast<base_object_t *>(obj) == nullptr)
    obj = object_t(static_cast<object_t::type_t>(type | object_t::_REF_FLAG), id);

  const char *rstr = role.empty() ? nullptr : static_cast<const char *>(role);
  members.push_back(member_t(obj, rstr));
}

void osm_t::parse_relation_member(xmlNode *a_node, std::vector<member_t> &members,
                                  const std::unordered_map<item_id_t, item_id_t> *replacedNodeIds,
                                  const std::unordered_map<item_id_t, item_id_t> *replacedWayIds)
{
  xmlString tp(xmlGetProp(a_node, BAD_CAST "type"));
  xmlString refstr(xmlGetProp(a_node, BAD_CAST "ref"));
  xmlString role(xmlGetProp(a_node, BAD_CAST "role"));

  parse_relation_member(tp, refstr, role, members, replacedNodeIds, replacedWayIds);
}

/* -------------------------- stream parser ------------------- */

namespace {

inline int __attribute__((nonnull(2)))
my_strcmp(const xmlChar *a, const xmlChar *b)
{
  if(a == nullptr)
    return -1;
  return strcmp(reinterpret_cast<const char *>(a), reinterpret_cast<const char *>(b));
}

/* skip current element incl. everything below (mainly for testing) */
void
skip_element(xmlTextReaderPtr reader)
{
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
std::optional<bounds_t>
process_bounds(xmlTextReaderPtr reader)
{
  bounds_t bounds;
  if(unlikely(!bounds.init(pos_area(pos_t::fromXmlProperties(reader, "minlat", "minlon"),
                                    pos_t::fromXmlProperties(reader, "maxlat", "maxlon"))))) {
    fprintf(stderr, "Invalid coordinate in bounds (%f/%f/%f/%f)\n",
            bounds.ll.min.lat, bounds.ll.min.lon,
            bounds.ll.max.lat, bounds.ll.max.lon);

    return std::optional<bounds_t>();
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

  return bounds;
}

void
process_tag(xmlTextReaderPtr reader, std::vector<tag_t> &tags)
{
  xmlString k(xmlTextReaderGetAttribute(reader, BAD_CAST "k"));
  xmlString v(xmlTextReaderGetAttribute(reader, BAD_CAST "v"));

  if(likely(!k.empty() && !v.empty()))
    tags.push_back(tag_t(k, v));
  else
    printf("incomplete tag key/value %s/%s\n", k.get(), v.get());
}

base_attributes
process_base_attributes(xmlTextReaderPtr reader, osm_t::ref osm)
{
  base_attributes ret;
  xmlString prop(xmlTextReaderGetAttribute(reader, BAD_CAST "id"));
  if(likely(prop))
    ret.id = strtoll(prop, nullptr, 10);

  /* new in api 0.6: */
  prop.reset(xmlTextReaderGetAttribute(reader, BAD_CAST "version"));
  if(likely(prop))
    ret.version = strtoul(prop, nullptr, 10);

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
    ret.user = osm_user_insert(osm->users, prop, uid);
  }

  prop.reset(xmlTextReaderGetAttribute(reader, BAD_CAST "timestamp"));
  if(likely(prop))
    ret.time = convert_iso8601(prop);

  return ret;
}

void
process_node(xmlTextReaderPtr reader, osm_t::ref osm)
{
  const pos_t pos = pos_t::fromXmlProperties(reader);

  base_attributes ba = process_base_attributes(reader, osm);

  node_t *node = osm->node_new(pos, ba);
  assert_cmpnum(node->flags, 0);

  osm->insert(node);

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

      skip_element(reader);
    }

    ret = xmlTextReaderRead(reader);
  }
  node->tags.replace(std::move(tags));
}

node_t *
process_nd(xmlTextReaderPtr reader, osm_t::ref osm)
{
  xmlString prop(xmlTextReaderGetAttribute(reader, BAD_CAST "ref"));
  return parse_node_ref(prop, osm.get());
}

void
process_way(xmlTextReaderPtr reader, osm_t::ref osm)
{
  base_attributes ba = process_base_attributes(reader, osm);

  way_t *way = new way_t(ba);
  assert_cmpnum(way->flags, 0);

  osm->insert(way);

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
      }

      skip_element(reader);
    }
    ret = xmlTextReaderRead(reader);
  }
  way->tags.replace(std::move(tags));
}

void
process_member(xmlTextReaderPtr reader, osm_t::ref osm, std::vector<member_t> &members)
{
  xmlString tp(xmlTextReaderGetAttribute(reader, BAD_CAST "type"));
  xmlString ref(xmlTextReaderGetAttribute(reader, BAD_CAST "ref"));
  xmlString role(xmlTextReaderGetAttribute(reader, BAD_CAST "role"));

  osm->parse_relation_member(tp, ref, role, members);
}

void
process_relation(xmlTextReaderPtr reader, osm_t::ref osm)
{
  base_attributes ba = process_base_attributes(reader, osm);

  relation_t *relation = new relation_t(ba);
  assert_cmpnum(relation->flags, 0);

  osm->insert(relation);

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
      if(strcmp(subname, "member") == 0)
        process_member(reader, osm, relation->members);
      else if(likely(strcmp(subname, "tag") == 0))
        process_tag(reader, tags);

      skip_element(reader);
    }
    ret = xmlTextReaderRead(reader);
  }
  relation->tags.replace(std::move(tags));
}

osm_t::UploadPolicy
parseUploadPolicy(const char *str)
{
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

osm_t *
process_osm(xmlTextReaderPtr reader)
{
  /* alloc osm structure */
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());

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
        std::optional<bounds_t> b = process_bounds(reader);
        if(unlikely(!b))
          return nullptr;
        osm->bounds = *b;
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

    default:
      break;
    }
    ret = xmlTextReaderRead(reader);

    if (num_elems++ > tick_every) {
      num_elems = 0;
      osm2go_platform::process_events();
    }
  }

  // no end tag for </osm> found in file, so assume it's invalid
  return nullptr;
}

struct relation_ref_functor {
  osm_t::ref osm;
  explicit inline relation_ref_functor(osm_t::ref o) : osm(o) {}
  void operator()(std::pair<item_id_t, relation_t *> p) {
    std::for_each(p.second->members.begin(), p.second->members.end(), *this);
  }
  void operator()(member_t &m) {
    if(m.object.type != object_t::RELATION_ID)
      return;
    relation_t *r = osm->object_by_id<relation_t>(m.object.get_id());
    if(r == nullptr)
      return;
    m.object = r;
  }
};

osm_t *
process_file(const std::string &filename)
{
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

} // namespace

/* ----------------------- end of stream parser ------------------- */

osm_t *osm_t::parse(const std::string &path, const std::string &filename) {

  // use stream parser
  if(unlikely(filename.find('/') != std::string::npos))
    return process_file(filename);
  else
    return process_file(path + filename);
}
