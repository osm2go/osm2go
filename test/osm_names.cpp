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

template<typename T> void
helper_node(const osm_t::TagMap &tags, T name)
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  set_bounds(osm);
  lpos_t pos(1, 1);
  node_t *n = osm->node_new(pos);
  osm->attach(n);

  n->tags.replace(tags);

  assert_cmpstr(object_t(n).get_name(*osm), name);
}

way_t *construct_way(std::unique_ptr<osm_t> &osm, int nodes)
{
  set_bounds(osm);

  way_t *w = new way_t();

  for (int i = 0; i < std::abs(nodes); i++) {
    node_t *n = osm->node_new(lpos_t(i, i * 2));
    osm->attach(n);
    w->append_node(n);
  }

  if (nodes < 0) {
    w->append_node(w->node_chain.front());
    assert(w->is_closed());
  } else {
    assert(!w->is_closed());
  }

  return osm->attach(w);
}

// if nodes is negative close the way
template<typename T> void
helper_way(const osm_t::TagMap &tags, T name, int nodes)
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  way_t * const w = construct_way(osm, nodes);

  w->tags.replace(tags);

  assert_cmpstr(object_t(w).get_name(*osm), name);
}

void test_unspecified()
{
  helper_node(osm_t::TagMap(), "unspecified node");

  // test the other "unspecified" code path: tags, but no known ones
  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("source", "bong"));
  helper_node(tags, "unspecified node");

  helper_way(osm_t::TagMap(), "unspecified way", 0);

  helper_way(osm_t::TagMap(), "unspecified way/area", -3);

  // this is a bit too underspecified, so this case isn't explicitely catched
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("area", "yes"));
  helper_way(tags, "area", -3);

  // add some worthless tags that should not change the description in any way
  tags.insert(osm_t::TagMap::value_type("created_by", "testcase"));
  helper_way(tags, "area", -3);

  tags.insert(osm_t::TagMap::value_type("source", "imagination"));
  helper_way(tags, "area", -3);

  // give it some more information
  tags.insert(osm_t::TagMap::value_type("foo", "bar"));
  helper_way(tags, "unspecified area", -3);
}

void test_unspecified_name()
{
  osm_t::TagMap tags;

  tags.insert(osm_t::TagMap::value_type("name", "foo"));
  helper_node(tags, "node: \"foo\"");

  tags.insert(osm_t::TagMap::value_type("source", "bong"));
  helper_node(tags, "node: \"foo\"");
}

void test_node_highway_ref()
{
  osm_t::TagMap tags;

  tags.insert(osm_t::TagMap::value_type("highway", "emergency_access_point"));
  tags.insert(osm_t::TagMap::value_type("ref", "H-112"));

  helper_node(tags, "emergency access point: \"H-112\"");

  // the barrier must not override the highway information
  tags.insert(osm_t::TagMap::value_type("barrier", "bollard"));
  helper_node(tags, "emergency access point: \"H-112\"");
}

void test_barrier()
{
  osm_t::TagMap tags;

  // test the special barrier code
  // have 2 tags, as the result could otherwise come from the "single tag" fallback code
  tags.insert(osm_t::TagMap::value_type("barrier", "bollard"));
  tags.insert(osm_t::TagMap::value_type("start_date", "2019-04-01"));

  helper_node(tags, "bollard");

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("barrier", "yes"));
  tags.insert(osm_t::TagMap::value_type("start_date", "2019-04-01"));

  helper_node(tags, "barrier");
}

