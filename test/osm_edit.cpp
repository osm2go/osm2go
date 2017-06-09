#include <osm.h>

#include <misc.h>
#include <osm2go_cpp.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>

static bool find_aa(const tag_t &t)
{
  return strcmp(t.value, "aa") == 0;
}

static bool find_bb(const tag_t &t)
{
  return strcmp(t.value, "bb") == 0;
}

static std::vector<tag_t> ab_with_creator(void)
{
  std::vector<tag_t> ntags;

  tag_t cr_by(g_strdup("created_by"), g_strdup("test"));
  g_assert_true(cr_by.is_creator_tag());
  ntags.push_back(cr_by);
  ntags.push_back(tag_t(g_strdup("a"), g_strdup("aa")));
  ntags.push_back(tag_t(g_strdup("b"), g_strdup("bb")));

  return ntags;
}

static bool rtrue(const tag_t &) {
  return true;
}

static void nevercalled(const tag_t &) {
  g_assert_not_reached();
}

static void test_taglist() {
  tag_list_t tags;
  std::vector<tag_t> ntags;

  // compare empty lists
  g_assert(tags == ntags);
  g_assert(!(tags != ntags));

  // a list with only created_by must still be considered empty
  tag_t cr_by(const_cast<char *>("created_by"), const_cast<char *>("test"));
  g_assert_true(cr_by.is_creator_tag());
  ntags.push_back(cr_by);
  g_assert(tags == ntags);
  g_assert(!(tags != ntags));
  ntags.clear();

  // check replacing the tag list from osm_t::TagMap::value_type
  osm_t::TagMap nstags;
  nstags.insert(osm_t::TagMap::value_type("a", "A"));
  nstags.insert(osm_t::TagMap::value_type("b", "B"));

  // check self intersection
  g_assert_true(osm_t::tagSubset(nstags, nstags));
  // real subsets
  osm_t::TagMap tmpTags;
  tmpTags.insert(osm_t::TagMap::value_type("a", "A"));
  g_assert_true(osm_t::tagSubset(tmpTags, nstags));
  tmpTags.clear();
  tmpTags.insert(osm_t::TagMap::value_type("b", "B"));
  g_assert_true(osm_t::tagSubset(tmpTags, nstags));
  // non-intersecting
  tmpTags.insert(osm_t::TagMap::value_type("c", "C"));
  g_assert_false(osm_t::tagSubset(tmpTags, nstags));
  g_assert_false(osm_t::tagSubset(nstags, tmpTags));

  tags.replace(nstags);

  g_assert_cmpuint(nstags.size(), ==, 2);
  g_assert_nonnull(tags.get_value("a"));
  g_assert_cmpint(strcmp(tags.get_value("a"), "A"), ==, 0);
  g_assert_nonnull(tags.get_value("b"));
  g_assert_cmpint(strcmp(tags.get_value("b"), "B"), ==, 0);
  g_assert_false(tags.hasTagCollisions());

  // check replacing the tag list from tag_t
  ntags.push_back(tag_t(g_strdup("a"), g_strdup("aa")));
  ntags.push_back(tag_t(g_strdup("b"), g_strdup("bb")));

  tags.replace(ntags);

  g_assert_true(ntags.empty());
  g_assert_nonnull(tags.get_value("a"));
  g_assert_cmpint(strcmp(tags.get_value("a"), "aa"), ==, 0);
  g_assert_nonnull(tags.get_value("b"));
  g_assert_cmpint(strcmp(tags.get_value("b"), "bb"), ==, 0);
  g_assert_false(tags.hasTagCollisions());

  osm_t::TagMap lowerTags = tags.asMap();

  // replace again
  tags.replace(nstags);

  g_assert_cmpuint(nstags.size(), ==, 2);
  g_assert_nonnull(tags.get_value("a"));
  g_assert_cmpint(strcmp(tags.get_value("a"), "A"), ==, 0);
  g_assert_nonnull(tags.get_value("b"));
  g_assert_cmpint(strcmp(tags.get_value("b"), "B"), ==, 0);
  g_assert_false(tags.hasTagCollisions());

  tag_list_t tags2;
  tags2.replace(nstags);

  // merging the same things shouldn't change anything
  bool collision = tags.merge(tags2);
  g_assert_false(collision);
  g_assert_false(tags.hasTagCollisions());

  g_assert_nonnull(tags.get_value("a"));
  g_assert_cmpint(strcmp(tags.get_value("a"), "A"), ==, 0);
  g_assert_nonnull(tags.get_value("b"));
  g_assert_cmpint(strcmp(tags.get_value("b"), "B"), ==, 0);

  g_assert_null(tags2.get_value("a"));
  g_assert_null(tags2.get_value("b"));

  tags2.replace(lowerTags);
  g_assert_cmpuint(tags2.asMap().size(), ==, 2);
  g_assert_false(lowerTags.empty());
  g_assert_nonnull(tags2.get_value("a"));
  g_assert_cmpint(strcmp(tags2.get_value("a"), "aa"), ==, 0);
  g_assert_nonnull(tags2.get_value("b"));
  g_assert_cmpint(strcmp(tags2.get_value("b"), "bb"), ==, 0);
  g_assert_false(osm_t::tagSubset(tags2.asMap(), tags.asMap()));
  g_assert_false(osm_t::tagSubset(tags.asMap(), tags2.asMap()));

  collision = tags.merge(tags2);
  g_assert_true(collision);
  // moving something back and forth shouldn't change anything
  collision = tags2.merge(tags);
  g_assert_false(collision);
  collision = tags.merge(tags2);
  g_assert_false(collision);
  // tags2 is now empty, merging shouldn't change anything
  g_assert_true(tags2.empty());
  collision = tags.merge(tags2);
  g_assert_false(collision);

  g_assert_true(tags.hasTagCollisions());
  g_assert_nonnull(tags.get_value("a"));
  g_assert_cmpint(strcmp(tags.get_value("a"), "A"), ==, 0);
  g_assert_nonnull(tags.get_value("b"));
  g_assert_cmpint(strcmp(tags.get_value("b"), "B"), ==, 0);
  g_assert_cmpuint(tags.asMap().size(), ==, 4);
  g_assert_true(tags.contains(find_aa));
  g_assert_true(tags.contains(find_bb));

  // check identity with permutations
  ntags = ab_with_creator();
  tags.replace(ntags);
  ntags = ab_with_creator();
  g_assert(tags == ntags);
  std::rotate(ntags.begin(), ntags.begin() + 1, ntags.end());
  g_assert(tags == ntags);
  std::rotate(ntags.begin(), ntags.begin() + 1, ntags.end());
  g_assert(tags == ntags);

  std::for_each(ntags.begin(), ntags.end(), osm_tag_free);
  ntags.clear();
  tags.clear();

  // check that all these methods work on empty objects, both newly created and cleared ones
  g_assert_true(tags.empty());
  g_assert_false(tags.hasRealTags());
  g_assert_null(tags.get_value("foo"));
  g_assert_false(tags.contains(rtrue));
  tags.for_each(nevercalled);
  g_assert_true(tags.asMap().empty());
  g_assert(tags == std::vector<tag_t>());
  g_assert(tags == osm_t::TagMap());
  tags.clear();

  tag_list_t virgin;
  g_assert_true(virgin.empty());
  g_assert_false(virgin.hasRealTags());
  g_assert_null(virgin.get_value("foo"));
  g_assert_false(virgin.contains(rtrue));
  virgin.for_each(nevercalled);
  g_assert_true(virgin.asMap().empty());
  g_assert(virgin == std::vector<tag_t>());
  g_assert(virgin == osm_t::TagMap());
  virgin.clear();

  ntags.push_back(tag_t(g_strdup("one"), g_strdup("1")));
  g_assert(tags != ntags);
  tags.replace(ntags);
  ntags.push_back(tag_t(g_strdup("one"), g_strdup("1")));
  g_assert(tags == ntags);
  g_assert(virgin != tags.asMap());

  std::for_each(ntags.begin(), ntags.end(), osm_tag_free);
}

