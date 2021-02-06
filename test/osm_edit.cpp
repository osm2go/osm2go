/*
 * SPDX-FileCopyrightText: 2017-2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "test_osmdb.h"

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

bool find_aa(const tag_t &t)
{
  return strcmp(t.value, "aa") == 0;
}

bool find_bb(const tag_t &t)
{
  return strcmp(t.value, "bb") == 0;
}

std::vector<tag_t> ab_with_creator()
{
  std::vector<tag_t> ntags;

  tag_t cr_by = tag_t::uncached("created_by", "test");
  assert(cr_by.is_discardable());
  ntags.push_back(cr_by);
  ntags.push_back(tag_t("a", "aa"));
  ntags.push_back(tag_t("b", "bb"));

  return ntags;
}

bool rtrue(const tag_t &)
{
  return true;
}

void nevercalled(const tag_t &)
{
  assert_unreachable();
}

void set_bounds(osm_t::ref o)
{
  bool b = o->bounds.init(pos_area(pos_t(52.2692786, 9.5750497), pos_t(52.2695463, 9.5755)));
  assert(b);
}

/**
 * @brief collection of trivial tests to get some coverage
 */
void test_trivial()
{
  object_t obj;

  assert(obj == obj);

  tag_list_t tags;
  assert(!tags.hasTagCollisions());
  assert_null(tags.singleTag());
  tag_t cr_by("created_by", "test");
  assert(cr_by.is_discardable());
  std::vector<tag_t> ntags(1, cr_by);
  tags.replace(std::move(ntags));
  assert(!tags.hasRealTags());
  assert(!tags.hasNonDiscardableTags());
  assert(!tags.hasTagCollisions());
  // only trivial tag
  assert_null(tags.singleTag());
  tag_t src("source", "test");
  assert(!src.is_discardable());
  ntags.clear();
  ntags.push_back(cr_by);
  ntags.push_back(src);
  tags.replace(std::move(ntags));
  // still only trivial tags
  assert_null(tags.singleTag());
  assert(!tags.hasRealTags());
  assert(tags.hasNonDiscardableTags());
  assert(!tags.hasTagCollisions());
  // another discardable
  tag_t tiger("tiger:source", "foobar");
  ntags.clear();
  ntags.push_back(cr_by);
  ntags.push_back(src);
  ntags.push_back(tiger);
  tags.replace(std::move(ntags));
  assert_null(tags.singleTag());
  assert(!tags.hasRealTags());
  assert(tags.hasNonDiscardableTags());
  assert(!tags.hasTagCollisions());

  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  osm->bounds.min = lpos_t(0, 0);
  osm->bounds.max = lpos_t(0, 0);
  assert_cmpstr(osm->sanity_check(), trstring::native_type(_("Invalid data in OSM file:\nBoundary box invalid!")));
  set_bounds(osm);
  assert_cmpstr(osm->sanity_check(), trstring::native_type(_("Invalid data in OSM file:\nNo drawable content found!")));

  assert(osm->bounds.contains(lpos_t(0, 0)));
  assert(!osm->bounds.contains(lpos_t(-1, 0)));
  assert(!osm->bounds.contains(lpos_t(0, -1)));

  way_t *w = osm->attach(new way_t());
  // must work even on empty way
  assert_null(w->first_node());
  assert_null(w->last_node());

  lpos_t l(10, 20);
  node_t *n = osm->node_new(l);
  osm->attach(n);
  // the sanity check look on the node map which now isn't empty anymore
  assert(osm->sanity_check().isEmpty());

  w->append_node(n);
  assert(w->ends_with_node(n));
  // deleted ways never return true for any node
  w->flags |= OSM_FLAG_DELETED;
  assert(!w->ends_with_node(n));

  relation_t *r = osm->attach(new relation_t());
  object_t robj(r);
  // check compare
  assert(robj.operator==(r));
  assert(robj.operator!=(w));

  object_t inv;
  assert_cmpnum(inv.get_id(), ID_ILLEGAL);

  assert_null(r->descriptiveName());
  assert_cmpstr(r->descriptiveNameOrId(), "<ID #-1>");

  osm_t::TagMap tmap;
  tmap.insert(osm_t::TagMap::value_type("ref", "KHM 55"));
  r->tags.replace(tmap);
  assert_cmpstr(r->descriptiveName(), "KHM 55");
  assert_cmpstr(r->descriptiveNameOrId(), "KHM 55");
  // one non-trivial tag
  const tag_t *st = r->tags.singleTag();
  assert(st != nullptr);
  assert_cmpstr(st->key, "ref");
  assert_cmpstr(st->value, "KHM 55");
  // name is preferred over ref
  tmap.insert(osm_t::TagMap::value_type("name", "Rumpelstilzchen"));
  r->tags.replace(tmap);
  assert_cmpstr(r->descriptiveNameOrId(), "Rumpelstilzchen");
  // multiple non-trivial tags
  assert_null(r->tags.singleTag());
  // another way to clear
  std::vector<tag_t> notags;
  r->tags.replace(std::move(notags));
  assert_null(r->descriptiveName());
  assert_cmpstr(r->idName(), "<ID #-1>");
  r->id = std::numeric_limits<typeof(r->id)>::max();
  assert_cmpstr(r->idName(), "<ID #9223372036854775807>");
  r->id = std::numeric_limits<typeof(r->id)>::min();
  assert_cmpstr(r->idName(), "<ID #-9223372036854775808>");

  member_t mb(object_t::RELATION);
  assert_null(mb.role);
}

void test_taglist()
{
  tag_list_t tags;
  std::vector<tag_t> ntags;

  // compare empty lists
  assert(tags == ntags);
  assert(!(tags != ntags));

  // a list with only created_by must still be considered empty
  tag_t cr_by(const_cast<char *>("created_by"), const_cast<char *>("test"));
  assert(cr_by.is_discardable());
  ntags.push_back(cr_by);
  assert(tags == ntags);
  assert(!(tags != ntags));
  ntags.clear();

  // check replacing the tag list from osm_t::TagMap::value_type
  osm_t::TagMap nstags;
  nstags.insert(osm_t::TagMap::value_type("a", "A"));
  nstags.insert(osm_t::TagMap::value_type("b", "B"));

  // check self intersection
  assert(osm_t::tagSubset(nstags, nstags));
  // real subsets
  osm_t::TagMap tmpTags;
  assert(osm_t::tagSubset(tmpTags, nstags));
  tmpTags.insert(osm_t::TagMap::value_type("a", "A"));
  assert(osm_t::tagSubset(tmpTags, nstags));
  tmpTags.clear();
  tmpTags.insert(osm_t::TagMap::value_type("b", "B"));
  assert(osm_t::tagSubset(tmpTags, nstags));
  // non-intersecting
  tmpTags.insert(osm_t::TagMap::value_type("c", "C"));
  assert(!osm_t::tagSubset(tmpTags, nstags));
  assert(!osm_t::tagSubset(nstags, tmpTags));

  tags.replace(nstags);

  assert_cmpnum(nstags.size(), 2);
  assert(tags.get_value("a") != nullptr);
  assert_cmpstr(tags.get_value("a"), "A");
  assert(tags.get_value("b") != nullptr);
  assert_cmpstr(tags.get_value("b"), "B");
  assert(!tags.hasTagCollisions());

  // check replacing the tag list from tag_t
  ntags.push_back(tag_t("a", "aa"));
  ntags.push_back(tag_t("b", "bb"));

  tags.replace(std::move(ntags));

  assert(tags.get_value("a") != nullptr);
  assert_cmpstr(tags.get_value("a"), "aa");
  assert(tags.get_value("b") != nullptr);
  assert_cmpstr(tags.get_value("b"), "bb");
  assert(!tags.hasTagCollisions());

  osm_t::TagMap lowerTags = tags.asMap();

  // replace again
  tags.replace(nstags);

  assert_cmpnum(nstags.size(), 2);
  assert(tags.get_value("a") != nullptr);
  assert_cmpstr(tags.get_value("a"), "A");
  assert(tags.get_value("b") != nullptr);
  assert_cmpstr(tags.get_value("b"), "B");
  assert(!tags.hasTagCollisions());

  tag_list_t tags2;
  tags2.replace(nstags);

  // merging the same things shouldn't change anything
  assert(!tags.merge(tags2));
  assert(!tags.hasTagCollisions());

  assert(tags.get_value("a") != nullptr);
  assert_cmpstr(tags.get_value("a"), "A");
  assert(tags.get_value("b") != nullptr);
  assert_cmpstr(tags.get_value("b"), "B");

  assert_null(tags2.get_value("a"));
  assert_null(tags2.get_value("b"));

  tags2.replace(lowerTags);
  assert_cmpnum(tags2.asMap().size(), 2);
  assert(!lowerTags.empty());
  assert(tags2.get_value("a") != nullptr);
  assert_cmpstr(tags2.get_value("a"), "aa");
  assert(tags2.get_value("b") != nullptr);
  assert_cmpstr(tags2.get_value("b"), "bb");
  assert(!osm_t::tagSubset(tags2.asMap(), tags.asMap()));
  assert(!osm_t::tagSubset(tags.asMap(), tags2.asMap()));

  assert(tags.merge(tags2));
  // moving something back and forth shouldn't change anything
  assert(!tags2.merge(tags));
  assert(!tags.merge(tags2));
  // tags2 is now empty, merging shouldn't change anything
  assert(tags2.empty());
  assert(!tags.merge(tags2));

  assert(tags.hasTagCollisions());
  assert(tags.get_value("a") != nullptr);
  assert_cmpstr(tags.get_value("a"), "A");
  assert(tags.get_value("b") != nullptr);
  assert_cmpstr(tags.get_value("b"), "B");
  assert_cmpnum(tags.asMap().size(), 4);
  assert(tags.contains(find_aa));
  assert(tags.contains(find_bb));

  // check identity with permutations
  ntags = ab_with_creator();
  tags.replace(std::move(ntags));
  ntags = ab_with_creator();
  assert(tags == ntags);
  std::rotate(ntags.begin(), std::next(ntags.begin()), ntags.end());
  assert(tags == ntags);
  std::rotate(ntags.begin(), std::next(ntags.begin()), ntags.end());
  assert(tags == ntags);
  // other discardable tags shouldn't change identity
  tag_t tiger("tiger:source", "foobar");
  ntags.push_back(tiger);
  assert(tags == ntags);

  ntags.clear();
  tags.clear();

  // check that all these methods work on empty objects, both newly created and cleared ones
  assert(tags.empty());
  assert(!tags.hasRealTags());
  assert_null(tags.get_value("foo"));
  assert(!tags.contains(rtrue));
  tags.for_each(nevercalled);
  assert(tags.asMap().empty());
  assert(tags == std::vector<tag_t>());
  assert(tags == osm_t::TagMap());
  tags.clear();

  tag_list_t virgin;
  assert(virgin.empty());
  assert(!virgin.hasRealTags());
  assert_null(virgin.get_value("foo"));
  assert(!virgin.contains(rtrue));
  virgin.for_each(nevercalled);
  assert(virgin.asMap().empty());
  assert(virgin == std::vector<tag_t>());
  assert(virgin == osm_t::TagMap());
  virgin.clear();

  ntags.push_back(tag_t("one", "1"));
  assert(tags != ntags);
  tags.replace(std::move(ntags));
  ntags.clear();
  ntags.push_back(tag_t("one", "1"));
  assert(tags == ntags);
  ntags.clear();
  ntags.push_back(tag_t::uncached("one", "1"));
  assert(tags == ntags);
  assert(virgin != tags.asMap());
}

