/*
 * SPDX-FileCopyrightText: 2017 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "josm_presets_p.h"

#include "SaxParser.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fdguard.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <memory>
#include <numeric>
#include <stack>
#include <strings.h>
#include <sys/stat.h>
#include <unordered_map>

#include "osm2go_annotations.h"
#include <osm2go_platform.h>
#include "osm2go_stl.h"

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

typedef std::unordered_map<std::string, presets_item *> ChunkMap;

/* --------------------- presets.xml parsing ----------------------- */

std::string josm_icon_name_adjust(const char *name) {
  size_t len = strlen(name);

  if(likely(len > 4)) {
    const char * const ext = name + len - 4;
    /* the icon loader uses names without extension */
    if(strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".svg") == 0)
      len -= 4;
  }

  return std::string(name, len);
}

namespace {

std::string __attribute__((nonnull(1)))
josm_icon_name_adjust(const char *name, const std::string &basepath, int basedirfd)
{
  struct stat st;
  if(fstatat(basedirfd, name, &st, 0) == 0 && S_ISREG(st.st_mode))
    return basepath + name;

  return ::josm_icon_name_adjust(name);
}

typedef std::vector<std::pair<presets_item_t::item_type, std::string> > TypeStrMap;

TypeStrMap
type_map_init()
{
  TypeStrMap ret(5);

  TypeStrMap::size_type pos = 0;
  ret[pos++] = TypeStrMap::value_type(presets_item_t::TY_WAY, "way");
  ret[pos++] = TypeStrMap::value_type(presets_item_t::TY_NODE, "node");
  ret[pos++] = TypeStrMap::value_type(presets_item_t::TY_RELATION, "relation");
  ret[pos++] = TypeStrMap::value_type(presets_item_t::TY_CLOSED_WAY, "closedway");
  ret[pos++] = TypeStrMap::value_type(presets_item_t::TY_MULTIPOLYGON, "multipolygon");

  return ret;
}

class find_type {
  const char * const type;
  const char sep;
public:
  explicit inline find_type(const char *t, const char s) : type(t), sep(s) {}
  inline bool operator()(const TypeStrMap::value_type &v)
  {
    const size_t tlen = v.second.size();
    return strncmp(v.second.c_str(), type, tlen) == 0 && type[tlen] == sep;
  }
};

int
josm_type_bit(const char *type, char sep)
{
  static const TypeStrMap types = type_map_init();
  const TypeStrMap::const_iterator itEnd = types.end();

  const TypeStrMap::const_iterator it = std::find_if(types.begin(), itEnd,
                                                     find_type(type, sep));

  if(likely(it != itEnd))
    return it->first;

  // This prints the remaining string even if a separator follows. Who cares.
  printf("WARNING: unexpected type %s\n", type);
  return 0;
}

/* parse a comma seperated list of types and set their bits */
unsigned int
josm_type_parse(const char *type)
{
  unsigned int type_mask = 0;
  if(type == nullptr)
    return presets_item_t::TY_ALL;

  for(const char *ntype = strchr(type, ',');
      ntype != nullptr; ntype = strchr(type, ',')) {
    type_mask |= josm_type_bit(type, ',');
    type = ntype + 1;
  }

  type_mask |= josm_type_bit(type, '\0');
  return type_mask;
}

// custom find to avoid memory allocations for std::string
template<typename T>
struct str_map_find {
  const char * const name;
  explicit str_map_find(const char * n)
    : name(n) {}
  bool operator()(const typename T::value_type &p) {
    return (strcmp(p.name, name) == 0);
  }
};

class PresetSax : public SaxParser {
public:
  enum State {
    DocStart,
    TagPresets,
    TagGroup,
    TagItem,
    TagChunk,
    TagReference,
    TagPresetLink,
    TagKey,
    TagText,
    TagCombo,
    TagMultiselect,
    TagListEntry,
    TagCheck,
    TagLabel,
    TagSpace,
    TagSeparator,
    TagLink,
    TagRoles,
    TagRole,
    IntermediateTag,    ///< tag itself is ignored, but childs are processed
    UnknownTag
  };

  // this maps the XML tag name to the target state and the list of allowed source states
  struct StateChange {
    StateChange(const char *nm, State os, const std::vector<State> &ns)
      : name(nm), oldState(os), newStates(ns) {}
    const char *name;
    State oldState;
    std::vector<State> newStates;
  };

  typedef std::vector<StateChange> StateMap;
  static const StateMap &preset_state_map();

private:
  // not a stack because that can't be iterated
  std::vector<State> state;
  std::stack<presets_item_t *> items; // the current item stack (i.e. menu layout)
  std::stack<presets_element_t *> widgets; // the current widget stack (i.e. dialog layout)
  presets_items_internal &presets;
  const std::string &basepath;
  const int basedirfd;
  const std::vector<std::string> &langs;

  bool resolvePresetLink(presets_element_link* link, const std::string &id);

  /**
   * @brief dump out the current object stack
   * @param before message to print before the stack listing
   * @param after0 first message to print after the stack listing
   * @param after1 second message to print after the stack listing
   *
   * The line is always terminated with a LF.
   */
  void dumpState(const char *before = nullptr, const char *after0 = nullptr, const char *after1 = nullptr) const;

public:
  explicit PresetSax(presets_items_internal &p, const std::string &b, int basefd);

  bool parse(const std::string &filename);

  ChunkMap chunks;

  typedef std::vector<std::pair<presets_element_link *, std::string> > LLinks;

  ChunkMap itemsNames;
  LLinks laterLinks; ///< unresolved preset_link references

private:
  struct find_link_ref {
    PresetSax &px;
    explicit find_link_ref(PresetSax &p) : px(p) {}
    void operator()(LLinks::value_type &l);
  };

  void characters(const char *ch, int len) override;
  void startElement(const char *name, const char **attrs) override;
  void endElement(const char *name) override;

  /**
   * @brief find attribute with the given name
   * @param attrs the attribute list to search
   * @param name the key to look for
   * @param useLang if localized keys should be preferred
   * @returns the attribute string if present
   *
   * If the attribute is present but the string is empty a nullptr is returned.
   */
  const char *findAttribute(const char **attrs, const char *name, bool useLang = true) const;