static void test_replace() {
  node_t node;

  g_assert_true(node.tags.empty());

  osm_t::TagMap nstags;
  node.updateTags(nstags);
  g_assert_cmpuint(node.flags, ==, 0);
  g_assert_true(node.tags.empty());

  osm_t::TagMap::value_type cr_by("created_by", "test");
  g_assert_true(tag_t::is_creator_tag(cr_by.first.c_str()));
  nstags.insert(cr_by);
  node.updateTags(nstags);
  g_assert(node.flags == 0);
  g_assert_true(node.tags.empty());

  node.tags.replace(nstags);
  g_assert_cmpuint(node.flags, ==, 0);
  g_assert_true(node.tags.empty());

  osm_t::TagMap::value_type aA("a", "A");
  nstags.insert(aA);

  node.updateTags(nstags);
  g_assert_cmpuint(node.flags, ==, OSM_FLAG_DIRTY);
  g_assert_false(node.tags.empty());
  g_assert(node.tags == nstags);

  node.flags = 0;

  node.updateTags(nstags);
  g_assert_cmpuint(node.flags, ==, 0);
  g_assert_false(node.tags.empty());
  g_assert(node.tags == nstags);

  node.tags.clear();
  g_assert_true(node.tags.empty());

  // use the other replace() variant that is also used by diff_restore(),
  // which can also insert created_by tags
  std::vector<tag_t> ntags;
  ntags.push_back(tag_t(g_strdup("created_by"), g_strdup("foo")));
  ntags.push_back(tag_t(g_strdup("a"), g_strdup("A")));
  node.tags.replace(ntags);

  g_assert_cmpuint(node.flags, ==, 0);
  g_assert_false(node.tags.empty());
  g_assert(node.tags == nstags);

  // updating with the same "real" tag shouldn't change anything
  node.updateTags(nstags);
  g_assert_cmpuint(node.flags, ==, 0);
  g_assert_false(node.tags.empty());
  g_assert(node.tags == nstags);
}

