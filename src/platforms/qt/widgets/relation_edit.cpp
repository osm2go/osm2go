/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <object_dialogs.h>

void relation_membership_dialog(osm2go_platform::Widget *, const presets_items *,
                                osm_t::ref, object_t &)
{
  abort();
}

bool
relation_show_members(QWidget *, relation_t *, osm_t::ref, const presets_items *)
{
  abort();
}

void relation_list(osm2go_platform::Widget *, map_t *, osm_t::ref, presets_items *)
{
  abort();
}
