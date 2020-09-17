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

#define _DEFAULT_SOURCE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "osm.h"
#include "osm_p.h"

#include "osm_objects.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_platform.h>

namespace {

class trstring_or_key {
#ifdef TRSTRING_NATIVE_TYPE_IS_TRSTRING
  trstring::native_type tval;
#else
  struct {
    inline void clear() const {}
    inline bool isEmpty() const { return true; }
    inline std::string toStdString() const { return std::string(); }
  } tval;
#endif
  const char *kval;
public:
  explicit trstring_or_key(const char *k = nullptr) : kval(k) {}

#ifdef TRSTRING_NATIVE_TYPE_IS_TRSTRING
  inline trstring_or_key &operator=(trstring::native_type_arg n)
  {
    tval = std::move(n);
    kval = nullptr;
    return *this;
  }
#endif

  inline trstring_or_key &operator=(const char *k)
  {
    tval.clear();
    kval = k;
    return *this;
  }

  inline operator bool() const
  {
    return !tval.isEmpty() || kval != nullptr;
  }

  inline operator std::string() const
  {
    return kval != nullptr ? kval : tval.toStdString();
  }
};

class typed_relation_member_functor {
  const member_t member;
  const char * const type;
public:
  inline typed_relation_member_functor(const char *t, const char *r, const object_t &o)
    : member(o, r), type(value_cache.insert(t)) {}
  bool operator()(const std::pair<item_id_t, relation_t *> &it) const
  { return it.second->tags.get_value("type") == type &&
           std::find(it.second->members.begin(), it.second->members.end(), member) != it.second->members.end(); }
};

class pt_relation_member_functor {
  const member_t member;
  const char * const type;
  const char * const stop_area;
public:
  inline pt_relation_member_functor(const char *r, const object_t &o)
    : member(o, r), type(value_cache.insert("public_transport"))
    , stop_area(value_cache.insert("stop_area")) {}
  bool operator()(const std::pair<item_id_t, relation_t *> &it) const
  { return it.second->tags.get_value("type") == type &&
           it.second->tags.get_value("public_transport") == stop_area &&
           std::find(it.second->members.begin(), it.second->members.end(), member) != it.second->members.end(); }
};

/**
 * @brief remove underscores from string and replace them by spaces
 *
 * Tags usually have underscores in them, but to display this to the user a version
 * with spaces looks nicer.
 */
inline void clean_underscores_inplace(std::string &s)
{
  std::replace(s.begin(), s.end(), '_', ' ');
}

inline std::string __attribute__((nonnull(1))) __attribute__ ((__warn_unused_result__)) clean_underscores(const char *s)
{
  std::string ret = s;
  clean_underscores_inplace(ret);
  return ret;
}

} // namespace

trstring osm_t::unspecified_name(const object_t &obj) const
{
  const std::map<item_id_t, relation_t *>::const_iterator itEnd = relations.end();
  const char *bmrole = nullptr; // the role "obj" has in the "best" relation
  int rtype = -1; // type of the relation: 3 mp with name, 2 mp, 1 name, 0 anything else
  std::map<item_id_t, relation_t *>::const_iterator best = itEnd;
  std::string bname;

  for (std::map<item_id_t, relation_t *>::const_iterator it = relations.begin(); it != itEnd && rtype < 3; it++) {
    // ignore all relations where obj is no member
    const std::vector<member_t>::const_iterator mit = it->second->find_member_object(obj);
    if (mit == it->second->members.end())
      continue;

    int nrtype = 0;
    if(it->second->is_multipolygon())
      nrtype += 2;
    std::string nname = it->second->descriptive_name();
    assert(!nname.empty());
    if(nname[0] != '<')
      nrtype += 1;

    if(nrtype > rtype) {
      rtype = nrtype;
      best = it;
      bname.swap(nname);
      clean_underscores_inplace(bname);
      bmrole = mit->role;
    }
  }

  if(best == itEnd)
    return trstring("unspecified %1").arg(obj.type_string());

  std::string brole;
  if (bmrole != nullptr)
    brole = clean_underscores(bmrole);

  if(best->second->is_multipolygon() && !brole.empty())
    return trstring("%1: '%2' of multipolygon '%3'").arg(obj.type_string()).arg(brole).arg(bname);

  const char *type = best->second->tags.get_value("type");
  std::string reltype;
  if (type != nullptr)
    reltype = clean_underscores(type);
  else
    reltype = trstring("relation").toStdString();
  if(!brole.empty())
    return trstring("%1: '%2' in %3 '%4'").arg(obj.type_string()).arg(brole).arg(reltype).arg(bname);
  else
    return trstring("%1: member of %2 '%3'").arg(obj.type_string()).arg(reltype).arg(bname);
}

