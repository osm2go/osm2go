/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <osm.h>

#include <optional>

typedef struct _GtkWidget GtkWidget;
class presets_items;

/**
 * @brief select a new role for the given object
 * @returns the new member object
 */
std::optional<member_t> selectObjectRole(GtkWidget *parent, const relation_t *relation, const object_t &object, const presets_items *presets, const char *role);