void test_way_highway()
{
  osm_t::TagMap tags;

  tags.insert(osm_t::TagMap::value_type("highway", "pedestrian"));
  helper_way(tags, _("pedestrian way"), 0);

  // no area without specifying it as area
  tags.insert(osm_t::TagMap::value_type("highway", "pedestrian"));
  helper_way(tags, _("pedestrian way"), -3);

  tags.insert(osm_t::TagMap::value_type("area", "yes"));
  helper_way(tags, _("pedestrian way"), 0);

  // needs to be a closed way to be considered an area
  helper_way(tags, _("pedestrian area"), -3);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "construction"));
  helper_way(tags, _("road under construction"), 0);

  tags.insert(osm_t::TagMap::value_type("construction", "emergency_access_point"));
  helper_way(tags, trstring("%1 under construction").arg("emergency access point"), 0);

  // construction:highway is the proper namespaced tag, so prefer that one
  tags.insert(osm_t::TagMap::value_type("construction:highway", "path"));
  helper_way(tags, trstring("%1 under construction").arg("path"), 0);

  tags.insert(osm_t::TagMap::value_type("name", "baz"));
  helper_way(tags, trstring("%1 under construction").arg("path").toStdString() + ": \"baz\"", 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("name", "foo"));
  tags.insert(osm_t::TagMap::value_type("highway", "residential"));
  helper_way(tags, "residential road: \"foo\"", 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("ref", "B217"));
  tags.insert(osm_t::TagMap::value_type("highway", "primary"));
  helper_way(tags, "primary road: \"B217\"", 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "abandoned"));
  helper_way(tags, trstring("abandoned %1").arg(_("road")), 0);

  tags.insert(osm_t::TagMap::value_type("name", "foo"));
  helper_way(tags, trstring("%1: \"%2\"").arg(trstring("abandoned %1").arg(_("road"))).arg("foo"), 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "proposed"));
  tags.insert(osm_t::TagMap::value_type("proposed:highway", "primary"));
  helper_way(tags, trstring("proposed %1").arg(trstring("%1 road").arg("primary")), 0);
}

void test_way_building_simple()
{
  osm_t::TagMap tags;

  tags.insert(osm_t::TagMap::value_type("building", "yes"));
  helper_way(tags, _("building"), 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("building", "residential"));
  helper_way(tags, "residential building", 0);

  tags.insert(osm_t::TagMap::value_type("addr:housename", "Baskerville Hall"));
  helper_way(tags, "residential building: \"Baskerville Hall\"", 0);

  // name is favored over addr:housename
  tags.insert(osm_t::TagMap::value_type("name", "Brook Hall"));
  helper_way(tags, "residential building: \"Brook Hall\"", 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("building:part", "yes"));
  helper_way(tags, "building part", -3);

  // there is still only a single tag because these 2 are ignored
  tags.insert(osm_t::TagMap::value_type("source", "foo"));
  tags.insert(osm_t::TagMap::value_type("created_by", "testcase"));
  helper_way(tags, "building part", -3);

  tags.insert(osm_t::TagMap::value_type("building:levels", "3"));
  helper_way(tags, "building part", -3);

  // lifecycle
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("building", "abandoned"));
  helper_way(tags, trstring("abandoned %1").arg(_("building")), 0);

  tags.insert(osm_t::TagMap::value_type("name", "Brook Hall"));
  helper_way(tags, trstring("%1: \"%2\"").arg(trstring("abandoned %1").arg(_("building"))).arg("Brook Hall"), 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("building", "abandoned"));
  tags.insert(osm_t::TagMap::value_type("addr:street", "Heisterweg"));
  tags.insert(osm_t::TagMap::value_type("addr:housenumber", "2"));
  helper_way(tags, trstring("abandoned %1").arg(trstring("building %1 %2").arg("Heisterweg").arg("2")), 0);

  // specify the former type
  tags.insert(osm_t::TagMap::value_type("abandoned", "civic"));
  helper_way(tags, trstring("abandoned %1").arg(trstring("%1 building %2 %3").arg("civic").arg("Heisterweg").arg("2")), 0);

  // explicit type tag takes precedence
  tags.insert(osm_t::TagMap::value_type("abandoned:building", "fire_station"));
  helper_way(tags, trstring("abandoned %1").arg(trstring("%1 building %2 %3").arg("fire station").arg("Heisterweg").arg("2")), 0);
}

