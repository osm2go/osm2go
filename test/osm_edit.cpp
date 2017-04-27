#include <osm.h>

#include <osm2go_cpp.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>

static void delete_stag(stag_t *s)
{
  delete s;
}

static bool find_aa(const tag_t *t)
{
  return strcmp(t->value, "aa") == 0;
}

static bool find_bb(const tag_t *t)
{
  return strcmp(t->value, "bb") == 0;
}

int main()
{
  tag_list_t tags;
  std::vector<tag_t *> ntags;

  // compare empty lists
  g_assert(tags == ntags);
  g_assert(!(tags != ntags));

  // a list with only created_by must still be considered empty
  tag_t cr_by(const_cast<char *>("created_by"), const_cast<char *>("test"));
  g_assert(cr_by.is_creator_tag());
  ntags.push_back(&cr_by);
  g_assert(tags == ntags);
  g_assert(!(tags != ntags));
  ntags.clear();

  // check replacing the tag list from stag_t
  std::vector<stag_t *> nstags;
  nstags.push_back(new stag_t("a", "A"));
  nstags.push_back(new stag_t("b", "B"));

  tags.replace(nstags);

  g_assert_cmpuint(nstags.size(), ==, 2);
  g_assert(tags.get_value("a") != O2G_NULLPTR);
  g_assert(strcmp(tags.get_value("a"), "A") == 0);
  g_assert(tags.get_value("b") != O2G_NULLPTR);
  g_assert(strcmp(tags.get_value("b"), "B") == 0);

  // check replacing the tag list from tag_t
  ntags.push_back(new tag_t(g_strdup("a"), g_strdup("aa")));
  ntags.push_back(new tag_t(g_strdup("b"), g_strdup("bb")));

  tags.replace(ntags);

  g_assert(ntags.empty());
  g_assert(tags.get_value("a") != O2G_NULLPTR);
  g_assert(strcmp(tags.get_value("a"), "aa") == 0);
  g_assert(tags.get_value("b") != O2G_NULLPTR);
  g_assert(strcmp(tags.get_value("b"), "bb") == 0);

  std::vector<stag_t *> lowerTags = tags.asPointerVector();

  // replace again
  tags.replace(nstags);

  g_assert_cmpuint(nstags.size(), ==, 2);
  g_assert(tags.get_value("a") != O2G_NULLPTR);
  g_assert(strcmp(tags.get_value("a"), "A") == 0);
  g_assert(tags.get_value("b") != O2G_NULLPTR);
  g_assert(strcmp(tags.get_value("b"), "B") == 0);

  tag_list_t tags2;
  tags2.replace(nstags);

  // merging the same things shouldn't change anything
  bool collision = tags.merge(tags2);
  g_assert(!collision);

  g_assert(tags.get_value("a") != O2G_NULLPTR);
  g_assert(strcmp(tags.get_value("a"), "A") == 0);
  g_assert(tags.get_value("b") != O2G_NULLPTR);
  g_assert(strcmp(tags.get_value("b"), "B") == 0);

  g_assert(tags2.get_value("a") == O2G_NULLPTR);
  g_assert(tags2.get_value("b") == O2G_NULLPTR);

  tags2.replace(lowerTags);
  g_assert_cmpuint(tags2.asVector().size(), ==, 2);
  g_assert(!lowerTags.empty());
  g_assert(tags2.get_value("a") != O2G_NULLPTR);
  g_assert(strcmp(tags2.get_value("a"), "aa") == 0);
  g_assert(tags2.get_value("b") != O2G_NULLPTR);
  g_assert(strcmp(tags2.get_value("b"), "bb") == 0);

  collision = tags.merge(tags2);
  g_assert(collision);
  g_assert(tags.get_value("a") != O2G_NULLPTR);
  g_assert(strcmp(tags.get_value("a"), "A") == 0);
  g_assert(tags.get_value("b") != O2G_NULLPTR);
  g_assert(strcmp(tags.get_value("b"), "B") == 0);
  g_assert_cmpuint(tags.asVector().size(), ==, 4);
  g_assert(tags.contains(find_aa));
  g_assert(tags.contains(find_bb));

  std::for_each(nstags.begin(), nstags.end(), delete_stag);
  std::for_each(lowerTags.begin(), lowerTags.end(), delete_stag);
  tags.clear();

  return 0;
}
