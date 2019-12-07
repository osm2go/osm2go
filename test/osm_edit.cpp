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
  assert(cr_by.is_creator_tag());
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
  assert(cr_by.is_creator_tag());
  std::vector<tag_t> ntags(1, cr_by);
  tags.replace(std::move(ntags));
  assert(!tags.hasRealTags());
  assert(!tags.hasNonCreatorTags());
  assert(!tags.hasTagCollisions());
  // only trivial tag
  assert_null(tags.singleTag());
  tag_t src("source", "test");
  assert(!src.is_creator_tag());
  ntags.clear();
  ntags.push_back(cr_by);
  ntags.push_back(src);
  tags.replace(std::move(ntags));
  // still only trivial tags
  assert_null(tags.singleTag());
  assert(!tags.hasRealTags());
  assert(tags.hasNonCreatorTags());
  assert(!tags.hasTagCollisions());

  std::unique_ptr<osm_t> osm(new osm_t());
  osm->bounds.min = lpos_t(0, 0);
  osm->bounds.max = lpos_t(0, 0);
  assert_cmpstr(osm->sanity_check(), trstring::native_type(_("Invalid data in OSM file:\nBoundary box invalid!")));
  set_bounds(osm);
  assert_cmpstr(osm->sanity_check(), trstring::native_type(_("Invalid data in OSM file:\nNo drawable content found!")));

  assert(osm->bounds.contains(lpos_t(0, 0)));
  assert(!osm->bounds.contains(lpos_t(-1, 0)));
  assert(!osm->bounds.contains(lpos_t(0, -1)));

  way_t *w = osm->way_attach(new way_t());
  // must work even on empty way
  assert_null(w->first_node());
  assert_null(w->last_node());

  lpos_t l(10, 20);
  node_t *n = osm->node_new(l);
  osm->node_attach(n);
  // the sanity check look on the node map which now isn't empty anymore
  assert(osm->sanity_check().isEmpty());

  w->append_node(n);
  assert(w->ends_with_node(n));
  // deleted ways never return true for any node
  w->flags |= OSM_FLAG_DELETED;
  assert(!w->ends_with_node(n));

  relation_t *r = osm->relation_attach(new relation_t());
  object_t robj(r);
  // check compare
  assert(robj == r);
  assert(robj != w);

  object_t inv;
  assert_cmpnum(inv.get_id(), ID_ILLEGAL);

  assert_cmpstr(r->descriptive_name(), "<ID #-1>");

  osm_t::TagMap tmap;
  tmap.insert(osm_t::TagMap::value_type("ref", "KHM 55"));
  r->tags.replace(tmap);
  assert_cmpstr(r->descriptive_name(), "KHM 55");
  // one non-trivial tag
  const tag_t *st = r->tags.singleTag();
  assert(st != nullptr);
  assert_cmpstr(st->key, "ref");
  assert_cmpstr(st->value, "KHM 55");
  // name is preferred over ref
  tmap.insert(osm_t::TagMap::value_type("name", "Rumpelstilzchen"));
  r->tags.replace(tmap);
  assert_cmpstr(r->descriptive_name(), "Rumpelstilzchen");
  // multiple non-trivial tags
  assert_null(r->tags.singleTag());
  // another way to clear
  std::vector<tag_t> notags;
  r->tags.replace(std::move(notags));
  assert_cmpstr(r->descriptive_name(), "<ID #-1>");
  r->id = std::numeric_limits<typeof(r->id)>::max();
  assert_cmpstr(r->descriptive_name(), "<ID #9223372036854775807>");
  r->id = std::numeric_limits<typeof(r->id)>::min();
  assert_cmpstr(r->descriptive_name(), "<ID #-9223372036854775808>");

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
  assert(cr_by.is_creator_tag());
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

