/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 * SPDX-FileCopyrightText: 2019-2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
#include <optional>
#include <string>
#include <utility>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_platform.h>

namespace {

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
inline std::string __attribute__((nonnull(1))) __attribute__ ((warn_unused_result)) clean_underscores(const char *s)
{
  std::string ret = s;
  std::replace(ret.begin(), ret.end(), '_', ' ');
  return ret;
}

/**
 * @brief the parts we already have found to construct the final description from
 */
struct nameParts {
  nameParts() : name(nullptr) {}

  const char *name; ///< the value of a "name" key
  class typeWrapper {
#ifndef TRSTRING_NATIVE_TYPE_IS_TRSTRING
    trstring::native_type nt;
#else
    struct {
      [[nodiscard]] static inline constexpr bool isEmpty() { return true; }
      [[noreturn]] explicit inline operator trstring() const
      {
        // should always be optimized away
        assert_unreachable();
      }
    } nt;
#endif

  public:
    trstring tr;      ///< an already translated type description
    const char *key;  ///< the raw value of a specific key used as description

    explicit typeWrapper() : key(nullptr) {}
#if __cplusplus >= 201103L
    typeWrapper &operator=(trstring &&n) { tr = std::move(n); assert(nt.isEmpty()); return *this; }
#else
    typeWrapper &operator=(trstring n) { tr.swap(n); assert(nt.isEmpty()); return *this; }
#endif

#ifndef TRSTRING_NATIVE_TYPE_IS_TRSTRING
    typeWrapper &operator=(trstring::native_type_arg n) { assert(tr.isEmpty()); nt = n; return *this; }

    O2G_OPERATOR_EXPLICIT inline operator trstring::native_type() const
    {
      assert(key == nullptr);
      assert(tr.isEmpty());
      assert(isNative());
      return nt;
    }

    inline bool isNative() const
    {
      return !nt.isEmpty();
    }
#endif

    trstring::any_type toTrstring() const
    {
      assert(key == nullptr);
      if (!nt.isEmpty())
        return trstring::any_type(nt);
      assert(!tr.isEmpty());
      return tr;
    }

    inline bool isTranslated() const
    {
      return !tr.isEmpty() || !nt.isEmpty();
    }