void test_way_building_area()
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  way_t *w = construct_way(osm, 0);

  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("building", "residential"));

  w->tags.replace(tags);
  assert(!w->is_closed());
  // unclosed ways are not considered an area
  assert(!w->is_area());

  w = construct_way(osm, -3);
  w->tags.replace(tags);
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
}

void test_way_building_relation()
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  way_t *w = construct_way(osm, -3);

  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("building", "yes"));
  tags.insert(osm_t::TagMap::value_type("addr:housenumber", "42"));

  w->tags.replace(tags);
  assert_cmpstr(object_t(w).get_name(*osm), "building housenumber 42");

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("building", "residential"));
  tags.insert(osm_t::TagMap::value_type("addr:housenumber", "42"));

  w->tags.replace(tags);
  assert_cmpstr(object_t(w).get_name(*osm), "residential building housenumber 42");

  relation_t *r = new relation_t();
  osm->attach(r);
  osm_t::TagMap rtags;
  rtags.insert(osm_t::TagMap::value_type("type", "associatedStreet"));
  rtags.insert(osm_t::TagMap::value_type("name", "21 Jump Street"));
  r->tags.replace(rtags);
  r->members.push_back(member_t(object_t(w), nullptr));

  // description should not have changed by now
  assert_cmpstr(object_t(w).get_name(*osm), "residential building housenumber 42");
  r->members.push_back(member_t(object_t(w), "house"));
  assert_cmpstr(object_t(w).get_name(*osm), "residential building 21 Jump Street 42");

  // addr:street takes precedence
  tags.insert(osm_t::TagMap::value_type("addr:street", "Highway to hell"));
  w->tags.replace(tags);
  assert_cmpstr(object_t(w).get_name(*osm), "residential building Highway to hell 42");

  // if there are not tags there is a description by relation
  w->tags.clear();
  assert_cmpstr(object_t(w).get_name(*osm), "way/area: member of associatedStreet \"21 Jump Street\"");

  // when this is no building, it is no building
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("building", "no"));
  w->tags.replace(tags);
  assert_cmpstr(object_t(w).get_name(*osm), "way/area: member of associatedStreet \"21 Jump Street\"");

  // but when it is, it is
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("building", "yes"));
  w->tags.replace(tags);
  r->members.clear();
  r->members.push_back(member_t(object_t(w), "house"));
  assert_cmpstr(object_t(w).get_name(*osm), "building in 21 Jump Street");
}

void test_multipolygon()
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  way_t * const w = construct_way(osm, -3);

  relation_t *simple_r = new relation_t();
  osm->attach(simple_r);
  simple_r->members.push_back(member_t(object_t(w), "outer"));

  // multipolygons take precedence over other relations
  osm_t::TagMap rtags;
  rtags.clear();
  rtags.insert(osm_t::TagMap::value_type("type", "multipolygon"));
  simple_r->tags.replace(rtags);
  assert(simple_r->is_multipolygon());
  assert_cmpstr(object_t(w).get_name(*osm), "way/area: 'outer' of multipolygon <ID #-1>");
  simple_r->members.clear();
  simple_r->members.push_back(member_t(object_t(w)));
  assert_cmpstr(object_t(w).get_name(*osm), "way/area: member of multipolygon <ID #-1>");

  // another relation, found first in the map because of lower id
  relation_t *other_r  = new relation_t();
  osm->attach(other_r);
  other_r->members.push_back(member_t(object_t(w)));
  other_r->tags.replace(rtags);
  assert_cmpstr(object_t(w).get_name(*osm), "way/area: member of multipolygon <ID #-2>");

  // but if the first one has a name (or any non-default description) it is picked
  rtags.insert(osm_t::TagMap::value_type("name", "Deister"));
  simple_r->tags.replace(rtags);
  assert_cmpstr(object_t(w).get_name(*osm), "way/area: member of multipolygon \"Deister\"");

  // and if the name is our magic ID string then it is of course also enclosed in quotes
  rtags.clear();
  rtags.insert(osm_t::TagMap::value_type("type", "multipolygon"));
  rtags.insert(osm_t::TagMap::value_type("name", "<ID #-2>"));
  simple_r->tags.replace(rtags);
  assert_cmpstr(object_t(w).get_name(*osm), "way/area: member of multipolygon \"<ID #-2>\"");
}

