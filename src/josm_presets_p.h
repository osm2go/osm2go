/*
 * Copyright (C) 2016 Rolf Eike Beer <eike@sf-mail.de>.
 *
 * This file is part of OSM2Go.
 *
 * OSM2Go is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OSM2Go is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "josm_presets.h"

#include "osm.h"

#include <map>
#include <string>
#include <vector>

#include <osm2go_cpp.h>

enum presets_element_type_t {
  WIDGET_TYPE_LABEL = 0,
  WIDGET_TYPE_SEPARATOR,
  WIDGET_TYPE_SPACE,
  WIDGET_TYPE_COMBO,
  WIDGET_TYPE_MULTISELECT,
  WIDGET_TYPE_CHECK,
  WIDGET_TYPE_TEXT,
  WIDGET_TYPE_KEY,
  WIDGET_TYPE_LINK,
  WIDGET_TYPE_REFERENCE
};

struct preset_attach_context;

class presets_element_t {
public:
  enum Match {
    MatchIgnore,          ///< "none"
    MatchKey,             ///< "key"
    MatchKey_Force,       ///< "key!"
    MatchKeyValue,        ///< "keyvalue"
    MatchKeyValue_Force   ///< "keyvalue!"
  };

  /**
   * @brief parse the match specification
   * @param matchstring the matchstring from the presets file, may be nullptr
   * @param def default value returned if matchstring is empty or cannot be parsed
   */
  static Match parseMatch(const char *matchstring, Match def = MatchIgnore);

  struct attach_key; ///< return value from attach() that can be used by getValue()
protected:
  presets_element_t(presets_element_type_t t, Match m, const std::string &k = std::string(),
                   const std::string &txt = std::string());

  /**
   * @brief check if the tag value matches this item
   */
  virtual bool matchValue(const std::string &) const {
    return true;
  }

public:
  virtual ~presets_element_t() {}

  const presets_element_type_t type;

  const std::string key;
  const std::string text;
  const Match match;

  virtual bool is_interactive() const;
  static inline bool isInteractive(const presets_element_t *w) {
    return w->is_interactive();
  }

  /**
   * @brief create an entry element for the given element
   * @param attctx attach context (implementation defined)
   * @param preset current value of the tag
   * @returns a key to allow value extraction
   *
   * The return value will be passed to getValue().
   */
  virtual attach_key *attach(preset_attach_context &attctx, const std::string &preset) const;

  /**
   * @brief get the selected value
   * @param akey the key returned by attach()
   * @returns the new value for the tag
   *
   * The default implementation returns an empty string and enforces that akey
   * is nullptr.
   */
  virtual std::string getValue(attach_key *akey) const;

  /**
   * @brief query the number or rows needed for this element
   *
   * This is called for every element before the first call to attach().
   */
  virtual unsigned int rows() const = 0;

  /**
   * @brief checks if this widget matches the given tags
   * @param tags the tags of the object
   * @param interactive if only interactive items should be returned
   * @retval -1 negative match
   * @retval 0 no match, but continue searching
   * @retval 1 positive match
   */
  int matches(const osm_t::TagMap &tags, bool interactive = true) const;
};

/**
 * @brief a tag with an arbitrary text value
 */
class presets_element_text : public presets_element_t {
  // no matchValue as it doesn't make sense to match on the value
public:
  presets_element_text(const std::string &k, const std::string &txt,
                      const std::string &deflt, const char *m);

  const std::string def;

  virtual attach_key *attach(preset_attach_context &attctx, const std::string &preset) const override;
  virtual std::string getValue(attach_key *akey) const override;
  virtual unsigned int rows() const override {
    return 1;
  }
};

class presets_element_separator : public presets_element_t {
public:
  explicit presets_element_separator()
    : presets_element_t(WIDGET_TYPE_SEPARATOR, MatchIgnore) {}

  virtual attach_key *attach(preset_attach_context &attctx, const std::string &) const override;
  virtual unsigned int rows() const override {
    return 1;
  }
};

class presets_element_label : public presets_element_t {
public:
  explicit presets_element_label(const std::string &txt)
    : presets_element_t(WIDGET_TYPE_LABEL, MatchIgnore, std::string(), txt) {}

  virtual attach_key *attach(preset_attach_context &attctx, const std::string &) const override;
  virtual unsigned int rows() const override {
    return 1;
  }
};

/**
 * @brief base class for elements that allow to choose between multiple values
 */
class presets_element_selectable : public presets_element_t {
public:
  presets_element_selectable(presets_element_type_t t, const std::string &k, const std::string &txt,
                        const std::string &deflt, const char *m, std::vector<std::string> vals,
                        std::vector<std::string> dvals, bool canEdit);

  const std::string def;
  std::vector<std::string> values;
  std::vector<std::string> display_values;
  const bool editable;

  static std::vector<std::string> split_string(const char *str, char delimiter);
};

/**
 * @brief a combo box with pre-defined values
 */
class presets_element_combo : public presets_element_selectable {
protected:
  virtual bool matchValue(const std::string &val) const override;
public:
  presets_element_combo(const std::string &k, const std::string &txt,
                        const std::string &deflt, const char *m, std::vector<std::string> vals,
                        std::vector<std::string> dvals, bool canEdit);

  virtual attach_key *attach(preset_attach_context &attctx, const std::string &preset) const override;
  virtual std::string getValue(attach_key *akey) const override;
  virtual unsigned int rows() const override {
    return 1;
  }
};