/* try to get an as "speaking" description of the object as possible */
std::string object_t::get_name(const osm_t &osm) const {
  std::string ret;

  assert(is_real());

  /* worst case: we have no tags at all. return techincal info then */
  if(!obj->tags.hasRealTags()) {
    osm.unspecified_name(*this).swap(ret);
    return ret;
  }

  /* try to figure out _what_ this is */
  const char *name = obj->tags.get_value("name");

  /* search for some kind of "type" */
  const std::array<const char *, 9> type_tags =
                          { { "amenity", "place", "historic",
                              "tourism", "landuse", "waterway", "railway",
                              "natural", "man_made" } };
  trstring_or_key typestr;

  for(unsigned int i = 0; !typestr && i < type_tags.size(); i++)
    typestr = obj->tags.get_value(type_tags[i]);

  if (!typestr) {
    const char *ts = obj->tags.get_value("leisure");
    if (ts != nullptr) {
      const std::array<const char *, 4> sport_leisure = { {
        "pitch", "sports_centre", "stadium", "track"
      } };

      // in case nothing better is found
      typestr = ts;

      for (unsigned int i = 0; i < sport_leisure.size(); i++) {
        if (strcmp(ts, sport_leisure[i]) == 0) {
          const char *sp = obj->tags.get_value("sport");
          if (sp != nullptr) {
            trstring("%1 %2").arg(clean_underscores(sp)).arg(clean_underscores(ts)).swap(ret);
            typestr = nullptr;
          }
          break;
        }
      }
    }
  }

  if(!typestr) {
    const char *rawValue = obj->tags.get_value("building");

    if (rawValue != nullptr && strcmp(rawValue, "no") != 0) {
      const char *street = obj->tags.get_value("addr:street");
      const char *hn = obj->tags.get_value("addr:housenumber");

      // simplify further checks
      if (strcmp(rawValue, "yes") == 0)
        rawValue = nullptr;

      if(street == nullptr) {
        // check if there is an "associatedStreet" relation where this is a "house" member
        const relation_t *astreet = osm.find_relation(typed_relation_member_functor("associatedStreet", "house", *this));
        if(astreet != nullptr)
          street = astreet->tags.get_value("name");
      }

      if(hn != nullptr) {
        trstring dsc = street != nullptr ?
                          rawValue != nullptr ?
                              trstring("%1 building %2 %3").arg(clean_underscores(rawValue)).arg(street) :
                              trstring("building %1 %2").arg(street) :
                          rawValue != nullptr ?
                              trstring("%1 building housenumber %2").arg(clean_underscores(rawValue)) :
                              trstring("building housenumber %1");
        dsc.arg(hn).swap(ret);
      } else if (street != nullptr) {
        trstring dsc = rawValue != nullptr ?
                            trstring("%1 building in %2").arg(clean_underscores(rawValue)).arg(street) :
                            trstring("building in %1").arg(street);
        dsc.swap(ret);
      } else {
        if (rawValue == nullptr)
          typestr = _("building");
        else
          trstring("%1 building").arg(clean_underscores(rawValue)).swap(ret);
        if(name == nullptr)
          name = obj->tags.get_value("addr:housename");
      }
    }
  }

  if(!typestr && ret.empty()) {
    const char *highway = obj->tags.get_value("highway");
    if(highway == nullptr) {
      typestr = obj->tags.get_value("emergency");
    } else {
      /* highways are a little bit difficult */
      if(!strcmp(highway, "primary")     || !strcmp(highway, "secondary") ||
         !strcmp(highway, "tertiary")    || !strcmp(highway, "unclassified") ||
         !strcmp(highway, "residential") || !strcmp(highway, "service")) {
        // no underscores replacement here because the whitelisted flags above don't have them
        assert(strchr(highway, '_') == nullptr);
        trstring("%1 road").arg(highway).swap(ret);
        typestr = nullptr;
      }

      else if(type == WAY && strcmp(highway, "pedestrian") == 0) {
        if(way->is_area())
          typestr = _("pedestrian area");
        else
          typestr = _("pedestrian way");
      }

      else if(!strcmp(highway, "construction")) {
        const char *cstr = obj->tags.get_value("construction:highway");
        if(cstr == nullptr)
          cstr = obj->tags.get_value("construction");
        if(cstr == nullptr) {
          typestr = _("road/street under construction");
        } else {
          typestr = nullptr;
          trstring("%1 road under construction").arg(cstr).swap(ret);
        }
      }

      else
        typestr = highway;
    }
  }

  if(!typestr && ret.empty()) {
    const char *pttype = obj->tags.get_value("public_transport");
    typestr = pttype;

    // for PT objects without name that are part of another PT relation use the name of that one
    if(name == nullptr && pttype != nullptr) {
      const char *ptkey = strcmp(pttype, "stop_position") == 0 ? "stop" :
                          strcmp(pttype, "platform") == 0 ? pttype :
                          nullptr;
      if(ptkey != nullptr) {
        const relation_t *stoparea = osm.find_relation(pt_relation_member_functor(ptkey, *this));
        if(stoparea != nullptr)
          name = stoparea->tags.get_value("name");
      }
    }
  }

  if(!typestr && ret.empty()) {
    const char *btype = obj->tags.get_value("barrier");
    if(btype != nullptr) {
      if(strcmp("yes", btype) == 0)
        trstring("barrier").swap(ret);
      else
        typestr = btype;
    }
  }

  if(typestr) {
    assert(ret.empty());
    ret = static_cast<std::string>(typestr);
  }

  // no good name was found so far, just look into some other tags to get a useful description
  const std::array<const char *, 3> name_tags = { { "ref", "note", "fix" "me" } };
  for(unsigned int i = 0; name == nullptr && i < name_tags.size(); i++)
    name = obj->tags.get_value(name_tags[i]);

  if(name != nullptr) {
    if(ret.empty())
      ret = type_string();
    trstring("%1: \"%2\"").arg(ret).arg(name).swap(ret);
  } else if(ret.empty()) {
    // look if this has only one real tag and use that one
    const tag_t *stag = obj->tags.singleTag();
    if(stag != nullptr && strcmp(stag->value, "no") != 0) {
      ret = stag->key;
    } else {
      // last chance
      const char *bp = obj->tags.get_value("building:part");
      trstring tret;
      if(bp != nullptr && strcmp(bp, "yes") == 0)
        tret = trstring("building part");
      else
        tret = osm.unspecified_name(*this);
      tret.swap(ret);
      return ret;
    }
  }

  clean_underscores_inplace(ret);

  return ret;
}
