#include <icon.h>
#include <misc.h>
#include <osm.h>
#include <osm_objects.h>

#include <osm2go_cpp.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <libxml/parser.h>
#include <libxml/xmlstring.h>

namespace {

void
check_memberParser()
{
  xmlString tp(xmlStrdup(BAD_CAST "node"));
  xmlString refstr(xmlStrdup(BAD_CAST "47"));
  xmlString role;
  std::vector<member_t> members;
  std::unique_ptr<osm_t> o(std::make_unique<osm_t>());

  o->parse_relation_member(tp, refstr, role, members);
  assert_cmpnum(members.size(), 1);
  assert_null(members.front().role);
  assert_cmpnum(members.front().object.type, object_t::NODE_ID);
  assert_cmpnum(members.front().object.get_id(), 47);

  // no type
  tp.reset();
  o->parse_relation_member(tp, refstr, role, members);
  assert_cmpnum(members.size(), 1);

  // invalid type
  tp.reset(xmlStrdup(BAD_CAST "bogus"));
  o->parse_relation_member(tp, refstr, role, members);
  assert_cmpnum(members.size(), 1);

  // no ref
  tp.reset(xmlStrdup(BAD_CAST "way"));
  refstr.reset();
  o->parse_relation_member(tp, refstr, role, members);
  assert_cmpnum(members.size(), 1);

  // invalid ref
  refstr.reset(xmlStrdup(BAD_CAST "bogus"));
  o->parse_relation_member(tp, refstr, role, members);
  assert_cmpnum(members.size(), 1);

  // something valid again
  refstr.reset(xmlStrdup(BAD_CAST "42"));
  o->parse_relation_member(tp, refstr, role, members);

  assert_cmpnum(members.size(), 2);
  assert_null(members.front().role);
  assert_cmpnum(members.front().object.type, object_t::NODE_ID);
  assert_cmpnum(members.front().object.get_id(), 47);
  assert_null(members.back().role);
  assert_cmpnum(members.back().object.type, object_t::WAY_ID);
  assert_cmpnum(members.back().object.get_id(), 42);
}

template<typename T>
struct tag_counter {
  unsigned int &tags;
  unsigned int &tag_objs;
  tag_counter(unsigned int &t, unsigned int &o) : tags(t), tag_objs(o) {}
  void operator()(const tag_t &) {
    tags++;
  }
  void operator()(const std::pair<item_id_t, T *> &pair) {
    const base_object_t * const obj = pair.second;
    if(obj->tags.empty())
      return;
    tag_objs++;
    obj->tags.for_each(*this);
  }
};

} // namespace

int main(int argc, char **argv)
{
  if(argc != 2)
    return EINVAL;

  xmlInitParser();

  check_memberParser();

  std::unique_ptr<osm_t> osm(osm_t::parse(std::string(), argv[1]));
  if(!osm) {
    std::cerr << "cannot open " << argv[1] << ": " << strerror(errno) << std::endl;
    return 1;
  }

  unsigned int t[] = { 0, 0, 0 };
  unsigned int to[] = { 0, 0, 0 };

  std::for_each(osm->nodes.begin(), osm->nodes.end(), tag_counter<node_t>(t[0], to[0]));
  std::for_each(osm->ways.begin(), osm->ways.end(), tag_counter<way_t>(t[1], to[1]));
  std::for_each(osm->relations.begin(), osm->relations.end(), tag_counter<relation_t>(t[2], to[2]));

  std::cout
    << "Nodes: " << osm->nodes.size()     << ", " << to[0] << " with " << t[0] << " tags" << std::endl
    << "Ways: " << osm->ways.size()      << ", " << to[1] << " with " << t[1] << " tags" << std::endl
    << "Relations: " << osm->relations.size() << ", " << to[2] << " with " << t[2] << " tags" << std::endl;

  xmlCleanupParser();

  return 0;
}

#include "dummy_appdata.h"
