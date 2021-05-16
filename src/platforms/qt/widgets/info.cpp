/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "info_p.h"

namespace {

const osm_t::TagMap dummyMap;

class info_tag_context_t : public tag_context_t {
public:
  info_tag_context_t(map_t *m, osm_t::ref os, const object_t &o, QDialog *dlg, presets_items *ps);

  map_t * const map;
  osm_t::ref osm;
  presets_items * const presets;
};

} // namespace;

void
tag_context_t::info_tags_replace(const osm_t::TagMap &)
{
  abort();
}

/* edit tags of currently selected node or way or of the relation */
/* given */
bool
info_dialog(osm2go_platform::Widget *parent, map_t *map, osm_t::ref osm, presets_items *presets, object_t &object)
{
  info_tag_context_t context(map, osm, object, qobject_cast<QDialog *>(parent), presets);

  abort();
}

tag_context_t::tag_context_t(const object_t &o, const osm_t::TagMap &t, const osm_t::TagMap &ot, QDialog *dlg)
  : dialog(dlg)
  , object(o)
  , tags(t)
  , originalTags(ot)
{
}

info_tag_context_t::info_tag_context_t(map_t *m, osm_t::ref os, const object_t &o, QDialog *dlg, presets_items *ps)
  : tag_context_t(o, dummyMap, dummyMap, dlg)
  , map(m)
  , osm(os)
  , presets(ps)
{
}