void test_relation_precedence()
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  set_bounds(osm);
  way_t * const w = construct_way(osm, -2);
  node_t * const n = w->node_chain.front();

  osm_t::TagMap tags;

  object_t o(w);
  relation_t *r = new relation_t();
  osm->attach(r);
  osm_t::TagMap rtags;
  rtags.insert(osm_t::TagMap::value_type("type", "associatedStreet"));
  rtags.insert(osm_t::TagMap::value_type("name", "21 Jump Street"));
  r->tags.replace(rtags);
  r->members.push_back(member_t(object_t(w), nullptr));
  // description should not have changed by now
  r->members.push_back(member_t(object_t(w), "house"));

  // if there are not tags there is a description by relation
  assert_cmpstr(o.get_name(*osm), "way/area: member of associatedStreet \"21 Jump Street\"");

  // check PTv2 relation naming
  relation_t *pt_r = new relation_t();
  osm->attach(pt_r);
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
  osm->attach(simple_r);
  simple_r->members.push_back(member_t(object_t(w)));

  // a relation with name takes precedence
  assert_cmpstr(o.get_name(*osm), "way/area: member of associatedStreet \"21 Jump Street\"");
  // drop the member with empty role
  r->eraseMember(r->find_member_object(object_t(w)));
  assert_cmpstr(o.get_name(*osm), "way/area: 'house' in associatedStreet \"21 Jump Street\"");
  r->eraseMember(r->find_member_object(object_t(w)));

  assert_cmpstr(o.get_name(*osm), "way/area: member of relation <ID #-3>");
  simple_r->members.clear();
  simple_r->members.push_back(member_t(object_t(w), "outer"));
  assert_cmpstr(o.get_name(*osm), "way/area: 'outer' in relation <ID #-3>");

  pt_r->members.push_back(member_t(object_t(w)));
  assert_cmpstr(o.get_name(*osm), "way/area: member of public transport \"Kröpcke\"");
  pt_r->members.clear();
  pt_r->members.push_back(member_t(object_t(w), "foo"));
  assert_cmpstr(o.get_name(*osm), "way/area: 'foo' in public transport \"Kröpcke\"");

  // test that underscores in the relation name get also replaced
  rtags.erase(rtags.findTag("name", "Kröpcke"));
  rtags.insert(osm_t::TagMap::value_type("name", "Kröp_cke"));
  pt_r->tags.replace(rtags);
  assert_cmpstr(o.get_name(*osm), "way/area: 'foo' in public transport \"Kröp cke\"");

  // as well as role entries
  pt_r->members.clear();
  pt_r->members.push_back(member_t(object_t(w), "foo_bar"));
  assert_cmpstr(o.get_name(*osm), "way/area: 'foo bar' in public transport \"Kröp cke\"");
}

void test_sport()
{
  osm_t::TagMap tags;

  tags.insert(osm_t::TagMap::value_type("leisure", "pitch"));
  helper_way(tags, "pitch", -3);

  tags.insert(osm_t::TagMap::value_type("sport", "soccer"));
  helper_way(tags, "soccer pitch", -3);

  tags.insert(osm_t::TagMap::value_type("name", "Waldsportplatz"));
  helper_way(tags, "soccer pitch: \"Waldsportplatz\"", -3);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("leisure", "sports_centre"));
  tags.insert(osm_t::TagMap::value_type("sport", "american_football"));
  helper_node(tags, "american football sports centre");

  // fallback to the single value mode
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("sport", "american_football"));
  helper_node(tags, "sport");

  // this tag is not in the explicit whitelist, so "sport" is ignored
  tags.insert(osm_t::TagMap::value_type("leisure", "bowling_alley"));
  helper_node(tags, "bowling alley");
}

