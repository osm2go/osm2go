#include <josm_presets.h>
#include <josm_presets_p.h>
#include <misc.h>
#include <osm.h>

#include <algorithm>
#include <cstdlib>
#include <cerrno>
#include <glib.h>
#include <iostream>
#include <string>

#define TESTTAG(a, b) static stag_t tag_##a##_##b(#a, #b)

TESTTAG(testkey, other);
TESTTAG(testkey, testtext);
TESTTAG(neutral, neutral);

#if __cplusplus >= 201103L
# define VECTOR_ONE(a) std::vector<stag_t *>{ const_cast<stag_t *>(&a) }
#else
# define VECTOR_ONE(a) std::vector<stag_t *>(1, const_cast<stag_t *>(&(a)))
#endif

static bool checkTextMatch()
{
  presets_widget_text w_0(tag_testkey_testtext.key,
                          tag_testkey_testtext.value,
                          std::string(), O2G_NULLPTR);

  g_assert_cmpint(w_0.matches(VECTOR_ONE(tag_testkey_other)), ==, 0);
  g_assert_cmpint(w_0.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 0);
  g_assert_cmpint(w_0.matches(std::vector<stag_t *>()), ==, 0);

  presets_widget_text w_ign(tag_testkey_testtext.key,
                            tag_testkey_testtext.value,
                            std::string(), "none");

  g_assert_cmpint(w_ign.matches(VECTOR_ONE(tag_testkey_other)), ==, 0);
  g_assert_cmpint(w_ign.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 0);
  g_assert_cmpint(w_ign.matches(std::vector<stag_t *>()), ==, 0);

  presets_widget_text w_bad(tag_testkey_testtext.key,
                            tag_testkey_testtext.value,
                            std::string(), "nonsense");

  g_assert_cmpint(w_bad.matches(VECTOR_ONE(tag_testkey_other)), ==, 0);
  g_assert_cmpint(w_bad.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 0);
  g_assert_cmpint(w_bad.matches(std::vector<stag_t *>()), ==, 0);

  presets_widget_text w_key(tag_testkey_testtext.key,
                            tag_testkey_testtext.value,
                            std::string(), "key");

  g_assert_cmpint(w_key.matches(VECTOR_ONE(tag_testkey_other)), ==, 1);
  g_assert_cmpint(w_key.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 1);
  g_assert_cmpint(w_key.matches(std::vector<stag_t *>()), ==, 0);

  presets_widget_text w_keyf(tag_testkey_testtext.key,
                             tag_testkey_testtext.value,
                             std::string(), "key!");

  g_assert_cmpint(w_keyf.matches(VECTOR_ONE(tag_testkey_other)), ==, 1);
  g_assert_cmpint(w_keyf.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 1);
  g_assert_cmpint(w_keyf.matches(std::vector<stag_t *>()), ==, -1);

  presets_widget_text w_kv(tag_testkey_testtext.key,
                           tag_testkey_testtext.value,
                           std::string(), "keyvalue");

  g_assert_cmpint(w_kv.matches(VECTOR_ONE(tag_testkey_other)), ==, 1);
  g_assert_cmpint(w_kv.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 1);
  g_assert_cmpint(w_kv.matches(std::vector<stag_t *>()), ==, 0);

  presets_widget_text w_kvf(tag_testkey_testtext.key,
                            tag_testkey_testtext.value,
                            std::string(), "keyvalue!");

  g_assert_cmpint(w_kvf.matches(VECTOR_ONE(tag_testkey_other)), ==, 1);
  g_assert_cmpint(w_kvf.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 1);
  g_assert_cmpint(w_kvf.matches(std::vector<stag_t *>()), ==, -1);

  return true;
}

