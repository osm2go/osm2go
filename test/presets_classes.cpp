#include <josm_presets.h>
#include <josm_presets_p.h>
#include <misc.h>
#include <osm.h>

#include <osm2go_annotations.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdlib>
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
                          std::string(), nullptr);

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
                           nullptr, values, empty_vector, true);

  assert_cmpnum(w_0.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_0.matches(VECTOR_ONE(tag_testkey_testtext)), 0);
  assert_cmpnum(w_0.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_combo w_ign(tag_testkey_testtext.first,
                             "visual text",
                             values.front(),
                             "none", values, empty_vector, true);

  assert_cmpnum(w_ign.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_ign.matches(VECTOR_ONE(tag_testkey_testtext)), 0);
  assert_cmpnum(w_ign.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_combo w_bad(tag_testkey_testtext.first,
                             "visual text",
                             values.front(),
                             "nonsense", values, empty_vector, true);

  assert_cmpnum(w_bad.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_bad.matches(VECTOR_ONE(tag_testkey_testtext)), 0);
  assert_cmpnum(w_bad.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_combo w_key(tag_testkey_testtext.first,
                             "visual text",
                             values.front(),
                             "key", values, empty_vector, true);

  assert_cmpnum(w_key.matches(VECTOR_ONE(tag_testkey_other)), 1);
  assert_cmpnum(w_key.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_key.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_combo w_keyf(tag_testkey_testtext.first,
                              "visual text",
                              values.front(),
                              "key!", values, empty_vector, true);

  assert_cmpnum(w_keyf.matches(VECTOR_ONE(tag_testkey_other)), 1);
  assert_cmpnum(w_keyf.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_keyf.matches(osm_t::TagMap()), -1);

  values = backup;
  presets_element_combo w_kv(tag_testkey_testtext.first,
                            "visual text",
                            values.front(),
                            "keyvalue", values, empty_vector, true);

  assert_cmpnum(w_kv.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_kv.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_kv.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_combo w_kvf(tag_testkey_testtext.first,
                             "visual text",
                             values.front(),
                             "keyvalue!", values, empty_vector, true);

  assert_cmpnum(w_kvf.matches(VECTOR_ONE(tag_testkey_other)), -1);
  assert_cmpnum(w_kvf.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_kvf.matches(osm_t::TagMap()), -1);
}

static void checkMultiMatch()
{
  static osm_t::TagMap::value_type tag_first("testkey", tag_testkey_testtext.second + ";abc");
  static osm_t::TagMap::value_type tag_middle("testkey", "abc;" + tag_testkey_testtext.second + ";abc");
  static osm_t::TagMap::value_type tag_last("testkey", "abc;" + tag_testkey_testtext.second);
  std::vector<std::string> values;
  values.push_back("nonmatch");
  values.push_back("");
  values.push_back(tag_testkey_testtext.second);
  values.push_back("another nonmatch");
  std::vector<std::string> empty_vector;
  const std::vector<std::string> backup = values;

  presets_element_multiselect w_0(tag_testkey_testtext.first,
                           "visual text",
                           values.front(),
                           nullptr, ';', values, empty_vector, 0);

  assert_cmpnum(w_0.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_0.matches(VECTOR_ONE(tag_testkey_testtext)), 0);
  assert_cmpnum(w_0.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_0.matches(VECTOR_ONE(tag_first)), 0);
  assert_cmpnum(w_0.matches(VECTOR_ONE(tag_middle)), 0);
  assert_cmpnum(w_0.matches(VECTOR_ONE(tag_last)), 0);
  assert_cmpnum(w_0.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_multiselect w_ign(tag_testkey_testtext.first,
                             "visual text",
                             values.front(),
                             "none", ';', values, empty_vector, 0);

  assert_cmpnum(w_ign.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_ign.matches(VECTOR_ONE(tag_testkey_testtext)), 0);
  assert_cmpnum(w_ign.matches(VECTOR_ONE(tag_first)), 0);
  assert_cmpnum(w_ign.matches(VECTOR_ONE(tag_middle)), 0);
  assert_cmpnum(w_ign.matches(VECTOR_ONE(tag_last)), 0);
  assert_cmpnum(w_ign.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_multiselect w_bad(tag_testkey_testtext.first,
                             "visual text",
                             values.front(),
                             "nonsense", ';', values, empty_vector, 0);

  assert_cmpnum(w_bad.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_bad.matches(VECTOR_ONE(tag_testkey_testtext)), 0);
  assert_cmpnum(w_bad.matches(VECTOR_ONE(tag_first)), 0);
  assert_cmpnum(w_bad.matches(VECTOR_ONE(tag_middle)), 0);
  assert_cmpnum(w_bad.matches(VECTOR_ONE(tag_last)), 0);
  assert_cmpnum(w_bad.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_multiselect w_key(tag_testkey_testtext.first,
                             "visual text",
                             values.front(),
                             "key", ';', values, empty_vector, 0);

  assert_cmpnum(w_key.matches(VECTOR_ONE(tag_testkey_other)), 1);
  assert_cmpnum(w_key.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_key.matches(VECTOR_ONE(tag_first)), 1);
  assert_cmpnum(w_key.matches(VECTOR_ONE(tag_middle)), 1);
  assert_cmpnum(w_key.matches(VECTOR_ONE(tag_last)), 1);
  assert_cmpnum(w_key.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_multiselect w_keyf(tag_testkey_testtext.first,
                              "visual text",
                              values.front(),
                              "key!", ';', values, empty_vector, 0);

  assert_cmpnum(w_keyf.matches(VECTOR_ONE(tag_testkey_other)), 1);
  assert_cmpnum(w_keyf.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_keyf.matches(VECTOR_ONE(tag_first)), 1);
  assert_cmpnum(w_keyf.matches(VECTOR_ONE(tag_middle)), 1);
  assert_cmpnum(w_keyf.matches(VECTOR_ONE(tag_last)), 1);
  assert_cmpnum(w_keyf.matches(osm_t::TagMap()), -1);

  values = backup;
  presets_element_multiselect w_kv(tag_testkey_testtext.first,
                            "visual text",
                            values.front(),
                            "keyvalue", ';', values, empty_vector, 0);

  assert_cmpnum(w_kv.matches(VECTOR_ONE(tag_testkey_other)), 0);
  assert_cmpnum(w_kv.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_kv.matches(VECTOR_ONE(tag_first)), 1);
  assert_cmpnum(w_kv.matches(VECTOR_ONE(tag_middle)), 1);
  assert_cmpnum(w_kv.matches(VECTOR_ONE(tag_last)), 1);
  assert_cmpnum(w_kv.matches(osm_t::TagMap()), 0);

  values = backup;
  presets_element_multiselect w_kvf(tag_testkey_testtext.first,
                             "visual text",
                             values.front(),
                             "keyvalue!", ';', values, empty_vector, 0);

  assert_cmpnum(w_kvf.matches(VECTOR_ONE(tag_testkey_other)), -1);
  assert_cmpnum(w_kvf.matches(VECTOR_ONE(tag_testkey_testtext)), 1);
  assert_cmpnum(w_kvf.matches(VECTOR_ONE(tag_first)), 1);
  assert_cmpnum(w_kvf.matches(VECTOR_ONE(tag_middle)), 1);
  assert_cmpnum(w_kvf.matches(VECTOR_ONE(tag_last)), 1);
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

class presets_element_multiselect_test: public presets_element_multiselect {
public:
  presets_element_multiselect_test(char del, std::vector<std::string> vals)
    : presets_element_multiselect("key", "text", std::string(), nullptr, del,
                                  vals, std::vector<std::string>(), 0) {}

  inline std::vector<unsigned int> matchTest(const std::string &preset) const
  { return matchedIndexes(preset); }
};

static void checkMultiSplit()
{
  std::vector<std::string> values;
  values.push_back("aaa");
  values.push_back("bbb");
  values.push_back("ccc");
  values.push_back("ddd");
  const presets_element_multiselect_test test1(';', values);

  for(unsigned int i = 0; i < values.size(); i++) {
    std::vector<unsigned int> res = test1.matchTest(values[i]);
    assert_cmpnum(res.size(), 1);
    assert_cmpnum(res[0], i);

    std::string s = ';' + values[i];
    res = test1.matchTest(s);
    assert_cmpnum(res.size(), 1);
    assert_cmpnum(res[0], i);

    s = values[i] + ';';
    res = test1.matchTest(s);
    assert_cmpnum(res.size(), 1);
    assert_cmpnum(res[0], i);
  }

  std::vector<unsigned int> res = test1.matchTest("aa");
  assert_cmpnum(res.size(), 0);
  res = test1.matchTest("bb");
  assert_cmpnum(res.size(), 0);
  res = test1.matchTest("bb;cc");
  assert_cmpnum(res.size(), 0);

  res = test1.matchTest("aaa;ddd");
  assert_cmpnum(res.size(), 2);
  assert_cmpnum(res[0], 0);
  assert_cmpnum(res[1], 3);

  res = test1.matchTest("aa;ddd;f");
  assert_cmpnum(res.size(), 1);
  assert_cmpnum(res[0], 3);
}

int main()
{
  checkTextMatch();
  checkComboMatch();
  checkMultiMatch();
  checkMultiSplit();
  check_combined();

  return 0;
}