void test_replace_tags()
{
  std::unique_ptr<osm_t> o(std::make_unique<osm_t>());
  set_bounds(o);

  base_attributes ba(47);
  ba.version = 1;
  node_t *nd = o->node_new(pos_t(0, 0), ba);
  o->insert(nd);
  node_t &node = *nd;
  assert_cmpnum(node.flags, 0);

  assert(node.tags.empty());

  osm_t::TagMap nstags;
  o->updateTags(object_t(nd), nstags);
  assert_cmpnum(node.flags, 0);
  assert(node.tags.empty());
  verify_osm_db::run(o);

  osm_t::TagMap::value_type cr_by("created_by", "test");
  assert(tag_t::is_discardable(cr_by.first.c_str()));
  nstags.insert(cr_by);
  o->updateTags(object_t(nd), nstags);
  assert_cmpnum(node.flags, 0);
  assert(node.tags.empty());
  verify_osm_db::run(o);

  node.tags.replace(nstags);
  assert_cmpnum(node.flags, 0);
  assert(node.tags.empty());

  osm_t::TagMap::value_type aA("a", "A");
  nstags.insert(aA);

  o->updateTags(object_t(nd), nstags);
  assert_cmpnum(node.flags, OSM_FLAG_DIRTY);
  assert(!node.tags.empty());
  assert(node.tags == nstags);
  verify_osm_db::run(o);

  o->unmark_dirty(nd);

  o->updateTags(object_t(nd), nstags);
  assert_cmpnum(node.flags, 0);
  assert(!node.tags.empty());
  assert(node.tags == nstags);
  verify_osm_db::run(o);

  node.tags.clear();
  assert(node.tags.empty());

  // use the other replace() variant that is also used by diff_restore(),
  // which can also insert created_by tags
  std::vector<tag_t> ntags;
  ntags.push_back(tag_t("created_by", "foo"));
  ntags.push_back(tag_t("a", "A"));
  node.tags.replace(std::move(ntags));

  assert_cmpnum(node.flags, 0);
  assert(!node.tags.empty());
  assert(node.tags == nstags);

  // updating with the same "real" tag shouldn't change anything
  o->updateTags(object_t(nd), nstags);
  assert_cmpnum(node.flags, 0);
  assert(!node.tags.empty());
  assert(node.tags == nstags);
  verify_osm_db::run(o);
}

unsigned int intrnd(unsigned int r)
{
  return std::rand() % r;
}

void test_split()
{
  std::unique_ptr<osm_t> o(std::make_unique<osm_t>());
  set_bounds(o);

  way_t * const v = new way_t();
  base_attributes ba(47);
  ba.version = 1;
  way_t * const w = new way_t(ba);
  relation_t * const r1 = new relation_t();
  relation_t * const r2 = new relation_t();
  relation_t * const r3 = new relation_t();

  std::vector<tag_t> otags;
  otags.push_back(tag_t("a", "b"));
  otags.push_back(tag_t("b", "c"));
  otags.push_back(tag_t("created_by", "test"));
  otags.push_back(tag_t("d", "e"));
  otags.push_back(tag_t("f", "g"));
  const size_t ocnt = otags.size();

  w->tags.replace(std::move(otags));
  v->tags.replace(w->tags.asMap());

  o->attach(v);
  o->insert(w);

  r1->members.push_back(member_t(object_t(w)));
  o->attach(r1);
  r2->members.push_back(member_t(object_t(w)));
  r2->members.push_back(member_t(object_t(v)));
  // insert twice, to check if all entries get duplicated
  r2->members.push_back(member_t(object_t(w)));
  o->attach(r2);
  r3->members.push_back(member_t(object_t(v)));
  o->attach(r3);

  // create the way to split
  std::vector<node_t *> nodes;
  for(int i = 0; i < 6; i++) {
    node_t *n = o->node_new(pos_t(52.25 + i * 0.001, 9.58 + i * 0.001));
    o->attach(n);
    v->node_chain.push_back(n);
    w->node_chain.push_back(n);
    n->ways += 2;
    nodes.push_back(n);
  }

  assert_cmpnum(o->ways.size(), 2);
  way_t *neww = w->split(o, std::next(w->node_chain.begin(), 2), false);
  assert(neww != nullptr);
  assert_cmpnum(o->ways.size(), 3);
  assert(w->flags & OSM_FLAG_DIRTY);
  for(unsigned int i = 0; i < nodes.size(); i++)
    assert_cmpnum(nodes[i]->ways, 2);

  assert_cmpnum(w->node_chain.size(), 4);
  assert_cmpnum(neww->node_chain.size(), 2);
  assert(neww->tags == w->tags.asMap());
  assert(neww->tags == v->tags.asMap());
  assert_cmpnum(neww->tags.asMap().size(), ocnt - 1);
  assert_cmpnum(r1->members.size(), 2);
  assert_cmpnum(r2->members.size(), 5);
  assert_cmpnum(r3->members.size(), 1);

  osm_t::dirty_t dirty0 = o->modified();
  assert_cmpnum(dirty0.nodes.added.size(), 6);
  assert_cmpnum(dirty0.nodes.changed.size(), 0);
  assert_cmpnum(dirty0.nodes.deleted.size(), 0);
  assert_cmpnum(dirty0.ways.added.size(), 2);
  assert_cmpnum(dirty0.ways.changed.size(), 1);
  assert_cmpnum(dirty0.ways.deleted.size(), 0);

  verify_osm_db::run(o);
  // now split the remaining way at a node
  way_t *neww2 = w->split(o, std::next(w->node_chain.begin(), 2), true);
  assert(neww2 != nullptr);
  assert_cmpnum(o->ways.size(), 4);
  assert(w->flags & OSM_FLAG_DIRTY);
  for(unsigned int i = 0; i < nodes.size(); i++)
    if(i == 4)
      assert_cmpnum(nodes[4]->ways, 3);
    else
      assert_cmpnum(nodes[i]->ways, 2);

  verify_osm_db::run(o);
  osm_t::dirty_t dirty1 = o->modified();
  assert_cmpnum(dirty1.nodes.changed.size(), 0);
  assert_cmpnum(dirty1.nodes.added.size(), 6);
  assert_cmpnum(dirty1.nodes.deleted.size(), 0);
  assert_cmpnum(dirty1.ways.changed.size(), 1);
  assert_cmpnum(dirty1.ways.added.size(), 3);
  assert_cmpnum(dirty1.ways.deleted.size(), 0);

  assert(w->contains_node(nodes[4]));
  assert(w->ends_with_node(nodes[4]));
  assert_cmpnum(w->node_chain.size(), 3);
  assert_cmpnum(neww->node_chain.size(), 2);
  assert_cmpnum(neww2->node_chain.size(), 2);
  assert(neww2->tags == w->tags.asMap());
  assert(neww2->tags == v->tags.asMap());
  assert_cmpnum(neww2->tags.asMap().size(), ocnt - 1);
  assert_cmpnum(r1->members.size(), 3);
  assert_cmpnum(r2->members.size(), 7);
  assert_cmpnum(r3->members.size(), 1);

  // just split the last node out of the way
  o->unmark_dirty(w);
  assert_null(w->split(o, std::next(w->node_chain.begin(), 2), false));
  const way_t *origWay = o->originalObject(w);
  assert(origWay != nullptr);
  assert(origWay != w);
  assert_cmpnum(origWay->id, w->id);

  verify_osm_db::run(o);
  assert_cmpnum(o->ways.size(), 4);
  // this is the original way
  assert_cmpnum(origWay->flags, 0);
  assert(origWay->contains_node(nodes[4]));
  assert(origWay->ends_with_node(nodes[4]));
  assert_cmpnum(origWay->node_chain.size(), 3);

  // this is the modified one
  assert_cmpnum(w->flags, OSM_FLAG_DIRTY);
  for(unsigned int i = 0; i < nodes.size(); i++)
    assert_cmpnum(nodes[i]->ways, 2);

  assert(!w->contains_node(nodes[4]));
  assert(!w->ends_with_node(nodes[4]));
  assert_cmpnum(w->node_chain.size(), 2);

  // now test a closed way
  way_t * const area = new way_t();
  for(unsigned int i = 0; i < nodes.size(); i++)
    area->append_node(nodes[i]);
  area->append_node(nodes[0]);
  assert(area->is_closed());
  o->attach(area);

  // drop the other ways to make reference counting easier
  o->way_delete(v, nullptr);
  o->way_delete(w, nullptr);
  o->way_delete(neww, nullptr);
  o->way_delete(neww2, nullptr);
  assert_cmpnum(o->ways.size(), 2); // area and w, as w is present upstream
  for(unsigned int i = 1; i < nodes.size(); i++)
    assert_cmpnum(nodes[i]->ways, 1);
  assert_cmpnum(nodes.front()->ways, 2);

  assert_null(area->split(o, area->node_chain.begin(), true));
  assert_cmpnum(area->node_chain.size(), nodes.size());
  for(unsigned int i = 0; i < nodes.size(); i++) {
    assert(area->node_chain[i] == nodes[i]);
    assert_cmpnum(nodes[i]->ways, 1);
  }
  verify_osm_db::run(o);

  // close the way again
  area->append_node(const_cast<node_t *>(area->first_node()));
  assert_null(area->split(o, std::next(area->node_chain.begin()), false));
  assert_cmpnum(area->node_chain.size(), nodes.size());
  for(unsigned int i = 0; i < nodes.size(); i++) {
    assert(area->node_chain[i] == nodes[(i + 1) % nodes.size()]);
    assert_cmpnum(nodes[i]->ways, 1);
  }
  verify_osm_db::run(o);

  // recreate old layout
  area->append_node(const_cast<node_t *>(area->first_node()));
  assert_null(area->split(o, std::prev(area->node_chain.end()), true));
  assert_cmpnum(area->node_chain.size(), nodes.size());
  for(unsigned int i = 0; i < nodes.size(); i++) {
    assert(area->node_chain[i] == nodes[(i + 1) % nodes.size()]);
    assert_cmpnum(nodes[i]->ways, 1);
  }
  verify_osm_db::run(o);
}

