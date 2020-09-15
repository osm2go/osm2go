#include <icon.h>
#include <map.h>
#include <misc.h>
#include <osm.h>
#include <settings.h>

#include <osm2go_cpp.h>
#include <osm2go_i18n.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#if __cplusplus >= 201103L
#include <random>
#endif

#include <osm2go_annotations.h>

namespace {

void set_bounds(osm_t::ref o)
{
  bool b = o->bounds.init(pos_area(pos_t(52.2692786, 9.5750497), pos_t(52.2695463, 9.5755)));
  assert(b);
}

void test_description()
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  set_bounds(osm);
  lpos_t pos(1, 1);
  node_t *n = osm->node_new(pos);
  osm->node_attach(n);

  object_t o(n);
  assert_cmpstr(o.get_name(*osm), "unspecified node");

  // test the other "unspecified" code path: tags, but no known ones
  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("source", "bong"));
  n->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "unspecified node");

  tags.insert(osm_t::TagMap::value_type("name", "foo"));

  n->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "node: \"foo\"");

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "emergency_access_point"));
  tags.insert(osm_t::TagMap::value_type("ref", "H-112"));
  // the barrier must not override the highway information
  tags.insert(osm_t::TagMap::value_type("barrier", "bollard"));
  n->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "emergency access point: \"H-112\"");

  // test the special bollard code
  // have 2 tags, as the result could otherwise come from the "single tag" fallback code
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("barrier", "bollard"));
  tags.insert(osm_t::TagMap::value_type("start_date", "2019-04-01"));
  n->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "bollard");

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("barrier", "yes"));
  tags.insert(osm_t::TagMap::value_type("start_date", "2019-04-01"));
  n->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "barrier");

  way_t *w = new way_t();
  osm->way_attach(w);
  o = w;

  assert_cmpstr(o.get_name(*osm), "unspecified way");
  w->append_node(n);
  node_t *n2 = osm->node_new(pos);
  tags.clear();
  // prevent deletion of this node when the way count reaches 0
  tags.insert(osm_t::TagMap::value_type("keep", "me"));
  n2->tags.replace(tags);
  osm->node_attach(n2);
  w->append_node(n2);
  w->append_node(n);
  assert_cmpstr(o.get_name(*osm), "unspecified way/area");
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("area", "yes"));
  w->tags.replace(tags);
  // this is a bit too underspecified, so this case isn't explicitely catched
  assert_cmpstr(o.get_name(*osm), "area");
  // add some worthless tags that should not change the description in any way
  tags.insert(osm_t::TagMap::value_type("created_by", "testcase"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "area");
  tags.insert(osm_t::TagMap::value_type("source", "imagination"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "area");
  // give it some more information
  tags.insert(osm_t::TagMap::value_type("foo", "bar"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "unspecified area");
   osm_node_chain_free(w->node_chain);
  w->node_chain.clear();

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "pedestrian"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), _("pedestrian way"));
  tags.insert(osm_t::TagMap::value_type("area", "yes"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), _("pedestrian way"));
  // needs to be a closed way to be considered an area
  w->append_node(n);
  w->append_node(n2);
  w->append_node(n);
  assert_cmpstr(o.get_name(*osm), _("pedestrian area"));
  osm_node_chain_free(w->node_chain);
  w->node_chain.clear();

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "construction"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), _("road/street under construction"));
  tags.insert(osm_t::TagMap::value_type("construction", "foo"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), trstring("%1 road under construction").arg("foo").toStdString());
  // construction:highway is the proper namespaced tag, so prefer that one
  tags.insert(osm_t::TagMap::value_type("construction:highway", "bar"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), trstring("%1 road under construction").arg("bar").toStdString());
  tags.insert(osm_t::TagMap::value_type("name", "baz"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), trstring("%1 road under construction").arg("bar").toStdString() + ": \"baz\"");

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("name", "foo"));
  tags.insert(osm_t::TagMap::value_type("highway", "residential"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "residential road: \"foo\"");

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("ref", "B217"));
  tags.insert(osm_t::TagMap::value_type("highway", "primary"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "primary road: \"B217\"");

  // building without address given
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("building", "residential"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "building");

  tags.insert(osm_t::TagMap::value_type("addr:housename", "Baskerville Hall"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "building: \"Baskerville Hall\"");
  // name is favored over addr:housename
  tags.insert(osm_t::TagMap::value_type("name", "Brook Hall"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "building: \"Brook Hall\"");

  assert(!w->is_closed());
  // unclosed ways are not considered an area
  assert(!w->is_area());

  w->append_node(n);
  w->append_node(n2);
  w->append_node(n);

  assert(w->is_closed());
  // there is no explicit area tag, but all buildings are considered areas
  assert(w->is_area());

  // ... unless explicitely specified otherwise
  tags.insert(osm_t::TagMap::value_type("area", "no"));
  w->tags.replace(tags);
  assert(!w->is_area());

  // or we say it's no building
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("building", "no"));
  w->tags.replace(tags);
  assert(!w->is_area());

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("building", "residential"));
  tags.insert(osm_t::TagMap::value_type("addr:housenumber", "42"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "building housenumber 42");

  relation_t *r = new relation_t();
  osm->relation_attach(r);
  osm_t::TagMap rtags;
  rtags.insert(osm_t::TagMap::value_type("type", "associatedStreet"));
  rtags.insert(osm_t::TagMap::value_type("name", "21 Jump Street"));
  r->tags.replace(rtags);
  r->members.push_back(member_t(object_t(w), nullptr));
  // description should not have changed by now
  assert_cmpstr(o.get_name(*osm), "building housenumber 42");
  r->members.push_back(member_t(object_t(w), "house"));
  assert_cmpstr(o.get_name(*osm), "building 21 Jump Street 42");

  // addr:street takes precedence
  tags.insert(osm_t::TagMap::value_type("addr:street", "Highway to hell"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "building Highway to hell 42");

  // if there are not tags there is a description by relation
  w->tags.clear();
  assert_cmpstr(o.get_name(*osm), "way/area: member of associatedStreet '21 Jump Street'");

  // check PTv2 relation naming
  relation_t *pt_r = new relation_t();
  osm->relation_attach(pt_r);
  rtags.clear();
  rtags.insert(osm_t::TagMap::value_type("type", "public_transport"));
  rtags.insert(osm_t::TagMap::value_type("public_transport", "stop_area"));
  rtags.insert(osm_t::TagMap::value_type("name", "Kröpcke"));
  pt_r->tags.replace(rtags);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("public_transport", "platform"));
  o = n;
  n->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "platform");

  // wrong role
  pt_r->members.push_back(member_t(o, nullptr));
  assert_cmpstr(o.get_name(*osm), "platform");

  // correct role
  pt_r->members.push_back(member_t(o, "platform"));
  assert_cmpstr(o.get_name(*osm), "platform: \"Kröpcke\"");

  // local name takes precedence
  tags.insert(osm_t::TagMap::value_type("name", "Kroepcke"));
  n->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "platform: \"Kroepcke\"");

  // check description of untagged objects by relation membership
  o = w;
  relation_t *simple_r = new relation_t();
  osm->relation_attach(simple_r);
  simple_r->members.push_back(member_t(object_t(w)));

  // a relation with name takes precedence
  assert_cmpstr(o.get_name(*osm), "way/area: member of associatedStreet '21 Jump Street'");
  // drop the member with empty role
  r->remove_member(r->find_member_object(object_t(w)));
  assert_cmpstr(o.get_name(*osm), "way/area: 'house' in associatedStreet '21 Jump Street'");
  r->remove_member(r->find_member_object(object_t(w)));

  assert_cmpstr(o.get_name(*osm), "way/area: member of relation '<ID #-3>'");
  simple_r->members.clear();
  simple_r->members.push_back(member_t(object_t(w), "outer"));
  assert_cmpstr(o.get_name(*osm), "way/area: 'outer' in relation '<ID #-3>'");

  pt_r->members.push_back(member_t(object_t(w)));
  assert_cmpstr(o.get_name(*osm), "way/area: member of public transport 'Kröpcke'");
  pt_r->members.clear();
  pt_r->members.push_back(member_t(object_t(w), "foo"));
  assert_cmpstr(o.get_name(*osm), "way/area: 'foo' in public transport 'Kröpcke'");

  // test that underscores in the relation name get also replaced
  rtags.erase(rtags.findTag("name", "Kröpcke"));
  rtags.insert(osm_t::TagMap::value_type("name", "Kröp_cke"));
  pt_r->tags.replace(rtags);
  assert_cmpstr(o.get_name(*osm), "way/area: 'foo' in public transport 'Kröp cke'");

  // as well as role entries
  pt_r->members.clear();
  pt_r->members.push_back(member_t(object_t(w), "foo_bar"));
  assert_cmpstr(o.get_name(*osm), "way/area: 'foo bar' in public transport 'Kröp cke'");

  // multipolygons take precedence over other relations
  rtags.clear();
  rtags.insert(osm_t::TagMap::value_type("type", "multipolygon"));
  simple_r->tags.replace(rtags);
  assert(simple_r->is_multipolygon());
  assert_cmpstr(o.get_name(*osm), "way/area: 'outer' of multipolygon '<ID #-3>'");
  simple_r->members.clear();
  simple_r->members.push_back(member_t(object_t(w)));
  assert_cmpstr(o.get_name(*osm), "way/area: member of multipolygon '<ID #-3>'");

  // another relation, found first in the map because of lower id
  relation_t *other_r  = new relation_t();
  osm->relation_attach(other_r);
  other_r->members.push_back(member_t(object_t(w)));
  other_r->tags.replace(rtags);
  assert_cmpstr(o.get_name(*osm), "way/area: member of multipolygon '<ID #-4>'");

  // but if the first one has a name (or any non-default description) it is picked
  rtags.insert(osm_t::TagMap::value_type("name", "Deister"));
  simple_r->tags.replace(rtags);
  assert_cmpstr(o.get_name(*osm), "way/area: member of multipolygon 'Deister'");

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("building:part", "yes"));
  w->tags.replace(tags);
  // only a single tag, this is simply copied
  assert_cmpstr(o.get_name(*osm), "building:part");

  // there is still only a single tag because these 2 are ignored
  tags.insert(osm_t::TagMap::value_type("source", "foo"));
  tags.insert(osm_t::TagMap::value_type("created_by", "testcase"));
  w->tags.replace(tags);
  assert_cmpstr(o.get_name(*osm), "building:part");

  tags.insert(osm_t::TagMap::value_type("building:levels", "3"));
  w->tags.replace(tags);
  // but building:part is catched even if there are more tags
  assert_cmpstr(o.get_name(*osm), "building part");
}

} // namespace

int main()
{
  xmlInitParser();

  test_description();

  xmlCleanupParser();

  return 0;
}

#include "dummy_appdata.h"
