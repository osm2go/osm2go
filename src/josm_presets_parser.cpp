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

std::string josm_icon_name_adjust(const char *name) {
  std::string ret;

  if(!name)
    return ret;

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

  ret[PRESETS_TYPE_WAY] = "way";
  ret[PRESETS_TYPE_NODE] = "node";
  ret[PRESETS_TYPE_RELATION] = "relation";
  ret[PRESETS_TYPE_CLOSEDWAY] = "closedway";
  ret[PRESETS_TYPE_MULTIPOLYGON] = "multipolygon";

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
static presets_item_t::item_type josm_type_parse(const char *type) {
  int type_mask = 0;
  if(!type) return presets_item_t::TY_ALL;

  const char *ntype = strchr(type, ',');
  while(ntype) {
    type_mask |= josm_type_bit(type, ',');
    type = ntype + 1;
    ntype = strchr(type, ',');
  }

  type_mask |= josm_type_bit(type, '\0');
  return static_cast<presets_item_t::item_type>(type_mask);
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
    TagKey,
    TagText,
    TagCombo,
    TagListEntry,
    TagCheck,
    TagLabel,
    TagSpace,
    TagSeparator,
    TagLink,
    IntermediateTag,    ///< tag itself is ignored, but childs are processed
    UnknownTag
  };

  // not a stack because that can't be iterated
  std::vector<State> state;
  std::stack<presets_item_t *> items; // the current item stack (i.e. menu layout)
  std::stack<presets_widget_t *> widgets; // the current widget stack (i.e. dialog layout)

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

  void dumpState() const;

public:
  explicit PresetSax(presets_items &p);

  bool parse(const std::string &filename);

  presets_items &presets;
  ChunkMap chunks;

private:

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
    map.insert(StateMap::value_type("key", StateMap::mapped_type(TagKey, item_chunks)));
    map.insert(StateMap::value_type("text", StateMap::mapped_type(TagText, item_chunks)));
    map.insert(StateMap::value_type("combo", StateMap::mapped_type(TagCombo, item_chunks)));
    map.insert(StateMap::value_type("list_entry", StateMap::mapped_type(TagListEntry, VECTOR_ONE(TagCombo))));
    map.insert(StateMap::value_type("check", StateMap::mapped_type(TagCheck, item_chunks)));
    map.insert(StateMap::value_type("label", StateMap::mapped_type(TagLabel, item_chunks)));
    map.insert(StateMap::value_type("space", StateMap::mapped_type(TagSpace, item_chunks)));
    map.insert(StateMap::value_type("link", StateMap::mapped_type(TagLink, item_chunks)));

    map.insert(StateMap::value_type("checkgroup", StateMap::mapped_type(IntermediateTag, item_chunks)));
    map.insert(StateMap::value_type("optional", StateMap::mapped_type(IntermediateTag, item_chunks)));
  }

  return map;
}

void PresetSax::dumpState() const
{
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
}

PresetSax::PresetSax(presets_items &p)
  : presets(p)
{
  memset(&handler, 0, sizeof(handler));
  handler.characters = cb_characters;
  handler.startElement = cb_startElement;
  handler.endElement = cb_endElement;

  state.push_back(DocStart);
}

bool PresetSax::parse(const std::string &filename)
{
  if (xmlSAXUserParseFile(&handler, this, filename.c_str()) != 0)
    return false;

  return state.size() == 1;
}

void PresetSax::characters(const char *ch, int len)
{
  for(int pos = 0; pos < len; pos++)
    if(!isspace(ch[pos])) {
      printf("unhandled character data: %*.*s state %i\n", len, len, ch, state.back());
      return;
    }
}

/**
 * @brief find attribute with the given name
 * @returns the attribute string if present
 *
 * If the attribute is present but the string is empty a nullptr is returned.
 */
static const char *findAttribute(const char **attrs, const char *name) {
  for(unsigned int i = 0; attrs[i]; i += 2)
    if(strcmp(attrs[i], name) == 0) {
      if(*(attrs[i + 1]) == '\0')
        return 0;
      return attrs[i + 1];
    }

  return 0;
}

#define NULL_OR_VAL(a) (a ? a : std::string())