bool checkLinearRelation(const relation_t *r)
{
  std::cout << "checking order of relation " << r->id << std::endl;
  bool ret = true;

  const std::vector<member_t>::const_iterator itEnd = r->members.end();
  std::vector<member_t>::const_iterator it = r->members.begin();
  if(it->object.type == object_t::NODE)
    it++;
  assert_cmpnum(it->object.type, object_t::WAY);
  const way_t *last = static_cast<way_t *>(it->object);

  std::cout << "WAY " << last->id << " start " << last->first_node()->id
            << " end " << last->last_node()->id << " length " << last->node_chain.size()
            << std::endl;

  for(++it; it != itEnd; it++) {
    assert_cmpnum(it->object.type, object_t::WAY);
    const way_t *w = static_cast<way_t *>(it->object);
    std::cout << "WAY " << w->id << " start " << w->first_node()->id
              << " end " << w->last_node()->id << " length " << w->node_chain.size()
              << std::endl;

    if(!last->ends_with_node(w->node_chain.front()) &&
       !last->ends_with_node(w->node_chain.back())) {
      std::cout << "\tGAP DETECTED!" << std::endl;
      ret = false;
    }

    last = static_cast<way_t *>(it->object);
  }

  return ret;
}

// find out which part of the original way can be split at the given node
struct findWay {
  const node_t * const node;
  explicit findWay(const node_t *n) : node(n) {}
  bool operator()(way_t *way) const {
    const std::vector<node_t *> &ch = way->node_chain;
    return !way->ends_with_node(node) &&
      ch.end() != std::find(ch.begin(), ch.end(), node);
  }
};

void test_split_order()
{
  std::unique_ptr<osm_t> o(std::make_unique<osm_t>());
  std::vector<node_t *> nodes;
  for(unsigned int i = 1; i <= 10; i++) {
    base_attributes ba(1234500 + i);
    ba.version = 3;
    node_t *n = o->node_new(pos_t(52.25 + i * 0.001, 9.58 + i * 0.001), ba);
    n->id = i;
    o->insert(n);
    nodes.push_back(n);
  }
  // the ways that start and end each relation, opposing directions
  way_t *wstart = new way_t();
  o->attach(wstart);
  wstart->append_node(o->object_by_id<node_t>(1));
  wstart->append_node(o->object_by_id<node_t>(2));
  way_t *wend = new way_t();
  o->attach(wend);
  wend->append_node(o->object_by_id<node_t>(10));
  wend->append_node(o->object_by_id<node_t>(9));

  // now the ways that are split
  std::vector<way_t *> splitw;
  for(unsigned int i = 0; i < 12; i++) {
    way_t *w = new way_t();
    o->attach(w);
    splitw.push_back(w);
    const std::vector<node_t *>::const_iterator itEnd = std::prev(nodes.end());

    for(std::vector<node_t *>::const_iterator it = std::next(nodes.begin()); it != itEnd; it++)
      w->append_node(*it);
  }

  for(unsigned int i = 1; i <= splitw.size(); i++) {
    relation_t * const r = new relation_t();
    r->id = i;
    o->insert(r);
    // create relations where either the first way is a different way (in order), or is a node
    if(i % 4 == 1)
      r->members.push_back(member_t(object_t(wstart)));
    else if(i % 4 == 3)
      r->members.push_back(member_t(object_t(wstart->node_chain.front())));
    r->members.push_back(member_t(object_t(splitw[i - 1])));
    r->members.push_back(member_t(object_t(wend)));
  }

  // define the sequences in which the ways are split
  // insert every sequence twice to check both the relations that have
  // the split way in the middle and those that start with it
  std::vector<std::vector<node_t *> > sequences;
  std::vector<node_t *> tmpseq = splitw.front()->node_chain;
  // keep the first and last nodes, so removes them from the split sequence
  tmpseq.pop_back();
  tmpseq.erase(tmpseq.begin());
  sequences.push_back(tmpseq);
  sequences.push_back(tmpseq);
  std::reverse(tmpseq.begin(), tmpseq.end());
  sequences.push_back(tmpseq);
  sequences.push_back(tmpseq);

  // use also shorter random sequences
  while(sequences.size() < splitw.size()) {
    std::random_shuffle(tmpseq.begin(), tmpseq.end(), intrnd);
    sequences.push_back(tmpseq);
    sequences.push_back(tmpseq);
    tmpseq.erase(std::next(tmpseq.begin(), intrnd(tmpseq.size())));
  }

  // split the ways in several orders
  for(unsigned int i = 0; i < sequences.size(); i++) {
    std::vector<way_t *> sw;
    sw.push_back(splitw[i]);
    assert(checkLinearRelation(o->object_by_id<relation_t>(i + 1)));

    for(unsigned int j = 0; j < sequences[i].size(); j++) {
      std::vector<way_t *>::iterator it = std::find_if(sw.begin(), sw.end(),
                                                       findWay(sequences[i][j]));
      assert(it != sw.end());
      way_t *nw = (*it)->split(o, std::find((*it)->node_chain.begin(), (*it)->node_chain.end(), sequences[i][j]), true);
      sw.push_back(nw);
    }
    assert(checkLinearRelation(o->object_by_id<relation_t>(i + 1)));
  }
}

void test_changeset()
{
  const char message[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                         "<osm>\n"
                         "  <changeset>\n"
                         "    <tag k=\"created_by\" v=\"osm2go v" VERSION "\"/>\n"
                         "    <tag k=\"comment\" v=\"&lt;&amp;&gt;\"/>\n"
                         "  </changeset>\n"
                         "</osm>\n";
  const char message_src[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                             "<osm>\n"
                             "  <changeset>\n"
                             "    <tag k=\"created_by\" v=\"osm2go v" VERSION "\"/>\n"
                             "    <tag k=\"comment\" v=\"testcase comment\"/>\n"
                             "    <tag k=\"source\" v=\"survey\"/>\n"
                             "  </changeset>\n"
                             "</osm>\n";
  xmlString cs(osm_generate_xml_changeset("<&>", std::string()));

  assert_cmpstr(reinterpret_cast<char *>(cs.get()), message);
  assert_cmpstr(cs, message);

  cs.reset(osm_generate_xml_changeset("testcase comment", "survey"));

  assert_cmpstr(reinterpret_cast<char *>(cs.get()), message_src);
  assert_cmpstr(cs, message_src);
}

void test_reverse()
{
  std::unique_ptr<osm_t> o(std::make_unique<osm_t>());
  set_bounds(o);

  lpos_t l(10, 20);
  node_t *n1 = o->node_new(l);
  assert_cmpnum(n1->version, 0);
  assert_cmpnum(n1->flags, OSM_FLAG_DIRTY);
  o->attach(n1);
  l.y = 40;
  node_t *n2 = o->node_new(l);
  o->attach(n2);
  base_attributes ba(47);
  ba.version = 2;
  way_t *w = new way_t(ba);
  w->append_node(n1);
  w->append_node(n2);
  o->insert(w);

  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("highway", "residential"));
  tags.insert(osm_t::TagMap::value_type("foo:forward", "yes"));
  tags.insert(osm_t::TagMap::value_type("foo:backward", "2"));
  tags.insert(osm_t::TagMap::value_type("bar:left", "3"));
  tags.insert(osm_t::TagMap::value_type("bar:right", "4"));
  tags.insert(osm_t::TagMap::value_type("oneway", "YES"));
  tags.insert(osm_t::TagMap::value_type("sidewalk", "left"));

  assert(w->first_node() == n1);
  assert(w->last_node() == n2);
  assert(!w->isNew());
  assert_cmpnum(w->flags, 0);

  // some relations the way is member of to see how the roles change
  std::vector<relation_t *> rels;
  for(unsigned int i = 0; i < 5; i++) {
    relation_t *r = new relation_t();
    rels.push_back(r);
    o->attach(r);
    osm_t::TagMap rtags;
    rtags.insert(osm_t::TagMap::value_type("type", i == 0 ? "multipolygon" : "route"));
    r->tags.replace(rtags);
    if(i < 4) {
      const char *role = nullptr;
      switch(i) {
      case 0:
      case 1:
        role = "forward";
        break;
      case 2:
        role = "backward";
        break;
      }
      r->members.push_back(member_t(object_t(w), role));
      r->members.push_back(member_t(object_t(n1), role));
    }
  }

  w->tags.replace(tags);
  std::pair<unsigned int, unsigned int> flipped = w->reverse(o);
  verify_osm_db::run(o);

  assert_cmpnum(flipped.first, 6);
  assert_cmpnum(w->flags, OSM_FLAG_DIRTY);
  assert(w->node_chain.front() == n2);
  assert(w->node_chain.back() == n1);
  assert(w->tags != tags);
  osm_t::TagMap rtags;
  rtags.insert(osm_t::TagMap::value_type("highway", "residential"));
  rtags.insert(osm_t::TagMap::value_type("foo:backward", "yes"));
  rtags.insert(osm_t::TagMap::value_type("foo:forward", "2"));
  rtags.insert(osm_t::TagMap::value_type("bar:right", "3"));
  rtags.insert(osm_t::TagMap::value_type("bar:left", "4"));
  rtags.insert(osm_t::TagMap::value_type("oneway", "-1"));
  rtags.insert(osm_t::TagMap::value_type("sidewalk", "right"));

  assert(w->tags == rtags);

  // check relations and their roles
  assert_cmpnum(flipped.second, 2);
  // rels[0] has wrong type, roles should not be modified
  assert_cmpnum(rels[0]->members.size(), 2);
  assert_cmpstr(rels[0]->members.front().role, "forward");
  assert_cmpstr(rels[0]->members.back().role, "forward");
  // rels[1] has matching type, first member role should be changed
  assert_cmpnum(rels[1]->members.size(), 2);
  assert_cmpstr(rels[1]->members.front().role, "backward");
  assert(rels[1]->members.front().object.operator==(w));
  assert_cmpstr(rels[1]->members.back().role, "forward");
  // rels[2] has matching type, first member role should be changed (other direction)
  assert_cmpnum(rels[1]->members.size(), 2);
  assert_cmpstr(rels[2]->members.front().role, "forward");
  assert(rels[2]->members.front().object.operator==(w));
  assert_cmpstr(rels[2]->members.back().role, "backward");
  // rels[3] has matching type, but roles are empty
  assert_cmpnum(rels[1]->members.size(), 2);
  assert_null(rels[3]->members.front().role);
  assert(rels[3]->members.front().object.operator==(w));
  assert_null(rels[3]->members.back().role);

  // go back
  flipped = w->reverse(o);
  verify_osm_db::run(o);

  assert_cmpnum(flipped.first, 6);
  assert_cmpnum(flipped.second, 2);
  // the original value was uppercase
  tags.find("oneway")->second = "yes";
  assert(w->tags == tags);

  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "residential"));
  tags.insert(osm_t::TagMap::value_type("oneway", "strange"));
  w->tags.replace(tags);

  // the oneway key is unknown, so it is not touched.
  flipped = w->reverse(o);
  verify_osm_db::run(o);
  assert(w->tags == tags);
  assert_cmpnum(flipped.first, 0);
  assert_cmpnum(flipped.second, 2);
}

unsigned int nn_cnt;
void node_noop(node_t *)
{
  nn_cnt++;
}

