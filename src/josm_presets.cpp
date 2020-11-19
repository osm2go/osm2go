/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "josm_presets.h"
#include "josm_presets_p.h"

#include "osm.h"
#include "osm_objects.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <unordered_map>

#include <strings.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>

unsigned int presets_type_mask(const object_t &obj)
{
  unsigned int r;

  switch(obj.type) {
  case object_t::NODE:
    r = presets_item_t::TY_NODE;
    break;

  case object_t::WAY:
    if(static_cast<way_t *>(obj)->is_closed())
      r = presets_item_t::TY_CLOSED_WAY;
    else
      r = presets_item_t::TY_WAY;

    break;

  case object_t::RELATION:
    r = presets_item_t::TY_RELATION;

    if(static_cast<relation_t *>(obj)->is_multipolygon())
      r |= presets_item_t::TY_MULTIPOLYGON;
    break;

  default:
    assert_unreachable();
  }

  return r;
}

namespace {

/**
 * @brief find the first widget that gives a negative match
 */
struct used_preset_functor {
  const osm_t::TagMap &tags;
  bool &is_interactive;
  bool &hasPositive;   ///< set if a positive match is found at all
  inline used_preset_functor(const osm_t::TagMap &t, bool &i, bool &m)
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

} // namespace

/**
 * @brief check if the currently active object uses this preset and the preset is interactive
 */
bool presets_item_t::matches(const osm_t::TagMap &tags, bool interactive) const
{
  bool is_interactive = false;
  bool hasPositive = false;
  used_preset_functor fc(tags, is_interactive, hasPositive);
  if(isItem()) {
    const std::vector<presets_element_t *> &widgets = static_cast<const presets_item *>(this)->widgets;
    if(std::any_of(widgets.begin(), widgets.end(), fc))
      return false;
  }

  return hasPositive && (is_interactive || !interactive);
}

namespace {

struct relation_preset_functor {
  const relation_t * const relation;
  const unsigned int typemask;
  const osm_t::TagMap tags;
  const presets_item **result;
  inline relation_preset_functor(const relation_t *rl, const presets_item **rs)
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
    const std::vector<presets_item_t *> &items = static_cast<const presets_item_group *>(item)->items;
    return std::any_of(items.begin(), items.end(), *this);
  }

  if(!(item->type & typemask))
    return false;

  // This coule either be an item, a group, or a separator. Groups were already handled,
  // separators must not have a matching typemask.
  assert(item->isItem());

  // When searching for a relation this may end up matching als other items, usually because
  // the relation type is multipolygon. The other matches usually would also affect ways, so
  // they don't have roles inside the item, so just skip over them.
  if (static_cast<const presets_item *>(item)->roles.empty())
    return false;

  if(item->matches(tags, false)) {
    *result = static_cast<const presets_item *>(item);
    return true;
  } else {
    return false;
  }
}

struct role_collect_functor {
  std::set<std::string> &result;
  typedef std::unordered_map<std::string, unsigned int> RoleCountMap;
  const RoleCountMap &existing;
  const unsigned int typemask;
  inline role_collect_functor(std::set<std::string> &r, RoleCountMap &e, unsigned int m)
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

} // namespace

std::set<std::string> presets_items_internal::roles(const relation_t *relation, const object_t &obj) const
{
  std::set<std::string> ret;

  // members that are only references can't be really checked, so just return an empty list
  if (!obj.is_real())
    return ret;

  // collect existing roles first
  role_collect_functor::RoleCountMap existingRoles;
  const std::vector<member_t>::const_iterator mitEnd = relation->members.end();
  std::vector<member_t>::const_iterator mit = relation->members.begin();
  while((mit = std::find_if(mit, mitEnd, member_t::has_role)) != mitEnd) {
    existingRoles[mit->role]++;
    mit++;
  }

  const presets_item *item = nullptr;
  relation_preset_functor fc(relation, &item);
  const std::vector<presets_item_t *>::const_iterator itEnd = items.end();
  role_collect_functor rfc(ret, existingRoles, presets_type_mask(obj));

  if(std::any_of(items.begin(), itEnd, fc))
    std::for_each(item->roles.begin(), item->roles.end(), rfc);

  return ret;
}

bool presets_element_combo::matchValue(const std::string &val) const
{
  return std::find(values.begin(), values.end(), val) != values.end();
}

std::vector<unsigned int> presets_element_multiselect::matchedIndexes(const std::string &preset) const
{
  std::vector<unsigned int> indexes;

  if(preset.empty())
    return indexes;

  std::vector<std::string> parts;
  std::string::size_type pos = preset.find(delimiter);
  const std::vector<std::string>::const_iterator itEnd = values.end();
  const std::vector<std::string>::const_iterator itBegin = values.begin();

  std::vector<std::string>::const_iterator it;
  if(pos == std::string::npos) {
    it = std::find(itBegin, itEnd, preset);
  } else {
    // the first one
    it = std::find(itBegin, itEnd, preset.substr(0, pos));
    if(it != itEnd)
      indexes.push_back(std::distance(itBegin, it));
    // skip first delimiter
    pos++;
    // all indermediate ones
    std::string::size_type newpos = preset.find(delimiter, pos);
    while(newpos != std::string::npos) {
      it = std::find(itBegin, itEnd, preset.substr(pos, newpos - pos));
      if(it != itEnd)
        indexes.push_back(std::distance(itBegin, it));
      pos = newpos + 1;
      newpos = preset.find(delimiter, pos);
    }
    // the last one
    it = std::find(itBegin, itEnd, preset.substr(pos));
  }

  if(it != itEnd)
    indexes.push_back(std::distance(itBegin, it));

  return indexes;
}

bool presets_element_multiselect::matchValue(const std::string &val) const
{
  return !matchedIndexes(val).empty();
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

std::string presets_element_checkbox::getValue(presets_element_t::attach_key *akey) const
{
  return widgetValue(akey) ?
         (value_on.empty() ? "yes" : value_on) : std::string();
}
