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

#include <gtk/gtk.h>
#include <map>
#include <string>
#include <vector>

enum presets_widget_type_t {
  WIDGET_TYPE_LABEL = 0,
  WIDGET_TYPE_SEPARATOR,
  WIDGET_TYPE_SPACE,
  WIDGET_TYPE_COMBO,
  WIDGET_TYPE_CHECK,
  WIDGET_TYPE_TEXT,
  WIDGET_TYPE_KEY,
  WIDGET_TYPE_REFERENCE
};

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

  virtual GtkWidget *attach(GtkTable *table, guint &row, const char *preset) const;
  virtual const char *getValue(GtkWidget *widget) const;
  virtual guint rows() const = 0;

  /**
   * @brief checks if this widget matches the given tags
   * @param tags the tags of the object
   * @retval -1 negative match
   * @retval 0 no match, but continue searching
   * @retval 1 positive match
   */
  int matches(const std::vector<tag_t *> &tags) const;
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

  virtual GtkWidget *attach(GtkTable *table, guint &row, const char *preset) const;
  virtual const char *getValue(GtkWidget *widget) const;
  virtual guint rows() const {
    return 1;
  }
};

class presets_widget_separator : public presets_widget_t {
public:
  explicit presets_widget_separator()
    : presets_widget_t(WIDGET_TYPE_SEPARATOR, MatchIgnore) {}

  virtual GtkWidget *attach(GtkTable *table, guint &row, const char *) const;
  virtual guint rows() const {
    return 1;
  }
};

class presets_widget_label : public presets_widget_t {
public:
  explicit presets_widget_label(const std::string &text)
    : presets_widget_t(WIDGET_TYPE_LABEL, MatchIgnore, std::string(), text) {}

  virtual GtkWidget *attach(GtkTable *table, guint &row, const char *) const;
  virtual guint rows() const {
    return 1;
  }
};

/**
 * @brief a combo box with pre-defined values
 */
class presets_widget_combo : public presets_widget_t {
protected:
  virtual bool matchValue(const char *val) const;
public:
  presets_widget_combo(const std::string &key, const std::string &text,
                       const std::string &deflt, const char *matches,
                       std::vector<std::string> vals, std::vector<std::string> dvals);

  const std::string def;
  std::vector<std::string> values;
  std::vector<std::string> display_values;

  virtual GtkWidget *attach(GtkTable *table, guint &row, const char *preset) const;
  virtual const char *getValue(GtkWidget *widget) const;
  virtual guint rows() const {
    return 1;
  }

  static std::vector<std::string> split_string(const char *str, char delimiter);
};

/**
 * @brief a key is just a static key
 */
class presets_widget_key : public presets_widget_t {
protected:
  virtual bool matchValue(const char *val) const;
public:
  presets_widget_key(const std::string &key, const std::string &val, const char *matches);

  const std::string value;
  virtual const char *getValue(GtkWidget *widget) const;
  virtual guint rows() const {
    return 0;
  }
};

class presets_widget_checkbox : public presets_widget_t {
protected:
  virtual bool matchValue(const char *val) const;
public:
  presets_widget_checkbox(const std::string &key, const std::string &text, bool deflt,
                          const char *matches, const std::string &von = std::string());

  const bool def;
  std::string value_on;

  virtual GtkWidget *attach(GtkTable *table, guint &row, const char *preset) const;
  virtual const char *getValue(GtkWidget *widget) const;
  virtual guint rows() const {
    return 1;
  }
};

class presets_item;

class presets_widget_reference : public presets_widget_t {
public:
  presets_widget_reference(presets_item *i)
    : presets_widget_t(WIDGET_TYPE_REFERENCE, MatchIgnore), item(i) {}

  presets_item const *item;

  virtual bool is_interactive() const;
  virtual guint rows() const;
};

class presets_item_t {
public:
  enum item_type {
    TY_NONE = 0,
    TY_WAY = PRESETS_TYPE_WAY,
    TY_NODE = PRESETS_TYPE_NODE,
    TY_RELATION = PRESETS_TYPE_RELATION,
    TY_CLOSED_WAY = PRESETS_TYPE_CLOSEDWAY,
    TY_ALL = PRESETS_TYPE_ALL,
    TY_SEPARATOR = (1 << 16),
    TY_GROUP = (1 << 17)
  };

protected:
  presets_item_t(unsigned int t)
    : type(t) {}
public:
  virtual ~presets_item_t();

  const unsigned int type;

  std::vector<presets_widget_t *> widgets;

  bool matches(const std::vector<tag_t *> &tags) const;
};

class presets_item_visible : public presets_item_t {
public:
  presets_item_visible(unsigned int t, const std::string &n = std::string(),
                       const std::string &ic = std::string())
    : presets_item_t(t), name(n), icon(ic) {}

  std::string name, icon;
};

class presets_item : public presets_item_visible {
public:
  presets_item(unsigned int t, const std::string &n = std::string(),
               const std::string &ic = std::string(), bool edname = false)
    : presets_item_visible(t, n, ic), addEditName(edname) {}

  std::string link;
  bool addEditName;
};

class presets_item_separator : public presets_item_t {
public:
  presets_item_separator() : presets_item_t(TY_SEPARATOR) {}
};

class presets_item_group : public presets_item_visible {
public:
  presets_item_group(const unsigned int types, presets_item_group *p,
                     const std::string &n = std::string(),
                     const std::string &ic = std::string());
  virtual ~presets_item_group();

  presets_item_group * const parent;
  GtkWidget *widget;
  std::vector<presets_item_t *> items;
};

struct presets_items {
  ~presets_items();
  std::vector<presets_item_t *> items;
  std::vector<presets_item_t *> chunks;
};

static inline bool is_widget_interactive(const presets_widget_t *w) {
  return w->is_interactive();
}

static inline guint widget_rows(guint init, const presets_widget_t *w) {
  return init + w->rows();
}

#endif // JOSM_PRESETS_P_H
