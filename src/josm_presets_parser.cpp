/*
 * Copyright (C) 2017 Rolf Eike Beer <eike@sf-mail.de>
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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "josm_presets_p.h"

#include "misc.h"

#include <algorithm>
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <numeric>
#include <stack>
#include <strings.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

typedef std::map<std::string, presets_item *> ChunkMap;

/* --------------------- presets.xml parsing ----------------------- */

std::string josm_icon_name_adjust(const char *name, const std::string &basepath) {
  std::string ret;

  if(!name)
    return ret;

  if(!basepath.empty()) {
    ret = basepath + name;
    if(g_file_test(ret.c_str(), G_FILE_TEST_IS_REGULAR))
      return ret;
  }

  size_t len = strlen(name);

  if(G_LIKELY(len > 4)) {
    const char * const ext = name + len - 4;
    /* the icon loader uses names without extension */
    if(strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".svg") == 0)
      len -= 4;
  }

  ret.assign(name, len);

  return ret;
}

static std::map<int, std::string> type_map_init() {
  std::map<int, std::string> ret;

  ret[presets_item_t::TY_WAY] = "way";
  ret[presets_item_t::TY_NODE] = "node";
  ret[presets_item_t::TY_RELATION] = "relation";
  ret[presets_item_t::TY_CLOSED_WAY] = "closedway";
  ret[presets_item_t::TY_MULTIPOLYGON] = "multipolygon";

  return ret;
}

static int josm_type_bit(const char *type, char sep) {
  static const std::map<int, std::string> types = type_map_init();
  static const std::map<int, std::string>::const_iterator itEnd = types.end();

  for(std::map<int, std::string>::const_iterator it = types.begin(); it != itEnd; it++) {
    const size_t tlen = it->second.size();
    if(strncmp(it->second.c_str(), type, tlen) == 0 && type[tlen] == sep)
      return it->first;
  }

  printf("WARNING: unexpected type %s\n", type);
  return 0;
}

/* parse a comma seperated list of types and set their bits */
static unsigned int josm_type_parse(const char *type) {
  unsigned int type_mask = 0;
  if(!type) return presets_item_t::TY_ALL;

  const char *ntype = strchr(type, ',');
  while(ntype) {
    type_mask |= josm_type_bit(type, ',');
    type = ntype + 1;
    ntype = strchr(type, ',');
  }

  type_mask |= josm_type_bit(type, '\0');
  return type_mask;
}

// custom find to avoid memory allocations for std::string
template<typename T>
struct str_map_find {
  const char * const name;
  str_map_find(const char * n)
    : name(n) {}
  bool operator()(const typename T::value_type &p) {
    return (strcmp(p.first, name) == 0);
  }
};

class PresetSax {
  xmlSAXHandler handler;

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

  // not a stack because that can't be iterated
  std::vector<State> state;
  std::stack<presets_item_t *> items; // the current item stack (i.e. menu layout)
  std::stack<presets_widget_t *> widgets; // the current widget stack (i.e. dialog layout)
  presets_items &presets;
  const std::string &basepath;
  const std::vector<std::string> &langs;

  // this maps the XML tag name to the target state and the list of allowed source states
  typedef std::multimap<const char *, std::pair<State, const std::vector<State> > > StateMap;
  static const StateMap &preset_state_map();

  // Map back a state to it's string. Only used for debug messages.
  struct name_find {
    const State state;
    name_find(State s) : state(s) {}
    bool operator()(const StateMap::value_type &p) {
      return p.second.first == state;
    }
  };

  bool resolvePresetLink(presets_widget_link* link, const std::string &id);

  void dumpState(const char *before = O2G_NULLPTR, const char *after = O2G_NULLPTR) const;

public:
  explicit PresetSax(presets_items &p, const std::string &b);

  bool parse(const std::string &filename);

  ChunkMap chunks;

  typedef std::vector<std::pair<presets_widget_link *, std::string> > LLinks;

  ChunkMap itemsNames;
  LLinks laterLinks; ///< unresolved preset_link references

private:
  struct find_link_ref {
    PresetSax &px;
    find_link_ref(PresetSax &p) : px(p) {}
    void operator()(LLinks::value_type &l);
  };

