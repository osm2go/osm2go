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
public:
  presets_widget_t(presets_widget_type_t t);
  ~presets_widget_t();

  const presets_widget_type_t type;

  xmlChar *key, *text;

  union {
    /* a tag with an arbitrary text value */
    struct {
      xmlChar *def;
    } text_w;

    /* a combo box with pre-defined values */
    struct {
      xmlChar *def;
      xmlChar *values;
    } combo_w;

    /* a key is just a static key */
    struct {
      xmlChar *value;
    } key_w;

    /* single checkbox */
    struct {
      bool def;
    } check_w;

  };

  bool is_interactive() const;
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