void test_way_delete()
{
  std::unique_ptr<osm_t> o(std::make_unique<osm_t>());
  set_bounds(o);

  // delete a simple way
  lpos_t l(10, 20);
  node_t *n1 = o->node_new(l);
  o->attach(n1);
  l.y = 40;
  node_t *n2 = o->node_new(l);
  o->attach(n2);
  way_t *w = new way_t();
  w->append_node(n1);
  w->append_node(n2);
  o->attach(w);

  verify_osm_db::run(o);
  o->way_delete(w, nullptr);
  verify_osm_db::run(o);

  assert_cmpnum(o->nodes.size(), 0);
  assert_cmpnum(o->ways.size(), 0);
  assert(o->is_clean(true));
  assert(o->is_clean(false));

  // delete a closed way
  n1 = o->node_new(l);
  o->attach(n1);
  l.y = 20;
  n2 = o->node_new(l);
  o->attach(n2);
  w = new way_t();
  w->append_node(n1);
  w->append_node(n2);
  o->attach(w);
  l.x = 20;
  n2 = o->node_new(l);
  o->attach(n2);
  w->append_node(n2);
  assert(!w->is_closed());
  w->append_node(n1);
  assert(w->is_closed());

  o->way_delete(w, nullptr);

  assert_cmpnum(o->nodes.size(), 0);
  assert_cmpnum(o->ways.size(), 0);
  assert(o->is_clean(true));
  assert(o->is_clean(false));

  // test way deletion with nodes that should be preserved
  l.x = 10;
  l.y = 20;
  n1 = o->node_new(l);
  o->attach(n1);

  // this node will be removed when the way is removed
  l.y = 40;
  n2 = o->node_new(l);
  o->attach(n2);

  w = new way_t();
  w->append_node(n1);
  w->append_node(n2);
  o->attach(w);

  // this instance will persist
  l.x = 20;
  n2 = o->node_new(l);
  o->attach(n2);
  w->append_node(n2);

  relation_t *r = new relation_t();
  o->attach(r);
  r->members.push_back(member_t(object_t(n2)));

  osm_t::TagMap nstags;
  nstags.insert(osm_t::TagMap::value_type("a", "A"));
  n1->tags.replace(nstags);

  l.x = 5;
  node_t *n3 = o->node_new(l);
  o->attach(n3);
  l.y = 25;
  node_t *n4 = o->node_new(l);
  o->attach(n4);

  way_t *w2 = new way_t();
  o->attach(w2);
  w2->append_node(n3);
  w2->append_node(n4);

  w->append_node(n3);

  verify_osm_db::run(o);
  // now delete the way, which would reduce the use counter of all nodes
  // n1 should be preserved as it has tags on it's own
  // n2 should be preserved as it is still referenced by a relation
  // n3 should be preserved as it is used in another way
  o->way_delete(w, nullptr);
  verify_osm_db::run(o);

  assert_cmpnum(o->nodes.size(), 4);
  assert_cmpnum(o->ways.size(), 1);
  assert_cmpnum(o->relations.size(), 1);
  assert(o->object_by_id<node_t>(n1->id) == n1);
  assert(o->object_by_id<node_t>(n2->id) == n2);
  assert(o->object_by_id<node_t>(n3->id) == n3);
  assert(o->object_by_id<node_t>(n4->id) == n4);
  assert_cmpnum(r->members.size(), 1);

  // once again, with a custom unref function
  w = new way_t();
  // not attached here as map_edit also keeps separate
  w->append_node(n3);
  w->append_node(n4);
  o->attach(w);

  assert_cmpnum(nn_cnt, 0);
  o->way_delete(w, nullptr, node_noop);
  assert_cmpnum(nn_cnt, 2);
  // they have not been unrefed in the custom function
  assert_cmpnum(n3->ways, 2);
  n3->ways--;
  assert_cmpnum(n4->ways, 2);
  n4->ways--;

  // once more, but this time pretend this is not a new way
  base_attributes ba(42);
  ba.version = 1;
  w = new way_t(ba);
  o->insert(w);
  w->append_node(n3);
  w->append_node(n4);
  // keep it here, it ill only be reset, but not freed as that is done through the map
  std::unique_ptr<map_item_t> mi(new map_item_t(object_t(w), nullptr));
  w->map_item = mi.get();

  verify_osm_db::run(o);
  o->way_delete(w, nullptr);
  assert_cmpnum(n3->ways, 1);
  assert_cmpnum(n4->ways, 1);
  assert_cmpnum(w->node_chain.size(), 0);
  assert(w->flags & OSM_FLAG_DELETED);
  assert(w->tags.empty());
  verify_osm_db::run(o);
}

void test_member_delete()
{
  std::unique_ptr<osm_t> o(std::make_unique<osm_t>());
  set_bounds(o);

  // a way with 3 points
  lpos_t l(10, 20);
  node_t *n1 = o->node_new(l);
  o->attach(n1);
  l.y = 40;
  node_t *n2 = o->node_new(l);
  o->attach(n2);
  way_t *w = new way_t();
  w->append_node(n1);
  w->append_node(n2);
  o->attach(w);

  l.x = 20;
  base_attributes ba(42);
  ba.version = 1;
  n2 = o->node_new(l.toPos(o->bounds), ba);
  o->insert(n2);
  w->append_node(n2);

  // a relation containing both the way as well as the node
  relation_t * const r = new relation_t();
  r->members.push_back(member_t(object_t(w)));
  r->members.push_back(member_t(object_t(n2)));
  o->attach(r);

  osm_t::dirty_t dirty0 = o->modified();
  assert_cmpnum(dirty0.nodes.total, 3);
  assert_cmpnum(dirty0.nodes.changed.size(), 0);
  assert_cmpnum(dirty0.nodes.added.size(), 2);
  assert_cmpnum(dirty0.nodes.deleted.size(), 0);
  assert_cmpnum(dirty0.ways.changed.size(), 0);
  assert_cmpnum(dirty0.ways.added.size(), 1);
  assert_cmpnum(dirty0.ways.deleted.size(), 0);
  assert_cmpnum(dirty0.relations.changed.size(), 0);
  assert_cmpnum(dirty0.relations.added.size(), 1);
  assert_cmpnum(dirty0.relations.deleted.size(), 0);

  relation_t::memberCounts counts = r->members_by_type();
  assert_cmpnum(counts.nodes, 1);
  assert_cmpnum(counts.ways, 1);
  assert_cmpnum(counts.relations, 0);

  // keep it here, it ill only be reset, but not freed as that is done through the map
  std::unique_ptr<map_item_t> mi(new map_item_t(object_t(w), nullptr));
  n2->map_item = mi.get();

  // insert an unrelated relation, just because we can
  relation_t *rn = new relation_t();
  o->attach(rn);
  r->members.insert(r->members.begin(), member_t(object_t(rn), "dummy"));

  verify_osm_db::run(o);
  // now delete the node that is member of both other objects
  o->node_delete(n2);

  // since the object had a valid id it should still be there, but unreferenced
  assert_cmpnum(o->nodes.size(), 3);
  assert_cmpnum(o->ways.size(), 1);
  assert_cmpnum(o->relations.size(), 2);
  assert(n2->tags.empty());
  assert(n2->isDeleted());
  assert_cmpnum(n2->flags, OSM_FLAG_DELETED);

  osm_t::dirty_t dirty1 = o->modified();
  assert_cmpnum(dirty1.nodes.total, 3);
  assert_cmpnum(dirty1.nodes.changed.size(), 0);
  assert_cmpnum(dirty1.nodes.added.size(), 2);
  assert_cmpnum(dirty1.nodes.deleted.size(), 1);
  assert_cmpnum(dirty1.ways.changed.size(), 0);
  assert_cmpnum(dirty1.ways.added.size(), 1);
  assert_cmpnum(dirty1.ways.deleted.size(), 0);
  assert_cmpnum(dirty1.relations.changed.size(), 0);
  assert_cmpnum(dirty1.relations.added.size(), 2);
  assert_cmpnum(dirty1.relations.deleted.size(), 0);
  verify_osm_db::run(o);

  counts = r->members_by_type();
  assert_cmpnum(counts.nodes, 0);
  assert_cmpnum(counts.ways, 1);
  assert_cmpnum(counts.relations, 1);
}

struct node_collector {
  way_chain_t &chain;
  const node_t * const node;
  node_collector(way_chain_t &c, const node_t *n) : chain(c), node(n) {}
  bool operator()(const std::pair<item_id_t, way_t *> &pair) {
    if(pair.second->contains_node(node))
      chain.push_back(pair.second);
    return false;
  }
};

bool all_ways(const std::pair<item_id_t, way_t *>)
{
  return true;
}

struct first_way {
  unsigned int &cnt;
  first_way(unsigned int &c) : cnt(c) {}
  inline bool operator()(const std::pair<item_id_t, way_t *>)
  {
    return cnt++ == 0;
  }
};