    inline bool isEmpty() const
    {
      return key == nullptr && tr.isEmpty() && nt.isEmpty();
    }
  } type;
};

struct lifecycleEntry {
  lifecycleEntry(const char *t, trstring tr) : tag(t), translation(tr) {}
  const char *tag;
  trstring translation;
};

struct lifecycleResult {
  lifecycleResult() : value(nullptr), prefix(nullptr) {}
  trstring pattern;
  const char *value;
  const char *prefix; /* buildin constant, does not need to be freed */
};

const std::array<lifecycleEntry, 7> lifeCyclePrefixes = {{
  lifecycleEntry("proposed", trstring("proposed %1")),
  lifecycleEntry("construction", trstring("%1 under construction")),
  lifecycleEntry("disused", trstring("disused %1")),
  lifecycleEntry("abandoned", trstring("abandoned %1")),
  lifecycleEntry("demolished", trstring("demolished %1")),
  lifecycleEntry("removed", trstring("demolished %1")),
  lifecycleEntry("razed", trstring("demolished %1")),
}};

/**
 * @brief construct <prefix> + ":" + <key> and look up the value for that tag
 */
const char * __attribute__((nonnull(2, 3))) getLifecyclePrefixed(const tag_list_t &tags, const char *prefix, const char *key)
{
  std::string lc_key = prefix + std::string(1, ':') + key;

  return tags.get_value(lc_key.c_str());
}

/**
 * @brief check if rawValue is a lifecycle prefix, and look up if values exists inside that prefixed tags
 */
std::optional<lifecycleResult> lifecycle(const tag_list_t &tags, const char *key, const char *rawValue)
{
  lifecycleResult res;

  for (size_t i = 0; i < lifeCyclePrefixes.size(); i++)
    if (strcmp(rawValue, lifeCyclePrefixes[i].tag) == 0) {
      res.pattern = lifeCyclePrefixes[i].translation;
      res.prefix = lifeCyclePrefixes[i].tag;

      res.value = getLifecyclePrefixed(tags, lifeCyclePrefixes[i].tag, key);
      if (res.value == nullptr)
        res.value = tags.get_value(rawValue);

      return res;
    }

  return std::optional<lifecycleResult>();
}

/**
 * @brief check if the key exists completely inside a prefixed namespace
 *
 * This can be used to detect a <prefix>:<feature>=* when no <feature>=<prefix> exists.
 */
std::optional<lifecycleResult> lifecycleOnly(const tag_list_t &tags, const char *key)
{
  lifecycleResult res;

  for (size_t i = 0; i < lifeCyclePrefixes.size(); i++) {
    res.value = getLifecyclePrefixed(tags, lifeCyclePrefixes[i].tag, key);
    if (res.value != nullptr) {
      res.pattern = lifeCyclePrefixes[i].translation;
      res.prefix = lifeCyclePrefixes[i].tag;
      return res;
    }
  }

  return std::optional<lifecycleResult>();
}

std::optional<nameParts> describeBuilding(const osm_t &osm, const object_t &obj, const char *rawValue, const char *name, const bool useLifecycle)
{
  nameParts ret;

  ret.name = name;

  const tag_list_t &tags = static_cast<base_object_t *>(obj)->tags;
  const char *street = tags.get_value("addr:street");
  const char *hn = tags.get_value("addr:housenumber");
  trstring lifecycleDescr = trstring("%1");

  // simplify further checks
  if (rawValue != nullptr && strcmp(rawValue, "yes") == 0) {
    rawValue = nullptr;
  } else if (rawValue != nullptr) {
    std::optional<lifecycleResult> lcRet = lifecycle(tags, "building", rawValue);
    if (lcRet) {
      if (!useLifecycle)
        return std::optional<nameParts>();
      lifecycleDescr = lcRet->pattern;
      if (lcRet->value == nullptr || strcmp(lcRet->value, "yes") == 0)
        rawValue = nullptr;
      else
        rawValue = lcRet->value;
    }
  }

  if(street == nullptr) {
    // check if there is an "associatedStreet" relation where this is a "house" member
    const relation_t *astreet = osm.find_relation(typed_relation_member_functor("associatedStreet", "house", obj));
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
    ret.type = lifecycleDescr.arg(dsc.arg(hn));
  } else if (street != nullptr) {
    ret.type = lifecycleDescr.arg(rawValue != nullptr ?
                                  trstring("%1 building in %2").arg(clean_underscores(rawValue)).arg(street) :
                                  trstring("building in %1").arg(street));
  } else {
    if (rawValue == nullptr)
      ret.type = lifecycleDescr.arg(_("building"));
    else
      ret.type = lifecycleDescr.arg(trstring("%1 building").arg(clean_underscores(rawValue)));
    if(ret.name == nullptr)
      ret.name = tags.get_value("addr:housename");
  }

  return ret;
}

std::optional<nameParts> describeHighway(const tag_list_t &tags, const object_t &obj, const char *rawValue, const bool useLifecycle)
{
  nameParts ret;
  std::optional<lifecycleResult> lcRet = lifecycle(tags, "highway", rawValue);
  if (lcRet) {
    if (!useLifecycle)
      return std::optional<nameParts>();
    // make sure there is no nullptr in rawValue below.
    // This happens if there is only highway=<lifecycle> without further details
    if (lcRet->value == nullptr || strcmp(lcRet->value, "yes") == 0) {
      ret.type = lcRet->pattern.arg(_("road"));
      return ret;
    }
    rawValue = lcRet->value;
  }
  trstring descr;

  /* highways are a little bit difficult */
  if(!strcmp(rawValue, "primary")     || !strcmp(rawValue, "secondary") ||
      !strcmp(rawValue, "tertiary")    || !strcmp(rawValue, "unclassified") ||
      !strcmp(rawValue, "residential") || !strcmp(rawValue, "service")) {
    // no underscores replacement here because the whitelisted flags above don't have them
    assert(strchr(rawValue, '_') == nullptr);
    descr = trstring("%1 road").arg(rawValue);
  }

  else if(obj.type == object_t::WAY && strcmp(rawValue, "pedestrian") == 0) {
    if(static_cast<way_t *>(obj)->is_area())
      descr = trstring("pedestrian area");
    else
      descr = trstring("pedestrian way");
  }

  else if (lcRet) {
    ret.type = lcRet->pattern.arg(clean_underscores(rawValue));
    return ret;
  }

  else
    ret.type.key = rawValue;

  if (lcRet && !descr.isEmpty())
    ret.type = lcRet->pattern.arg(descr);
  else if (!descr.isEmpty())
    ret.type = std::move(descr);

  return ret;
}

nameParts nameElements(const osm_t &osm, const object_t &obj, const bool useLifecycle = false)
{
  nameParts ret;

  assert(obj.is_real());
  const tag_list_t &tags = static_cast<base_object_t *>(obj)->tags;

  /* try to figure out _what_ this is */
  ret.name = tags.get_value("name");

  /* search for some kind of "type" */
  const std::array<const char *, 9> type_tags =
                          { { "amenity", "place", "historic",
                              "tourism", "landuse", "waterway", "railway",
                              "natural", "man_made" } };

  for(unsigned int i = 0; i < type_tags.size(); i++) {
    const char *rawValue = tags.get_value(type_tags[i]);
    if (rawValue == nullptr)
      continue;

    std::optional<lifecycleResult> lcRet = lifecycle(tags, type_tags[i], rawValue);
    /* if this is key=<lifecycle> then ignore it on first pass */
    if (lcRet && !useLifecycle)
      continue;
    else if (lcRet && lcRet->value != nullptr)
      ret.type = lcRet->pattern.arg(clean_underscores(lcRet->value));
    else if (lcRet)
      ret.type = lcRet->pattern.arg(type_tags[i]);
    else
      ret.type.key = rawValue;

    return ret;
  }

  // it may happen that the "primary" feature only exists as <lifecycle>:<feature>
  if (useLifecycle) {
    for(unsigned int i = 0; i < type_tags.size(); i++) {
      std::optional<lifecycleResult> lcRet = lifecycleOnly(tags, type_tags[i]);
      if (!lcRet)
        continue;
      assert(lcRet->value != nullptr);

      if (strcmp(lcRet->value, "yes") != 0)
        ret.type = lcRet->pattern.arg(clean_underscores(lcRet->value));
      else
        ret.type = lcRet->pattern.arg(type_tags[i]);

      return ret;
    }
  }

  // ### LEISURE
  const char *rawValue = tags.get_value("leisure");
  if (rawValue != nullptr) {
    // these leisure values will get an extra description from sport=*
    const std::array<const char *, 4> sport_leisure = { {
      "pitch", "sports_centre", "stadium", "track"
    } };
    for (unsigned int i = 0; i < sport_leisure.size(); i++) {
      if (strcmp(rawValue, sport_leisure[i]) == 0) {
        const char *sp = tags.get_value("sport");
        if (sp != nullptr) {
          ret.type = trstring("%1 %2").arg(clean_underscores(sp)).arg(clean_underscores(rawValue));
          return ret;
        }
        break;
      }
    }

    ret.type.key = rawValue;
    return ret;
  }

  // ### BUILDINGS
  rawValue = tags.get_value("building");
  if (rawValue != nullptr && strcmp(rawValue, "no") != 0) {
    std::optional<nameParts> np = describeBuilding(osm, obj, rawValue, ret.name, useLifecycle);
    if (np)
      return *np;
  }
  if (useLifecycle && (rawValue == nullptr || strcmp(rawValue, "no") == 0)) {
    std::optional<lifecycleResult> lcRet = lifecycleOnly(tags, "building");
    if (lcRet) {
      // there was no building=<lifecycle>, but work as if it was
      std::optional<nameParts> np = describeBuilding(osm, obj, lcRet->prefix, ret.name, true);
      if (np)
        return *np;
    }
  }

  // ### HIGHWAYS
  rawValue = tags.get_value("highway");
  if(rawValue != nullptr) {
    std::optional<nameParts> hret = describeHighway(tags, obj, rawValue, useLifecycle);
    if (hret) {
      ret.type = hret->type;
      return ret;
    }
  } else if (useLifecycle) {
    std::optional<lifecycleResult> lcRet = lifecycleOnly(tags, "highway");
    if (lcRet) {
      // there was no highway=<lifecycle>, but work as if it was
      std::optional<nameParts> np = describeHighway(tags, obj, lcRet->prefix, true);
      if (np)
        return *np;
    }
  }

  // ### EMERGENCY
  rawValue = tags.get_value("emergency");
  if (rawValue != nullptr) {
    ret.type.key = rawValue;
    return ret;
  }

  // ### PUBLIC TRANSORT
  rawValue = tags.get_value("public_transport");
  if (rawValue != nullptr) {
    ret.type.key = rawValue;

    // for PT objects without name that are part of another PT relation use the name of that one
    if(ret.name == nullptr) {
      const char *ptkey = strcmp(rawValue, "stop_position") == 0 ? "stop" :
                          strcmp(rawValue, "platform") == 0 ? rawValue :
                          nullptr;
      if(ptkey != nullptr) {
        const relation_t *stoparea = osm.find_relation(pt_relation_member_functor(ptkey, obj));
        if(stoparea != nullptr)
          ret.name = stoparea->tags.get_value("name");
      }
    }

    return ret;
  }

  // ### BARRIER
  rawValue = tags.get_value("barrier");
  if(rawValue != nullptr) {
    if(strcmp("yes", rawValue) == 0)
      ret.type = _("barrier");
    else
      ret.type.key = rawValue;
    return ret;
  }

  // ### last chance
  rawValue = tags.get_value("building:part");

  if(rawValue != nullptr && strcmp(rawValue, "yes") == 0) {
    ret.type = trstring("building part");
  } else if (!useLifecycle) {
    return nameElements(osm, obj, true);
  } else {
    // look if this has only one real tag and use that one
    const tag_t *stag = tags.singleTag();
    if (stag != nullptr && strcmp(stag->value, "no") != 0) {
      // rule out a single name tag first
      if (ret.name == nullptr)
        ret.type.key = stag->key;
      return ret;
    }

    ret.type = osm.unspecified_name(obj);
  }

  return ret;
}

enum RelationNameFlags {
  Uninitialized = -1,             ///< no relation has been found until now
  Member = 0,                     ///< there is a relation that has the given object as member
  HasName = 1,                    ///< ... Member and the relation has a descriptive name
  IsMp = 2,                       ///< ... Member and that relation is a multipolygon
  IsMpWithName = HasName | Member ///< ... Member and that relation is a multipolygon with a descriptive name
};

} // namespace