  void characters(const char *ch, int len);
  static void cb_characters(void *ts, const xmlChar *ch, int len) {
    static_cast<PresetSax *>(ts)->characters(reinterpret_cast<const char *>(ch), len);
  }
  void startElement(const char *name, const char **attrs);
  static void cb_startElement(void *ts, const xmlChar *name, const xmlChar **attrs) {
    static_cast<PresetSax *>(ts)->startElement(reinterpret_cast<const char *>(name),
                                               reinterpret_cast<const char **>(attrs));
  }
  void endElement(const xmlChar *name);
  static void cb_endElement(void *ts, const xmlChar *name) {
    static_cast<PresetSax *>(ts)->endElement(name);
  }

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

  typedef std::map<const char *, const char *> AttrMap;
  /**
   * @brief find the attributes with the given names
   * @param attrs the attribute list to search
   * @param names the keys to look for
   * @param langflags if localized keys should be preferred (bitwise positions of entries in names)
   * @returns the found keys
   */
  AttrMap findAttributes(const char **attrs, const char **names, unsigned int langflags = 0) const;
};

const PresetSax::StateMap &PresetSax::preset_state_map() {
  static PresetSax::StateMap map;

  if(map.empty()) {
#if __cplusplus >= 201103L
    const StateMap::mapped_type::second_type item_chunks = { TagChunk, TagItem };
    const StateMap::mapped_type::second_type pr_gr = { TagPresets, TagGroup };
# define VECTOR_ONE(a) { a }
#else
    std::vector<State> item_chunks;
    item_chunks.push_back(TagChunk);
    item_chunks.push_back(TagItem);
    std::vector<State> pr_gr;
    pr_gr.push_back(TagPresets);
    pr_gr.push_back(TagGroup);
# define VECTOR_ONE(a) std::vector<State>(1, (a))
#endif

    map.insert(StateMap::value_type("presets", StateMap::mapped_type(TagPresets, VECTOR_ONE(DocStart))));
    map.insert(StateMap::value_type("chunk", StateMap::mapped_type(TagChunk, VECTOR_ONE(TagPresets))));
    map.insert(StateMap::value_type("group", StateMap::mapped_type(TagGroup, pr_gr)));

    // ignore the case of standalone items and separators for now as it does not happen yet
    map.insert(StateMap::value_type("item", StateMap::mapped_type(TagItem, VECTOR_ONE(TagGroup))));
    map.insert(StateMap::value_type("separator", StateMap::mapped_type(TagSeparator, VECTOR_ONE(TagGroup))));

    map.insert(StateMap::value_type("reference", StateMap::mapped_type(TagReference, item_chunks)));
    map.insert(StateMap::value_type("preset_link", StateMap::mapped_type(TagPresetLink, item_chunks)));
    map.insert(StateMap::value_type("key", StateMap::mapped_type(TagKey, item_chunks)));
    map.insert(StateMap::value_type("text", StateMap::mapped_type(TagText, item_chunks)));
    map.insert(StateMap::value_type("combo", StateMap::mapped_type(TagCombo, item_chunks)));
    map.insert(StateMap::value_type("list_entry", StateMap::mapped_type(TagListEntry, VECTOR_ONE(TagCombo))));
    map.insert(StateMap::value_type("check", StateMap::mapped_type(TagCheck, item_chunks)));
    map.insert(StateMap::value_type("label", StateMap::mapped_type(TagLabel, item_chunks)));
    map.insert(StateMap::value_type("space", StateMap::mapped_type(TagSpace, item_chunks)));
    map.insert(StateMap::value_type("link", StateMap::mapped_type(TagLink, item_chunks)));
    map.insert(StateMap::value_type("roles", StateMap::mapped_type(TagRoles, item_chunks)));
    map.insert(StateMap::value_type("role", StateMap::mapped_type(TagRole, VECTOR_ONE(TagRoles))));

    map.insert(StateMap::value_type("checkgroup", StateMap::mapped_type(IntermediateTag, item_chunks)));
    map.insert(StateMap::value_type("optional", StateMap::mapped_type(IntermediateTag, item_chunks)));
  }

  return map;
}

void PresetSax::dumpState(const char *before, const char *after) const
{
  if(before != O2G_NULLPTR)
    printf("%s ", before);
  const StateMap &tags = preset_state_map();
  std::vector<State>::const_iterator itEnd = state.end();
  for(std::vector<State>::const_iterator it = state.begin() + 1; it != itEnd; it++) {
    if(*it == UnknownTag || *it == IntermediateTag) {
      printf("*/");
    } else {
      const StateMap::const_iterator nit = std::find_if(tags.begin(), tags.end(), name_find(*it));
      g_assert(nit != tags.end());
      printf("%s/", nit->first);
    }
  }
  if(after != O2G_NULLPTR)
    printf("%s", after);
}

/**
 * @brief get the user language strings from environment
 */
