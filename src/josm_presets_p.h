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
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <vector>

enum presets_widget_type_t {
  WIDGET_TYPE_LABEL = 0,
  WIDGET_TYPE_SEPARATOR,
  WIDGET_TYPE_SPACE,
  WIDGET_TYPE_COMBO,
  WIDGET_TYPE_CHECK,
  WIDGET_TYPE_TEXT,
  WIDGET_TYPE_KEY
};

class presets_widget_t {
protected:
  presets_widget_t(presets_widget_type_t t, xmlChar *key = 0, xmlChar *text = 0);
public:
  virtual ~presets_widget_t();

  const presets_widget_type_t type;

  xmlChar * const key;
  xmlChar * const text;

  bool is_interactive() const;

  virtual GtkWidget *attach(GtkTable *table, int row, const char *preset) const;
  virtual const char *getValue(GtkWidget *widget) const;
};

/**
 * @brief a tag with an arbitrary text value
 */
class presets_widget_text : public presets_widget_t {
public:
  presets_widget_text(xmlChar *key, xmlChar *text, xmlChar *deflt);
  virtual ~presets_widget_text();

  xmlChar * const def;

  virtual GtkWidget *attach(GtkTable *table, int row, const char *preset) const;
  virtual const char *getValue(GtkWidget *widget) const;
};

class presets_widget_separator : public presets_widget_t {
public:
  explicit presets_widget_separator()
    : presets_widget_t(WIDGET_TYPE_SEPARATOR) {}

  virtual GtkWidget *attach(GtkTable *table, int row, const char *preset) const;
};

class presets_widget_label : public presets_widget_t {
public:
  explicit presets_widget_label(xmlChar *text)
    : presets_widget_t(WIDGET_TYPE_LABEL, 0, text) {}

  virtual GtkWidget *attach(GtkTable *table, int row, const char *preset) const;
};

/**
 * @brief a combo box with pre-defined values
 */
class presets_widget_combo : public presets_widget_t {
public:
  presets_widget_combo(xmlChar *key, xmlChar *text, xmlChar *deflt, xmlChar *vals);
  virtual ~presets_widget_combo();

  xmlChar * const def;
  xmlChar * const values;

  virtual GtkWidget *attach(GtkTable *table, int row, const char *preset) const;
  virtual const char *getValue(GtkWidget *widget) const;
};

/**
 * @brief a key is just a static key
 */
class presets_widget_key : public presets_widget_t {
public:
  presets_widget_key(xmlChar *key, xmlChar *val);
  virtual ~presets_widget_key();

  xmlChar * const value;
  virtual const char *getValue(GtkWidget *widget) const;
};

class presets_widget_checkbox : public presets_widget_t {
public:
  presets_widget_checkbox(xmlChar *key, xmlChar *text, bool deflt);

  const bool def;

  virtual GtkWidget *attach(GtkTable *table, int row, const char *preset) const;
  virtual const char *getValue(GtkWidget *widget) const;
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
};

class presets_item_visible : public presets_item_t {
public:
  presets_item_visible(unsigned int t)
    : presets_item_t(t), name(0), icon(0) {}
  virtual ~presets_item_visible()
  { xmlFree(name); xmlFree(icon); }

  xmlChar *name, *icon;
};

class presets_item : public presets_item_visible {
public:
  presets_item(unsigned int t)
    : presets_item_visible(t), link(0), addEditName(false) {}
  virtual ~presets_item()
  { xmlFree(link); }

  xmlChar *link;
  bool addEditName;
};

class presets_item_separator : public presets_item_t {
public:
  presets_item_separator() : presets_item_t(TY_SEPARATOR) {}
};

class presets_item_group : public presets_item_visible {
public:
  presets_item_group(const unsigned int types, presets_item_group *p)
    : presets_item_visible(types | TY_GROUP), parent(p), widget(0) {}
  virtual ~presets_item_group();

  presets_item_group * const parent;
  GtkWidget *widget;
  std::vector<presets_item_t *> items;
};

struct presets_items {
  ~presets_items();
  std::vector<presets_item_t *> items;
};

#endif // JOSM_PRESETS_P_H