static void test_split()
{
  osm_t o;
  way_t * const v = new way_t();
  way_t * const w = new way_t();

  std::vector<tag_t> otags;
  otags.push_back(tag_t(g_strdup("a"), g_strdup("b")));
  otags.push_back(tag_t(g_strdup("b"), g_strdup("c")));
  otags.push_back(tag_t(g_strdup("created_by"), g_strdup("test")));
  otags.push_back(tag_t(g_strdup("d"), g_strdup("e")));
  otags.push_back(tag_t(g_strdup("f"), g_strdup("g")));
  const size_t ocnt = otags.size();

  w->tags.replace(otags);
  v->tags.replace(w->tags.asMap());

  o.way_attach(v);
  o.way_attach(w);

  for(int i = 0; i < 4; i++) {
    node_t *n = new node_t(3, lpos_t(), pos_t(52.25 + i / 0.001, 9.58 + i / 0.001), 1234500 + i);
    o.node_attach(n);
    v->node_chain.push_back(n);
    w->node_chain.push_back(n);
    n->ways += 2;
  }

  g_assert_cmpuint(o.ways.size(), ==, 2);
  way_t *neww = w->split(&o, w->node_chain.begin() + 2, false);
  g_assert_cmpuint(o.ways.size(), ==, 3);

  g_assert_cmpuint(w->node_chain.size(), ==, 2);
  g_assert_cmpuint(neww->node_chain.size(), ==, 2);
  g_assert(neww->tags == w->tags.asMap());
  g_assert(neww->tags == v->tags.asMap());
  g_assert_cmpuint(neww->tags.asMap().size(), ==, ocnt - 1);
}