  // it's not worth the effort to convert this to an unordered_map as it has very few members
  typedef std::map<const char *, const char *> AttrMap;
  /**
   * @brief find the attributes with the given names
   * @param attrs the attribute list to search
   * @param names the keys to look for
   * @param count elements in names
   * @param langflags if localized keys should be preferred (bitwise positions of entries in names)
   * @returns the found keys
   */
  AttrMap findAttributes(const char **attrs, const char **names, unsigned int count, unsigned int langflags = 0) const;
};

PresetSax::StateMap
preset_state_map_init()
{
  PresetSax::StateMap map;

#if __cplusplus >= 201103L
#define STATE_VECTOR_START(n, N) const std::vector<PresetSax::State> n =
#define STATE_VECTOR_END(n)
# define VECTOR_ONE(a) { a }
#else
#define STATE_VECTOR_START(n, N) const std::array<PresetSax::State, N> n##_ref = {
#define STATE_VECTOR_END(n) }; const std::vector<PresetSax::State> n(n##_ref.begin(), n##_ref.end())
# define VECTOR_ONE(a) std::vector<PresetSax::State>(1, (a))
#endif

  STATE_VECTOR_START(item_chunks, 2) { PresetSax::TagChunk, PresetSax::TagItem } STATE_VECTOR_END(item_chunks);
  STATE_VECTOR_START(item_refs, 5) { PresetSax::TagChunk, PresetSax::TagItem, PresetSax::TagCombo, PresetSax::TagMultiselect, PresetSax::TagRoles } STATE_VECTOR_END(item_refs);
  STATE_VECTOR_START(pr_gr, 2) { PresetSax::TagPresets, PresetSax::TagGroup } STATE_VECTOR_END(pr_gr);
  STATE_VECTOR_START(selectables, 3) { PresetSax::TagCombo, PresetSax::TagMultiselect, PresetSax::TagChunk } STATE_VECTOR_END(selectables);
  STATE_VECTOR_START(roles_chunks, 2) { PresetSax::TagRoles, PresetSax::TagChunk } STATE_VECTOR_END(roles_chunks);

#define MAPFILL 20
  map.reserve(MAPFILL);

  map.push_back(PresetSax::StateMap::value_type("presets", PresetSax::TagPresets, VECTOR_ONE(PresetSax::DocStart)));
  map.push_back(PresetSax::StateMap::value_type("chunk", PresetSax::TagChunk, VECTOR_ONE(PresetSax::TagPresets)));
  map.push_back(PresetSax::StateMap::value_type("group", PresetSax::TagGroup, pr_gr));

  // ignore the case of standalone items and separators for now as it does not happen yet
  map.push_back(PresetSax::StateMap::value_type("item", PresetSax::TagItem, VECTOR_ONE(PresetSax::TagGroup)));
  map.push_back(PresetSax::StateMap::value_type("separator", PresetSax::TagSeparator, VECTOR_ONE(PresetSax::TagGroup)));

  map.push_back(PresetSax::StateMap::value_type("reference", PresetSax::TagReference, item_refs));
  map.push_back(PresetSax::StateMap::value_type("preset_link", PresetSax::TagPresetLink, item_chunks));
  map.push_back(PresetSax::StateMap::value_type("key", PresetSax::TagKey, item_chunks));
  map.push_back(PresetSax::StateMap::value_type("text", PresetSax::TagText, item_chunks));
  map.push_back(PresetSax::StateMap::value_type("combo", PresetSax::TagCombo, item_chunks));
  map.push_back(PresetSax::StateMap::value_type("multiselect", PresetSax::TagMultiselect, item_chunks));
  map.push_back(PresetSax::StateMap::value_type("list_entry", PresetSax::TagListEntry, selectables));
  map.push_back(PresetSax::StateMap::value_type("check", PresetSax::TagCheck, item_chunks));
  map.push_back(PresetSax::StateMap::value_type("label", PresetSax::TagLabel, item_chunks));
  map.push_back(PresetSax::StateMap::value_type("space", PresetSax::TagSpace, item_chunks));
  map.push_back(PresetSax::StateMap::value_type("link", PresetSax::TagLink, item_chunks));
  map.push_back(PresetSax::StateMap::value_type("roles", PresetSax::TagRoles, item_chunks));
  map.push_back(PresetSax::StateMap::value_type("role", PresetSax::TagRole, roles_chunks));

  map.push_back(PresetSax::StateMap::value_type("checkgroup", PresetSax::IntermediateTag, item_chunks));
  map.push_back(PresetSax::StateMap::value_type("optional", PresetSax::IntermediateTag, item_chunks));

  // make sure the one-time reservation is the correct size
  assert(map.size() == MAPFILL);

  return map;
}

const PresetSax::StateMap &PresetSax::preset_state_map()
{
  static const PresetSax::StateMap map = preset_state_map_init();

  return map;
}

// Map back a state to it's string. Only used for debug messages.
struct name_find {
  const PresetSax::State state;
  explicit inline name_find(PresetSax::State s) : state(s) {}
  inline bool operator()(const PresetSax::StateMap::value_type &p) const {
    return p.oldState == state;
  }
};

void
dumpTag(PresetSax::State st)
{
  if(st == PresetSax::UnknownTag || st == PresetSax::IntermediateTag) {
    printf("*/");
  } else {
    const PresetSax::StateMap &tags = PresetSax::preset_state_map();
    const PresetSax::StateMap::const_iterator nitEnd = tags.end();
    const PresetSax::StateMap::const_iterator nit = std::find_if(tags.begin(), nitEnd, name_find(st));
    assert(nit != nitEnd);
    printf("%s/", nit->name);
  }
}

void
PresetSax::dumpState(const char *before, const char *after0, const char *after1) const
{
  if(before != nullptr)
    printf("%s ", before);
  std::for_each(std::next(state.begin()), state.end(), dumpTag);
  if(after0 != nullptr) {
    fputs(after0, stdout);
    if(after1 != nullptr)
      fputs(after1, stdout);
  } else {
    assert_null(after1);
  }
  puts("");
}

/**
 * @brief get the user language strings from environment
 */
const std::vector<std::string> &userLangs()
{
  static std::vector<std::string> lcodes;
  if(lcodes.empty()) {
    const char *lcm = getenv("LC_MESSAGES");
    if(lcm == nullptr || *lcm == '\0')
      lcm = getenv("LANG");
    if(lcm != nullptr && *lcm != '\0') {
      std::string lc = lcm;
      std::string::size_type d = lc.find('.');
      if(d != std::string::npos)
        lc.erase(d + 1);
      else
        lc += '.';
      lcodes.push_back(lc);
      d = lc.find('_');
      if(d != std::string::npos) {
        lc.resize(d);
        lc += '.';
        lcodes.push_back(lc);
      }
    }
  }

  return lcodes;
}

PresetSax::PresetSax(presets_items_internal &p, const std::string &b, int basefd)
  : SaxParser()
  , presets(p)
  , basepath(b)
  , basedirfd(basefd)
  , langs(userLangs())
{
  state.push_back(DocStart);
}

struct find_link_parent {
  const presets_element_link *link;
  explicit find_link_parent(const presets_element_link *l) : link(l) {}
  bool operator()(presets_item_t *t);
  inline bool operator()(const ChunkMap::value_type &p) {
    return operator()(p.second);
  }
};

bool find_link_parent::operator()(presets_item_t *t)
{
  if(t->type & presets_item_t::TY_GROUP) {
    const presets_item_group * const gr = static_cast<presets_item_group *>(t);
    return std::any_of(gr->items.begin(), gr->items.end(), *this);
  }

  if(!t->isItem())
    return false;

  presets_item * const item = static_cast<presets_item *>(t);
  const std::vector<presets_element_t *>::iterator it = std::find(item->widgets.begin(),
                                                                 item->widgets.end(),
                                                                 link);
  if(it == item->widgets.end())
    return false;
  item->widgets.erase(it);
  delete link;
  link = nullptr;

  return true;
}

void PresetSax::find_link_ref::operator()(PresetSax::LLinks::value_type &l)
{
  if(unlikely(!px.resolvePresetLink(l.first, l.second))) {
    printf("found preset_link with unmatched preset_name '%s'\n", l.second.c_str());
    // delete the link widget everywhere it was inserted
    find_link_parent fc(l.first);
    const std::vector<presets_item_t *>::const_iterator itEnd = px.presets.items.end();
    if(std::none_of(std::cbegin(px.presets.items), itEnd, fc)) {
      const ChunkMap::const_iterator cit = std::find_if(px.chunks.begin(), px.chunks.end(), fc);
      assert(cit != px.chunks.end());
    }
  }
}

bool PresetSax::parse(const std::string &filename)
{
  if (!parseFile(filename))
    return false;

  std::for_each(laterLinks.begin(), laterLinks.end(), find_link_ref(*this));

  return state.size() == 1;
}

bool PresetSax::resolvePresetLink(presets_element_link *link, const std::string &id)
{
  const ChunkMap::const_iterator it = itemsNames.find(id);
  // these references may also target items that will only be added later
  if(it == itemsNames.end())
    return false;

  link->item = it->second;
  return true;
}

void PresetSax::characters(const char *ch, int len)
{
  for(int pos = 0; pos < len; pos++)
    if(unlikely(!isspace(ch[pos]))) {
      printf("unhandled character data: %*.*s state %i\n", len, len, ch, state.back());
      return;
    }
}

// check if the given string starts with the iterator value
struct matchHead {
  const char * const a;
  explicit inline matchHead(const char *attr) : a(attr) {}
  inline bool operator()(const std::string &l) const
  {
    return strncmp(a, l.c_str(), l.size()) == 0;
  }
};

const char *PresetSax::findAttribute(const char **attrs, const char *name, bool useLang) const {
  // If the entire key matches name this is the non-localized (i.e. fallback)
  // key. Continue search to find a localized text, if no other is found return
  // the defaut one.
  const char *c = nullptr;

  for(unsigned int i = 0; attrs[i] != nullptr; i += 2) {
    // Check if the given attribute begins with one of the preferred language
    // codes. If yes, skip over the language code and check this one.
    const char *a = attrs[i];
    if (useLang) {
      const std::vector<std::string>::const_iterator itEnd = langs.end();
      const std::vector<std::string>::const_iterator it = std::find_if(std::cbegin(langs), itEnd, matchHead(a));
      if (it != itEnd)
        a += it->size();
    }

    if(strcmp(a, name) == 0) {
      const char *ret;
      if(*(attrs[i + 1]) == '\0')
        ret = nullptr;
      else
        ret = attrs[i + 1];
      if(a != attrs[i])
        return ret;
      c = ret;
    }
  }

  return c;
}

PresetSax::AttrMap PresetSax::findAttributes(const char **attrs, const char **names, unsigned int count, unsigned int langflags) const
{
  AttrMap ret;

  for(unsigned int i = 0; attrs[i] != nullptr; i += 2) {
    // Check if the given attribute begins with one of the preferred language
    // codes. If yes, skip over the language code and check this one.
    const char *a = attrs[i];
    bool isLoc = false;
    const std::vector<std::string>::const_iterator itEnd = langs.end();
    const std::vector<std::string>::const_iterator it = std::find_if(std::cbegin(langs), itEnd, matchHead(a));
    if (it != itEnd) {
      a += it->size();
      isLoc = true;
    }

    for(unsigned int j = 0; j < count; j++) {
      if(strcmp(a, names[j]) == 0) {
        // if this is localized and no localization was permitted: skip
        if(isLoc && !(langflags & (1 << j)))
          continue;

        if(*(attrs[i + 1]) != '\0') {
          // if this is localized: store, if not, store only if nothing in map right now
          if(isLoc || ret.find(names[j]) == ret.end())
            ret[names[j]] = attrs[i + 1];
        }
      }
    }
  }

  return ret;
}

#define NULL_OR_VAL(a) ((a) ? (a) : std::string())
#define NULL_OR_MAP_STR(it) ((it) != aitEnd ? (it)->second : std::string())
#define NULL_OR_MAP_VAL(it) ((it) != aitEnd ? (it)->second : nullptr)

bool
not_intermediate(PresetSax::State st)
{
  return st != PresetSax::IntermediateTag;
}

void PresetSax::startElement(const char *name, const char **attrs)
{
  const StateMap &tags = preset_state_map();
  StateMap::const_iterator it = std::find_if(tags.begin(), tags.end(),
                                             str_map_find<StateMap>(name));
  if(it == tags.end()) {
    dumpState("found unhandled", name);
    state.push_back(UnknownTag);
    return;
  }

  // ignore IntermediateTag when checking for valid parent tags
  State oldState = state.back();
  if(oldState == IntermediateTag) {
    const std::vector<State>::const_reverse_iterator ritEnd = state.rend();
    const std::vector<State>::const_reverse_iterator rit =
          std::find_if(static_cast<std::vector<State>::const_reverse_iterator>(state.rbegin()),
                       ritEnd, not_intermediate);
    if(likely(rit != ritEnd))
      oldState = *rit;
  }

  if(std::find(it->newStates.begin(), it->newStates.end(), oldState) ==
     it->newStates.end()) {
    dumpState("found unexpected", name);
    state.push_back(UnknownTag);
    return;
  }

  presets_element_t *widget = nullptr;

  switch(it->oldState) {
  case IntermediateTag:
    break;
  case DocStart:
  case UnknownTag:
    assert_unreachable();
  case TagPresets:
    break;
  case TagChunk: {
    const char *id = findAttribute(attrs, "id", false);
    presets_item *item = new presets_item(presets_item_t::TY_ALL, NULL_OR_VAL(id));
    items.push(item);
    break;
  }
  case TagGroup: {
    std::array<const char *, 2> names = { { "name", "icon" } };
    const AttrMap &a = findAttributes(attrs, names.data(), names.size(), 1);

    const AttrMap::const_iterator aitEnd = a.end();
    const std::string &nm = NULL_OR_MAP_STR(a.find("name"));
    const AttrMap::const_iterator icit = a.find("icon");

    std::string ic;
    if(icit != aitEnd)
      ic = josm_icon_name_adjust(icit->second, basepath, basedirfd);

    presets_item_group *group = new presets_item_group(0,
                                items.empty() ? nullptr : static_cast<presets_item_group *>(items.top()),
                                nm, ic);

    if(items.empty())
      presets.items.push_back(group);
    else
      static_cast<presets_item_group *>(items.top())->items.push_back(group);
    items.push(group);
    break;
  }
  case TagSeparator: {
    assert(!items.empty());
    assert(items.top()->type & presets_item_t::TY_GROUP);
    presets_item_separator *sep = new presets_item_separator();
    static_cast<presets_item_group *>(items.top())->items.push_back(sep);
    items.push(sep);
    break;
  }
  case TagItem: {
    std::array<const char *, 4> names = { { "name", "type", "icon", "preset_name_label" } };
    const AttrMap &a = findAttributes(attrs, names.data(), names.size(), 1);

    const AttrMap::const_iterator aitEnd = a.end();
    AttrMap::const_iterator ait = a.find("preset_name_label");
    bool addEditName = ait != aitEnd && (strcmp(ait->second, "true") == 0);

    ait = a.find("icon");
    std::string ic;
    if(ait != aitEnd)
      ic = josm_icon_name_adjust(ait->second, basepath, basedirfd);

    const char *tp = NULL_OR_MAP_VAL(a.find("type"));
    const std::string &n = NULL_OR_MAP_STR(a.find("name"));

    presets_item *item = new presets_item(josm_type_parse(tp), n, ic,
                                          addEditName);
    assert((items.top()->type & presets_item_t::TY_GROUP) != 0);
    static_cast<presets_item_group *>(items.top())->items.push_back(item);
    items.push(item);
    if(likely(!n.empty())) {
      // search again: the key must be the unlocalized name here
      itemsNames[findAttribute(attrs, "name", false)] = item;
    } else {
      dumpState("found", "item without name");
    }
    break;
  }
  case TagPresetLink: {
    const char *id = findAttribute(attrs, "preset_name", false);
    presets_element_link *link = new presets_element_link();
    assert(!items.empty());
    assert(items.top()->isItem());
    presets_item *item = static_cast<presets_item *>(items.top());

    // make sure not to insert it as a stale link in case the item is invalid,
    // as that would be deleted on it's end tag and a stale reference would remain
    // in laterLinks
    if(likely(!item->name.empty())) {
      if(unlikely(!id)) {
        dumpState("found", "preset_link without preset_name");
      } else {
        if(!resolvePresetLink(link, id))
        // these references may also target items that will only be added later
          laterLinks.push_back(LLinks::value_type(link, id));
      }
      item->widgets.push_back(link);
    }
    widgets.push(link);
    break;
  }
  case TagReference: {
    const char *id = findAttribute(attrs, "ref", false);
    presets_item *ref = nullptr;
    if(unlikely(!id)) {
      dumpState("found", "reference without ref");
    } else {
      const ChunkMap::const_iterator ait = chunks.find(id);
      if(unlikely(ait == chunks.end())) {
        dumpState("found", "reference with unresolved ref ", id);
      } else {
        ref = ait->second;
        // if this is a reference to something that only contains list_entries, then just copy them over
        if(ref->widgets.size() == 1 && ref->widgets.front()->type == WIDGET_TYPE_CHUNK_LIST_ENTRIES &&
           (widgets.top()->type == WIDGET_TYPE_COMBO || widgets.top()->type == WIDGET_TYPE_MULTISELECT)) {
          presets_element_selectable * const selitem = static_cast<presets_element_selectable *>(widgets.top());

          const presets_element_list_entry_chunks * const lechunk = static_cast<presets_element_list_entry_chunks *>(ref->widgets.front());
          selitem->values.reserve(selitem->values.size() + lechunk->values.size());
          std::copy(lechunk->values.begin(), lechunk->values.end(), std::back_inserter(selitem->values));

          selitem->display_values.reserve(selitem->display_values.size() + lechunk->display_values.size());
          std::copy(lechunk->display_values.begin(), lechunk->display_values.end(), std::back_inserter(selitem->display_values));
        // same for role entries
        } else if (ref->widgets.size() == 1 && ref->widgets.front()->type == WIDGET_TYPE_CHUNK_ROLE_ENTRIES) {
          presets_element_role_entry_chunks * const selitem = static_cast<presets_element_role_entry_chunks *>(ref->widgets.front());
          assert(items.top()->isItem());
          presets_item * const target = static_cast<presets_item *>(items.top());
          std::copy(selitem->roles.begin(), selitem->roles.end(), std::back_inserter(target->roles));
        }
      }
    }
    widgets.push(new presets_element_reference(ref));
    break;
  }
  case TagLabel: {
    const char *text = findAttribute(attrs, "text");
    widgets.push(new presets_element_label(NULL_OR_VAL(text)));
    // do not push to items, will be done in endElement()
    break;
  }
  case TagSpace:
    assert(!items.empty());
#ifndef FREMANTLE
    widget = new presets_element_separator();
#endif
    break;
  case TagText: {
    std::array<const char *, 4> names = { { "key", "text", "default", "match" } };
    const AttrMap &a = findAttributes(attrs, names.data(), names.size(), 2);
    const AttrMap::const_iterator aitEnd = a.end();

    const std::string &key = NULL_OR_MAP_STR(a.find("key"));
    const std::string &text = NULL_OR_MAP_STR(a.find("text"));
    const std::string &def = NULL_OR_MAP_STR(a.find("default"));
    const char *match = NULL_OR_MAP_VAL(a.find("match"));

    widget = new presets_element_text(key, text, def, match);
    break;
  }
  case TagKey: {
    const char *key = nullptr;
    const char *value = nullptr;
    const char *match = nullptr;
    for(unsigned int i = 0; attrs[i] != nullptr; i += 2) {
      if(strcmp(attrs[i], "key") == 0) {
        key = attrs[i + 1];
      } else if(strcmp(attrs[i], "value") == 0) {
        value = attrs[i + 1];
      } else if(strcmp(attrs[i], "match") == 0) {
        match = attrs[i + 1];
      }
    }
    widget = new presets_element_key(NULL_OR_VAL(key), NULL_OR_VAL(value),
                                    match);
    break;
  }
  case TagCheck: {
    std::array<const char *, 5> names = { { "key", "text", "value_on", "match", "default" } };
    const AttrMap &a = findAttributes(attrs, names.data(), names.size(), 2);
    const AttrMap::const_iterator aitEnd = a.end();

    const std::string &key = NULL_OR_MAP_STR(a.find("key"));
    const std::string &text = NULL_OR_MAP_STR(a.find("text"));
    const std::string &von = NULL_OR_MAP_STR(a.find("value_on"));
    const char *match = NULL_OR_MAP_VAL(a.find("match"));

    bool on = NULL_OR_MAP_STR(a.find("default")) == "on";

    widget = new presets_element_checkbox(key, text, on, match, von);
    break;
  }
  case TagLink: {
    assert(!items.empty());
    assert(items.top()->isItem());
    presets_item * const item = static_cast<presets_item *>(items.top());
    std::array<const char *, 2> names = { { "wiki", "href" } };
    const AttrMap &a = findAttributes(attrs, names.data(), names.size(), 2);
    const AttrMap::const_iterator aitEnd = a.end();

    const std::string &href = NULL_OR_MAP_STR(a.find("href"));
    const std::string &wiki = NULL_OR_MAP_STR(a.find("wiki"));
    if(unlikely(href.empty() && wiki.empty())) {
      dumpState("ignoring", "link without href and wiki");
    } else {
      if(likely(item->link.empty())) {
        if(!wiki.empty())
          item->link = "https://wiki.openstreetmap.org/wiki/" + wiki;
        else
          item->link = href;
      } else {
        dumpState("found surplus", "link");
      }
    }
    break;
  }
  case TagCombo: {
    std::array<const char *, 8> names = { { "key", "text", "display_values", "match", "default", "delimiter", "values", "editable" } };
    const AttrMap &a = findAttributes(attrs, names.data(), names.size(), 6);
    const AttrMap::const_iterator aitEnd = a.end();

    const std::string &key = NULL_OR_MAP_STR(a.find("key"));
    const std::string &text = NULL_OR_MAP_STR(a.find("text"));
    const std::string &def = NULL_OR_MAP_STR(a.find("default"));
    const char *match = NULL_OR_MAP_VAL(a.find("match"));
    const char *display_values= NULL_OR_MAP_VAL(a.find("display_values"));
    const char *values = NULL_OR_MAP_VAL(a.find("values"));
    const char *del = NULL_OR_MAP_VAL(a.find("delimiter"));
    const AttrMap::const_iterator ait = a.find("editable");
    bool editable = ait == aitEnd || (strcmp(ait->second, "false") != 0);

    char delimiter = ',';
    if(del != nullptr) {
      if(unlikely(strlen(del) != 1))
        dumpState("found", "combo with invalid delimiter ", del);
      else
        delimiter = *del;
    }

    if(unlikely(!values && display_values)) {
      dumpState("found", "combo with display_values but not values");
      display_values = nullptr;
    }
    widget = new presets_element_combo(key, text, def, match,
                                      presets_element_selectable::split_string(values, delimiter),
                                      presets_element_selectable::split_string(display_values, delimiter),
                                      editable);
    break;
  }
  case TagMultiselect: {
    std::array<const char *, 8> names = { { "key", "text", "display_values", "match", "default", "delimiter", "values", "rows" } };
    const AttrMap &a = findAttributes(attrs, names.data(), names.size(), 6);
    const AttrMap::const_iterator aitEnd = a.end();

    const std::string &key = NULL_OR_MAP_STR(a.find("key"));
    const std::string &text = NULL_OR_MAP_STR(a.find("text"));
    const std::string &def = NULL_OR_MAP_STR(a.find("default"));
    const char *match = NULL_OR_MAP_VAL(a.find("match"));
    const char *display_values= NULL_OR_MAP_VAL(a.find("display_values"));
    const char *values = NULL_OR_MAP_VAL(a.find("values"));
    const char *del = NULL_OR_MAP_VAL(a.find("delimiter"));
    const char *rowstr = NULL_OR_MAP_VAL(a.find("rows"));

    char delimiter = ';';
    if(del != nullptr) {
      if(unlikely(strlen(del) != 1))
        dumpState("found", "combo with invalid delimiter ", del);
      else
        delimiter = *del;
    }

    if(unlikely(!values && display_values)) {
      dumpState("found", "combo with display_values but not values");
      display_values = nullptr;
    }

    unsigned int rows = 0;
    if(rowstr != nullptr) {
      char *endp;
      rows = strtoul(rowstr, &endp, 10);
      if(unlikely(*endp != '\0')) {
        dumpState("ignoring invalid count value of", "rows");
        rows = 0;
      }
    }

    widget = new presets_element_multiselect(key, text, def, match, delimiter,
                                      presets_element_selectable::split_string(values, delimiter),
                                      presets_element_selectable::split_string(display_values, delimiter), rows);
    break;
  }
  case TagListEntry: {
    assert(!items.empty());
    presets_element_selectable *sel;
    if(oldState == TagChunk) {
      // to store list_entries we need a special container as they are not standalone items
      presets_item * const pit = static_cast<presets_item *>(items.top());
      if(pit->widgets.empty())
        pit->widgets.push_back(new presets_element_list_entry_chunks());
      assert_cmpnum(pit->widgets.size(), 1);
      assert_cmpnum(pit->widgets.back()->type, WIDGET_TYPE_CHUNK_LIST_ENTRIES);
      sel = static_cast<presets_element_selectable *>(pit->widgets.back());
    } else {
      assert(!widgets.empty());
      assert(widgets.top()->type == WIDGET_TYPE_COMBO || widgets.top()->type == WIDGET_TYPE_MULTISELECT);
      sel = static_cast<presets_element_selectable *>(widgets.top());
    }

    std::array<const char *, 2> names = { { "display_value", "value" } };
    const AttrMap &a = findAttributes(attrs, names.data(), names.size(), 3);
    const AttrMap::const_iterator aitEnd = a.end();

    const char *value = NULL_OR_MAP_VAL(a.find("value"));

    if(unlikely(!value)) {
      dumpState("found", "list_entry without value");
    } else {
      sel->values.push_back(value);
      const char *dv = NULL_OR_MAP_VAL(a.find("display_value"));
      // make sure there is always a string to show, in case all elements have the
      // default value the list will be cleared when the containing widget tag is
      // closed
      sel->display_values.push_back(dv == nullptr ? value : dv);
    }
    break;
  }
  case TagRoles:
    assert(!items.empty());
    break;
  case TagRole: {
    assert(!items.empty());
    assert(items.top()->isItem());
    std::vector<presets_item::role> *roles;

    if(oldState == TagChunk) {
      // to store role entries we need a special container as they are not standalone items
      presets_item * const pit = static_cast<presets_item *>(items.top());
      if(pit->widgets.empty())
        pit->widgets.push_back(new presets_element_role_entry_chunks());
      assert_cmpnum(pit->widgets.size(), 1);
      assert_cmpnum(pit->widgets.back()->type, WIDGET_TYPE_CHUNK_ROLE_ENTRIES);
      roles = &static_cast<presets_element_role_entry_chunks *>(pit->widgets.back())->roles;
    } else {
      presets_item * const item = static_cast<presets_item *>(items.top());
      roles = &item->roles;
    }

    std::array<const char *, 4> names = { { "key", "type", "count", "regexp" } };
    const AttrMap &a = findAttributes(attrs, names.data(), names.size(), 0);
    const AttrMap::const_iterator aitEnd = a.end();

    // ignore roles marked as regexp, this is not implemented yet
    if(likely(a.find("regexp") == aitEnd)) {
      const std::string &key = NULL_OR_MAP_STR(a.find("key"));
      const char *tp = NULL_OR_MAP_VAL(a.find("type"));
      const char *cnt = NULL_OR_MAP_VAL(a.find("count"));
      unsigned int count = 0;
      if(cnt != nullptr) {
        char *endp;
        count = strtoul(cnt, &endp, 10);
        if(unlikely(*endp != '\0')) {
          dumpState("ignoring invalid count value of", "role");
          count = 0;
        }
      }

      roles->push_back(presets_item::role(key, josm_type_parse(tp), count));
    }
    break;
  }
  }

  state.push_back(it->oldState);
  if(widget != nullptr) {
    assert(!items.empty());
    assert(items.top()->isItem());
    widgets.push(widget);
    static_cast<presets_item *>(items.top())->widgets.push_back(widget);
  }
}

void PresetSax::endElement(const char *name)
{
  if(state.back() == UnknownTag) {
    state.pop_back();
    return;
  }

  const StateMap &tags = preset_state_map();
  StateMap::const_iterator it = std::find_if(tags.begin(), tags.end(),
                                             str_map_find<StateMap>(reinterpret_cast<const char *>(name)));

  assert(it != tags.end() || state.back() == UnknownTag);
  assert(state.back() == it->oldState);
  state.pop_back();

  switch(it->oldState) {
  case DocStart:
  case UnknownTag:
    assert_unreachable();
  case TagLink:
  case TagListEntry:
  case TagPresets:
  case IntermediateTag:
  case TagRoles:
  case TagRole:
    break;
  case TagItem: {
    assert_cmpnum(0, widgets.size());
    assert(!items.empty());
    assert(items.top()->isItem());
    presets_item * const item = static_cast<presets_item *>(items.top());
    items.pop();
    if(unlikely(item->name.empty())) {
      /* silently delete, was warned about before */
      delete item;
      break;
    } else {
      // update the group type
      assert(!items.empty());
      presets_item_t * const group = items.top();
      assert((group->type & presets_item_t::TY_GROUP) != 0);
      *const_cast<unsigned int *>(&group->type) |= item->type;
      if(unlikely(!item->roles.empty() && (item->type & (presets_item_t::TY_RELATION | presets_item_t::TY_MULTIPOLYGON)) == 0)) {
        dumpState("found", "item with roles, but type does not match relations or multipolygons");
        item->roles.clear();
      }
      break;
    }
  }
  case TagSeparator:
    assert(!items.empty());
    assert_cmpnum(items.top()->type, presets_item_t::TY_SEPARATOR);
    items.pop();
    break;
  case TagGroup: {
    assert(!items.empty());
    const presets_item_t * const item = items.top();
    assert((item->type & presets_item_t::TY_GROUP) != 0);
    items.pop();
    // update the parent group type
    if(!items.empty()) {
      presets_item_t * const group = items.top();
      assert((group->type & presets_item_t::TY_GROUP) != 0);
      *const_cast<unsigned int *>(&group->type) |= item->type;
    }
    break;
  }
  case TagChunk: {
    assert(!items.empty());
    std::unique_ptr<presets_item> chunk(static_cast<presets_item *>(items.top()));
    assert_cmpnum(chunk->type, presets_item_t::TY_ALL);
    items.pop();
    if(unlikely(chunk->name.empty())) {
      dumpState("ignoring", "chunk without id");
      return;
    }

    const std::string &id = chunk->name;

    if(unlikely(chunks.find(id) != chunks.end()))
      dumpState("ignoring", "chunk with duplicate id ", id.c_str());
    else
      chunks[id] = chunk.release();

    // if this was a top level chunk no active widgets should remain
    assert(!items.empty() || widgets.empty());
    break;
  }
  case TagReference: {
    assert(!items.empty());
    assert(items.top()->isItem());
    assert(!widgets.empty());
    std::unique_ptr<presets_element_reference> ref(static_cast<presets_element_reference *>(widgets.top()));
    widgets.pop();
    assert_cmpnum(ref->type, WIDGET_TYPE_REFERENCE);
    if(unlikely(ref->item == nullptr)) {
    // if this is just a collection of elements that has been inserted
    // then drop the pseudo widget, all information is in the actual item now
    } else if(ref->item->widgets.size() == 1 && ref->item->widgets.front()->type >= WIDGET_TYPE_CHUNK_CONTAINER) {
    } else
      static_cast<presets_item *>(items.top())->widgets.push_back(ref.release());
    break;
  }
  case TagLabel: {
    assert(!items.empty());
    assert(!widgets.empty());
    std::unique_ptr<presets_element_label> label(static_cast<presets_element_label *>(widgets.top()));
    widgets.pop();
    if(unlikely(label->text.empty()))
      dumpState("ignoring", "label without text");
    else
      static_cast<presets_item *>(items.top())->widgets.push_back(label.release());
    break;
  }
  case TagSpace:
    assert(!items.empty());
#ifndef FREMANTLE
    assert(!widgets.empty());
    widgets.pop();
#endif
    break;
  case TagCombo: {
    assert(!items.empty());
    assert(!widgets.empty());
    presets_element_combo * const combo = static_cast<presets_element_combo *>(widgets.top());
    widgets.pop();
    if(unlikely(combo->key.empty())) {
      dumpState("ignoring", "combo without key");
      delete combo;
    }
    // this usually happens when the list if filled by <list_entry> tags and
    // none of that has a display_value given
    if(unlikely(combo->values == combo->display_values))
      combo->display_values.clear();
    break;
  }
  case TagMultiselect: {
    assert(!items.empty());
    assert(!widgets.empty());
    presets_element_multiselect * const ms = static_cast<presets_element_multiselect *>(widgets.top());
    widgets.pop();
    if(unlikely(ms->key.empty())) {
      dumpState("ignoring", "multiselect without key");
      delete ms;
    }
    // this usually happens when the list if filled by <list_entry> tags and
    // none of that has a display_value given
    if(unlikely(ms->values == ms->display_values))
      ms->display_values.clear();
    break;
  }
  case TagText:
  case TagKey:
  case TagCheck:
  case TagPresetLink:
    assert(!items.empty());
    assert(!widgets.empty());
    widgets.pop();
    break;
  }
}

inline presets_item_t *
chunkFromPair(const ChunkMap::value_type &p)
{
  return p.second;
}

} // namespace