const std::vector<std::string> &userLangs()
{
  static std::vector<std::string> lcodes;
  if(lcodes.empty()) {
    const char *lcm = getenv("LC_MESSAGES");
    if(!lcm)
      lcm = getenv("LANG");
    if(lcm && *lcm) {
      std::string lc = lcm;
      std::string::size_type d = lc.find('.');
      if(d != std::string::npos)
        lc.erase(d);
      lcodes.push_back(lc + '.');
      d = lc.find('_');
      if(d != std::string::npos)
        lcodes.push_back(lc.substr(0, d) + '.');
    }
  }

  return lcodes;
}

PresetSax::PresetSax(presets_items &p, const std::string &b)
  : presets(p)
  , basepath(b)
  , langs(userLangs())
{
  memset(&handler, 0, sizeof(handler));
  handler.characters = cb_characters;
  handler.startElement = cb_startElement;
  handler.endElement = cb_endElement;

  state.push_back(DocStart);
}

struct find_link_parent {
  const presets_widget_link *link;
  find_link_parent(const presets_widget_link *l) : link(l) {}
  bool operator()(presets_item_t *t);
  bool operator()(const ChunkMap::value_type &p) {
    return operator()(p.second);
  }
};

bool find_link_parent::operator()(presets_item_t *t)
{
  if(t->type & presets_item_t::TY_GROUP) {
    const presets_item_group * const gr = static_cast<presets_item_group *>(t);
    return std::find_if(gr->items.begin(), gr->items.end(), *this) != gr->items.end();
  }

  if(!t->isItem())
    return false;

  presets_item * const item = static_cast<presets_item *>(t);
  const std::vector<presets_widget_t *>::iterator it = std::find(item->widgets.begin(),
                                                                 item->widgets.end(),
                                                                 link);
  if(it == item->widgets.end())
    return false;
  item->widgets.erase(it);
  delete link;
  link = O2G_NULLPTR;

  return true;
}

void PresetSax::find_link_ref::operator()(PresetSax::LLinks::value_type &l)
{
  if(G_UNLIKELY(!px.resolvePresetLink(l.first, l.second))) {
    printf("found preset_link with unmatched preset_name '%s'\n", l.second.c_str());
    find_link_parent fc(l.first);
    std::vector<presets_item_t *>::const_iterator it =
        std::find_if(px.presets.items.begin(), px.presets.items.end(), fc);
    if(it == px.presets.items.end()) {
      const ChunkMap::const_iterator cit =
          std::find_if(px.chunks.begin(), px.chunks.end(), fc);
      g_assert(cit != px.chunks.end());
    }
  }
}

bool PresetSax::parse(const std::string &filename)
{
  if (xmlSAXUserParseFile(&handler, this, filename.c_str()) != 0)
    return false;

  std::for_each(laterLinks.begin(), laterLinks.end(), find_link_ref(*this));

  return state.size() == 1;
}

bool PresetSax::resolvePresetLink(presets_widget_link *link, const std::string &id)
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
    if(!isspace(ch[pos])) {
      printf("unhandled character data: %*.*s state %i\n", len, len, ch, state.back());
      return;
    }
}

const char *PresetSax::findAttribute(const char **attrs, const char *name, bool useLang) const {
  // If the entire key matches name this is the non-localized (i.e. fallback)
  // key. Continue search to find a localized text, if no other is found return
  // the defaut one.
  const char *c = O2G_NULLPTR;

  for(unsigned int i = 0; attrs[i]; i += 2) {
    // Check if the given attribute begins with one of the preferred language
    // codes. If yes, skip over the language code and check this one.
    const char *a = attrs[i];
    for(std::vector<std::string>::size_type j = 0; (j < langs.size()) && useLang; j++) {
      if(strncmp(a, langs[j].c_str(), langs[j].size()) == 0) {
        a += langs[j].size();
        break;
      }
    }

    if(strcmp(a, name) == 0) {
      const char *ret;
      if(*(attrs[i + 1]) == '\0')
        ret = O2G_NULLPTR;
      else
        ret = attrs[i + 1];
      if(a != attrs[i])
        return ret;
      c = ret;
    }
  }

  return c;
}

