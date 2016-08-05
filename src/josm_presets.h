/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
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

#ifndef JOSM_PRESETS_H
#define JOSM_PRESETS_H

#include "info.h"

// set this if _all_ josm icons are in data/presets
// #define JOSM_PATH_ADJUST

typedef enum {
  WIDGET_TYPE_LABEL = 0,
  WIDGET_TYPE_SEPARATOR,
  WIDGET_TYPE_SPACE,
  WIDGET_TYPE_COMBO,
  WIDGET_TYPE_CHECK,
  WIDGET_TYPE_TEXT,
  WIDGET_TYPE_KEY
} presets_widget_type_t;

/* the presets type specifies which item type it is */
/* appropriate for */
#define PRESETS_TYPE_WAY       (1<<0)
#define PRESETS_TYPE_NODE      (1<<1)
#define PRESETS_TYPE_RELATION  (1<<2)
#define PRESETS_TYPE_CLOSEDWAY (1<<3)
#define PRESETS_TYPE_ALL       (0xffff)

typedef struct presets_widget_s {
  presets_widget_type_t type;

  char *key, *text;

  union {
    /* a tag with an arbitrary text value */
    struct {
      char *def;
    } text_w;

    /* a combo box with pre-defined values */
    struct {
      char *def;
      char *values;
    } combo_w;

    /* a key is just a static key */
    struct {
      char *value;
    } key_w;

    /* single checkbox */
    struct {
      gboolean def;
    } check_w;

  };

  struct presets_widget_s *next;
} presets_widget_t;

typedef struct presets_item_s {
  int type;
  char *name, *icon, *link;
  gboolean is_group;

  union {
    presets_widget_t *widget;
    struct presets_item_s *group;
  };

  struct presets_item_s *next;
} presets_item_t;

presets_item_t *josm_presets_load(void);
GtkWidget *josm_build_presets_button(appdata_t *appdata, tag_context_t *tag_context);
void josm_presets_free(presets_item_t *presets);
char *josm_icon_name_adjust(char *name);

#endif // JOSM_PRESETS_H