void test_simple()
{
  osm_t::TagMap tags;

  tags.insert(osm_t::TagMap::value_type("amenity", "waste_basket"));
  helper_node(tags, "waste basket");

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("emergency", "fire_hydrant"));
  tags.insert(osm_t::TagMap::value_type("ref", "42"));
  helper_node(tags, "fire hydrant: \"42\"");

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("shop", "beauty"));
  helper_node(tags, "beauty shop");
}

// lifecycle replacements for things that are not handled specially like buildings and highways
void test_lifecycle()
{
  osm_t::TagMap tags;

  tags.insert(osm_t::TagMap::value_type("railway", "tram"));
  helper_way(tags, "tram", 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("railway", "abandoned"));
  helper_way(tags, trstring("abandoned %1").arg("railway"), 0);

  tags.insert(osm_t::TagMap::value_type("abandoned", "tram"));
  helper_way(tags, trstring("abandoned %1").arg("tram"), 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "construction"));
  helper_way(tags, "road under construction", 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "abandoned"));
  helper_way(tags, "abandoned road", 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "proposed"));
  helper_way(tags, "proposed road", 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("railway", "abandoned"));
  helper_way(tags, "abandoned railway", 0);

  tags.clear();
  // there are several of these, all mapped to the same display value for simplicity
  tags.insert(osm_t::TagMap::value_type("highway", "razed"));
  helper_way(tags, "demolished road", 0);

  // other things under construction
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("railway", "construction"));
  helper_way(tags, "railway under construction", 0);

  tags.insert(osm_t::TagMap::value_type("construction", "light_rail"));
  helper_way(tags, "light rail under construction", 0);

  // if some active element exists instead this should be used
  tags.insert(osm_t::TagMap::value_type("highway", "cycleway"));
  helper_way(tags, "cycleway", 0);

  // the other way
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "abandoned"));
  tags.insert(osm_t::TagMap::value_type("abandoned:highway", "path"));
  helper_way(tags, trstring("abandoned %1").arg("path"), 0);

  tags.insert(osm_t::TagMap::value_type("railway", "tram"));
  helper_way(tags, "tram", 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("building", "abandoned"));
  helper_way(tags, trstring("abandoned %1").arg(trstring("building")), 0);

  tags.insert(osm_t::TagMap::value_type("abandoned:building", "residential"));
  helper_way(tags, trstring("abandoned %1").arg(trstring("%1 building").arg("residential")), 0);

  // the tagging doesn't make sense, but who cares ;)
  tags.insert(osm_t::TagMap::value_type("emergency", "fire_hydrant"));
  helper_way(tags, "fire hydrant", 0);

  // something that only exists in a prefixed namespace
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("demolished:railway", "rail"));
  helper_way(tags, trstring("demolished %1").arg("rail"), 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("demolished:railway", "yes"));
  helper_way(tags, trstring("demolished %1").arg("railway"), 0);

  // same for buildings
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("demolished:building", "yes"));
  helper_way(tags, trstring("demolished %1").arg(trstring("building")), 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("demolished:building", "commercial"));
  helper_way(tags, trstring("demolished %1").arg(trstring("%1 building").arg("commercial")), 0);

  // same for highways
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("demolished:highway", "yes"));
  helper_way(tags, trstring("demolished %1").arg(trstring("road")), 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("demolished:highway", "secondary"));
  helper_way(tags, trstring("demolished %1").arg(trstring("%1 road").arg("secondary")), 0);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("disused:shop", "beauty"));
  helper_node(tags, "disused beauty shop");
}

