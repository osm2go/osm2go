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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef JOSM_PRESETS_P_H
#define JOSM_PRESETS_P_H

#include "josm_presets.h"

#include "osm.h"

#include <gtk/gtk.h>
#include <map>
#include <string>
#include <vector>

#include <osm2go_cpp.h>

enum presets_widget_type_t {
  WIDGET_TYPE_LABEL = 0,
  WIDGET_TYPE_SEPARATOR,
  WIDGET_TYPE_SPACE,
  WIDGET_TYPE_COMBO,
  WIDGET_TYPE_CHECK,
  WIDGET_TYPE_TEXT,
  WIDGET_TYPE_KEY,
  WIDGET_TYPE_LINK,
  WIDGET_TYPE_REFERENCE
};

struct presets_context_t;
struct tag_t;

class presets_widget_t {
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
protected:
  presets_widget_t(presets_widget_type_t t, Match m, const std::string &key = std::string(),
                   const std::string &text = std::string());

  /**
   * @brief check if the tag value matches this item
   */
  virtual bool matchValue(G_GNUC_UNUSED const char *val) const {
    return true;
  }

public:
  virtual ~presets_widget_t() {};

  const presets_widget_type_t type;

  const std::string key;
  const std::string text;
  const Match match;

  virtual bool is_interactive() const;
  static inline bool isInteractive(const presets_widget_t *w) {
    return w->is_interactive();
  }

  virtual GtkWidget *attach(GtkTable *table, guint &row, const char *preset,
                            presets_context_t *context) const;
  virtual std::string getValue(GtkWidget *widget) const;
  virtual guint rows() const = 0;

  /**
   * @brief checks if this widget matches the given tags
   * @param tags the tags of the object
   * @retval -1 negative match
   * @retval 0 no match, but continue searching
   * @retval 1 positive match
   */
  int matches(const osm_t::TagMap &tags) const;
};

/**
 * @brief a tag with an arbitrary text value
 */
class presets_widget_text : public presets_widget_t {
  // no matchValue as it doesn't make sense to match on the value
public:
  presets_widget_text(const std::string &key, const std::string &text,
                      const std::string &deflt, const char *matches);

  const std::string def;

  virtual GtkWidget *attach(GtkTable *table, guint &row, const char *preset,
                            presets_context_t *) const O2G_OVERRIDE;
  virtual std::string getValue(GtkWidget *widget) const O2G_OVERRIDE;
  virtual guint rows() const O2G_OVERRIDE {
    return 1;
  }
};

class presets_widget_separator : public presets_widget_t {
public:
  explicit presets_widget_separator()
    : presets_widget_t(WIDGET_TYPE_SEPARATOR, MatchIgnore) {}

  virtual GtkWidget *attach(GtkTable *table, guint &row, const char *,
                            presets_context_t *) const O2G_OVERRIDE;
  virtual guint rows() const O2G_OVERRIDE {
    return 1;
  }
};

class presets_widget_label : public presets_widget_t {
public:
  explicit presets_widget_label(const std::string &text)
    : presets_widget_t(WIDGET_TYPE_LABEL, MatchIgnore, std::string(), text) {}

  virtual GtkWidget *attach(GtkTable *table, guint &row, const char *,
                            presets_context_t *) const O2G_OVERRIDE;
  virtual guint rows() const O2G_OVERRIDE {
    return 1;
  }
};

/**
 * @brief a combo box with pre-defined values
 */
class presets_widget_combo : public presets_widget_t {
protected:
  virtual bool matchValue(const char *val) const O2G_OVERRIDE;
public:
  presets_widget_combo(const std::string &key, const std::string &text,
                       const std::string &deflt, const char *matches,
                       std::vector<std::string> vals, std::vector<std::string> dvals);

  const std::string def;
  std::vector<std::string> values;
  std::vector<std::string> display_values;

  virtual GtkWidget *attach(GtkTable *table, guint &row, const char *preset,
                            presets_context_t *) const O2G_OVERRIDE;
  virtual std::string getValue(GtkWidget *widget) const O2G_OVERRIDE;
  virtual guint rows() const O2G_OVERRIDE {
    return 1;
  }

  static std::vector<std::string> split_string(const char *str, char delimiter);
};

/**
 * @brief a key is just a static key
 */
class presets_widget_key : public presets_widget_t {
protected:
  virtual bool matchValue(const char *val) const O2G_OVERRIDE;
public:
  presets_widget_key(const std::string &key, const std::string &val, const char *matches);

  const std::string value;
  virtual std::string getValue(GtkWidget *widget) const O2G_OVERRIDE;
  virtual guint rows() const O2G_OVERRIDE {
    return 0;
  }
};

class presets_widget_checkbox : public presets_widget_t {
protected:
  virtual bool matchValue(const char *val) const O2G_OVERRIDE;
public:
  presets_widget_checkbox(const std::string &key, const std::string &text, bool deflt,
                          const char *matches, const std::string &von = std::string());

  const bool def;
  std::string value_on;

  virtual GtkWidget *attach(GtkTable *table, guint &row, const char *preset,
                            presets_context_t *) const O2G_OVERRIDE;
  virtual std::string getValue(GtkWidget *widget) const O2G_OVERRIDE;
  virtual guint rows() const O2G_OVERRIDE {
    return 1;
  }
};

class presets_item;

class presets_widget_reference : public presets_widget_t {
public:
  explicit presets_widget_reference(presets_item *i)
    : presets_widget_t(WIDGET_TYPE_REFERENCE, MatchIgnore), item(i) {}

  presets_item * const item;

  virtual bool is_interactive() const O2G_OVERRIDE;
  virtual guint rows() const O2G_OVERRIDE;
};

class presets_widget_link : public presets_widget_t {
public:
  explicit presets_widget_link()
    : presets_widget_t(WIDGET_TYPE_LINK, MatchIgnore), item(O2G_NULLPTR) {}

  presets_item *item;

  virtual bool is_interactive() const O2G_OVERRIDE {
    return false;
  }
  virtual GtkWidget *attach(GtkTable *table, guint &row, const char *preset,
                            presets_context_t *context) const O2G_OVERRIDE;
  virtual guint rows() const O2G_OVERRIDE {
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

  bool matches(const osm_t::TagMap &tags) const;
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

  std::vector<presets_widget_t *> widgets;
  std::vector<role> roles;

  virtual bool isItem() const O2G_OVERRIDE {
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
  GtkWidget *widget;
  std::vector<presets_item_t *> items;
};

#define LRU_MAX 10	///< how many items we want in presets_items::lru at most

struct presets_items {
  presets_items();
  ~presets_items();
  std::vector<presets_item_t *> items;
  std::vector<presets_item_t *> chunks;
  std::vector<presets_item_t *> lru;

  bool addFile(const std::string &filename, const std::string &basepath);
};

static inline guint widget_rows(guint init, const presets_widget_t *w) {
  return init + w->rows();
}

#endif // JOSM_PRESETS_P_H