void test_merge_nodes()
{
  std::unique_ptr<osm_t> o(std::make_unique<osm_t>());
  set_bounds(o);

  /// ==================
  // join 2 new nodes
  lpos_t oldpos(10, 10);
  lpos_t newpos(20, 20);
  node_t *n1 = o->node_new(oldpos);
  node_t *n2 = o->node_new(newpos);
  o->attach(n1);
  o->attach(n2);

  std::array<way_t *, 2> ways2join;

  {
    osm_t::mergeResult<node_t> mergeRes = o->mergeNodes(n1, n2, ways2join);
    assert(mergeRes.obj == n1);
    assert(!mergeRes.conflict);
  }
  assert(n1->lpos == newpos);
  assert_cmpnum(o->nodes.size(), 1);
  assert_cmpnum(n1->flags, OSM_FLAG_DIRTY);
  assert_null(ways2join[0]);
  assert_null(ways2join[1]);

  /// ==================
  // join a new and an old node, the old one should be preserved
  base_attributes ba(1234);
  ba.version = 1;
  n2 = o->node_new(oldpos.toPos(o->bounds), ba);
  n2->lpos = oldpos;
  o->insert(n2);

  {
    osm_t::mergeResult<node_t> mergeRes = o->mergeNodes(n2, n1, ways2join);
    assert(mergeRes.obj == n2);
    assert(!mergeRes.conflict);
  }
  assert(n2->lpos == newpos);
  assert_cmpnum(o->nodes.size(), 1);
  assert_cmpnum(n2->flags, OSM_FLAG_DIRTY);
  assert_null(ways2join[0]);
  assert_null(ways2join[1]);
  // verify that this has been inserted into the original map
  assert_cmpnum(o->original.nodes.size(), 1);
  assert_cmpnum(o->original.ways.size(), 0);
  assert_cmpnum(o->original.relations.size(), 0);
  assert_cmpnum(o->original.nodes.begin()->first, n2->id);
  assert_cmpnum(o->original.nodes.begin()->second->id, n2->id);
  assert(o->original.nodes.begin()->second == o->originalObject(n2));
  assert(o->original.nodes.begin()->second != n2); // must be a distinct instance
  assert(*(o->original.nodes.begin()->second) != *n2);
  // now do an update, which should detect that things are still different
  o->updateTags(object_t(n2), n2->tags.asMap());
  assert_cmpnum(o->original.nodes.size(), 1);
  assert_cmpnum(n2->flags, OSM_FLAG_DIRTY);

  delete o->original.nodes.begin()->second;
  o->original.nodes.clear();

  /// ==================
  // do the same join again, but with swapped arguments
  o->unmark_dirty(n2);
  n2->lpos = newpos;
  assert_cmpnum(n2->flags, 0);
  n1 = o->node_new(oldpos);
  o->attach(n1);

  {
    osm_t::mergeResult<node_t> mergeRes = o->mergeNodes(n1, n2, ways2join);
    assert(mergeRes.obj == n2);
    assert(!mergeRes.conflict);
  }
  // order is important for the position, but nothing else
  assert(n2->lpos == newpos);
  assert_cmpnum(o->nodes.size(), 1);
  assert_cmpnum(n2->flags, OSM_FLAG_DIRTY);
  assert_null(ways2join[0]);
  assert_null(ways2join[1]);

  assert_cmpnum(o->original.nodes.size(), 1);
  assert(o->original.nodes.begin()->second == o->originalObject(n2));
  o->wipe(n2);
  assert_cmpnum(o->nodes.size(), 0);

  assert_cmpnum(o->original.nodes.size(), 0);
  assert_cmpnum(o->original.ways.size(), 0);
  assert_cmpnum(o->original.relations.size(), 0);

  /// ==================
  // start new
  n1 = o->node_new(oldpos);
  n2 = o->node_new(newpos);
  o->attach(n1);
  o->attach(n2);

  // attach one node to a way, that one should be preserved
  way_t *w = new way_t();
  o->attach(w);
  w->append_node(n2);

  {
    osm_t::mergeResult<node_t> mergeRes = o->mergeNodes(n1, n2, ways2join);
    assert(mergeRes.obj == n2);
    assert(!mergeRes.conflict);
  }
  assert(n2->lpos == newpos);
  assert_cmpnum(o->nodes.size(), 1);
  assert_cmpnum(n2->flags, OSM_FLAG_DIRTY);
  assert_cmpnum(w->node_chain.size(), 1);
  assert(w->node_chain.front() == n2);
  assert_null(ways2join[0]);
  assert_null(ways2join[1]);

  o->way_delete(w, nullptr);
  assert_cmpnum(o->nodes.size(), 0);
  assert_cmpnum(o->ways.size(), 0);

  /// ==================
  // now check with relation membership
  relation_t *r = new relation_t();
  o->attach(r);
  n1 = o->node_new(oldpos);
  n2 = o->node_new(newpos);
  o->attach(n1);
  o->attach(n2);

  r->members.push_back(member_t(object_t(n2)));

  {
    osm_t::mergeResult<node_t> mergeRes = o->mergeNodes(n1, n2, ways2join);
    assert(mergeRes.obj == n2);
    assert(!mergeRes.conflict);
  }
  assert(n2->lpos == newpos);
  assert_cmpnum(o->nodes.size(), 1);
  assert_cmpnum(n2->flags, OSM_FLAG_DIRTY);
  assert_cmpnum(r->members.size(), 1);
  assert(r->members.front().object.operator==(n2));
  assert_null(ways2join[0]);
  assert_null(ways2join[1]);

  o->relation_delete(r);
  assert_cmpnum(o->nodes.size(), 1);
  assert_cmpnum(o->ways.size(), 0);
  assert_cmpnum(o->relations.size(), 0);
  assert(o->nodes.begin()->second == n2);
  o->node_delete(n2);
  assert_cmpnum(o->nodes.size(), 0);

  /// ==================
  // have short ways of 2 nodes, deletion of one of the nodes should trigger deletion of the ways
  n1 = o->node_new(oldpos);
  n2 = o->node_new(newpos);
  o->attach(n1);
  o->attach(n2);

  // one way
  w = new way_t();
  o->attach(w);
  w->append_node(n1);
  w->append_node(n2);

  node_t *n3 = o->node_new(pos_t(25, 45));
  o->attach(n3);
  // important: set non-trivial tags so this will not be deleted when the referencing way is deleted
  std::vector<tag_t> tags = ab_with_creator();
  n3->tags.replace(std::move(tags));

  way_t *w2 = new way_t();
  o->attach(w2);
  w2->append_node(n1);
  w2->append_node(n3);

  o->node_delete(n1, nullptr);
  assert_cmpnum(o->nodes.size(), 1);
  assert_cmpnum(o->ways.size(), 0);
  assert_cmpnum(o->relations.size(), 0);
  assert(o->nodes.begin()->second == n3);
  o->node_delete(n3);
  assert_cmpnum(o->nodes.size(), 0);

  /// ==================
  // now put both into a way, the way of the second node should be updated
  std::vector<way_t *> ways;
  std::vector<relation_t *> relations;
  for(int i = 0; i < 3; i++) {
    w = new way_t();
    o->attach(w);
    lpos_t pos(i + 4, i + 4);
    n1 = o->node_new(pos);
    o->attach(n1);
    w->append_node(n1);
    r = new relation_t();
    o->attach(r);
    ways.push_back(w);
    relations.push_back(r);
  }

  // check that find_only_way() really matches exactly one way
  unsigned int cnt = 0;
  first_way fw(cnt);
  assert_null(o->find_only_way(all_ways));
  assert(o->find_only_way(fw) != nullptr);

  n1 = o->node_new(oldpos);
  n2 = o->node_new(newpos);
  o->attach(n1);
  o->attach(n2);

  // one way with only n1
  w = ways.back();
  w->append_node(n1);
  std::pair<unsigned int, unsigned int> flipped = w->reverse(o);
  assert_cmpnum(flipped.first, 0);
  assert_cmpnum(flipped.second, 0);

  // one way with only n2
  w = ways.front();
  // put both nodes here, only one instance should remain
  w->append_node(n2);
  o->unmark_dirty(w);

  std::vector<way_t *>::iterator wit = std::next(ways.begin());
  w = *wit;
  // put both nodes here, only one instance should remain
  w->append_node(n1);
  w->append_node(n2);
  o->unmark_dirty(w);

  relations.back()->members.push_back(member_t(object_t(n1)));
  r = relations.front();
  r->members.push_back(member_t(object_t(n2)));
  o->unmark_dirty(r);
  assert_cmpnum(ways.back()->node_chain.size(), 2);
  assert_cmpnum(w->node_chain.size(), 3);
  assert(ways.back()->node_chain.front() == n1);
  assert(ways.back()->ends_with_node(n1));
  assert(w->node_chain.back() == n2);
  assert(w->ends_with_node(n2));
  assert_cmpnum(n1->ways, 2);
  assert_cmpnum(n2->ways, 2);
  assert(relations.back()->members.front().object.operator==(n1));
  assert(r->members.front().object.operator==(n2));
  assert_cmpnum(o->nodes.size(), 5);

  {
    osm_t::mergeResult<node_t> mergeRes = o->mergeNodes(n1, n2, ways2join);
    assert(mergeRes.obj == n1);
    assert(!mergeRes.conflict);
  }
  assert(n1->lpos == newpos);
  assert_cmpnum(o->nodes.size(), 4);
  assert_cmpnum(n1->flags, OSM_FLAG_DIRTY);
  assert_cmpnum(r->members.size(), 1);
  assert(ways.back()->first_node() == n1);
  assert(ways.back()->ends_with_node(n1));
  assert(w->last_node() == n1);
  assert(w->ends_with_node(n1));
  assert_cmpnum(w->node_chain.size(), 2);
  assert_cmpnum(w->flags, 0); // not marked dirty as it is a new object
  assert(w->isNew());

  assert_cmpnum(n1->ways, 3);
  assert(relations.back()->members.front().object.operator==(n1));
  // test member_t::operator==(object_t)
  assert(relations.back()->members.front() == object_t(n1));
  assert(r->members.front().object.operator==(n1));
  assert_cmpnum(r->flags, 0); // not marked dirty as it is a new object
  assert(r->isNew());
  assert_null(ways2join[0]);
  assert_null(ways2join[1]);
  // no entries for new items
  assert_cmpnum(o->original.nodes.size(), 0);
  assert_cmpnum(o->original.nodes.size(), 0);
  assert_cmpnum(o->original.ways.size(), 0);

  // while at it: test backwards mapping to containing objects
  way_chain_t wchain;
  assert(o->find_way(node_collector(wchain, n1)) == nullptr);
  assert_cmpnum(wchain.size(), 3);
  assert(std::find(wchain.begin(), wchain.end(), ways.back()) != wchain.end());
  assert(std::find(wchain.begin(), wchain.end(), w) != wchain.end());

  /// ==================
  // now join 2 nodes which both terminate one way
  assert_cmpnum(o->ways.size(), 3);
  o->way_delete(w, nullptr);
  ways.erase(wit);
  w = ways.back();
  assert_cmpnum(w->node_chain.size(), 2);
  assert(w->node_chain.front() == n1);

  n2 = o->node_new(newpos);
  o->attach(n2);
  n1->ways--;
  w->node_chain.front() = n2;
  n2->ways++;

  {
    osm_t::mergeResult<node_t> mergeRes = o->mergeNodes(n1, n2, ways2join);
    assert(mergeRes.obj == n1);
    assert(!mergeRes.conflict);
  }
  assert(ways2join[0] == w || ways2join[1] == w);
  assert(ways2join[0] != ways2join[1]);
  assert(ways2join[0]->ends_with_node(n1));
  assert(ways2join[1]->ends_with_node(n1));
  assert_cmpnum(n1->ways, 2);

  /// ==================
  // now join 2 nodes which are 2 ends of the same way
  // this should trigger the second "mayMerge = false" in node_t::mergeNodes()
  std::vector<node_t *> nn;
  w = new way_t();
  o->attach(w);
  for (int i = 0; i < 4; i++) {
    lpos_t p(10 + (i % 2) * 10, 10 + (i / 2) * 10);
    nn.push_back(o->node_new(p));
    o->attach(nn.back());
    w->append_node(nn.back());
  }
  n1 = nn.front();
  n2 = nn.back();
  assert(w->ends_with_node(n1));
  assert(w->ends_with_node(n2));

  {
    osm_t::mergeResult<node_t> mergeRes = o->mergeNodes(n1, n2, ways2join);
    assert(mergeRes.obj == n1);
    assert(!mergeRes.conflict);
  }
  assert(ways2join[0] == nullptr);
  assert(ways2join[1] == nullptr);

  /// ==================
  // now join 2 nodes where the first is in the middle of a way
  // this should trigger the first "mayMerge = false" in node_t::mergeNodes()
  n1 = nn.at(1);
  w = new way_t();
  o->attach(w);
  for (int i = 0; i < 3; i++) {
    lpos_t p(30 + (i % 2) * 10, 30 + (i / 2) * 10);
    nn.push_back(o->node_new(p));
    o->attach(nn.back());
    w->append_node(nn.back());
  }
  n2 = nn.back();

  {
    osm_t::mergeResult<node_t> mergeRes = o->mergeNodes(n1, n2, ways2join);
    assert(mergeRes.obj == n1);
    assert(!mergeRes.conflict);
  }
  assert(ways2join[0] == nullptr);
  assert(ways2join[1] == nullptr);

  // final verification: none of the tests above should have created something here
  assert_cmpnum(o->original.nodes.size(), 0);
  assert_cmpnum(o->original.ways.size(), 0);
  assert_cmpnum(o->original.relations.size(), 0);
}