void test_replace()
{
  node_t node(1, pos_t(0, 0), 1);
  assert_cmpnum(node.flags, 0);

  assert(node.tags.empty());

  osm_t::TagMap nstags;
  node.updateTags(nstags);
  assert_cmpnum(node.flags, 0);
  assert(node.tags.empty());

  osm_t::TagMap::value_type cr_by("created_by", "test");
  assert(tag_t::is_creator_tag(cr_by.first.c_str()));
  nstags.insert(cr_by);
  node.updateTags(nstags);
  assert(node.flags == 0);
  assert(node.tags.empty());

  node.tags.replace(nstags);
  assert_cmpnum(node.flags, 0);
  assert(node.tags.empty());

  osm_t::TagMap::value_type aA("a", "A");
  nstags.insert(aA);

  node.updateTags(nstags);
  assert_cmpnum(node.flags, OSM_FLAG_DIRTY);
  assert(!node.tags.empty());
  assert(node.tags == nstags);

  node.flags = 0;

  node.updateTags(nstags);
  assert_cmpnum(node.flags, 0);
  assert(!node.tags.empty());
  assert(node.tags == nstags);

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
  node.updateTags(nstags);
  assert_cmpnum(node.flags, 0);
  assert(!node.tags.empty());
  assert(node.tags == nstags);
}

unsigned int intrnd(unsigned int r)
{
  return std::rand() % r;
}