static void test_changeset()
{
  const char message[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                         "<osm>\n"
                         "  <changeset>\n"
                         "    <tag k=\"created_by\" v=\"osm2go v0.9.5\"/>\n"
                         "    <tag k=\"comment\" v=\"&lt;&amp;&gt;\"/>\n"
                         "  </changeset>\n"
                         "</osm>\n";
  const char message_src[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                             "<osm>\n"
                             "  <changeset>\n"
                             "    <tag k=\"created_by\" v=\"osm2go v0.9.5\"/>\n"
                             "    <tag k=\"comment\" v=\"testcase comment\"/>\n"
                             "    <tag k=\"source\" v=\"survey\"/>\n"
                             "  </changeset>\n"
                             "</osm>\n";
  xmlChar *cs = osm_generate_xml_changeset("<&>", std::string());

  g_assert_cmpint(memcmp(cs, message, strlen(message)), ==, 0);
  xmlFree(cs);

  cs = osm_generate_xml_changeset("testcase comment", "survey");

  g_assert_cmpint(memcmp(cs, message_src, strlen(message_src)), ==, 0);
  xmlFree(cs);
}

static void test_reverse()
{
  osm_t o;
  o.rbounds.ll_min.lat = 52.2692786;
  o.rbounds.ll_min.lon = 9.5750497;
  o.rbounds.ll_max.lat = 52.2695463;
  o.rbounds.ll_max.lon = 9.5755;

  pos_t center((o.rbounds.ll_max.lat + o.rbounds.ll_min.lat) / 2,
               (o.rbounds.ll_max.lon + o.rbounds.ll_min.lon) / 2);

  pos2lpos_center(&center, &o.rbounds.center);
  o.rbounds.scale = cos(DEG2RAD(center.lat));

  o.bounds = &o.rbounds;

  lpos_t l(10, 20);
  node_t *n1 = o.node_new(l);
  o.node_attach(n1);
  l.y = 40;
  node_t *n2 = o.node_new(l);
  o.node_attach(n2);
  way_t *w = new way_t(1);
  w->append_node(n1);
  w->append_node(n2);
  o.way_attach(w);

  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("highway", "residential"));
  tags.insert(osm_t::TagMap::value_type("foo:forward", "yes"));
  tags.insert(osm_t::TagMap::value_type("foo:backward", "2"));
  tags.insert(osm_t::TagMap::value_type("bar:left", "3"));
  tags.insert(osm_t::TagMap::value_type("bar:right", "4"));
  tags.insert(osm_t::TagMap::value_type("oneway", "-1"));

  g_assert(w->node_chain.front() == n1);
  g_assert(w->node_chain.back() == n2);
  g_assert_cmpuint(w->flags, ==, OSM_FLAG_NEW);

  w->flags = 0;

  w->tags.replace(tags);
  w->reverse();
  unsigned int r = w->reverse_direction_sensitive_tags();

  g_assert_cmpuint(r, ==, 5);
  g_assert_cmpuint(w->flags, ==, OSM_FLAG_DIRTY);
  g_assert(w->node_chain.front() == n2);
  g_assert(w->node_chain.back() == n1);
  g_assert(w->tags != tags);

  osm_t::TagMap rtags;
  rtags.insert(osm_t::TagMap::value_type("highway", "residential"));
  rtags.insert(osm_t::TagMap::value_type("foo:backward", "yes"));
  rtags.insert(osm_t::TagMap::value_type("foo:forward", "2"));
  rtags.insert(osm_t::TagMap::value_type("bar:right", "3"));
  rtags.insert(osm_t::TagMap::value_type("bar:left", "4"));
  rtags.insert(osm_t::TagMap::value_type("oneway", "yes"));

  g_assert(w->tags == rtags);
}

int main()
{
  xmlInitParser();

  test_taglist();
  test_replace();
  test_split();
  test_changeset();
  test_reverse();

  xmlCleanupParser();

  return 0;
}