void setup_way_relations_for_merge(osm_t::ref o, way_t *w0, way_t *w1)
{
  o->object_by_id<relation_t>(-3)->members.push_back(member_t(object_t(w0), "foo"));
  o->object_by_id<relation_t>(-4)->members.push_back(member_t(object_t(w1), "bar"));
  o->object_by_id<relation_t>(-4)->members.push_back(member_t(object_t(w0)));
}

node_chain_t setup_ways_for_merge(const node_chain_t &nodes, osm_t::ref o, way_t *&w0,
                                  way_t *&w1, const unsigned int i, int relations)
{
  node_chain_t expect;

  w0 = new way_t();
  if(i < 2) {
    for(unsigned int j = 0; j < nodes.size() / 2; j++)
      w0->append_node(nodes[j]);
  } else {
    for(int j = nodes.size() / 2 - 1; j >= 0; j--)
      w0->append_node(nodes[j]);
  }
  o->attach(w0);

  w1 = new way_t();
  for(unsigned int j = nodes.size() / 2 - 1; j < nodes.size(); j++)
    w1->append_node(nodes[j]);
  expect = nodes;
  if(i % 2 == 0) {
    std::reverse(w1->node_chain.begin(), w1->node_chain.end());
    std::reverse(expect.begin(), expect.end());
  }
  o->attach(w1);

  switch (relations) {
  case 0:
    break;
  case 1:
    setup_way_relations_for_merge(o, w1, w0);
    break;
  case 2:
    setup_way_relations_for_merge(o, w0, w1);
    break;
  }

  return expect;
}

void test_osm_way_free(node_t *n)
{
  assert_cmpnum_op(n->ways, >, 0);
  n->ways--;
}

void verify_merged_way(way_t *w, osm_t::ref o, const node_chain_t &nodes, const node_chain_t &expect, bool expectRels)
{
  assert_cmpnum(w->node_chain.size(), nodes.size());
  assert_cmpnum(o->ways.size(), 1);
  const size_t nodecount = o->nodes.size();
  assert_cmpnum_op(nodecount, >=, nodes.size());
  for(node_chain_t::const_iterator it = nodes.begin(); it != nodes.end(); it++) {
    w->contains_node(*it);
    assert_cmpnum((*it)->ways, 1);
  }
  assert(expect == w->node_chain);

  assert_cmpnum(o->object_by_id<relation_t>(-1)->members.size(), 0);
  // check the expected relation memberships of the way
  if(expectRels) {
    relation_t *rel = o->object_by_id<relation_t>(-3);
    std::vector<member_t>::const_iterator it = rel->find_member_object(object_t(w));
    assert(it != rel->members.end());
    assert_cmpstr(it->role, "foo");
    rel->eraseMember(it);

    rel = o->object_by_id<relation_t>(-4);
    it = rel->find_member_object(object_t(w));
    assert(it != rel->members.end());
    assert_cmpstr(it->role, "bar");
    rel->eraseMember(it);

    it = rel->find_member_object(object_t(w));
    assert(it != rel->members.end());
    assert_null(it->role);
    rel->eraseMember(it);
  }
  for(unsigned int i = 1; i < o->relations.size(); i++)
    assert_cmpnum(o->object_by_id<relation_t>(-1 - static_cast<item_id_t>(i))->members.size(), i - 1);

  verify_osm_db::run(o);
  o->way_delete(w, nullptr, test_osm_way_free);
  verify_osm_db::run(o);

  assert_cmpnum(o->ways.size(), 0);
  assert_cmpnum(o->nodes.size(), nodecount);
  for(node_chain_t::const_iterator it = nodes.begin(); it != nodes.end(); it++)
    assert_cmpnum((*it)->ways, 0);
}

void test_merge_ways()
{
  std::unique_ptr<osm_t> o(std::make_unique<osm_t>());
  set_bounds(o);

  node_chain_t nodes;
  for(int i = 0; i < 8; i++) {
    nodes.push_back(o->node_new(lpos_t(i * 3, i * 3)));
    o->attach(nodes.back());
  }

  for(int i = 0; i < 5; i++) {
    relation_t *r = new relation_t();
    for(int j = 1; j < i; j++)
      r->members.push_back(member_t(object_t(nodes[j])));
    o->attach(r);
  }

  const node_chain_t shortNodes(nodes.begin(), std::next(nodes.begin(), 3));

  // test all 4 combinations how the ways can be oriented
  for(unsigned int i = 0; i < 4; i++) {
    way_t *w0, *w1;
    const node_chain_t &expect = setup_ways_for_merge(nodes, o, w0, w1, i, 0);

    // verify direct merging
    assert(!w1->merge(w0, o, nullptr));

    verify_merged_way(w1, o, nodes, expect, false);

    assert(expect == setup_ways_for_merge(nodes, o, w0, w1, i, 1));

    // check that merging with relation checking works
    {
      osm_t::mergeResult<way_t> mergeRes = o->mergeWays(w1, w0, nullptr);
      assert(mergeRes.obj == w1);
      assert(!mergeRes.conflict);
    }

    verify_merged_way(w1, o, nodes, expect, true);

    // now put the other way into more relations
    assert(expect == setup_ways_for_merge(nodes, o, w0, w1, i, 2));

    // check that the right way is picked
    {
      osm_t::mergeResult<way_t> mergeRes = o->mergeWays(w0, w1, nullptr);
      assert(mergeRes.obj == w1);
      assert(!mergeRes.conflict);
    }

    verify_merged_way(w1, o, nodes, expect, true);

    // Merge a "normal" and a "degenerated" way. The latter shouldn't really exist
    // in the first place, but it shouldn't cause any problems in the software.
    const node_chain_t &shortExpect = setup_ways_for_merge(shortNodes, o, w0, w1, 0, 1);
    assert_cmpnum(w0->node_chain.size(), 1);
    assert_cmpnum(w1->node_chain.size(), 3);
    assert_cmpnum(shortExpect.size(), 3);
    assert(w1->node_chain == shortExpect);

    {
      osm_t::mergeResult<way_t> mergeRes = o->mergeWays(w0, w1, nullptr);
      assert(mergeRes.obj == w1);
      assert(!mergeRes.conflict);
    }

    verify_merged_way(w1, o, shortNodes, shortExpect, true);
  }
}

// all objects already exist in the OSM database, the victim way should be scheduled for deletion
void test_merge_existing_ways()
{
  std::unique_ptr<osm_t> o(std::make_unique<osm_t>());
  set_bounds(o);

  node_chain_t nodes;
  for(int i = 0; i < 6; i++) {
    base_attributes ba(470420 + i);
    ba.version = 1;
    nodes.push_back(o->node_new(pos_t(i * 3, i * 3), ba));
    o->insert(nodes.back());
  }

  base_attributes ba(47);
  ba.version = 1;
  way_t *w0 = new way_t(ba);
  for(unsigned int j = 0; j < nodes.size() / 2; j++)
    w0->append_node(nodes[j]);
  o->insert(w0);

  base_attributes ba2(42);
  ba2.version = 1;
  way_t *w1 = new way_t(ba2);
  for(unsigned int j = nodes.size() / 2 - 1; j < nodes.size(); j++)
    w1->append_node(nodes[j]);
  o->insert(w1);

  relation_t *r = new relation_t(ba2);
  r->members.push_back(member_t(object_t(w0)));
  o->insert(r);

  // verify direct merging
  osm_t::mergeResult<way_t > mr = o->mergeWays(w0, w1, nullptr);
  assert(!mr.conflict);
  assert(mr.obj == w1);

  assert_cmpnum(w1->node_chain.size(), nodes.size());
  assert_cmpnum(o->relations.size(), 1);
  assert_cmpnum(o->ways.size(), 2);
  assert_cmpnum(o->nodes.size(), nodes.size());
  assert_cmpnum(o->original.relations.size(), 1);
  assert_cmpnum(o->original.ways.size(), 2);
  // only the reference count has changed, which is no property of the object in the database
  assert_cmpnum(o->original.nodes.size(), 0);
  for(node_chain_t::const_iterator it = nodes.begin(); it != nodes.end(); it++) {
    w1->contains_node(*it);
    assert_cmpnum((*it)->ways, 1);
  }
  assert(nodes == w1->node_chain);

  assert_cmpnum(r->members.size(), 1);
  assert(r->members.front() == object_t(w1));

  for(node_chain_t::const_iterator it = nodes.begin(); it != nodes.end(); it++)
    assert_cmpnum((*it)->ways, 1);

  // the relation should have been saved
  const relation_t * const origR = o->originalObject(r);
  assert(origR != nullptr);
  assert_cmpnum(origR->members.size(), 1);
  assert(origR->members.front() == object_t(w0));

  o->relation_delete(r);

  // deleting the relation must not invalidate the original
  assert(origR == o->originalObject(origR));
  assert_cmpnum(origR->members.size(), 1);
  assert(origR->members.front() == object_t(w0));
}

// test that neighbors in relations are merged if necessary
void test_way_merge_relation_neighbors()
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  set_bounds(osm);

  // delete a simple way
  lpos_t l(10, 20);
  node_t *n1 = osm->node_new(l);
  osm->attach(n1);
  l.y = 40;
  node_t *n2 = osm->node_new(l);
  osm->attach(n2);
  l.x = 30;
  node_t *n3 = osm->node_new(l);
  osm->attach(n3);

  way_t *w1 = new way_t();
  w1->append_node(n1);
  w1->append_node(n2);
  osm->attach(w1);

  way_t *w2 = new way_t();
  w2->append_node(n2);
  w2->append_node(n3);
  osm->attach(w2);

  relation_t *rel = new relation_t();
  osm->attach(rel);
  relation_t *relcmp = new relation_t(); // the intended target state
  osm->attach(relcmp);

  // now put several instances of the same things into the relation to
  // see that merging happens the right way

  // to remove is first element, merge with next
  rel->members.push_back(member_t(object_t(w2)));
  rel->members.push_back(member_t(object_t(w1)));
  relcmp->members.push_back(member_t(object_t(w1)));

  // should not be touched
  rel->members.push_back(member_t(object_t(w1)));
  relcmp->members.push_back(member_t(object_t(w1)));
  rel->members.push_back(member_t(object_t(w1), "role0"));
  relcmp->members.push_back(member_t(object_t(w1), "role0"));

  // merge with previous member
  rel->members.push_back(member_t(object_t(w1)));
  rel->members.push_back(member_t(object_t(w2)));
  rel->members.push_back(member_t(object_t(w2))); // double-merge
  relcmp->members.push_back(member_t(object_t(w1)));

  // do not merge
  rel->members.push_back(member_t(object_t(w1), "role1"));
  relcmp->members.push_back(member_t(object_t(w1), "role1"));
  rel->members.push_back(member_t(object_t(w2)));
  relcmp->members.push_back(member_t(object_t(w2)));
  rel->members.push_back(member_t(object_t(w1), "role2"));
  relcmp->members.push_back(member_t(object_t(w1), "role2"));

  // merge at the end
  rel->members.push_back(member_t(object_t(w1), "rolem"));
  rel->members.push_back(member_t(object_t(w2), "rolem"));
  relcmp->members.push_back(member_t(object_t(w1), "rolem"));

  {
    osm_t::mergeResult<way_t> mergeRes = osm->mergeWays(w1, w2, nullptr);
    assert(!mergeRes.conflict);
  }

  for (unsigned int i = 0; i < relcmp->members.size(); i++) {
    // first check individually to get better output in case of error
    assert_cmpnum(rel->members[i].object.type, relcmp->members[i].object.type);
    assert_cmpnum(rel->members[i].object.get_id(), relcmp->members[i].object.get_id());
    if(rel->members[i].role == nullptr)
      assert_null(relcmp->members[i].role);
    else
      assert_cmpstr(rel->members[i].role, relcmp->members[i].role);
    assert(rel->members[i] == relcmp->members[i]);
  }

  // just to be sure
  assert_cmpnum(rel->members.size(), relcmp->members.size());
  assert(rel->members == relcmp->members);
}

