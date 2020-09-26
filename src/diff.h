/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "osm.h"
#include "project.h"

#include <libxml/tree.h>

#include <osm2go_platform.h>

class MainUi;

enum diff_restore_results {
  DIFF_RESTORED = 0, ///< a diff was present and has successfully been restored
  DIFF_NONE_PRESENT = (1 << 0), ///< no diff file was present at all
  DIFF_INVALID = (1 << 1), ///< the diff file was completely invalid
  DIFF_PROJECT_MISMATCH = (1 << 3), ///< the name given in the diff does not match the given project
  DIFF_ELEMENTS_IGNORED = (1 << 4), ///< parts of the diff file were invalid and have been ignored
  DIFF_HAS_HIDDEN = (1 << 5), ///< some of the object have the hidden flag set
};

void diff_restore(project_t::ref project, MainUi *uicontrol);

/**
 * @brief move the diff from one project to another
 * @param oldproj the source project
 * @param nproj the destination project
 *
 * The diff file is removed from the old project.
 */
bool diff_rename(project_t::ref oldproj, project_t *nproj);

/**
 * @brief create an empty osmChange document
 *
 * This sets the root node (osmChange) and it's properties, but does not add any content.
 */
xmlDocPtr osmchange_init();

/**
 * @brief generate a "delete" XML section in OsmChange format
 * @param dirty the OSM objects to delete
 * @param xml_node the parent node (usually <osmChange>)
 * @param changeset the changeset id
 */
void osmchange_delete(const osm_t::dirty_t &dirty, xmlNodePtr xml_node, const char *changeset);