/**
 * @brief a combo box with pre-defined values
 */
class presets_element_multiselect : public presets_element_selectable {
protected:
  /**
   * @brief check which selection items are matched by preset
   */
  std::vector<unsigned int> matchedIndexes(const std::string &preset) const;

  virtual bool matchValue(const std::string &val) const override;
public:
  presets_element_multiselect(const std::string &k, const std::string &txt,
                              const std::string &deflt, const char *m, char del,
                              std::vector<std::string> vals, std::vector<std::string> dvals, unsigned int rws);

  const char delimiter;
#ifndef FREMANTLE
  const unsigned int rows_height;
#endif

  virtual attach_key *attach(preset_attach_context &attctx, const std::string &preset) const override;
  virtual std::string getValue(attach_key *akey) const override;
  virtual unsigned int rows() const override {
    return 1;
  }
};

/**
 * @brief a key is just a static key
 */
class presets_element_key : public presets_element_t {
protected:
  virtual bool matchValue(const std::string &val) const override;
public:
  presets_element_key(const std::string &k, const std::string &val, const char *m);

  const std::string value;
  virtual std::string getValue(attach_key *akey) const override;
  virtual unsigned int rows() const override {
    return 0;
  }
};

class presets_element_checkbox : public presets_element_t {
protected:
  virtual bool matchValue(const std::string &val) const override;
public:
  presets_element_checkbox(const std::string &k, const std::string &txt, bool deflt,
                          const char *m, const std::string &von = std::string());

  const bool def;
  std::string value_on;

  virtual attach_key *attach(preset_attach_context &attctx, const std::string &preset) const override;
  virtual std::string getValue(attach_key *akey) const override;
  virtual unsigned int rows() const override {
    return 1;
  }
};

class presets_item;

class presets_element_reference : public presets_element_t {
public:
  explicit presets_element_reference(presets_item *i)
    : presets_element_t(WIDGET_TYPE_REFERENCE, MatchIgnore), item(i) {}

  presets_item * const item;

  virtual bool is_interactive() const override;
  virtual unsigned int rows() const override;
};

class presets_element_link : public presets_element_t {
public:
  explicit presets_element_link()
    : presets_element_t(WIDGET_TYPE_LINK, MatchIgnore), item(nullptr) {}

  presets_item *item;

  virtual bool is_interactive() const override {
    return false;
  }
  virtual attach_key *attach(preset_attach_context &attctx, const std::string &preset) const override;
  virtual unsigned int rows() const override {
    return 1;
  }
};

class presets_item_t {
public:
  enum item_type {
    TY_NONE = 0,
    TY_WAY = (1<<0),
    TY_NODE = (1<<1),
    TY_RELATION = (1<<2),
    TY_CLOSED_WAY = (1<<3),
    TY_MULTIPOLYGON = (1<<4),
    TY_ALL = (0xffff),
    TY_SEPARATOR = (1 << 16),
    TY_GROUP = (1 << 17)
  };

protected:
  explicit presets_item_t(unsigned int t)
    : type(t) {}
public:
  virtual ~presets_item_t() {}

  virtual bool isItem() const {
    return false;
  }

  const unsigned int type;

  bool matches(const osm_t::TagMap &tags, bool interactive = true) const;
};

class presets_item_named : public presets_item_t {
public:
  explicit presets_item_named(unsigned int t, const std::string &n = std::string(),
                       const std::string &ic = std::string())
    : presets_item_t(t), name(n), icon(ic) {}

  const std::string name, icon;
};

class presets_item : public presets_item_named {
public:
  struct role {
    role(const std::string &n, unsigned int t, unsigned int cnt) : name(n), type(t), count(cnt) {}
    std::string name;
    unsigned int type;  ///< object types that may have this role
    unsigned int count; ///< maximum amount of members with that role
  };

  explicit presets_item(unsigned int t, const std::string &n = std::string(),
               const std::string &ic = std::string(), bool edname = false)
    : presets_item_named(t, n, ic), addEditName(edname) {}
  virtual ~presets_item();

  std::vector<presets_element_t *> widgets;
  std::vector<role> roles;

  virtual bool isItem() const override {
    return true;
  }

  std::string link;
  bool addEditName;
};

class presets_item_separator : public presets_item_t {
public:
  explicit presets_item_separator() : presets_item_t(TY_SEPARATOR) {}
};

class presets_item_group : public presets_item_named {
public:
  presets_item_group(const unsigned int types, presets_item_group *p,
                     const std::string &n = std::string(),
                     const std::string &ic = std::string());
  virtual ~presets_item_group();

  presets_item_group * const parent;
  std::vector<presets_item_t *> items;
};

#define LRU_MAX 10	///< how many items we want in presets_items::lru at most

class presets_items_internal : public presets_items {
public:
  presets_items_internal();
  ~presets_items_internal();
  std::vector<presets_item_t *> items;
  std::vector<presets_item_t *> chunks;
  std::vector<presets_item_t *> lru;

  bool addFile(const std::string &filename, const std::string &basepath, int basefd);

  virtual std::set<std::string> roles(const relation_t *relation, const object_t &obj) const override;
};

static inline unsigned int widget_rows(unsigned int init, const presets_element_t *w) {
  return init + w->rows();
}

unsigned int presets_type_mask(const object_t &obj);