void test_api_adjust()
{
 const std::string api06https = "https://api.openstreetmap.org/api/0.6";
 const std::string apihttp = "http://api.openstreetmap.org/api/0.";
 const std::string apidev = "http://master.apis.dev.openstreetmap.org/api/0.6";
 std::string server;

 assert(!api_adjust(server));
 assert(server.empty());

 server = apihttp + '5';
 assert(api_adjust(server));
 assert(server == api06https);

 assert(!api_adjust(server));
 assert(server == api06https);

 server = apihttp + '6';
 assert(api_adjust(server));
 assert(server == api06https);

 server = apihttp + '7';
 assert(!api_adjust(server));
 assert(server != api06https);

 server = apidev;
 assert(!api_adjust(server));
 assert(server == apidev);
}

void test_relation_members()
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  set_bounds(osm);
  relation_t *r = new relation_t();
  osm->attach(r);
  node_t *n1 = osm->node_new(lpos_t(1, 1));
  osm->attach(n1);
  node_t *n2 = osm->node_new(lpos_t(2, 2));
  osm->attach(n2);

  r->members.push_back(member_t(object_t(n1), "foo"));
  r->members.push_back(member_t(object_t(n2), "bar"));

  r->eraseMember(r->find_member_object(object_t(n2)));

  assert_cmpnum(r->members.size(), 1);
}

void test_way_insert()
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  set_bounds(osm);

  node_t * const n0 = osm->node_new(lpos_t(10, 10));
  osm->attach(n0);
  node_t * const n1 = osm->node_new(lpos_t(20, 20));
  osm->attach(n1);
  way_t * const w = osm->attach(new way_t());
  w->append_node(n0);
  w->append_node(n1);

  node_t * const in = w->insert_node(osm, 1, lpos_t(15, 16));
  assert(in != nullptr);
  assert(in != n0);
  assert(in != n1);
  assert(w->ends_with_node(n0));
  assert(w->ends_with_node(n1));
  assert(!w->ends_with_node(in));
  assert_cmpnum(w->node_chain.size(), 3);
  assert(w->node_chain.at(0) == n0);
  assert(w->node_chain.at(1) == in);
  assert(w->node_chain.at(2) == n1);
}

template<typename T ENABLE_IF_CONVERTIBLE(T *, base_object_t *)> void
helper_test_compare_tags(T &b1, T &b2)
{
  assert(b1 == b2);
  b1.tags.clear();
  if (b2.tags.empty()) {
    assert(b1 == b2);
    assert(b2 == b1);
  } else {
    assert(b1 != b2);
    assert(b2 != b1);
  }
  std::vector<tag_t> tags = ab_with_creator();
  b1.tags.replace(std::move(tags));
  assert(b1 != b2);
  assert(b1.tags != b2.tags);

  tags = ab_with_creator();
  b2.tags.replace(std::move(tags));
  assert(b1 == b2);
  assert(b1.tags == b2.tags);
}

// check the compare operators
void test_compare()
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  set_bounds(osm);

  // ==== NODES
  node_t * const n1 = new node_t(base_attributes(), lpos_t(0, 0), pos_t(0, 0));
  osm->attach(n1);

  std::unique_ptr<node_t> n2(std::make_unique<node_t>(*n1));
  // same position, but different object
  base_attributes ba(42);
  ba.version = 2;
  node_t * const otherN = new node_t(ba, n1->lpos, n1->pos);

  osm->nodes[otherN->id] = otherN;

  const object_t o1(n1);
  const object_t o2(n2.get());
  const object_t oN(otherN);

  // both objects refer the same id and type, so should be identical
  assert(o1 == o2);
  assert(o1 != oN);
  assert(o2 != oN);
  // the tags are both empty
  assert(n1->tags == n2->tags);
  assert(n1->tags == otherN->tags);

  const object_t o2ref(object_t::NODE_ID, o1.get_id());
  assert(o1 == o2ref);
  assert(oN != o2ref);

  // even with different tags
  helper_test_compare_tags(*n1, *n2);
  assert(o1 == o2);
  assert(o1 == o2ref);
  assert(*n1 != *otherN);

  // of course the tags are not the same
  assert(n1->tags != otherN->tags);

  // with the same tags the are identical again
  std::vector<tag_t>  tags = ab_with_creator();
  otherN->tags.replace(std::move(tags));

  assert(otherN->tags == n2->tags);
  assert(*otherN != *n2);

  // create a complete copy and see that this matches
  std::unique_ptr<node_t> otherCopy(std::make_unique<node_t>(*otherN));
  assert(*n1 != *otherCopy);
  assert(*otherN == *otherCopy);

  // ==== WAYS
  way_t * const w1 = new way_t();
  osm->attach(w1);
  std::unique_ptr<way_t> w2(std::make_unique<way_t>(*w1));

  assert(*w1 == *w2);

  // different node chains
  w1->append_node(n1);
  assert(*w1 != *w2);

  w2->append_node(n1);
  assert(*w1 == *w2);

  w1->append_node(otherN);
  w2->append_node(otherN);
  assert(*w1 == *w2);

  helper_test_compare_tags(*w1, *w2);

  // same nodes, but different order
  std::pair<unsigned int, unsigned int> flipped = w2->reverse(osm);
  assert_cmpnum(flipped.first, 0);
  assert_cmpnum(flipped.second, 0);

  assert(*w1 != *w2);

  // check way copies
  std::unique_ptr<way_t> w3(std::make_unique<way_t>(*w2));
  assert(*w2 == *w3);

  // ==== MEMBERS
  member_t mo1(o1);
  member_t mo2ref(o2);
  // members are the same if their objects are the same
  assert(mo1 == mo2ref);

  // but of course not if their roles differ
  mo1.role = "foobar";
  assert(mo1 != mo2ref);

  // same role -> equality
  mo2ref.role = mo1.role;
  assert(mo1 == mo2ref);
  const std::string rolecopy = mo1.role;
  mo2ref.role = rolecopy.c_str();
  assert(mo1 == mo2ref);

  // ==== RELATIONS
  relation_t * const r1 = new relation_t();
  osm->attach(r1);
  std::unique_ptr<relation_t> r2(std::make_unique<relation_t>(*r1));

  assert(*r1 == *r2);

  helper_test_compare_tags(*r1, *r2);

  // check members also
  r1->members.push_back(mo1);
  assert(*r1 != *r2);
  r2->members.push_back(mo2ref);
  assert(*r1 == *r2);
  r2->members.back().role = nullptr;
  assert(*r1 != *r2);
  r1->members.back().role = nullptr;
  assert(*r1 == *r2);

  r1->members.push_back(member_t(object_t(w2.get()), "forward"));
  assert(*r1 != *r2);
  r2->members.insert(r2->members.begin(), member_t(object_t(object_t::WAY_ID, w1->id), "forward"));
  assert(*r1 != *r2);
  std::reverse(r2->members.begin(), r2->members.end());
  assert(*r1 == *r2);

  // check relation copies
  std::unique_ptr<relation_t> r3(std::make_unique<relation_t>(*r1));
  assert(*r1 == *r3);
  assert(*r3 == *r2);
}

void check_updateTags_all(osm_t::ref osm, object_t obj)
{
  assert(obj.is_real());

  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("a", "aa"));

  osm_t::TagMap otags = tags;
  static_cast<base_object_t *>(obj)->tags.replace(otags);

  tags.insert(osm_t::TagMap::value_type("b", "bb"));

  assert(tags != otags); // paranoia

  verify_osm_db::run(osm);

  osm->updateTags(obj, otags);
  // nothing changed
  assert_cmpnum(osm->original.nodes.size(), 0);
  assert_cmpnum(osm->original.ways.size(), 0);
  assert_cmpnum(osm->original.relations.size(), 0);
  assert_cmpnum(static_cast<base_object_t *>(obj)->flags, 0);

  verify_osm_db::run(osm);

  // now actually change something
  osm->updateTags(obj, tags);
  if (obj.type == object_t::NODE)
    assert_cmpnum(osm->original.nodes.size(), 1);
  else
    assert_cmpnum(osm->original.nodes.size(), 0);
  if (obj.type == object_t::WAY)
    assert_cmpnum(osm->original.ways.size(), 1);
  else
    assert_cmpnum(osm->original.ways.size(), 0);
  if (obj.type == object_t::RELATION)
    assert_cmpnum(osm->original.relations.size(), 1);
  else
    assert_cmpnum(osm->original.relations.size(), 0);
  assert_cmpnum(static_cast<base_object_t *>(obj)->flags, OSM_FLAG_DIRTY);
  const base_object_t *orig = osm->originalObject(obj);
  assert(orig != nullptr);
  assert(orig != static_cast<base_object_t *>(obj));
  assert_cmpnum(obj.get_id(), orig->id);
  assert(orig->tags == otags);
  assert(static_cast<base_object_t *>(obj)->tags == tags);

  verify_osm_db::run(osm);

  // setting it back to original state should clear it from map
  osm->updateTags(obj, otags);
  // nothing changed
  assert_cmpnum(osm->original.nodes.size(), 0);
  assert_cmpnum(osm->original.ways.size(), 0);
  assert_cmpnum(osm->original.relations.size(), 0);
  assert_cmpnum(static_cast<base_object_t *>(obj)->flags, 0);
}

