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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef DIFF_H
#define DIFF_H

#include "osm.h"

#include <gtk/gtk.h>
#include <libxml/tree.h>

struct appdata_t;
struct project_t;

enum diff_restore_results {
  DIFF_RESTORED = 0, ///< a diff was present and has successfully been restored
  DIFF_NONE_PRESENT = (1 << 0), ///< no diff file was present at all
  DIFF_INVALID = (1 << 1), ///< the diff file was completely invalid
  DIFF_PROJECT_MISMATCH = (1 << 3), ///< the name given in the diff does not match the given project
  DIFF_ELEMENTS_IGNORED = (1 << 4), ///< parts of the diff file were invalid and have been ignored
  DIFF_HAS_HIDDEN = (1 << 5), ///< some of the object have the hidden flag set
};

void diff_save(const project_t *project, const osm_t *osm) __attribute__((nonnull(1,2)));
unsigned int diff_restore_file(GtkWidget *window, const project_t *project, osm_t *osm) __attribute__((nonnull(2,3)));
void diff_restore(appdata_t &appdata);
bool diff_present(const project_t *project);
void diff_remove(const project_t *project);
bool diff_is_clean(const osm_t *osm, bool honor_hidden_flags);

/**
 * @brief create an empty osmChange document
 *
 * This sets the root node (osmChange) and it's properties, but does not add any content.
 */
xmlDocPtr osmchange_init();

/**
 * @brief generate a "delete" XML section in OsmChange format
 * @param osm the OSM data
 * @param xml_node the parent node (usually <osmChange>)
 * @param changeset the changeset id
 */
void osmchange_delete(const osm_t::dirty_t &dirty, xmlNodePtr xml_node, const char *changeset);

#endif // DIFF_H
