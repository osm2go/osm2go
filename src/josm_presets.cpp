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

#include "josm_presets.h"
#include "josm_presets_p.h"

#include "osm.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>

#include <strings.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>

unsigned int presets_type_mask(const object_t &obj)
{
  unsigned int r = 0;

  switch(obj.type) {
  case object_t::NODE:
    r = presets_item_t::TY_NODE;
    break;

  case object_t::WAY:
    r = presets_item_t::TY_WAY;

    if(obj.way->is_closed())
      r |= presets_item_t::TY_CLOSED_WAY;

    break;

  case object_t::RELATION:
    r = presets_item_t::TY_RELATION;

    if(obj.relation->is_multipolygon())
      r |= presets_item_t::TY_MULTIPOLYGON;
    break;

  default:
    assert_unreachable();
  }

  return r;
}

/**
 * @brief find the first widget that gives a negative match
 */
struct used_preset_functor {
  const osm_t::TagMap &tags;
  bool &is_interactive;
  bool &hasPositive;   ///< set if a positive match is found at all
  used_preset_functor(const osm_t::TagMap &t, bool &i, bool &m)
    : tags(t), is_interactive(i), hasPositive(m) {}
  bool operator()(const presets_element_t *w);
};

bool used_preset_functor::operator()(const presets_element_t* w)
{
  is_interactive |= w->is_interactive();

  int ret = w->matches(tags);
  hasPositive |= (ret > 0);

  return (ret < 0);
}

/**
 * @brief check if the currently active object uses this preset and the preset is interactive
 */
bool presets_item_t::matches(const osm_t::TagMap &tags, bool interactive) const
{
  bool is_interactive = false;
  bool hasPositive = false;
  used_preset_functor fc(tags, is_interactive, hasPositive);
  if(isItem()) {
    const presets_item * const item = static_cast<const presets_item *>(this);
    if(std::find_if(item->widgets.begin(), item->widgets.end(), fc) != item->widgets.end())
      return false;
  }

  return hasPositive && (is_interactive || !interactive);
}

struct relation_preset_functor {
  const relation_t * const relation;
  const unsigned int typemask;
  const osm_t::TagMap tags;
  const presets_item **result;
  relation_preset_functor(const relation_t *rl, const presets_item **rs)
    : relation(rl)
    , typemask(presets_item_t::TY_RELATION | (rl->is_multipolygon() ? presets_item_t::TY_MULTIPOLYGON : 0))
    , tags(rl->tags.asMap())
    , result(rs)
  {
  }
  bool operator()(const presets_item_t *item);
};

bool relation_preset_functor::operator()(const presets_item_t *item)
{
  if(item->type & presets_item_t::TY_GROUP) {
    const presets_item_group * const gr = static_cast<const presets_item_group *>(item);
    const std::vector<presets_item_t *>::const_iterator itEnd = gr->items.end();
    return std::find_if(gr->items.begin(), itEnd, *this) != itEnd;
  }

  if(!(item->type & typemask))
    return false;

  if(item->matches(tags, false)) {
    assert(item->isItem());
    *result = static_cast<const presets_item *>(item);
    return true;
  } else {
    return false;
  }
}

struct role_collect_functor {
  std::set<std::string> &result;
  typedef std::map<std::string, unsigned int> RoleCountMap;
  const RoleCountMap &existing;
  const unsigned int typemask;
  role_collect_functor(std::set<std::string> &r, RoleCountMap &e, unsigned int m)
    : result(r), existing(e), typemask(m) {}
  void operator()(const presets_item::role &role);
};

void role_collect_functor::operator()(const presets_item::role &role)
{
  if(!(typemask & role.type))
    return;

  // check count limit if one is set
  if(role.count > 0) {
    const RoleCountMap::const_iterator it = existing.find(role.name);

    // if the limit of members with that type is already reached do not show it again
    if(it != existing.end() && it->second >= role.count)
      return;
  }

  result.insert(role.name);
}

std::set<std::string> preset_roles(const relation_t *relation, const object_t &obj, const presets_items *presets)
{
  // collect existing roles first
  role_collect_functor::RoleCountMap existingRoles;
  const std::vector<member_t>::const_iterator mitEnd = relation->members.end();
  std::vector<member_t>::const_iterator mit = relation->members.begin();
  while((mit = std::find_if(mit, mitEnd, member_t::has_role)) != mitEnd) {
    existingRoles[mit->role]++;
    mit++;
  }

  std::set<std::string> ret;
  const presets_item *item = nullptr;
  relation_preset_functor fc(relation, &item);
  const std::vector<presets_item_t *>::const_iterator itEnd = presets->items.end();
  role_collect_functor rfc(ret, existingRoles, presets_type_mask(obj));

  if(std::find_if(presets->items.begin(), itEnd, fc) != itEnd)
    std::for_each(item->roles.begin(), item->roles.end(), rfc);

  return ret;
}

bool presets_element_combo::matchValue(const std::string &val) const
{
  return std::find(values.begin(), values.end(), val) != values.end();
}

bool presets_element_key::matchValue(const std::string &val) const
{
  return (value == val);
}

std::string presets_element_key::getValue(presets_element_t::attach_key *akey) const
{
  assert_null(akey);
  return value;
}

bool presets_element_checkbox::matchValue(const std::string &val) const
{
  if(!value_on.empty())
    return (value_on == val);

  return (strcasecmp(val.c_str(), "true") == 0 || strcasecmp(val.c_str(), "yes") == 0);
}