trstring osm_t::unspecified_name(const object_t &obj) const
{
  const std::map<item_id_t, relation_t *>::const_iterator itEnd = relations.end();
  const char *bmrole = nullptr; // the role "obj" has in the "best" relation
  int rtype = Uninitialized; // type of the best matching relation this object is member of
  std::map<item_id_t, relation_t *>::const_iterator best = itEnd;
  const char *bnameTag = nullptr;

  for (std::map<item_id_t, relation_t *>::const_iterator it = relations.begin(); it != itEnd && rtype < 3; it++) {
    // ignore all relations where obj is no member
    const std::vector<member_t>::const_iterator mit = it->second->find_member_object(obj);
    if (mit == it->second->members.end())
      continue;

    int nrtype = Member;
    if(it->second->is_multipolygon())
      nrtype |= IsMp;
    const char *nameTag = it->second->descriptiveName();
    if (nameTag != nullptr)
      nrtype |= HasName;

    if(nrtype > rtype) {
      rtype = nrtype;
      best = it;
      bnameTag = nameTag;
      bmrole = mit->role;
    }
  }

  if(best == itEnd)
    return trstring("unspecified %1").arg(obj.type_string());

  trstring bname;
  if (bnameTag != nullptr)
    bname = trstring("\"%1\"").arg(clean_underscores(bnameTag));
  else
    bname = best->second->idName();

  std::string brole;
  if (bmrole != nullptr)
    brole = clean_underscores(bmrole);

  if(rtype & IsMp && !brole.empty())
    return trstring("%1: '%2' of multipolygon %3").arg(obj.type_string()).arg(brole).arg(bname);

  const char *type = best->second->tags.get_value("type");
  std::string reltype;
  if (type != nullptr)
    reltype = clean_underscores(type);
  else
    reltype = trstring("relation").toStdString();
  if(!brole.empty())
    return trstring("%1: '%2' in %3 %4").arg(obj.type_string()).arg(brole).arg(reltype).arg(bname);
  else
    return trstring("%1: member of %2 %3").arg(obj.type_string()).arg(reltype).arg(bname);
}