bool presets_items_internal::addFile(const std::string &filename, const std::string &basepath, int basefd)
{
  printf("... %s\n", filename.c_str());

  PresetSax p(*this, basepath, basefd);
  if(!p.parse(filename))
    return false;

  // now move all chunks to the presets list
  chunks.reserve(chunks.size() + p.chunks.size());
  std::transform(p.chunks.begin(), p.chunks.end(), std::back_inserter(chunks), chunkFromPair);

  return true;
}

void presets_items_internal::lru_update(const presets_item_t *item)
{
  const std::vector<const presets_item_t *>::iterator litBegin = lru.begin();
  const std::vector<const presets_item_t *>::iterator litEnd = lru.end();
  const std::vector<const presets_item_t *>::iterator lit = std::find(litBegin, litEnd, item);
  if(lit == litEnd) {
    // drop the oldest ones if too many
    if(lru.size() >= LRU_MAX)
      lru.resize(LRU_MAX - 1);
    lru.insert(litBegin, item);
  // if it is already the first item in the list nothing is to do
  } else if(lit != litBegin) {
    // move to front
    std::rotate(litBegin, lit, lit + 1);
  }
}

presets_items *presets_items::load()
{
  printf("Loading JOSM presets ...\n");

  std::unique_ptr<presets_items_internal> presets(std::make_unique<presets_items_internal>());

  const std::string &filename = find_file("defaultpresets.xml");
  if(likely(!filename.empty()))
    presets->addFile(filename, std::string(), -1);

  // check for user presets
  dirguard dir(osm2go_platform::userdatapath());

  if(dir.valid()) {
    dirent *d;
    std::string xmlname;
    while ((d = dir.next()) != nullptr) {
      if(d->d_type != DT_DIR && d->d_type != DT_UNKNOWN)
        continue;
      if(strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
        continue;

      dirguard pdir(dir, d->d_name);
      if(likely(pdir.valid())) {
        // find first XML file inside those directories
        dirent *pd;
        while ((pd = pdir.next()) != nullptr) {
          if(pd->d_type == DT_DIR)
            continue;
          const size_t nlen = strlen(pd->d_name);
          if(nlen > 4 && strcasecmp(pd->d_name + nlen - 4, ".xml") == 0) {
            presets->addFile(pdir.path() + pd->d_name, pdir.path(), pdir.dirfd());
            break;
          }
        }
      }
    }
  }

  if(unlikely(presets->items.empty()))
    return nullptr;

  return presets.release();
}

/* ----------------------- cleaning up --------------------- */

struct MatchValue {
  MatchValue(const char *n, presets_element_t::Match m) : name(n), match(m) {}
  const char *name;
  presets_element_t::Match match;
};

presets_element_t::Match presets_element_t::parseMatch(const char *matchstring, Match def)
{
  if(matchstring == nullptr)
    return def;

  typedef std::vector<MatchValue> VMap;
  static VMap matches;
  if(unlikely(matches.empty())) {
    matches.push_back(VMap::value_type("none", MatchIgnore));
    matches.push_back(VMap::value_type("key", MatchKey));
    matches.push_back(VMap::value_type("key!", MatchKey_Force));
    matches.push_back(VMap::value_type("keyvalue", MatchKeyValue));
    matches.push_back(VMap::value_type("keyvalue!", MatchKeyValue_Force));
  }
  const VMap::const_iterator itEnd = matches.end();
  const VMap::const_iterator it = std::find_if(std::cbegin(matches),
                                               itEnd, str_map_find<VMap>(matchstring));

  return (it == itEnd) ? def : it->match;
}

presets_element_t::presets_element_t(presets_element_type_t t, Match m, const std::string &k, const std::string &txt)
  : type(t)
  , key(k)
  , text(txt)
  , match(m)
{
}

bool presets_element_t::is_interactive() const
{
  switch(type) {
  case WIDGET_TYPE_LABEL:
  case WIDGET_TYPE_SEPARATOR:
  case WIDGET_TYPE_SPACE:
  case WIDGET_TYPE_KEY:
    return false;
  default:
    return true;
  }
}

presets_element_t::attach_key *presets_element_t::attach(preset_attach_context &,
                                                         const std::string &) const
{
  return nullptr;
}

std::string presets_element_t::getValue(presets_element_t::attach_key *) const
{
  assert_unreachable();
}

int presets_element_t::matches(const osm_t::TagMap &tags, bool) const
{
  if(match == MatchIgnore)
    return 0;

  const osm_t::TagMap::const_iterator itEnd = tags.end();
  const osm_t::TagMap::const_iterator it = tags.find(key);

  if(it == itEnd) {
    switch(match) {
    case MatchKey:
    case MatchKeyValue:
      return 0;
    default:
      return -1;
    }
  }

  if(match == MatchKey || match == MatchKey_Force)
    return 1;

  if(matchValue(it->second))
    return 1;

  return match == MatchKeyValue_Force ? -1 : 0;
}

presets_element_text::presets_element_text(const std::string &k, const std::string &txt,
                                         const std::string &deflt, const char *m)
  : presets_element_t(WIDGET_TYPE_TEXT, parseMatch(m), k, txt)
  , def(deflt)
{
}

presets_element_selectable::presets_element_selectable(presets_element_type_t t, const std::string &k,
                                                       const std::string &txt, const std::string &deflt,
                                                       const char *m, std::vector<std::string> vals,
                                                       std::vector<std::string> dvals, bool canEdit)
  : presets_element_t(t, parseMatch(m), k, txt)
  , def(deflt)
  , values(std::move(vals))
  , display_values(std::move(dvals))
  , editable(canEdit)
{
  if (!display_values.empty()) {
    size_t dsz = display_values.size();
    const size_t vsz = values.size();
    // catch mismatches of the array sizes
    if (unlikely(dsz < vsz)) {
      printf("WARNING: got %zu values, but %zu display_values, filling with values\n", vsz, dsz);
      // simply use the values as display_values as it would have happened on empty display_values list
      while (dsz < vsz)
        display_values.push_back(values[dsz++]);
    } else if (unlikely(dsz > vsz)) {
      // drop the superfluous values at the back
      printf("WARNING: got %zu values, but %zu display_values, truncating\n", vsz, dsz);
      display_values.resize(vsz);
    }
  }
}

std::vector<std::string> presets_element_selectable::split_string(const char *str, const char delimiter)
{
  std::vector<std::string> ret;

  if(str == nullptr)
    return ret;

  const char *c, *p = str;
  while((c = strchr(p, delimiter)) != nullptr) {
    ret.push_back(std::string(p, c - p));
    p = c + 1;
  }
  /* attach remaining string as last value */
  ret.push_back(p);

  // this vector will never be appended to again, so shrink it to the size
  // that is actually needed
  shrink_to_fit(ret);

  return ret;
}

presets_element_combo::presets_element_combo(const std::string &k, const std::string &txt,
                                           const std::string &deflt, const char *m,
                                           std::vector<std::string> vals, std::vector<std::string> dvals,
                                           bool canEdit)
  : presets_element_selectable(WIDGET_TYPE_COMBO, k, txt, deflt, m, std::move(vals), std::move(dvals), canEdit)
{
}

presets_element_multiselect::presets_element_multiselect(const std::string &k, const std::string &txt,
                                                         const std::string &deflt, const char *m, char del,
                                                         std::vector<std::string> vals,
                                                         std::vector<std::string> dvals, unsigned int rws)
  : presets_element_selectable(WIDGET_TYPE_MULTISELECT, k, txt, deflt, m, std::move(vals), std::move(dvals), false)
  , delimiter(del)
#ifndef FREMANTLE
  , rows_height(rws == 0 ? std::min(8, static_cast<int>(values.size())) : rws)
#endif
{
#ifdef FREMANTLE
  (void)rws;
#endif
}

presets_element_list_entry_chunks::presets_element_list_entry_chunks()
  : presets_element_selectable(WIDGET_TYPE_CHUNK_LIST_ENTRIES, std::string(), std::string(),
                               std::string(), nullptr, std::vector<std::string>(), std::vector<std::string>(), false)
{
}

presets_element_role_entry_chunks::presets_element_role_entry_chunks()
  : presets_element_selectable(WIDGET_TYPE_CHUNK_ROLE_ENTRIES, std::string(), std::string(),
                               std::string(), nullptr, std::vector<std::string>(), std::vector<std::string>(), false)
{
}

presets_element_key::presets_element_key(const std::string &k, const std::string &val,
                                       const char *m)
  : presets_element_t(WIDGET_TYPE_KEY, parseMatch(m, MatchKeyValue_Force), k)
  , value(val)
{
}

presets_element_checkbox::presets_element_checkbox(const std::string &k, const std::string &txt,
                                                 bool deflt, const char *m, const std::string &von)
  : presets_element_t(WIDGET_TYPE_CHECK, parseMatch(m), k, txt)
  , def(deflt)
  , value_on(von)
{
}

bool presets_element_reference::is_interactive() const
{
  return std::any_of(item->widgets.begin(), item->widgets.end(), presets_element_t::isInteractive);
}

unsigned int presets_element_reference::rows() const
{
  return std::accumulate(item->widgets.begin(), item->widgets.end(), 0, widget_rows);
}

presets_item::~presets_item()
{
  std::for_each(widgets.begin(), widgets.end(), std::default_delete<presets_element_t>());
}

presets_item_group::presets_item_group(const unsigned int types, presets_item_group *p,
                                       const std::string &n, const std::string &ic)
  : presets_item_named(types | TY_GROUP, n, ic), parent(p)
{
  assert(p == nullptr || ((p->type & TY_GROUP) != 0));
}

presets_item_group::~presets_item_group()
{
  std::for_each(items.begin(), items.end(), std::default_delete<presets_item_t>());
}

presets_items_internal::presets_items_internal()
  : presets_items()
{
  lru.reserve(LRU_MAX);
}

presets_items_internal::~presets_items_internal()
{
  std::for_each(items.begin(), items.end(), std::default_delete<presets_item_t>());
  std::for_each(chunks.begin(), chunks.end(), std::default_delete<presets_item_t>());
}
