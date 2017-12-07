#include <josm_presets.h>
#include <josm_presets_p.h>
#include <misc.h>
#include <osm.h>

#include <osm2go_annotations.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cerrno>
#include <glib.h>
#include <iostream>
#include <string>

#define TESTTAG(a, b) static osm_t::TagMap::value_type tag_##a##_##b(#a, #b)

TESTTAG(testkey, other);
TESTTAG(testkey, testtext);
TESTTAG(neutral, neutral);

static osm_t::TagMap VECTOR_ONE(const osm_t::TagMap::value_type &a)
{
  osm_t::TagMap ret;
  ret.insert(a);
  return ret;
}

static void checkTextMatch()
{
  presets_element_text w_0(tag_testkey_testtext.first,
                          tag_testkey_testtext.second,
                          std::string(), O2G_NULLPTR);

  assert_cmpnum(w_0.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_0.matches(VECTOR_ONE(tag_testkey_testtext)), 0);
  assert_cmpnum(w_0.matches(osm_t::TagMap()), 0);

  presets_element_text w_ign(tag_testkey_testtext.first,
                            tag_testkey_testtext.second,
                            std::string(), "none");

  assert_cmpnum(w_ign.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_ign.matches(VECTOR_ONE(tag_testkey_testtext)), 0);
  assert_cmpnum(w_ign.matches(osm_t::TagMap()), 0);

  presets_element_text w_bad(tag_testkey_testtext.first,
                            tag_testkey_testtext.second,
                            std::string(), "nonsense");

  assert_cmpnum(w_bad.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_bad.matches(VECTOR_ONE(tag_testkey_testtext)), 0);
  assert_cmpnum(w_bad.matches(osm_t::TagMap()), 0);

  presets_element_text w_key(tag_testkey_testtext.first,
                            tag_testkey_testtext.second,
                            std::string(), "key");

  assert_cmpnum(w_key.matches(VECTOR_ONE(tag_testkey_other)), 1);
  assert_cmpnum(w_key.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_key.matches(osm_t::TagMap()), 0);

  presets_element_text w_keyf(tag_testkey_testtext.first,
                             tag_testkey_testtext.second,
                             std::string(), "key!");

  assert_cmpnum(w_keyf.matches(VECTOR_ONE(tag_testkey_other)), 1);
  assert_cmpnum(w_keyf.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_keyf.matches(osm_t::TagMap()), -1);

  presets_element_text w_kv(tag_testkey_testtext.first,
                           tag_testkey_testtext.second,
                           std::string(), "keyvalue");

  assert_cmpnum(w_kv.matches(VECTOR_ONE(tag_testkey_other)), 1);
  assert_cmpnum(w_kv.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_kv.matches(osm_t::TagMap()), 0);

  presets_element_text w_kvf(tag_testkey_testtext.first,
                            tag_testkey_testtext.second,
                            std::string(), "keyvalue!");

  assert_cmpnum(w_kvf.matches(VECTOR_ONE(tag_testkey_other)), 1);
  assert_cmpnum(w_kvf.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_kvf.matches(osm_t::TagMap()), -1);
}

static void checkComboMatch()
{
  std::vector<std::string> values;
  values.push_back("nonmatch");
  values.push_back("");
  values.push_back(tag_testkey_testtext.second);
  values.push_back("another nonmatch");
  std::vector<std::string> empty_vector;
  const std::vector<std::string> backup = values;

  presets_element_combo w_0(tag_testkey_testtext.first,
                           "visual text",
                           values.front(),
                           O2G_NULLPTR, values, empty_vector);

  assert_cmpnum(w_0.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_0.matches(VECTOR_ONE(tag_testkey_testtext)), 0);
  assert_cmpnum(w_0.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_combo w_ign(tag_testkey_testtext.first,
                             "visual text",
                             values.front(),
                             "none", values, empty_vector);

  assert_cmpnum(w_ign.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_ign.matches(VECTOR_ONE(tag_testkey_testtext)), 0);
  assert_cmpnum(w_ign.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_combo w_bad(tag_testkey_testtext.first,
                             "visual text",
                             values.front(),
                             "nonsense", values, empty_vector);

  assert_cmpnum(w_bad.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_bad.matches(VECTOR_ONE(tag_testkey_testtext)), 0);
  assert_cmpnum(w_bad.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_combo w_key(tag_testkey_testtext.first,
                             "visual text",
                             values.front(),
                             "key", values, empty_vector);

  assert_cmpnum(w_key.matches(VECTOR_ONE(tag_testkey_other)), 1);
  assert_cmpnum(w_key.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_key.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_combo w_keyf(tag_testkey_testtext.first,
                              "visual text",
                              values.front(),
                              "key!", values, empty_vector);

  assert_cmpnum(w_keyf.matches(VECTOR_ONE(tag_testkey_other)), 1);
  assert_cmpnum(w_keyf.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_keyf.matches(osm_t::TagMap()), -1);

  values = backup;
  presets_element_combo w_kv(tag_testkey_testtext.first,
                            "visual text",
                            values.front(),
                            "keyvalue", values, empty_vector);

  assert_cmpnum(w_kv.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_kv.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_kv.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_combo w_kvf(tag_testkey_testtext.first,
                             "visual text",
                             values.front(),
                             "keyvalue!", values, empty_vector);

  assert_cmpnum(w_kvf.matches(VECTOR_ONE(tag_testkey_other)), -1);
  assert_cmpnum(w_kvf.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_kvf.matches(osm_t::TagMap()), -1);
}

static void check_combined()
{
  presets_item item(presets_item_t::TY_ALL);

  osm_t::TagMap tags = VECTOR_ONE(tag_neutral_neutral);
  tags.insert(osm_t::TagMap::value_type(tag_testkey_testtext.first, tag_testkey_testtext.second));

  // one that is ignored
  item.widgets.push_back(new presets_element_text("different",
                                                 "different",
                                                 std::string(), "none"));

  assert(!item.matches(tags));

  // another one that reports neutral
  item.widgets.push_back(new presets_element_text("different",
                                                 "different",
                                                 std::string(), "key"));

  assert(!item.matches(tags));

  // one that matches on key
  item.widgets.push_back(new presets_element_text(tag_testkey_testtext.first,
                                                 "different",
                                                 std::string(), "key"));

  assert(item.matches(tags));

  // one that matches on key+value
  item.widgets.push_back(new presets_element_key(tag_testkey_testtext.first,
                                                tag_testkey_testtext.second,
                                                "keyvalue"));

  assert(item.matches(tags));

  // key matches, value not, still neutral
  item.widgets.push_back(new presets_element_key(tag_testkey_other.first,
                                                tag_testkey_other.second,
                                                "keyvalue"));

  assert(item.matches(tags));

  // key matches, value not, fail
  item.widgets.push_back(new presets_element_key(tag_testkey_other.first,
                                                tag_testkey_other.second,
                                                "keyvalue!"));

  assert(!item.matches(tags));
}

int main()
{
  checkTextMatch();
  checkComboMatch();
  check_combined();

  return 0;
}