void test_power_generator()
{
  osm_t::TagMap tags;

  tags.insert(osm_t::TagMap::value_type("power", "generator"));
  helper_node(tags, _("power generator"));

  tags.insert(osm_t::TagMap::value_type("generator:source", "solar"));
  helper_node(tags, trstring("%1 power generator").arg("solar"));

  tags.insert(osm_t::TagMap::value_type("generator:method", "photovoltaic"));
  helper_node(tags, trstring("%1 power generator").arg("photovoltaic"));

  tags.insert(osm_t::TagMap::value_type("generator:type", "solar_photovoltaic_panel"));
  helper_node(tags, "solar photovoltaic panel");

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("power", "generator"));
  tags.insert(osm_t::TagMap::value_type("generator:source", "dark_matter"));
  helper_node(tags, trstring("%1 power generator").arg("dark matter"));

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("power", "sink"));
  helper_node(tags, "sink");
}

void test_railway_signals()
{
  osm_t::TagMap tags;

  tags.insert(osm_t::TagMap::value_type("railway", "signal"));
  helper_node(tags, _("signal"));

  tags.insert(osm_t::TagMap::value_type("ref", "U 13"));
  helper_node(tags, trstring("%1: \"%2\"").arg("signal").arg(trstring("U 13")));

  tags.insert(osm_t::TagMap::value_type("railway:signal:combined", "DE-ESO:ks"));
  helper_node(tags, trstring("%1: \"%2\"").arg(trstring("%1 %2 signal").arg("DE-ESO:ks").arg("combined")).arg("U 13"));

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("railway", "signal"));
  tags.insert(osm_t::TagMap::value_type("railway:signal:combined", "DE-ESO:ks"));
  helper_node(tags, trstring("%1 %2 signal").arg("DE-ESO:ks").arg("combined"));

  // additional signal types that should be ignored because combined is present
  tags.insert(osm_t::TagMap::value_type("railway:signal:speed_limit", "DE-ESO:zs3"));
  tags.insert(osm_t::TagMap::value_type("railway:signal:speed_limit_distant", "DE-ESO:zs3v"));
  helper_node(tags, trstring("%1 %2 signal").arg("DE-ESO:ks").arg("combined"));

  // now a different hierarchy
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("railway", "signal"));
  tags.insert(osm_t::TagMap::value_type("railway:signal:speed_limit", "DE-ESO:zs3"));
  tags.insert(osm_t::TagMap::value_type("railway:signal:speed_limit_distant", "DE-ESO:zs3v"));
  helper_node(tags, trstring("%1 %2 signal").arg("DE-ESO:zs3").arg("speed limit"));

  // also should work for something entirely different
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("railway", "signal"));
  tags.insert(osm_t::TagMap::value_type("railway:signal:foo", "bar"));
  helper_node(tags, trstring("%1 %2 signal").arg("bar").arg("foo"));

  // special values that should not end up being used
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("railway", "signal"));
  tags.insert(osm_t::TagMap::value_type("railway:signal:direction", "forward"));
  tags.insert(osm_t::TagMap::value_type("railway:signal:position", "left"));
  helper_node(tags, _("signal"));

  // subtags must not be found either
  tags.insert(osm_t::TagMap::value_type("railway:signal:foo:bar", "baz"));
  helper_node(tags, _("signal"));
}

} // namespace

int main()
{
  xmlInitParser();

  test_unspecified();
  test_unspecified_name();
  test_node_highway_ref();
  test_barrier();
  test_way_highway();
  test_way_building_simple();
  test_way_building_area();
  test_way_building_relation();
  test_multipolygon();
  test_relation_precedence();
  test_sport();
  test_simple();
  test_lifecycle();
  test_power_generator();
  test_railway_signals();

  xmlCleanupParser();

  return 0;
}

#include "dummy_appdata.h"