void PresetSax::startElement(const char *name, const char **attrs)
{
  const StateMap &tags = preset_state_map();
  StateMap::const_iterator it = std::find_if(tags.begin(), tags.end(),
                                             str_map_find<StateMap>(name));
  if(it == tags.end()) {
    printf("found unhandled ");
    dumpState();
    printf("%s\n", name);
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
    printf("found unexpected ");
    dumpState();
    printf("%s\n", name);
    state.push_back(UnknownTag);
    return;
  }

  presets_widget_t *widget = 0;

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
    const char *id = findAttribute(attrs, "id");
    presets_item *item = new presets_item(presets_item_t::TY_ALL, NULL_OR_VAL(id));
    items.push(item);
    break;
  }
  case TagGroup: {
    presets_item_group *group = new presets_item_group(0,
                                items.empty() ? 0 : static_cast<presets_item_group *>(items.top()));
    for(unsigned int i = 0; attrs[i]; i += 2) {
      if(strcmp(attrs[i], "name") == 0) {
        group->name = attrs[i + 1];
      } else if(strcmp(attrs[i], "icon") == 0 &&
                strlen(attrs[i + 1]) > 0) {
        group->icon = josm_icon_name_adjust(attrs[i + 1]);
      }
    }
    if(items.empty())
      presets.items.push_back(group);
    else
      static_cast<presets_item_group *>(items.top())->items.push_back(group);
    items.push(group);
    break;
  }
  case TagSeparator: {
    g_assert(!items.empty());
    g_assert(items.top()->type & presets_item_t::TY_GROUP);
    presets_item_separator * sep = new presets_item_separator();
    static_cast<presets_item_group *>(items.top())->items.push_back(sep);
    items.push(sep);
    break;
  }
  case TagItem: {
    const char *tp = 0;
    const char *n = 0;
    std::string ic;
    bool addEditName = false;
    for(unsigned int i = 0; attrs[i]; i += 2) {
      if(strcmp(attrs[i], "name") == 0) {
        n = attrs[i + 1];
      } else if(strcmp(attrs[i], "type") == 0) {
        tp = attrs[i + 1];
      } else if(strcmp(attrs[i], "icon") == 0) {
        ic = josm_icon_name_adjust(attrs[i + 1]);
      } else if(strcmp(attrs[i], "preset_name_label") == 0) {
        addEditName = (strcmp(attrs[i + 1], "true") == 0);
      }
    }
    presets_item *item = new presets_item(josm_type_parse(tp), NULL_OR_VAL(n),
                                          ic,
                                          addEditName);
    g_assert((items.top()->type & presets_item_t::TY_GROUP) != 0);
    static_cast<presets_item_group *>(items.top())->items.push_back(item);
    items.push(item);
    break;
  }
  case TagReference: {
    const char *id = findAttribute(attrs, "ref");
    presets_item *ref = 0;
    if(!id) {
      printf("found presets/item/reference without ref\n");
    } else {
      const ChunkMap::const_iterator it = chunks.find(id);
      if(G_UNLIKELY(it == chunks.end()))
        printf("found presets/item/reference with unresolved ref %s\n", id);
      else
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
    g_assert(!items.empty());
#ifndef USE_HILDON
    widget = new presets_widget_separator();
#endif
    break;
  case TagText: {
    const char *key = 0;
    const char *text = 0;
    const char *def = 0;
    const char *match = 0;
    for(unsigned int i = 0; attrs[i]; i += 2) {
      if(strcmp(attrs[i], "key") == 0) {
        key = attrs[i + 1];
      } else if(strcmp(attrs[i], "text") == 0) {
        text = attrs[i + 1];
      } else if(strcmp(attrs[i], "default") == 0) {
        def = attrs[i + 1];
      } else if(strcmp(attrs[i], "match") == 0) {
        match = attrs[i + 1];
      }
    }
    widget = new presets_widget_text(NULL_OR_VAL(key), NULL_OR_VAL(text),
                                     NULL_OR_VAL(def), match);
    break;
  }
  case TagKey: {
    const char *key = 0;
    const char *value = 0;
    const char *match = 0;
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
    const char *von = 0;
    const char *key = 0;
    const char *txt = 0;
    const char *match = 0;
    bool on = false;
    for(unsigned int i = 0; attrs[i]; i += 2) {
      if(strcmp(attrs[i], "value_on") == 0) {
        von = attrs[i + 1];
      } else if(strcmp(attrs[i], "key") == 0) {
        key = attrs[i + 1];
      } else if(strcmp(attrs[i], "text") == 0) {
        txt = attrs[i + 1];
      } else if(strcmp(attrs[i], "default") == 0) {
        on = (strcmp(attrs[i + 1], "on") == 0);
      } else if(strcmp(attrs[i], "match") == 0) {
        match = attrs[i + 1];
      }
    }
    widget = new presets_widget_checkbox(NULL_OR_VAL(key), NULL_OR_VAL(txt), on,
                                         match, NULL_OR_VAL(von));
    break;
  }
  case TagLink: {
    g_assert(!items.empty());
    g_assert((items.top()->type & (presets_item_t::TY_GROUP | presets_item_t::TY_SEPARATOR)) == 0);
    presets_item * const item = static_cast<presets_item *>(items.top());
    const char *href = findAttribute(attrs, "href");
    if(G_UNLIKELY(href == 0)) {
      printf("ignoring link without href\n");
    } else {
      if(G_LIKELY(item->link.empty()))
       item->link = href;
      else
        printf("ignoring surplus link\n");
    }
    break;
  }
  case TagCombo: {
    const char *key = 0;
    const char *txt = 0;
    const char *def = 0;
    const char *match = 0;
    const char *values = 0;
    const char *display_values = 0;
    char delimiter = ',';

    for(unsigned int i = 0; attrs[i]; i += 2) {
      if(strcmp(attrs[i], "default") == 0) {
        def = attrs[i + 1];
      } else if(strcmp(attrs[i], "key") == 0) {
        key = attrs[i + 1];
      } else if(strcmp(attrs[i], "text") == 0) {
        txt = attrs[i + 1];
      } else if(strcmp(attrs[i], "values") == 0) {
        values = attrs[i + 1];
      } else if(strcmp(attrs[i], "display_values") == 0) {
        display_values = attrs[i + 1];
      } else if(strcmp(attrs[i], "match") == 0) {
        match = attrs[i + 1];
      } else if(strcmp(attrs[i], "delimiter") == 0) {
        if(G_UNLIKELY(strlen(attrs[i + 1]) != 1))
          printf("found invalid delimiter '%s'\n", attrs[i + 1]);
        else
          delimiter = *(attrs[i + 1]);
      }
    }

    if(G_UNLIKELY(!values && display_values)) {
      printf("found display_values but not values\n");
      display_values = 0;
    }
    widget = new presets_widget_combo(NULL_OR_VAL(key), NULL_OR_VAL(txt),
                                      NULL_OR_VAL(def), match,
                                      presets_widget_combo::split_string(values, delimiter),
                                      presets_widget_combo::split_string(display_values, delimiter));
    break;
  }
  case TagListEntry: {
    g_assert(!items.empty());
    g_assert(!widgets.empty());
    g_assert_cmpuint(widgets.top()->type, ==, WIDGET_TYPE_COMBO);
    presets_widget_combo * const combo = static_cast<presets_widget_combo *>(widgets.top());

    const char *value = 0;
    const char *dvalue = 0;
    for(unsigned int i = 0; attrs[i]; i += 2) {
      if(strcmp(attrs[i], "display_value") == 0) {
        dvalue = attrs[i + 1];
      } else if(strcmp(attrs[i], "value") == 0) {
        value = attrs[i + 1];
      }
    }

    if(G_UNLIKELY(!value)) {
      printf("ignoring list_entry without value\n");
    } else {
      combo->values.push_back(value);
      if(dvalue && *dvalue)
        combo->display_values.push_back(dvalue);
      else
        combo->display_values.push_back(std::string());
    }
    break;
  }
  }

  state.push_back(it->second.first);
  if(widget != 0) {
    g_assert(!items.empty());
    widgets.push(widget);
    items.top()->widgets.push_back(widget);
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
    break;
  case TagItem: {
    g_assert_cmpint(0, ==, widgets.size());
    g_assert(!items.empty());
    const presets_item_t * const item = items.top();
    g_assert((item->type & presets_item_t::TY_GROUP) == 0);
    items.pop();
    // update the group type
    g_assert(!items.empty());
    presets_item_t * const group = items.top();
    g_assert((group->type & presets_item_t::TY_GROUP) != 0);
    *const_cast<unsigned int *>(&group->type) |= item->type;
    break;
  }
  case TagSeparator:
    g_assert(!items.empty());
    g_assert_cmpuint(items.top()->type, ==, presets_item_t::TY_SEPARATOR);
    items.pop();
    break;
  case TagGroup: {
    g_assert(!items.empty());
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
    g_assert(!items.empty());
    presets_item * const chunk = static_cast<presets_item *>(items.top());
    g_assert_cmpuint(chunk->type, ==, presets_item_t::TY_ALL);
    items.pop();
    if(G_UNLIKELY(chunk->name.empty())) {
      printf("ignoring presets/chunk without id\n");
      delete chunk;
      return;
    }

    const std::string &id = chunk->name;

    if(chunks.find(id) != chunks.end()) {
      printf("ignoring presets/chunk duplicate id %s\n", id.c_str());
      delete chunk;
    } else {
      chunks[id] = chunk;
    }
    // if this was a top level chunk no active widgets should remain
    g_assert(!items.empty() || widgets.empty());
    break;
  }
  case TagReference: {
    g_assert(!items.empty());
    g_assert(!widgets.empty());
    presets_widget_reference * const ref = static_cast<presets_widget_reference *>(widgets.top());
    widgets.pop();
    g_assert_cmpuint(ref->type, ==, WIDGET_TYPE_REFERENCE);
    if(G_UNLIKELY(ref->item == 0))
      delete ref;
    else
      items.top()->widgets.push_back(ref);
    break;
  }
  case TagLabel: {
    g_assert(!items.empty());
    g_assert(!widgets.empty());
    presets_widget_label * const label = static_cast<presets_widget_label *>(widgets.top());
    widgets.pop();
    if(G_UNLIKELY(label->text.empty())) {
      printf("found presets/item/label without text\n");
      delete label;
    } else {
      items.top()->widgets.push_back(label);
    }
    break;
  }
  case TagSpace:
    g_assert(!items.empty());
#ifndef USE_HILDON
    g_assert(!widgets.empty());
    widgets.pop();
#endif
    break;
  case TagText:
  case TagKey:
  case TagCheck:
  case TagCombo:
    g_assert(!items.empty());
    g_assert(!widgets.empty());
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

struct presets_items *josm_presets_load(void) {
  printf("Loading JOSM presets ...\n");

  const std::string &filename = find_file("defaultpresets.xml");
  if(G_UNLIKELY(filename.empty()))
    return NULL;

  struct presets_items *presets = new presets_items();

  PresetSax p(*presets);
  if(!p.parse(filename)) {
    delete presets;
    return 0;
  }

  // now move all chunks to the presets list
  presets->chunks.reserve(presets->chunks.size() + p.chunks.size());
  std::for_each(p.chunks.begin(), p.chunks.end(), move_chunks_functor(presets->chunks));

  return presets;
}

/* ----------------------- cleaning up --------------------- */

static inline void free_widget(presets_widget_t *widget) {
  delete widget;
}

static void free_item(presets_item_t *item) {
  delete item;
}

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
  const VMap::const_iterator it = !matchstring ? itEnd : std::find_if(
#if __cplusplus >= 201103L
                                               matches.cbegin(),
#else
                                               VMap::const_iterator(matches.begin()),
#endif
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
  return std::find_if(item->widgets.begin(), item->widgets.end(), is_widget_interactive) !=
         item->widgets.end();
}

guint presets_widget_reference::rows() const
{
  return std::accumulate(item->widgets.begin(), item->widgets.end(), 0, widget_rows);
}

presets_item_t::~presets_item_t()
{
  std::for_each(widgets.begin(), widgets.end(), free_widget);
}

presets_item_group::presets_item_group(const unsigned int types, presets_item_group *p,
                                       const std::string &n, const std::string &ic)
  : presets_item_visible(types | TY_GROUP, n, ic), parent(p), widget(0)
{
  g_assert(p == 0 || ((p->type & TY_GROUP) != 0));
}

presets_item_group::~presets_item_group()
{
  std::for_each(items.begin(), items.end(), free_item);
}

presets_items::~presets_items()
{
  std::for_each(items.begin(), items.end(), free_item);
  std::for_each(chunks.begin(), chunks.end(), free_item);
}

// vim:et:ts=8:sw=2:sts=2:ai