PresetSax::AttrMap PresetSax::findAttributes(const char **attrs, const char **names, unsigned int langflags) const
{
  AttrMap ret;

  for(unsigned int i = 0; attrs[i]; i += 2) {
    // Check if the given attribute begins with one of the preferred language
    // codes. If yes, skip over the language code and check this one.
    const char *a = attrs[i];
    bool isLoc = false;
    for(std::vector<std::string>::size_type j = 0; (j < langs.size()); j++) {
      if(strncmp(a, langs[j].c_str(), langs[j].size()) == 0) {
        a += langs[j].size();
        isLoc = true;
        break;
      }
    }

    for(unsigned int j = 0; names[j]; j++) {
      if(strcmp(a, names[j]) == 0) {
        // if this is localized and no localization was permitted: skip
        if(isLoc && !(langflags & (1 << j)))
          continue;

        if(*(attrs[i + 1]) != '\0') {
          // if this is localized: store, if not, store only if nothing in map right now
          if(isLoc || !ret[names[j]])
            ret[names[j]] = attrs[i + 1];
        }
      }
    }
  }

  return ret;
}

#define NULL_OR_VAL(a) (a ? a : std::string())
#define NULL_OR_MAP_STR(it) (it != itEnd ? it->second : std::string())
#define NULL_OR_MAP_VAL(it) (it != itEnd ? it->second : O2G_NULLPTR)

