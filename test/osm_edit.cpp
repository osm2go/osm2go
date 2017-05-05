#include <osm.h>

#include <misc.h>
#include <osm2go_cpp.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>

static void delete_stag(stag_t *s)
{
  delete s;
}

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

  // check replacing the tag list from stag_t
  std::vector<stag_t *> nstags;
  nstags.push_back(new stag_t("a", "A"));
  nstags.push_back(new stag_t("b", "B"));

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

  std::vector<stag_t *> lowerTags = tags.asPointerVector();

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
  g_assert_cmpuint(tags2.asVector().size(), ==, 2);
  g_assert_false(lowerTags.empty());
  g_assert_nonnull(tags2.get_value("a"));
  g_assert_cmpint(strcmp(tags2.get_value("a"), "aa"), ==, 0);
  g_assert_nonnull(tags2.get_value("b"));
  g_assert_cmpint(strcmp(tags2.get_value("b"), "bb"), ==, 0);

  collision = tags.merge(tags2);
  g_assert_true(collision);
  g_assert_true(tags.hasTagCollisions());
  g_assert_nonnull(tags.get_value("a"));
  g_assert_cmpint(strcmp(tags.get_value("a"), "A"), ==, 0);
  g_assert_nonnull(tags.get_value("b"));
  g_assert_cmpint(strcmp(tags.get_value("b"), "B"), ==, 0);
  g_assert_cmpuint(tags.asVector().size(), ==, 4);
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
  std::for_each(nstags.begin(), nstags.end(), delete_stag);
  std::for_each(lowerTags.begin(), lowerTags.end(), delete_stag);
  tags.clear();

  // check that all these methods work on empty objects, both newly created and cleared ones
  g_assert_true(tags.empty());
  g_assert_false(tags.hasRealTags());
  g_assert_null(tags.get_value("foo"));
  g_assert_false(tags.contains(rtrue));
  tags.for_each(nevercalled);
  g_assert_true(tags.asVector().empty());
  g_assert_true(tags.asPointerVector().empty());
  g_assert(tags == std::vector<tag_t>());
  g_assert(tags == std::vector<stag_t *>());
  g_assert(tags == osm_t::TagMap());
  tags.clear();

  tag_list_t virgin;
  g_assert_true(virgin.empty());
  g_assert_false(virgin.hasRealTags());
  g_assert_null(virgin.get_value("foo"));
  g_assert_false(virgin.contains(rtrue));
  virgin.for_each(nevercalled);
  g_assert_true(virgin.asVector().empty());
  g_assert_true(virgin.asPointerVector().empty());
  g_assert(virgin == std::vector<tag_t>());
  g_assert(virgin == std::vector<stag_t *>());
  g_assert(virgin == osm_t::TagMap());
  virgin.clear();
}

static void test_replace() {
  node_t node;

  g_assert_true(node.tags.empty());

  std::vector<stag_t *> nstags;
  node.updateTags(nstags);
  g_assert_cmpuint(node.flags, ==, 0);
  g_assert_true(node.tags.empty());

  stag_t cr_by("created_by", "test");
  g_assert_true(cr_by.is_creator_tag());
  nstags.push_back(&cr_by);
  node.updateTags(nstags);
  g_assert(node.flags == 0);
  g_assert_true(node.tags.empty());

  node.tags.replace(nstags);
  g_assert_cmpuint(node.flags, ==, 0);
  g_assert_true(node.tags.empty());

  stag_t aA("a", "A");
  nstags.push_back(&aA);

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

int main()
{
  test_taglist();
  test_replace();

  return 0;
}