void test_split()
{
  std::unique_ptr<osm_t> o(new osm_t());
  way_t * const v = new way_t();
  way_t * const w = new way_t();
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

  o->way_attach(v);
  o->way_attach(w);

  r1->members.push_back(member_t(object_t(w)));
  o->relation_attach(r1);
  r2->members.push_back(member_t(object_t(w)));
  r2->members.push_back(member_t(object_t(v)));
  // insert twice, to check if all entries get duplicated
  r2->members.push_back(member_t(object_t(w)));
  o->relation_attach(r2);
  r3->members.push_back(member_t(object_t(v)));
  o->relation_attach(r3);

  // create the way to split
  std::vector<node_t *> nodes;
  for(int i = 0; i < 6; i++) {
    node_t *n = new node_t(3, pos_t(52.25 + i * 0.001, 9.58 + i * 0.001), 1234500 + i);
    o->node_attach(n);
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
  assert_cmpnum(dirty0.ways.added.size(), 3);
  assert_cmpnum(dirty0.ways.changed.size(), 0);
  assert_cmpnum(dirty0.ways.deleted.size(), 0);

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

  osm_t::dirty_t dirty1 = o->modified();
  assert_cmpnum(dirty1.nodes.changed.size(), 0);
  assert_cmpnum(dirty1.nodes.added.size(), 6);
  assert_cmpnum(dirty1.nodes.deleted.size(), 0);
  assert_cmpnum(dirty1.ways.changed.size(), 0);
  assert_cmpnum(dirty1.ways.added.size(), 4);
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
  w->flags = 0;
  assert_null(w->split(o, std::next(w->node_chain.begin(), 2), false));
  assert_cmpnum(o->ways.size(), 4);
  assert(w->flags & OSM_FLAG_DIRTY);
  for(unsigned int i = 0; i < nodes.size(); i++)
    assert_cmpnum(nodes[i]->ways, 2);

  assert(!w->contains_node(nodes[4]));
  assert(!w->ends_with_node(nodes[4]));
  assert_cmpnum(w->node_chain.size(), 2);
  assert_cmpnum(neww->node_chain.size(), 2);
  assert_cmpnum(neww2->node_chain.size(), 2);
  assert_cmpnum(r1->members.size(), 3);
  assert_cmpnum(r2->members.size(), 7);
  assert_cmpnum(r3->members.size(), 1);

  // now test a closed way
  way_t * const area = new way_t(0);
  for(unsigned int i = 0; i < nodes.size(); i++)
    area->append_node(nodes[i]);
  area->append_node(nodes[0]);
  assert(area->is_closed());
  o->way_attach(area);

  // drop the other ways to make reference counting easier
  o->way_delete(v, nullptr);
  o->way_delete(w, nullptr);
  o->way_delete(neww, nullptr);
  o->way_delete(neww2, nullptr);
  assert_cmpnum(o->ways.size(), 1);
  for(unsigned int i = 1; i < nodes.size(); i++)
    assert_cmpnum(nodes[i]->ways, 1);
  assert_cmpnum(nodes.front()->ways, 2);

  assert_null(area->split(o, area->node_chain.begin(), true));
  assert_cmpnum(area->node_chain.size(), nodes.size());
  for(unsigned int i = 0; i < nodes.size(); i++) {
    assert(area->node_chain[i] == nodes[i]);
    assert_cmpnum(nodes[i]->ways, 1);
  }

  // close the way again
  area->append_node(const_cast<node_t *>(area->first_node()));
  assert_null(area->split(o, std::next(area->node_chain.begin()), false));
  assert_cmpnum(area->node_chain.size(), nodes.size());
  for(unsigned int i = 0; i < nodes.size(); i++) {
    assert(area->node_chain[i] == nodes[(i + 1) % nodes.size()]);
    assert_cmpnum(nodes[i]->ways, 1);
  }

  // recreate old layout
  area->append_node(const_cast<node_t *>(area->first_node()));
  assert_null(area->split(o, std::prev(area->node_chain.end()), true));
  assert_cmpnum(area->node_chain.size(), nodes.size());
  for(unsigned int i = 0; i < nodes.size(); i++) {
    assert(area->node_chain[i] == nodes[(i + 1) % nodes.size()]);
    assert_cmpnum(nodes[i]->ways, 1);
  }
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
  const way_t *last = it->object.way;

  std::cout << "WAY " << last->id << " start " << last->first_node()->id
            << " end " << last->last_node()->id << " length " << last->node_chain.size()
            << std::endl;

  for(++it; it != itEnd; it++) {
    assert_cmpnum(it->object.type, object_t::WAY);
    const way_t *w = it->object.way;
    std::cout << "WAY " << w->id << " start " << w->first_node()->id
              << " end " << w->last_node()->id << " length " << w->node_chain.size()
              << std::endl;

    if(!last->ends_with_node(w->node_chain.front()) &&
       !last->ends_with_node(w->node_chain.back())) {
      std::cout << "\tGAP DETECTED!" << std::endl;
      ret = false;
    }

    last = it->object.way;
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
  std::unique_ptr<osm_t> o(new osm_t());
  std::vector<node_t *> nodes;
  for(unsigned int i = 1; i <= 10; i++) {
    node_t *n = new node_t(3, pos_t(52.25 + i * 0.001, 9.58 + i * 0.001), 1234500 + i);
    n->id = i;
    o->node_insert(n);
    nodes.push_back(n);
  }
  // the ways that start and end each relation, opposing directions
  way_t *wstart = new way_t();
  o->way_attach(wstart);
  wstart->append_node(o->node_by_id(1));
  wstart->append_node(o->node_by_id(2));
  way_t *wend = new way_t();
  o->way_attach(wend);
  wend->append_node(o->node_by_id(10));
  wend->append_node(o->node_by_id(9));

  // now the ways that are split
  std::vector<way_t *> splitw;
  for(unsigned int i = 0; i < 12; i++) {
    way_t *w = new way_t();
    o->way_attach(w);
    splitw.push_back(w);
    const std::vector<node_t *>::const_iterator itEnd = std::prev(nodes.end());

    for(std::vector<node_t *>::const_iterator it = std::next(nodes.begin()); it != itEnd; it++)
      w->append_node(*it);
  }

  for(unsigned int i = 1; i <= splitw.size(); i++) {
    relation_t * const r = new relation_t();
    r->id = i;
    o->relation_insert(r);
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
    assert(checkLinearRelation(o->relation_by_id(i + 1)));

    for(unsigned int j = 0; j < sequences[i].size(); j++) {
      std::vector<way_t *>::iterator it = std::find_if(sw.begin(), sw.end(),
                                                       findWay(sequences[i][j]));
      assert(it != sw.end());
      way_t *nw = (*it)->split(o, std::find((*it)->node_chain.begin(), (*it)->node_chain.end(), sequences[i][j]), true);
      sw.push_back(nw);
    }
    assert(checkLinearRelation(o->relation_by_id(i + 1)));
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
  std::unique_ptr<osm_t> o(new osm_t());
  set_bounds(o);

  lpos_t l(10, 20);
  node_t *n1 = o->node_new(l);
  assert_cmpnum(n1->version, 0);
  assert_cmpnum(n1->flags, OSM_FLAG_DIRTY);
  o->node_attach(n1);
  l.y = 40;
  node_t *n2 = o->node_new(l);
  o->node_attach(n2);
  way_t *w = new way_t(0);
  w->append_node(n1);
  w->append_node(n2);
  o->way_attach(w);

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
  assert(w->isNew());

  w->flags = 0;

  // some relations the way is member of to see how the roles change
  std::vector<relation_t *> rels;
  for(unsigned int i = 0; i < 5; i++) {
    relation_t *r = new relation_t();
    rels.push_back(r);
    o->relation_attach(r);
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
  unsigned int r, rroles;
  w->reverse(o, r, rroles);

  assert_cmpnum(r, 6);
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
  assert_cmpnum(rroles, 2);
  // rels[0] has wrong type, roles should not be modified
  assert_cmpnum(rels[0]->members.size(), 2);
  assert_cmpstr(rels[0]->members.front().role, "forward");
  assert_cmpstr(rels[0]->members.back().role, "forward");
  // rels[1] has matching type, first member role should be changed
  assert_cmpnum(rels[1]->members.size(), 2);
  assert_cmpstr(rels[1]->members.front().role, "backward");
  assert(rels[1]->members.front().object == w);
  assert_cmpstr(rels[1]->members.back().role, "forward");
  // rels[2] has matching type, first member role should be changed (other direction)
  assert_cmpnum(rels[1]->members.size(), 2);
  assert_cmpstr(rels[2]->members.front().role, "forward");
  assert(rels[2]->members.front().object == w);
  assert_cmpstr(rels[2]->members.back().role, "backward");
  // rels[3] has matching type, but roles are empty
  assert_cmpnum(rels[1]->members.size(), 2);
  assert_null(rels[3]->members.front().role);
  assert(rels[3]->members.front().object == w);
  assert_null(rels[3]->members.back().role);

  // go back
  w->reverse(o, r, rroles);

  assert_cmpnum(r, 6);
  assert_cmpnum(rroles, 2);
  // the original value was uppercase
  tags.find("oneway")->second = "yes";
  assert(w->tags == tags);
}

unsigned int nn_cnt;
void node_noop(node_t *)
{
  nn_cnt++;
}

void test_way_delete()
{
  std::unique_ptr<osm_t> o(new osm_t());
  set_bounds(o);

  // delete a simple way
  lpos_t l(10, 20);
  node_t *n1 = o->node_new(l);
  o->node_attach(n1);
  l.y = 40;
  node_t *n2 = o->node_new(l);
  o->node_attach(n2);
  way_t *w = new way_t(0);
  w->append_node(n1);
  w->append_node(n2);
  o->way_attach(w);

  o->way_delete(w, nullptr);

  assert_cmpnum(o->nodes.size(), 0);
  assert_cmpnum(o->ways.size(), 0);

  // delete a closed way
  n1 = o->node_new(l);
  o->node_attach(n1);
  l.y = 20;
  n2 = o->node_new(l);
  o->node_attach(n2);
  w = new way_t(0);
  w->append_node(n1);
  w->append_node(n2);
  o->way_attach(w);
  l.x = 20;
  n2 = o->node_new(l);
  o->node_attach(n2);
  w->append_node(n2);
  assert(!w->is_closed());
  w->append_node(n1);
  assert(w->is_closed());

  o->way_delete(w, nullptr);

  assert_cmpnum(o->nodes.size(), 0);
  assert_cmpnum(o->ways.size(), 0);

  // test way deletion with nodes that should be preserved
  l.x = 10;
  l.y = 20;
  n1 = o->node_new(l);
  o->node_attach(n1);

  // this node will be removed when the way is removed
  l.y = 40;
  n2 = o->node_new(l);
  o->node_attach(n2);

  w = new way_t(0);
  w->append_node(n1);
  w->append_node(n2);
  o->way_attach(w);

  // this instance will persist
  l.x = 20;
  n2 = o->node_new(l);
  o->node_attach(n2);
  w->append_node(n2);

  relation_t *r = new relation_t(0);
  o->relation_attach(r);
  r->members.push_back(member_t(object_t(n2)));

  osm_t::TagMap nstags;
  nstags.insert(osm_t::TagMap::value_type("a", "A"));
  n1->tags.replace(nstags);

  l.x = 5;
  node_t *n3 = o->node_new(l);
  o->node_attach(n3);
  l.y = 25;
  node_t *n4 = o->node_new(l);
  o->node_attach(n4);

  way_t *w2 = new way_t(0);
  o->way_attach(w2);
  w2->append_node(n3);
  w2->append_node(n4);

  w->append_node(n3);

  // now delete the way, which would reduce the use counter of all nodes
  // n1 should be preserved as it has tags on it's own
  // n2 should be preserved as it is still referenced by a relation
  // n3 should be preserved as it is used in another way
  o->way_delete(w, nullptr);

  assert_cmpnum(o->nodes.size(), 4);
  assert_cmpnum(o->ways.size(), 1);
  assert_cmpnum(o->relations.size(), 1);
  assert(o->node_by_id(n1->id) == n1);
  assert(o->node_by_id(n2->id) == n2);
  assert(o->node_by_id(n3->id) == n3);
  assert(o->node_by_id(n4->id) == n4);
  assert_cmpnum(r->members.size(), 1);

  // once again, with a custom unref function
  w = new way_t(0);
  // not attached here as map_edit also keeps separate
  w->append_node(n3);
  w->append_node(n4);

  assert_cmpnum(nn_cnt, 0);
  o->way_delete(w, nullptr, node_noop);
  assert_cmpnum(nn_cnt, 2);
  // they have not been unrefed in the custom function
  assert_cmpnum(n3->ways, 2);
  n3->ways--;
  assert_cmpnum(n4->ways, 2);
  n4->ways--;

  // once more, but this time pretend this is not a new way
  w = new way_t(1, 42);
  o->way_insert(w);
  w->append_node(n3);
  w->append_node(n4);
  // keep it here, it ill only be reset, but not freed as that is done through the map
  std::unique_ptr<map_item_t> mi(new map_item_t(object_t(w), nullptr));
  w->map_item = mi.get();

  o->way_delete(w, nullptr);
  assert_cmpnum(n3->ways, 1);
  assert_cmpnum(n4->ways, 1);
  assert_cmpnum(w->node_chain.size(), 0);
  assert(w->flags & OSM_FLAG_DELETED);
  assert(w->tags.empty());
}

void test_member_delete()
{
  std::unique_ptr<osm_t> o(new osm_t());
  set_bounds(o);

  // a way with 3 points
  lpos_t l(10, 20);
  node_t *n1 = o->node_new(l);
  o->node_attach(n1);
  l.y = 40;
  node_t *n2 = o->node_new(l);
  o->node_attach(n2);
  way_t *w = new way_t(0);
  w->append_node(n1);
  w->append_node(n2);
  o->way_attach(w);

  l.x = 20;
  n2 = o->node_new(l);
  n2->flags = 0;
  n2->version = 1;
  n2->id = 42;
  o->node_insert(n2);
  w->append_node(n2);

  // a relation containing both the way as well as the node
  relation_t * const r = new relation_t(0);
  r->members.push_back(member_t(object_t(w)));
  r->members.push_back(member_t(object_t(n2)));
  o->relation_attach(r);

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

  unsigned int nodes = 0, ways = 0, relations = 0;
  r->members_by_type(nodes, ways, relations);
  assert_cmpnum(nodes, 1);
  assert_cmpnum(ways, 1);
  assert_cmpnum(relations, 0);

  // keep it here, it ill only be reset, but not freed as that is done through the map
  std::unique_ptr<map_item_t> mi(new map_item_t(object_t(w), nullptr));
  n2->map_item = mi.get();

  // now delete the node that is member of both other objects
  o->node_delete(n2, true);
  fflush(stdout);
  // since the object had a valid id it should still be there, but unreferenced
  assert_cmpnum(o->nodes.size(), 3);
  assert_cmpnum(o->ways.size(), 1);
  assert_cmpnum(o->relations.size(), 1);
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
  assert_cmpnum(dirty1.relations.added.size(), 1);
  assert_cmpnum(dirty1.relations.deleted.size(), 0);

  nodes = 0;
  ways = 0;
  relations = 0;
  r->members_by_type(nodes, ways, relations);
  assert_cmpnum(nodes, 0);
  assert_cmpnum(ways, 1);
  assert_cmpnum(relations, 0);
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
  std::unique_ptr<osm_t> o(new osm_t());
  set_bounds(o);

  /// ==================
  // join 2 new nodes
  lpos_t oldpos(10, 10);
  lpos_t newpos(20, 20);
  node_t *n1 = o->node_new(oldpos);
  node_t *n2 = o->node_new(newpos);
  o->node_attach(n1);
  o->node_attach(n2);

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
  n2 = o->node_new(oldpos);
  n2->id = 1234;
  n2->flags = 0;
  o->node_insert(n2);

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

  /// ==================
  // do the same join again, but with swapped arguments
  n2->lpos = newpos;
  n2->flags = 0;
  n1 = o->node_new(oldpos);
  o->node_attach(n1);

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

  o->node_free(n2);
  assert_cmpnum(o->nodes.size(), 0);

  /// ==================
  // start new
  n1 = o->node_new(oldpos);
  n2 = o->node_new(newpos);
  o->node_attach(n1);
  o->node_attach(n2);

  // attach one node to a way, that one should be preserved
  way_t *w = new way_t(0);
  o->way_attach(w);
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
  relation_t *r = new relation_t(0);
  o->relation_attach(r);
  n1 = o->node_new(oldpos);
  n2 = o->node_new(newpos);
  o->node_attach(n1);
  o->node_attach(n2);

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
  assert(r->members.front().object == n2);
  assert_null(ways2join[0]);
  assert_null(ways2join[1]);

  o->relation_delete(r);
  assert_cmpnum(o->nodes.size(), 1);
  assert_cmpnum(o->ways.size(), 0);
  assert_cmpnum(o->relations.size(), 0);
  o->node_delete(n2);
  assert_cmpnum(o->nodes.size(), 0);

  /// ==================
  // now put both into a way, the way of the second node should be updated
  std::vector<way_t *> ways;
  std::vector<relation_t *> relations;
  for(int i = 0; i < 3; i++) {
    w = new way_t(0);
    o->way_attach(w);
    lpos_t pos(i + 4, i + 4);
    n1 = o->node_new(pos);
    o->node_attach(n1);
    w->append_node(n1);
    r = new relation_t(0);
    o->relation_attach(r);
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
  o->node_attach(n1);
  o->node_attach(n2);

  // one way with only n1
  w = ways.back();
  w->append_node(n1);
  unsigned int rc, rrc;
  w->reverse(o, rc, rrc);
  assert_cmpnum(rc, 0);
  assert_cmpnum(rrc, 0);

  // one way with only n2
  w = ways.front();
  // put both nodes here, only one instance should remain
  w->append_node(n2);
  w->flags = 0;

  std::vector<way_t *>::iterator wit = std::next(ways.begin());
  w = *wit;
  // put both nodes here, only one instance should remain
  w->append_node(n1);
  w->append_node(n2);
  w->flags = 0;

  relations.back()->members.push_back(member_t(object_t(n1)));
  r = relations.front();
  r->members.push_back(member_t(object_t(n2)));
  r->flags = 0;
  assert_cmpnum(ways.back()->node_chain.size(), 2);
  assert_cmpnum(w->node_chain.size(), 3);
  assert(ways.back()->node_chain.front() == n1);
  assert(ways.back()->ends_with_node(n1));
  assert(w->node_chain.back() == n2);
  assert(w->ends_with_node(n2));
  assert_cmpnum(n1->ways, 2);
  assert_cmpnum(n2->ways, 2);
  assert(relations.back()->members.front().object == n1);
  assert(r->members.front().object == n2);
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
  assert_cmpnum(w->flags, OSM_FLAG_DIRTY);
  assert_cmpnum(n1->ways, 3);
  assert(relations.back()->members.front().object == n1);
  // test member_t::operator==(object_t)
  assert(relations.back()->members.front() == object_t(n1));
  assert(r->members.front().object == n1);
  assert_cmpnum(r->flags, OSM_FLAG_DIRTY);
  assert_null(ways2join[0]);
  assert_null(ways2join[1]);

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
  o->node_attach(n2);
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
  w = new way_t(0);
  o->way_attach(w);
  for (int i = 0; i < 4; i++) {
    lpos_t p(10 + (i % 2) * 10, 10 + (i / 2) * 10);
    nn.push_back(o->node_new(p));
    o->node_attach(nn.back());
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
  w = new way_t(0);
  o->way_attach(w);
  for (int i = 0; i < 3; i++) {
    lpos_t p(30 + (i % 2) * 10, 30 + (i / 2) * 10);
    nn.push_back(o->node_new(p));
    o->node_attach(nn.back());
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
}

void setup_way_relations_for_merge(osm_t::ref o, way_t *w0, way_t *w1)
{
  o->relation_by_id(-3)->members.push_back(member_t(object_t(w0), "foo"));
  o->relation_by_id(-4)->members.push_back(member_t(object_t(w1), "bar"));
  o->relation_by_id(-4)->members.push_back(member_t(object_t(w0)));
}

node_chain_t setup_ways_for_merge(const node_chain_t &nodes, osm_t::ref o, way_t *&w0,
                                  way_t *&w1, const unsigned int i, int relations)
{
  node_chain_t expect;

  w0 = new way_t(0);
  if(i < 2) {
    for(unsigned int j = 0; j < nodes.size() / 2; j++)
      w0->append_node(nodes[j]);
  } else {
    for(int j = nodes.size() / 2 - 1; j >= 0; j--)
      w0->append_node(nodes[j]);
  }
  o->way_attach(w0);

  w1 = new way_t(0);
  if(i % 2) {
    for(unsigned int j = nodes.size() / 2 - 1; j < nodes.size(); j++)
      w1->append_node(nodes[j]);
    expect = nodes;
  } else {
    for(unsigned int j = nodes.size() - 1; j >= nodes.size() / 2 - 1; j--)
      w1->append_node(nodes[j]);
    expect = nodes;
    std::reverse(expect.begin(), expect.end());
  }
  o->way_attach(w1);

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

void verify_merged_way(way_t *w, osm_t::ref o, const node_chain_t &nodes, const node_chain_t &expect, bool expectRels)
{
  assert_cmpnum(w->node_chain.size(), nodes.size());
  assert_cmpnum(o->ways.size(), 1);
  assert_cmpnum(o->nodes.size(), nodes.size());
  for(node_chain_t::const_iterator it = nodes.begin(); it != nodes.end(); it++) {
    w->contains_node(*it);
    assert_cmpnum((*it)->ways, 1);
  }
  assert(expect == w->node_chain);

  assert_cmpnum(o->relation_by_id(-1)->members.size(), 0);
  // check the expected relation memberships of the way
  if(expectRels) {
    relation_t *rel = o->relation_by_id(-3);
    std::vector<member_t>::iterator it = rel->find_member_object(object_t(w));
    assert(it != rel->members.end());
    assert_cmpstr(it->role, "foo");
    rel->remove_member(it);

    rel = o->relation_by_id(-4);
    it = rel->find_member_object(object_t(w));
    assert(it != rel->members.end());
    assert_cmpstr(it->role, "bar");
    rel->remove_member(it);

    it = rel->find_member_object(object_t(w));
    assert(it != rel->members.end());
    assert_null(it->role);
    rel->remove_member(it);
  }
  for(unsigned int i = 1; i < o->relations.size(); i++)
    assert_cmpnum(o->relation_by_id(-1 - static_cast<item_id_t>(i))->members.size(), i - 1);

  o->way_free(w);

  assert_cmpnum(o->ways.size(), 0);
  assert_cmpnum(o->nodes.size(), nodes.size());
  for(node_chain_t::const_iterator it = nodes.begin(); it != nodes.end(); it++)
    assert_cmpnum((*it)->ways, 0);
}

void test_merge_ways()
{
  std::unique_ptr<osm_t> o(new osm_t());
  set_bounds(o);

  node_chain_t nodes;
  for(int i = 0; i < 8; i++) {
    nodes.push_back(o->node_new(lpos_t(i * 3, i * 3)));
    o->node_attach(nodes.back());
  }

  for(int i = 0; i < 5; i++) {
    relation_t *r = new relation_t();
    for(int j = 1; j < i; j++)
      r->members.push_back(member_t(object_t(nodes[j])));
    o->relation_attach(r);
  }

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
  }
}

// test that neighbors in relations are merged if necessary
void test_way_merge_relation_neighbors()
{
  std::unique_ptr<osm_t> osm(new osm_t());
  set_bounds(osm);

  // delete a simple way
  lpos_t l(10, 20);
  node_t *n1 = osm->node_new(l);
  osm->node_attach(n1);
  l.y = 40;
  node_t *n2 = osm->node_new(l);
  osm->node_attach(n2);
  l.x = 30;
  node_t *n3 = osm->node_new(l);
  osm->node_attach(n3);

  way_t *w1 = new way_t(0);
  w1->append_node(n1);
  w1->append_node(n2);
  osm->way_attach(w1);

  way_t *w2 = new way_t(0);
  w2->append_node(n2);
  w2->append_node(n3);
  osm->way_attach(w2);

  relation_t *rel = new relation_t(0);
  osm->relation_attach(rel);
  relation_t *relcmp = new relation_t(0); // the intended target state
  osm->relation_attach(relcmp);

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

void test_description()
{
  std::unique_ptr<osm_t> osm(new osm_t());
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
  assert_cmpstr(o.get_name(*osm), "way/area: member of public_transport 'Kröpcke'");
  pt_r->members.clear();
  pt_r->members.push_back(member_t(object_t(w), "foo"));
  assert_cmpstr(o.get_name(*osm), "way/area: 'foo' in public_transport 'Kröpcke'");

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

void test_relation_members()
{
  std::unique_ptr<osm_t> osm(new osm_t());
  set_bounds(osm);
  relation_t *r = new relation_t(0);
  osm->relation_attach(r);
  node_t *n1 = osm->node_new(lpos_t(1, 1));
  osm->node_attach(n1);
  node_t *n2 = osm->node_new(lpos_t(2, 2));
  osm->node_attach(n2);

  r->members.push_back(member_t(object_t(n1), "foo"));
  r->members.push_back(member_t(object_t(n2), "bar"));

  r->remove_member(r->find_member_object(object_t(n2)));

  assert_cmpnum(r->members.size(), 1);
}

void test_way_insert()
{
  std::unique_ptr<osm_t> osm(new osm_t());
  set_bounds(osm);

  node_t * const n0 = osm->node_new(lpos_t(10, 10));
  osm->node_attach(n0);
  node_t * const n1 = osm->node_new(lpos_t(20, 20));
  osm->node_attach(n1);
  way_t * const w = osm->way_attach(new way_t());
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

} // namespace

int main()
{
  xmlInitParser();

  test_trivial();
  test_taglist();
  test_replace();
  test_split();
  test_split_order();
  test_changeset();
  test_reverse();
  test_way_delete();
  test_way_merge_relation_neighbors();
  test_member_delete();
  test_merge_nodes();
  test_merge_ways();
  test_api_adjust();
  test_description();
  test_relation_members();
  test_way_insert();

  xmlCleanupParser();

  return 0;
}

#include "appdata_dummy.h"