/* try to get an as "speaking" description of the object as possible */
trstring
object_t::get_name(const osm_t &osm) const
{
  assert(is_real());

  /* worst case: we have no tags at all. return techincal info then */
  if(!obj->tags.hasRealTags())
    return osm.unspecified_name(*this);

  /* try to figure out _what_ this is */
  nameParts np = nameElements(osm, *this);

  // no good name was found so far, just look into some other tags to get a useful description
  const std::array<const char *, 3> name_tags = { { "ref", "note", "fix" "me" } };
  for(unsigned int i = 0; np.name == nullptr && i < name_tags.size(); i++)
    np.name = obj->tags.get_value(name_tags[i]);

  if(np.name != nullptr) {
    if (np.type.isEmpty())
      np.type = type_string();

    trstring r;
    if (np.type.isTranslated())
      r = trstring("%1: \"%2\"").arg(np.type.toTrstring());
    else
      r = trstring("%1: \"%2\"").arg(clean_underscores(np.type.key));
    return r.arg(np.name);
  }

  if (!np.type.tr.isEmpty()) {
    return np.type.tr;
#ifndef TRSTRING_NATIVE_TYPE_IS_TRSTRING
  } else if (np.type.isNative()) {
    return trstring(static_cast<trstring::native_type>(np.type));
#endif
  } else {
    trstring ret;
    assert(!np.type.isTranslated());
    ret.assign(clean_underscores(np.type.key));
    return ret;
  }
}

const char *
relation_t::descriptiveName() const
{
  const std::array<const char *, 5> keys = { { "name", "ref", "description", "note", "fix" "me" } };
  for (unsigned int i = 0; i < keys.size(); i++) {
    const char *name = tags.get_value(keys[i]);
    if(name != nullptr)
      return name;
  }

  return nullptr;
}

trstring
relation_t::descriptiveNameOrId() const
{
  const char *name = descriptiveName();

  if (name == nullptr)
    return idName();

  trstring ret;
  ret.assign(name);
  return ret;
}

trstring
relation_t::idName() const
{
  return trstring("<ID #%1>").arg(id);
}