void test_updateTags()
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  set_bounds(osm);

  const lpos_t startPos(10, 10);
  base_attributes ba(1234);
  ba.version = 1;
  node_t * const n = osm->node_new(startPos.toPos(osm->bounds), ba);
  const object_t objN(n);
  osm->insert(n);

  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("a", "aa"));

  osm_t::TagMap otags = tags;
  n->tags.replace(otags);

  tags.insert(osm_t::TagMap::value_type("b", "bb"));

  assert(tags != otags); // paranoia

  check_updateTags_all(osm, objN);

  way_t *w = new way_t(ba);
  osm->insert(w);
  check_updateTags_all(osm, object_t(w));

  relation_t *r = new relation_t(ba);
  osm->insert(r);
  check_updateTags_all(osm, object_t(r));

  // NODE: updateTags not resetting back
  // now do the same changes, but...
  osm->updateTags(objN, tags);
  assert_cmpnum(osm->original.nodes.size(), 1);
  assert_cmpnum(n->flags, OSM_FLAG_DIRTY);

  // ...change the position...
  const lpos_t movedPos(10, 20);
  n->pos = movedPos.toPos(osm->bounds);

  // ...and this should not mark it as not dirty
  osm->updateTags(objN, otags);
  assert_cmpnum(osm->original.nodes.size(), 1);
  assert_cmpnum(n->flags, OSM_FLAG_DIRTY);

  // moving it back should not have an immediate effect as updateTags() checks tags for equality first
  n->pos = startPos.toPos(osm->bounds);
  osm->updateTags(objN, otags);
  assert_cmpnum(osm->original.nodes.size(), 1);
  assert_cmpnum(n->flags, OSM_FLAG_DIRTY);

  osm->updateTags(objN, tags);
  assert_cmpnum(osm->original.nodes.size(), 1);
  assert_cmpnum(n->flags, OSM_FLAG_DIRTY);

  // but now everything should reset back to the original state
  osm->updateTags(objN, otags);
  assert_cmpnum(osm->original.nodes.size(), 0);
  assert_cmpnum(n->flags, 0);

  // WAY: updateTags not resetting back
  const object_t objW(w);
  osm->updateTags(objW, tags);
  assert_cmpnum(osm->original.ways.size(), 1);
  assert_cmpnum(w->flags, OSM_FLAG_DIRTY);

  w->append_node(n);
  // this doesn't change the node, only increases the refcount
  assert_cmpnum(osm->original.nodes.size(), 0);
  assert_cmpnum(n->flags, 0);

  // the nodechain is still modified, so this must not clear the dirty flag
  osm->updateTags(objW, otags);
  assert_cmpnum(osm->original.ways.size(), 1);
  assert_cmpnum(w->flags, OSM_FLAG_DIRTY);

  // RELATION: updateTags not resetting back
  const object_t objR(r);
  osm->updateTags(objR, tags);
  assert_cmpnum(osm->original.relations.size(), 1);
  assert_cmpnum(r->flags, OSM_FLAG_DIRTY);

  r->members.push_back(member_t(objN));
  // this doesn't change the node either
  assert_cmpnum(osm->original.nodes.size(), 0);
  assert_cmpnum(n->flags, 0);

  // the member list is still modified, so this must not clear the dirty flag
  osm->updateTags(objR, otags);
  assert_cmpnum(osm->original.relations.size(), 1);
  assert_cmpnum(w->flags, OSM_FLAG_DIRTY);
}

void check_clean(osm_t::ref osm)
{
  assert(osm->is_clean(true));
  assert(osm->is_clean(false));
  assert_cmpnum(osm->original.nodes.size(), 0);
  assert_cmpnum(osm->original.ways.size(), 0);
  assert_cmpnum(osm->original.relations.size(), 0);
  verify_osm_db::run(osm);
}

template<typename T> void
toggle_deleted(osm_t::ref osm, T *o, size_t nc, size_t wc, size_t rc)
{
  verify_osm_db::run(osm);
  assert(!osm->is_clean(true));
  assert(!osm->is_clean(false));
  assert_cmpnum(osm->original.nodes.size(), nc);
  assert_cmpnum(osm->original.ways.size(), wc);
  assert_cmpnum(osm->original.relations.size(), rc);

  // when something like osm_t::undelete() exists use that here
  o->flags ^= OSM_FLAG_DELETED;
  osm->unmark_dirty(o);

  check_clean(osm);
}

// test that deleting an object marks it as dirty
void test_delete_markdirty()
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  set_bounds(osm);

  const lpos_t startPos(10, 10);
  base_attributes ba(1234);
  ba.version = 1;
  node_t * const n = osm->node_new(startPos.toPos(osm->bounds), ba);
  osm->insert(n);

  check_clean(osm);

  osm->node_delete(n);

  toggle_deleted(osm, n, 1, 0, 0);

  way_t * const w = new way_t(ba);
  osm->insert(w);

  check_clean(osm);

  osm->way_delete(w, nullptr);

  toggle_deleted(osm, w, 0, 1, 0);

  relation_t * const r = new relation_t(ba);
  osm->insert(r);

  check_clean(osm);

  osm->relation_delete(r);

  toggle_deleted(osm, r, 0, 0, 1);
}

void test_membership_state()
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  set_bounds(osm);

  const lpos_t startPos(10, 10);
  base_attributes ba(1234);
  ba.version = 1;
  node_t * const n = osm->node_new(startPos.toPos(osm->bounds), ba);
  osm->insert(n);
  ba.id = 43;
  node_t * const n2 = osm->node_new(startPos.toPos(osm->bounds), ba);
  osm->insert(n2);

  relation_t * const r = new relation_t(ba);
  r->members.push_back(member_t(object_t(n), "foo"));
  r->members.push_back(member_t(object_t(n), "bar"));
  osm->insert(r);

  relation_t * const r2 = new relation_t();
  r2->members.push_back(member_t(object_t(n), "bar"));
  r2->members.push_back(member_t(object_t(n)));
  osm->attach(r2);

  // has a role set in a new relation, both things are considered changed
  assert_cmpnum(r2->objectMembershipState(object_t(n), nullptr), relation_t::MembershipChanged | relation_t::RoleChanged);
  r2->members.erase(r2->members.begin());
  // no role set anymore
  assert_cmpnum(r2->objectMembershipState(object_t(n), nullptr), relation_t::MembershipChanged);
  // no member, nothing changed
  assert_cmpnum(r2->objectMembershipState(object_t(n2), nullptr), relation_t::MembershipUnmodified);

  // no member, nothing changed
  assert_cmpnum(r->objectMembershipState(object_t(n2), nullptr), relation_t::MembershipUnmodified);
  // member, but not marked dirty
  assert_cmpnum(r->objectMembershipState(object_t(n), nullptr), relation_t::MembershipUnmodified);

  osm->mark_dirty(r);
  const relation_t *oR = osm->originalObject(r);
  assert(oR != nullptr);

  // the relation is actually unmodified, so this should still say everything is fine
  assert_cmpnum(r->objectMembershipState(object_t(n), oR), relation_t::MembershipUnmodified);
  assert_cmpnum(r->objectMembershipState(object_t(n2), oR), relation_t::MembershipUnmodified);

  // use this method to ensure it does the right thing
  r->eraseMember(std::cbegin(r->members));

  assert_cmpnum(r->objectMembershipState(object_t(n), oR), relation_t::MembershipChanged | relation_t::RoleChanged);

  // set the role back to the original value, but it should still detect that the membership count doesn't match
  r->members.front() = oR->members.front();
  assert_cmpnum(r->objectMembershipState(object_t(n), oR), relation_t::MembershipChanged);

  // reset the members back to normal, and add a new one
  r->members = oR->members;
  r->members.push_back(member_t(object_t(n2)));

  assert_cmpnum(r->objectMembershipState(object_t(n), oR), relation_t::MembershipUnmodified);

  // add another new one, this time of the same object
  r->members.push_back(member_t(object_t(n)));

  assert_cmpnum(r->objectMembershipState(object_t(n), oR), relation_t::MembershipChanged);

  // membership and role gone away
  r->members.clear();
  assert_cmpnum(r->objectMembershipState(object_t(n), oR), relation_t::MembershipChanged | relation_t::RoleChanged);

  // added membership, no role
  r->members = oR->members;
  r->members.push_back(member_t(object_t(n2)));
  assert_cmpnum(r->objectMembershipState(object_t(n2), oR), relation_t::MembershipChanged);

  // with role
  std::prev(r->members.end())->role = "foo";
  assert_cmpnum(r->objectMembershipState(object_t(n2), oR), relation_t::MembershipChanged | relation_t::RoleChanged);
}

void test_updateMembers()
{
  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());
  set_bounds(osm);

  const lpos_t startPos(10, 10);
  base_attributes ba(1234);
  ba.version = 1;
  node_t * const n = osm->node_new(startPos.toPos(osm->bounds), ba);
  osm->insert(n);
  ba.id = 43;
  node_t * const n2 = osm->node_new(startPos.toPos(osm->bounds), ba);
  osm->insert(n2);

  relation_t * const r = new relation_t(ba);
  r->members.push_back(member_t(object_t(n), "foo"));
  r->members.push_back(member_t(object_t(n), "bar"));
  const std::vector<member_t> origMembers = r->members;
  osm->insert(r);

  // update with same members again
  {
    std::vector<member_t> nMembers = r->members;
    r->updateMembers(nMembers, osm);
    assert_null(osm->originalObject(r));
  }

  // actually modify
  {
    std::vector<member_t> nMembers = r->members;
    std::reverse(nMembers.begin(), nMembers.end());
    r->updateMembers(nMembers, osm);
    assert(osm->originalObject(r) != nullptr);
  }

  // reset back
  {
    std::vector<member_t> nMembers = origMembers;
    r->updateMembers(nMembers, osm);
    assert_null(osm->originalObject(r));
  }

  // modify again
  {
    std::vector<member_t> nMembers = r->members;
    std::reverse(nMembers.begin(), nMembers.end());
    r->updateMembers(nMembers, osm);
    assert(osm->originalObject(r) != nullptr);
  }

  // now modify the tags
  {
    std::vector<tag_t> nTags = ab_with_creator();
    r->tags.replace(std::move(nTags));
  }

  // reset back, should still be dirty because the tags have changed
  {
    std::vector<member_t> nMembers = origMembers;
    r->updateMembers(nMembers, osm);
    assert(osm->originalObject(r) != nullptr);
  }

  r->tags.clear();
  r->members.clear();

  // now again set back, should be back to original state now
  {
    std::vector<member_t> nMembers = origMembers;
    r->updateMembers(nMembers, osm);
    assert_null(osm->originalObject(r));
  }

  ///////////////// now test with a new relation
  relation_t * const rNew = new relation_t();
  osm->attach(rNew);
  assert(rNew->isNew());

  {
    std::vector<member_t> nMembers = origMembers;
    rNew->updateMembers(nMembers, osm);
    assert(osm->originalObject(rNew) == nullptr);
    assert(rNew->members == origMembers);
  }

  {
    std::vector<member_t> nMembers;
    rNew->updateMembers(nMembers, osm);
    assert(osm->originalObject(rNew) == nullptr);
    assert(rNew->members.empty());
  }
}

} // namespace

int main()
{
  xmlInitParser();

  test_trivial();
  test_taglist();
  test_replace_tags();
  test_split();
  test_split_order();
  test_changeset();
  test_reverse();
  test_way_delete();
  test_way_merge_relation_neighbors();
  test_member_delete();
  test_merge_nodes();
  test_merge_ways();
  test_merge_existing_ways();
  test_api_adjust();
  test_relation_members();
  test_way_insert();
  test_compare();
  test_updateTags();
  test_delete_markdirty();
  test_membership_state();
  test_updateMembers();

  xmlCleanupParser();

  return 0;
}

#include "dummy_appdata.h"