void PresetSax::startElement(const char *name, const char **attrs)
{
  const StateMap &tags = preset_state_map();
  StateMap::const_iterator it = std::find_if(tags.begin(), tags.end(),
                                             str_map_find<StateMap>(name));
  if(it == tags.end()) {
    dumpState("found unhandled", name);
    printf("\n");
    state.push_back(UnknownTag);
    return;
  }

  // ignore IntermediateTag when checking for valid parent tags
  State oldState = state.back();
  if(oldState == IntermediateTag) {
    const std::vector<State>::const_reverse_iterator itEnd = state.rend();
    for(std::vector<State>::const_reverse_iterator it = state.rbegin() + 1; it != itEnd; it++)
      if(*it != IntermediateTag) {
        oldState = *it;
        break;
      }
  }

  if(std::find(it->second.second.begin(), it->second.second.end(), oldState) ==
     it->second.second.end()) {
    dumpState("found unexpected", name);
    printf("\n");
    state.push_back(UnknownTag);
    return;
  }

  presets_widget_t *widget = O2G_NULLPTR;

  switch(it->second.first) {
  case IntermediateTag:
    break;
  case DocStart:
  case UnknownTag:
    g_assert_not_reached();
    return;
  case TagPresets:
    break;
  case TagChunk: {
    const char *id = findAttribute(attrs, "id", false);
    presets_item *item = new presets_item(presets_item_t::TY_ALL, NULL_OR_VAL(id));
    items.push(item);
    break;
  }
  case TagGroup: {
    const char *names[] = { "name", "icon", O2G_NULLPTR };
    const AttrMap &a = findAttributes(attrs, names, 1);

    const AttrMap::const_iterator itEnd = a.end();
    const std::string &name = NULL_OR_MAP_STR(a.find("name"));
    const AttrMap::const_iterator icit = a.find("icon");

    std::string ic;
    if(icit != itEnd)
      ic = josm_icon_name_adjust(icit->second, basepath);

    presets_item_group *group = new presets_item_group(0,
                                items.empty() ? O2G_NULLPTR : static_cast<presets_item_group *>(items.top()),
                                name, ic);

    if(items.empty())
      presets.items.push_back(group);
    else
      static_cast<presets_item_group *>(items.top())->items.push_back(group);
    items.push(group);
    break;
  }
  case TagSeparator: {
    g_assert_false(items.empty());
    g_assert(items.top()->type & presets_item_t::TY_GROUP);
    presets_item_separator *sep = new presets_item_separator();
    static_cast<presets_item_group *>(items.top())->items.push_back(sep);
    items.push(sep);
    break;
  }
  case TagItem: {
    const char *names[] = { "name", "type", "icon", "preset_name_label", O2G_NULLPTR };
    const AttrMap &a = findAttributes(attrs, names, 1);

    const AttrMap::const_iterator itEnd = a.end();
    AttrMap::const_iterator it = a.find("preset_name_label");
    bool addEditName = it != itEnd && (strcmp(it->second, "true") == 0);

    it = a.find("icon");
    std::string ic;
    if(it != itEnd)
      ic = josm_icon_name_adjust(it->second, basepath);

    const char *tp = NULL_OR_MAP_VAL(a.find("type"));
    const std::string &n = NULL_OR_MAP_STR(a.find("name"));

    presets_item *item = new presets_item(josm_type_parse(tp), n, ic,
                                          addEditName);
    g_assert((items.top()->type & presets_item_t::TY_GROUP) != 0);
    static_cast<presets_item_group *>(items.top())->items.push_back(item);
    items.push(item);
    if(G_LIKELY(!n.empty())) {
      // search again: the key must be the unlocalized name here
      itemsNames[findAttribute(attrs, "name", false)] = item;
    } else {
      dumpState("found", "item without name\n");
    }
    break;
  }
  case TagPresetLink: {
    const char *id = findAttribute(attrs, "preset_name", false);
    presets_widget_link *link = new presets_widget_link();
    g_assert_false(items.empty());
    g_assert_true(items.top()->isItem());
    presets_item *item = static_cast<presets_item *>(items.top());

    // make sure not to insert it as a stale link in case the item is invalid,
    // as that would be deleted on it's end tag and a stale reference would remain
    // in laterLinks
    if(G_LIKELY(!item->name.empty())) {
      if(!id) {
        dumpState("found", "preset_link without preset_name\n");
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
    presets_item *ref = O2G_NULLPTR;
    if(!id) {
      dumpState("found", "reference without ref\n");
    } else {
      const ChunkMap::const_iterator it = chunks.find(id);
      if(G_UNLIKELY(it == chunks.end())) {
        dumpState("found");
        printf("reference with unresolved ref %s\n", id);
      } else
        ref = it->second;
    }
    widgets.push(new presets_widget_reference(ref));
    break;
  }
  case TagLabel: {
    const char *text = findAttribute(attrs, "text");
    widgets.push(new presets_widget_label(NULL_OR_VAL(text)));
    // do not push to items, will be done in endElement()
    break;
  }
  case TagSpace:
    g_assert_false(items.empty());
#ifndef USE_HILDON
    widget = new presets_widget_separator();
#endif
    break;
  case TagText: {
    const char *names[] = { "key", "text", "default", "match", O2G_NULLPTR };
    const AttrMap &a = findAttributes(attrs, names, 2);
    const AttrMap::const_iterator itEnd = a.end();

    const std::string &key = NULL_OR_MAP_STR(a.find("key"));
    const std::string &text = NULL_OR_MAP_STR(a.find("text"));
    const std::string &def = NULL_OR_MAP_STR(a.find("default"));
    const char *match = NULL_OR_MAP_VAL(a.find("match"));

    widget = new presets_widget_text(key, text, def, match);
    break;
  }
  case TagKey: {
    const char *key = O2G_NULLPTR;
    const char *value = O2G_NULLPTR;
    const char *match = O2G_NULLPTR;
    for(unsigned int i = 0; attrs[i]; i += 2) {
      if(strcmp(attrs[i], "key") == 0) {
        key = attrs[i + 1];
      } else if(strcmp(attrs[i], "value") == 0) {
        value = attrs[i + 1];
      } else if(strcmp(attrs[i], "match") == 0) {
        match = attrs[i + 1];
      }
    }
    widget = new presets_widget_key(NULL_OR_VAL(key), NULL_OR_VAL(value),
                                    match);
    break;
  }
  case TagCheck: {
    const char *names[] = { "key", "text", "value_on", "match", "default", O2G_NULLPTR };
    const AttrMap &a = findAttributes(attrs, names, 2);
    const AttrMap::const_iterator itEnd = a.end();

    const std::string &key = NULL_OR_MAP_STR(a.find("key"));
    const std::string &text = NULL_OR_MAP_STR(a.find("text"));
    const std::string &von = NULL_OR_MAP_STR(a.find("value_on"));
    const char *match = NULL_OR_MAP_VAL(a.find("match"));

    bool on = NULL_OR_MAP_STR(a.find("default")) == "on";

    widget = new presets_widget_checkbox(key, text, on, match, von);
    break;
  }
  case TagLink: {
    g_assert_false(items.empty());
    g_assert_true(items.top()->isItem());
    presets_item * const item = static_cast<presets_item *>(items.top());
    const char *href = findAttribute(attrs, "href");
    if(G_UNLIKELY(href == O2G_NULLPTR)) {
      dumpState("ignoring", "link without href\n");
    } else {
      if(G_LIKELY(item->link.empty()))
       item->link = href;
      else {
        dumpState("found surplus", "link\n");
      }
    }
    break;
  }
  case TagCombo: {
    const char *names[] = { "key", "text", "display_values", "match", "default", "delimiter", "values", O2G_NULLPTR };
    const AttrMap &a = findAttributes(attrs, names, 6);
    const AttrMap::const_iterator itEnd = a.end();

    const std::string &key = NULL_OR_MAP_STR(a.find("key"));
    const std::string &text = NULL_OR_MAP_STR(a.find("text"));
    const std::string &def = NULL_OR_MAP_STR(a.find("default"));
    const char *match = NULL_OR_MAP_VAL(a.find("match"));
    const char *display_values= NULL_OR_MAP_VAL(a.find("display_values"));
    const char *values = NULL_OR_MAP_VAL(a.find("values"));
    const char *del = NULL_OR_MAP_VAL(a.find("delimiter"));

    char delimiter = ',';
    if(del) {
      if(G_UNLIKELY(strlen(del) != 1)) {
        dumpState("found");
        printf("combo with invalid delimiter '%s'\n", del);
      } else
        delimiter = *del;
    }

    if(G_UNLIKELY(!values && display_values)) {
      dumpState("found", "combo with display_values but not values\n");
      display_values = O2G_NULLPTR;
    }
    widget = new presets_widget_combo(key, text, def, match,
                                      presets_widget_combo::split_string(values, delimiter),
                                      presets_widget_combo::split_string(display_values, delimiter));
    break;
  }
  case TagListEntry: {
    g_assert_false(items.empty());
    g_assert_false(widgets.empty());
    g_assert_cmpuint(widgets.top()->type, ==, WIDGET_TYPE_COMBO);
    presets_widget_combo * const combo = static_cast<presets_widget_combo *>(widgets.top());

    const char *names[] = { "display_value", "value", O2G_NULLPTR };
    const AttrMap &a = findAttributes(attrs, names, 3);
    const AttrMap::const_iterator itEnd = a.end();

    const char *value = NULL_OR_MAP_VAL(a.find("value"));

    if(G_UNLIKELY(!value)) {
      dumpState("found", "list_entry without value\n");
    } else {
      combo->values.push_back(value);
      combo->display_values.push_back(NULL_OR_MAP_STR(a.find("display_value")));
    }
    break;
  }
  case TagRoles:
    g_assert_false(items.empty());
    break;
  case TagRole: {
    g_assert_false(items.empty());
    g_assert_true(items.top()->isItem());
    presets_item * const item = static_cast<presets_item *>(items.top());

    const char *names[] = { "key", "type", O2G_NULLPTR };
    const AttrMap &a = findAttributes(attrs, names, 0);
    const AttrMap::const_iterator itEnd = a.end();

    const std::string &key = NULL_OR_MAP_STR(a.find("key"));
    const char *tp = NULL_OR_MAP_VAL(a.find("type"));
    const char *cnt = NULL_OR_MAP_VAL(a.find("count"));
    unsigned int count = 0;
    if(cnt) {
      char *endp;
      count = strtoul(cnt, &endp, 10);
      if(G_UNLIKELY(*endp != '\0')) {
        dumpState("ignoring invalid count value of", "role\n");
        count = 0;
      }
    }

    item->roles.push_back(presets_item::role(key, josm_type_parse(tp), count));
    break;
  }
  }

  state.push_back(it->second.first);
  if(widget != O2G_NULLPTR) {
    g_assert_false(items.empty());
    g_assert_true(items.top()->isItem());
    widgets.push(widget);
    static_cast<presets_item *>(items.top())->widgets.push_back(widget);
  }
}

void PresetSax::endElement(const xmlChar *name)
{
  if(state.back() == UnknownTag) {
    state.pop_back();
    return;
  }

  const StateMap &tags = preset_state_map();
  StateMap::const_iterator it = std::find_if(tags.begin(), tags.end(),
                                             str_map_find<StateMap>(reinterpret_cast<const char *>(name)));

  g_assert(it != tags.end() || state.back() == UnknownTag);
  g_assert(state.back() == it->second.first);
  state.pop_back();

  switch(it->second.first) {
  case DocStart:
  case UnknownTag:
    g_assert_not_reached();
  case TagLink:
  case TagListEntry:
  case TagPresets:
  case IntermediateTag:
  case TagRoles:
  case TagRole:
    break;
  case TagItem: {
    g_assert_cmpint(0, ==, widgets.size());
    g_assert_false(items.empty());
    g_assert_true(items.top()->isItem());
    presets_item * const item = static_cast<presets_item *>(items.top());
    items.pop();
    if(G_UNLIKELY(item->name.empty())) {
      /* silently delete, was warned about before */
      delete item;
      break;
    } else {
      // update the group type
      g_assert_false(items.empty());
      presets_item_t * const group = items.top();
      g_assert((group->type & presets_item_t::TY_GROUP) != 0);
      *const_cast<unsigned int *>(&group->type) |= item->type;
      if(G_UNLIKELY(!item->roles.empty() && (item->type & (presets_item_t::TY_RELATION | presets_item_t::TY_MULTIPOLYGON)) == 0)) {
        dumpState("found", "item with roles, but type does not match relations or multipolygons\n");
        item->roles.clear();
      }
      break;
    }
  }
  case TagSeparator:
    g_assert_false(items.empty());
    g_assert_cmpuint(items.top()->type, ==, presets_item_t::TY_SEPARATOR);
    items.pop();
    break;
  case TagGroup: {
    g_assert_false(items.empty());
    const presets_item_t * const item = items.top();
    g_assert((item->type & presets_item_t::TY_GROUP) != 0);
    items.pop();
    // update the parent group type
    if(!items.empty()) {
      presets_item_t * const group = items.top();
      g_assert((group->type & presets_item_t::TY_GROUP) != 0);
      *const_cast<unsigned int *>(&group->type) |= item->type;
    }
    break;
  }
  case TagChunk: {
    g_assert_false(items.empty());
    presets_item * const chunk = static_cast<presets_item *>(items.top());
    g_assert_cmpuint(chunk->type, ==, presets_item_t::TY_ALL);
    items.pop();
    if(G_UNLIKELY(chunk->name.empty())) {
      dumpState("ignoring", "chunk without id\n");
      delete chunk;
      return;
    }

    const std::string &id = chunk->name;

    if(chunks.find(id) != chunks.end()) {
      dumpState("ignoring");
      printf("chunk with duplicate id %s\n", id.c_str());
      delete chunk;
    } else {
      chunks[id] = chunk;
    }
    // if this was a top level chunk no active widgets should remain
    g_assert(!items.empty() || widgets.empty());
    break;
  }
  case TagReference: {
    g_assert_false(items.empty());
    g_assert_true(items.top()->isItem());
    g_assert_false(widgets.empty());
    presets_widget_reference * const ref = static_cast<presets_widget_reference *>(widgets.top());
    widgets.pop();
    g_assert_cmpuint(ref->type, ==, WIDGET_TYPE_REFERENCE);
    if(G_UNLIKELY(ref->item == O2G_NULLPTR))
      delete ref;
    else
      static_cast<presets_item *>(items.top())->widgets.push_back(ref);
    break;
  }
  case TagLabel: {
    g_assert_false(items.empty());
    g_assert_false(widgets.empty());
    presets_widget_label * const label = static_cast<presets_widget_label *>(widgets.top());
    widgets.pop();
    if(G_UNLIKELY(label->text.empty())) {
      dumpState("ignoring", "label without text\n");
      delete label;
    } else {
      static_cast<presets_item *>(items.top())->widgets.push_back(label);
    }
    break;
  }
  case TagSpace:
    g_assert_false(items.empty());
#ifndef USE_HILDON
    g_assert_false(widgets.empty());
    widgets.pop();
#endif
    break;
  case TagText:
  case TagKey:
  case TagCheck:
  case TagCombo:
  case TagPresetLink:
    g_assert_false(items.empty());
    g_assert_false(widgets.empty());
    widgets.pop();
    break;
  }
}

struct move_chunks_functor {
  std::vector<presets_item_t *> &chunks;
  move_chunks_functor(std::vector<presets_item_t *> &c) : chunks(c) {}
  void operator()(const ChunkMap::value_type &p) {
    chunks.push_back(p.second);
  }
};

bool presets_items::addFile(const std::string &filename, const std::string &basepath)
{
  PresetSax p(*this, basepath);
  if(!p.parse(filename))
    return false;

  // now move all chunks to the presets list
  chunks.reserve(chunks.size() + p.chunks.size());
  std::for_each(p.chunks.begin(), p.chunks.end(), move_chunks_functor(chunks));

  return true;
}

struct presets_items *josm_presets_load(void) {
  printf("Loading JOSM presets ...\n");

  struct presets_items *presets = new presets_items();

  const std::string &filename = find_file("defaultpresets.xml");
  if(G_LIKELY(!filename.empty()))
    presets->addFile(filename, std::string());

  // check for user presets
  std::string dirname = getenv("HOME");
  dirname += "/.local/share/osm2go/presets/";
  GDir *dir = g_dir_open(dirname.c_str(), 0, O2G_NULLPTR);

  if(dir != O2G_NULLPTR) {
    const gchar *name;
    std::string xmlname;
    while ((name = g_dir_read_name(dir)) != O2G_NULLPTR) {
      const std::string dn = dirname + name + '/';
      GDir *pdir = g_dir_open(dn.c_str(), 0, O2G_NULLPTR);
      if(pdir != O2G_NULLPTR) {
        // find first XML file inside those directories
        const gchar *fname;
        while ((fname = g_dir_read_name(pdir)) != O2G_NULLPTR) {
          if(g_str_has_suffix(fname, ".xml")) {
            presets->addFile(dn + fname, dn);
            break;
          }
        }
        g_dir_close(pdir);
      }
    }

    g_dir_close(dir);
  }

  if(G_UNLIKELY(presets->items.empty())) {
    delete presets;
    return O2G_NULLPTR;
  }

  return presets;
}

/* ----------------------- cleaning up --------------------- */

void josm_presets_free(struct presets_items *presets) {
  delete presets;
}

presets_widget_t::Match presets_widget_t::parseMatch(const char *matchstring, Match def)
{
  typedef std::map<const char *, Match> VMap;
  static VMap matches;
  if(G_UNLIKELY(matches.empty())) {
    matches["none"] = MatchIgnore;
    matches["key"] = MatchKey;
    matches["key!"] = MatchKey_Force;
    matches["keyvalue"] = MatchKeyValue;
    matches["keyvalue!"] = MatchKeyValue_Force;
  }
  const VMap::const_iterator itEnd = matches.end();
  const VMap::const_iterator it = !matchstring ? itEnd : std::find_if(cbegin(matches),
                                               itEnd, str_map_find<VMap>(matchstring));

  return (it == itEnd) ? def : it->second;
}

presets_widget_t::presets_widget_t(presets_widget_type_t t, Match m, const std::string &key, const std::string &text)
  : type(t)
  , key(key)
  , text(text)
  , match(m)
{
}

bool presets_widget_t::is_interactive() const
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

presets_widget_text::presets_widget_text(const std::string &key, const std::string &text,
                                         const std::string &deflt, const char *matches)
  : presets_widget_t(WIDGET_TYPE_TEXT, parseMatch(matches), key, text)
  , def(deflt)
{
}

presets_widget_combo::presets_widget_combo(const std::string &key, const std::string &text,
                                           const std::string &deflt, const char *matches,
                                           std::vector<std::string> vals, std::vector<std::string> dvals)
  : presets_widget_t(WIDGET_TYPE_COMBO, parseMatch(matches), key, text)
  , def(deflt)
  , values(vals)
  , display_values(dvals)
{
}

std::vector<std::string> presets_widget_combo::split_string(const char *str, const char delimiter)
{
  std::vector<std::string> ret;

  if(!str)
    return ret;

  const char *c, *p = str;
  while((c = strchr(p, delimiter))) {
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

presets_widget_key::presets_widget_key(const std::string &key, const std::string &val,
                                       const char *matches)
  : presets_widget_t(WIDGET_TYPE_KEY, parseMatch(matches, MatchKeyValue_Force), key)
  , value(val)
{
}

presets_widget_checkbox::presets_widget_checkbox(const std::string &key, const std::string &text,
                                                 bool deflt, const char *matches, const std::string &von)
  : presets_widget_t(WIDGET_TYPE_CHECK, parseMatch(matches), key, text)
  , def(deflt)
  , value_on(von)
{
}

bool presets_widget_reference::is_interactive() const
{
  return std::find_if(item->widgets.begin(), item->widgets.end(), presets_widget_t::isInteractive) !=
         item->widgets.end();
}

guint presets_widget_reference::rows() const
{
  return std::accumulate(item->widgets.begin(), item->widgets.end(), 0, widget_rows);
}

presets_item::~presets_item()
{
  std::for_each(widgets.begin(), widgets.end(), std::default_delete<presets_widget_t>());
}

presets_item_group::presets_item_group(const unsigned int types, presets_item_group *p,
                                       const std::string &n, const std::string &ic)
  : presets_item_named(types | TY_GROUP, n, ic), parent(p), widget(O2G_NULLPTR)
{
  g_assert(p == O2G_NULLPTR || ((p->type & TY_GROUP) != 0));
}

presets_item_group::~presets_item_group()
{
  std::for_each(items.begin(), items.end(), std::default_delete<presets_item_t>());
}

presets_items::presets_items()
{
  lru.reserve(LRU_MAX);
}

presets_items::~presets_items()
{
  std::for_each(items.begin(), items.end(), std::default_delete<presets_item_t>());
  std::for_each(chunks.begin(), chunks.end(), std::default_delete<presets_item_t>());
}

// vim:et:ts=8:sw=2:sts=2:ai