static bool checkComboMatch()
{
  std::vector<std::string> values;
  values.push_back("nonmatch");
  values.push_back("");
  values.push_back(tag_testkey_testtext.value);
  values.push_back("another nonmatch");
  std::vector<std::string> empty_vector;
  const std::vector<std::string> backup = values;

  presets_widget_combo w_0(tag_testkey_testtext.key,
                           "visual text",
                           values.front(),
                           O2G_NULLPTR, values, empty_vector);

  g_assert_cmpint(w_0.matches(VECTOR_ONE(tag_testkey_other)), ==, 0);
  g_assert_cmpint(w_0.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 0);
  g_assert_cmpint(w_0.matches(std::vector<stag_t *>()), ==, 0);

  values = backup;
  presets_widget_combo w_ign(tag_testkey_testtext.key,
                             "visual text",
                             values.front(),
                             "none", values, empty_vector);

  g_assert_cmpint(w_ign.matches(VECTOR_ONE(tag_testkey_other)), ==, 0);
  g_assert_cmpint(w_ign.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 0);
  g_assert_cmpint(w_ign.matches(std::vector<stag_t *>()), ==, 0);

  values = backup;
  presets_widget_combo w_bad(tag_testkey_testtext.key,
                             "visual text",
                             values.front(),
                             "nonsense", values, empty_vector);

  g_assert_cmpint(w_bad.matches(VECTOR_ONE(tag_testkey_other)), ==, 0);
  g_assert_cmpint(w_bad.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 0);
  g_assert_cmpint(w_bad.matches(std::vector<stag_t *>()), ==, 0);

  values = backup;
  presets_widget_combo w_key(tag_testkey_testtext.key,
                             "visual text",
                             values.front(),
                             "key", values, empty_vector);

  g_assert_cmpint(w_key.matches(VECTOR_ONE(tag_testkey_other)), ==, 1);
  g_assert_cmpint(w_key.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 1);
  g_assert_cmpint(w_key.matches(std::vector<stag_t *>()), ==, 0);

  values = backup;
  presets_widget_combo w_keyf(tag_testkey_testtext.key,
                              "visual text",
                              values.front(),
                              "key!", values, empty_vector);

  g_assert_cmpint(w_keyf.matches(VECTOR_ONE(tag_testkey_other)), ==, 1);
  g_assert_cmpint(w_keyf.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 1);
  g_assert_cmpint(w_keyf.matches(std::vector<stag_t *>()), ==, -1);

  values = backup;
  presets_widget_combo w_kv(tag_testkey_testtext.key,
                            "visual text",
                            values.front(),
                            "keyvalue", values, empty_vector);

  g_assert_cmpint(w_kv.matches(VECTOR_ONE(tag_testkey_other)), ==, 0);
  g_assert_cmpint(w_kv.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 1);
  g_assert_cmpint(w_kv.matches(std::vector<stag_t *>()), ==, 0);

  values = backup;
  presets_widget_combo w_kvf(tag_testkey_testtext.key,
                             "visual text",
                             values.front(),
                             "keyvalue!", values, empty_vector);

  g_assert_cmpint(w_kvf.matches(VECTOR_ONE(tag_testkey_other)), ==, -1);
  g_assert_cmpint(w_kvf.matches(VECTOR_ONE(tag_testkey_testtext)), ==, 1);
  g_assert_cmpint(w_kvf.matches(std::vector<stag_t *>()), ==, -1);

  return true;
}

static bool check_combined()
{
  presets_item item(presets_item_t::TY_ALL);

  std::vector<stag_t *> tags = VECTOR_ONE(tag_neutral_neutral);
  tags.push_back(const_cast<stag_t *>(&tag_testkey_testtext));

  // one that is ignored
  item.widgets.push_back(new presets_widget_text("different",
                                                 "different",
                                                 std::string(), "none"));

  g_assert_false(item.matches(tags));

  // another one that reports neutral
  item.widgets.push_back(new presets_widget_text("different",
                                                 "different",
                                                 std::string(), "key"));

  g_assert_false(item.matches(tags));

  // one that matches on key
  item.widgets.push_back(new presets_widget_text(tag_testkey_testtext.key,
                                                 "different",
                                                 std::string(), "key"));

  g_assert_true(item.matches(tags));

  // one that matches on key+value
  item.widgets.push_back(new presets_widget_key(tag_testkey_testtext.key,
                                                tag_testkey_testtext.value,
                                                "keyvalue"));

  g_assert_true(item.matches(tags));

  // key matches, value not, still neutral
  item.widgets.push_back(new presets_widget_key(tag_testkey_other.key,
                                                tag_testkey_other.value,
                                                "keyvalue"));

  g_assert_true(item.matches(tags));

  // key matches, value not, fail
  item.widgets.push_back(new presets_widget_key(tag_testkey_other.key,
                                                tag_testkey_other.value,
                                                "keyvalue!"));

  g_assert_false(item.matches(tags));

  return true;
}

int main()
{
  if(!checkTextMatch())
    return 1;

  if(!checkComboMatch())
    return 1;

  if(!check_combined())
    return 1;

  return 0;
}
