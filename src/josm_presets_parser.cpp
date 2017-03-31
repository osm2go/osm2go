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
#include <strings.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

typedef std::map<std::string, presets_item *> ChunkMap;

/* --------------------- presets.xml parsing ----------------------- */

xmlChar *josm_icon_name_adjust(xmlChar *xname) {
  if(!xname) return NULL;

  char *name = reinterpret_cast<char *>(xname);

  /* the icon loader uses names without extension */
  if(!strcasecmp(name+strlen(name)-4, ".png"))
    name[strlen(name)-4] = 0;
  else if(!strcasecmp(name+strlen(name)-4, ".svg"))
    name[strlen(name)-4] = 0;

  return xname;
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
static presets_item_t::item_type josm_type_parse(xmlChar *xtype) {
  int type_mask = 0;
  const char *type = (const char*)xtype;

  if(!type) return presets_item_t::TY_ALL;

  const char *ntype = strchr(type, ',');
  while(ntype) {
    type_mask |= josm_type_bit(type, ',');
    type = ntype + 1;
    ntype = strchr(type, ',');
  }

  type_mask |= josm_type_bit(type, '\0');
  xmlFree(xtype);
  return static_cast<presets_item_t::item_type>(type_mask);
}

// custom find to avoid memory allocations for std::string
template<typename T>
struct str_map_find {
  const char * const name;
  str_map_find(const typename T::key_type n)
    : name(reinterpret_cast<const char *>(n)) {}
  bool operator()(const typename T::value_type &p) {
    return (strcmp(reinterpret_cast<const char *>(p.first), name) == 0);
  }
};

static void parse_widgets(xmlNode *a_node, presets_item *item,
                          const ChunkMap &chunks);

/* parse children of a given node for into *widget */
static presets_widget_t *parse_widget(xmlNode *cur_node, const ChunkMap &chunks) {
  presets_widget_t *widget = 0;

  if(strcmp((char*)cur_node->name, "label") == 0) {
    /* --------- label widget --------- */
    xmlChar *text = xmlGetProp(cur_node, BAD_CAST "text");

    if(G_UNLIKELY(!text))
      printf("found presets/item/label without text\n");
    else
      widget = new presets_widget_label(text);

  } else if(strcmp((char*)cur_node->name, "space") == 0) {
#ifndef USE_HILDON
    // new-style separators
    widget = new presets_widget_separator();
#endif
  } else if(strcmp((char*)cur_node->name, "text") == 0) {

    /* --------- text widget --------- */
    widget = new presets_widget_text(
                     xmlGetProp(cur_node, BAD_CAST "key"),
                     xmlGetProp(cur_node, BAD_CAST "text"),
                     xmlGetProp(cur_node, BAD_CAST "default"),
		     xmlGetProp(cur_node, BAD_CAST "match"));
  } else if(strcmp((char*)cur_node->name, "combo") == 0) {
    /* --------- combo widget --------- */
    std::vector<std::string> v, dv;
    if(cur_node->children) {
      bool hasDV = false;
      for(xmlNode *child = cur_node->children; child; child = child->next) {
        if(child->type == XML_ELEMENT_NODE && strcmp(reinterpret_cast<const char *>(child->name), "list_entry") == 0) {
          xmlChar *value = xmlGetProp(child, BAD_CAST "value");
          g_assert(value);
          v.push_back(reinterpret_cast<char *>(value));
          xmlFree(value);

          xmlChar *display_value = xmlGetProp(child, BAD_CAST "display_value");
          dv.push_back(display_value ? reinterpret_cast<char *>(display_value) : std::string());
          hasDV |= !dv.back().empty();
          xmlFree(display_value);
        }
      }
      if(!hasDV)
        dv.clear();
    } else {
      xmlChar *del = xmlGetProp(cur_node, BAD_CAST "delimiter");
      char delimiter = ',';
      if(del) {
        if(G_UNLIKELY(strlen(reinterpret_cast<char *>(del)) != 1))
          printf("found presets/item/combo with invalid separator '%s'\n", del);
        else
          delimiter = *del;
        xmlFree(del);
      }

      xmlChar *values = xmlGetProp(cur_node, BAD_CAST "values");
      xmlChar *display_values = xmlGetProp(cur_node, BAD_CAST "display_values");
      v = presets_widget_combo::split_string(values, delimiter);
      dv = presets_widget_combo::split_string(display_values, delimiter);
      xmlFree(values);
      xmlFree(display_values);
      if(dv.size() != v.size())
        dv.clear();
    }

    widget = new presets_widget_combo(
                                  xmlGetProp(cur_node, BAD_CAST "key"),
                                  xmlGetProp(cur_node, BAD_CAST "text"),
                                  xmlGetProp(cur_node, BAD_CAST "default"),
                                  xmlGetProp(cur_node, BAD_CAST "match"),
                                  v, dv);
  } else if(strcmp((char*)cur_node->name, "key") == 0) {

    /* --------- invisible key widget --------- */
    widget = new presets_widget_key(
                     xmlGetProp(cur_node, BAD_CAST "key"),
                     xmlGetProp(cur_node, BAD_CAST "value"),
                     xmlGetProp(cur_node, BAD_CAST "match"));
  } else if(strcmp((char*)cur_node->name, "check") == 0) {
    /* --------- check widget --------- */
    widget = new presets_widget_checkbox(
                               xmlGetProp(cur_node, BAD_CAST "key"),
                               xmlGetProp(cur_node, BAD_CAST "text"),
                               xml_get_prop_is(cur_node, "default", "on"),
                               xmlGetProp(cur_node, BAD_CAST "value_on"));
  } else if(strcmp((char*)cur_node->name, "reference") == 0) {
    xmlChar *id = xmlGetProp(cur_node, BAD_CAST "ref");
    if(!id) {
      printf("found presets/item/reference without ref\n");
    } else {
      const ChunkMap::const_iterator it =
          chunks.find(std::string(reinterpret_cast<char *>(id)));
      if(G_UNLIKELY(it == chunks.end()))
        printf("found presets/item/reference with unresolved ref %s\n", id);
      else
        widget = new presets_widget_reference(it->second);
      xmlFree(id);
    }
  } else
    printf("found unhandled presets/item/%s\n", cur_node->name);

  return widget;
}

/* parse children of a given node for into *widget */
static void parse_widgets(xmlNode *a_node, presets_item *item,
                          const ChunkMap &chunks) {
  xmlNode *cur_node = NULL;

  for(cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if(cur_node->type == XML_ELEMENT_NODE) {
      if (strcmp((char*)cur_node->name, "optional") == 0) {
        // Could be done as a fold-out box width twisties.
        // Or maybe as a separate dialog for small screens.
        // For now, just recurse and build up our current list.
        parse_widgets(cur_node, item, chunks);
      } else if (strcmp((char*)cur_node->name, "link") == 0) {

	/* --------- link is not a widget, but a property of item --------- */
	if(!item->link) {
	  item->link = xmlGetProp(cur_node, BAD_CAST "href");
	} else
	  printf("ignoring surplus link\n");

      } else {
        presets_widget_t *widget = parse_widget(cur_node, chunks);
        if(widget)
          item->widgets.push_back(widget);
      }
    }
  }
}

static presets_item_t *parse_item(xmlNode *a_node, const ChunkMap &chunks) {
  presets_item *item = new presets_item(josm_type_parse(xmlGetProp(a_node, BAD_CAST "type")),
                                        xmlGetProp(a_node, BAD_CAST "name"),
                                        josm_icon_name_adjust(xmlGetProp(a_node, BAD_CAST "icon")),
                                        xml_get_prop_is(a_node, "preset_name_label", "true"));

  parse_widgets(a_node, item, chunks);
  return item;
}

static presets_item_t *parse_group(xmlDocPtr doc, xmlNode *a_node, presets_item_group *parent,
                                   const ChunkMap &chunks) {
  presets_item_group *group = new presets_item_group(presets_item_t::TY_GROUP,
                                                     parent,
                                                     xmlGetProp(a_node, BAD_CAST "name"),
                                                     josm_icon_name_adjust(xmlGetProp(a_node, BAD_CAST "icon")));
  /* ------ parse groups own properties ------ */

  for (xmlNode *cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcmp((char*)cur_node->name, "item") == 0) {
        presets_item_t *preset = parse_item(cur_node, chunks);
        if(preset) {
          *const_cast<unsigned int *>(&group->type) |= preset->type;
          group->items.push_back(preset);
	}
      } else if(strcmp((char*)cur_node->name, "group") == 0) {
        presets_item_t *preset = parse_group(doc, cur_node, group, chunks);
        if(preset) {
          *const_cast<unsigned int *>(&group->type) |= preset->type;
          group->items.push_back(preset);
	}
      } else if(strcmp((char*)cur_node->name, "separator") == 0) {
        group->items.push_back(new presets_item_separator());
      } else
	printf("found unhandled presets/group/%s\n", cur_node->name);
    }
  }

  return group;
}

struct move_chunks_functor {
  std::vector<presets_item_t *> &chunks;
  move_chunks_functor(std::vector<presets_item_t *> &c) : chunks(c) {}
  void operator()(const ChunkMap::value_type &p) {
    chunks.push_back(p.second);
  }
};

static void parse_annotations(xmlDocPtr doc, xmlNode *a_node, struct presets_items &presets) {
  ChunkMap chunks;

  // <chunk> elements are first
  xmlNode *cur_node;
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcmp((char*)cur_node->name, "chunk") == 0) {
        xmlChar *xid = xmlGetProp(cur_node, BAD_CAST "id");
        if(!xid) {
          printf("ignoring presets/chunk without id\n");
        } else {
          std::string id(reinterpret_cast<char *>(xid));
          xmlFree(xid);
          if(chunks.find(id) != chunks.end()) {
            printf("ignoring presets/chunk duplicate id %s\n", id.c_str());
          } else {
            presets_item *item = new presets_item(presets_item_t::TY_ALL);
            parse_widgets(cur_node, item, chunks);
            chunks[id] = item;
          }
        }
      } else {
        break;
      }
    }
  }

  for (; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      presets_item_t *preset = 0;
      if(strcmp((char*)cur_node->name, "item") == 0) {
        preset = parse_item(cur_node, chunks);
      } else if(strcmp((char*)cur_node->name, "group") == 0) {
        preset = parse_group(doc, cur_node, 0, chunks);
      } else if(strcmp((char*)cur_node->name, "separator") == 0) {
        preset = new presets_item_separator();
      } else
	printf("found unhandled presets/%s\n", cur_node->name);
      if(preset)
        presets.items.push_back(preset);
    }
  }

  // now move all chunks to the presets list
  presets.chunks.reserve(presets.chunks.size() + chunks.size());
  std::for_each(chunks.begin(), chunks.end(), move_chunks_functor(presets.chunks));
}

struct presets_items *josm_presets_load(void) {
  struct presets_items *presets = new presets_items();

  printf("Loading JOSM presets ...\n");

  const std::string &filename = find_file("defaultpresets.xml");
  if(G_UNLIKELY(filename.empty()))
    return NULL;

  /* parse the file and get the DOM */
  xmlDoc *doc = NULL;
  if(G_UNLIKELY((doc = xmlReadFile(filename.c_str(), NULL, 0)) == NULL)) {
    xmlErrorPtr errP = xmlGetLastError();
    printf("presets download failed: "
	   "XML error while parsing:\n"
	   "%s\n", errP->message);
  } else {
    printf("ok, parse doc tree\n");
    for(xmlNode *cur_node = xmlDocGetRootElement(doc);
        cur_node; cur_node = cur_node->next) {
      if(cur_node->type == XML_ELEMENT_NODE) {
        if(G_LIKELY(strcmp((char*)cur_node->name, "presets") == 0))
          parse_annotations(doc, cur_node, *presets);
        else
          printf("found unhandled %s\n", cur_node->name);
      }
    }

    xmlFreeDoc(doc);
  }

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

presets_widget_t::Match presets_widget_t::parseMatch(xmlChar *matchstring, Match def)
{
  typedef std::map<const xmlChar *, Match> VMap;
  static VMap matches;
  if(G_UNLIKELY(matches.empty())) {
    matches[BAD_CAST "none"] = MatchIgnore;
    matches[BAD_CAST "key"] = MatchKey;
    matches[BAD_CAST "key!"] = MatchKey_Force;
    matches[BAD_CAST "keyvalue"] = MatchKeyValue;
    matches[BAD_CAST "keyvalue!"] = MatchKeyValue_Force;
  }
  const VMap::const_iterator itEnd = matches.end();
  const VMap::const_iterator it = !matchstring ? itEnd : std::find_if(
#if __cplusplus >= 201103L
                                               matches.cbegin(),
#else
                                               VMap::const_iterator(matches.begin()),
#endif
                                               itEnd, str_map_find<VMap>(matchstring));

  xmlFree(matchstring);

  return (it == itEnd) ? def : it->second;
}

presets_widget_t::presets_widget_t(presets_widget_type_t t, Match m, xmlChar *key, xmlChar *text)
  : type(t)
  , key(key)
  , text(text)
  , match(m)
{
}

presets_widget_t::~presets_widget_t()
{
  xmlFree(key);
  xmlFree(text);
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

presets_widget_text::presets_widget_text(xmlChar *key, xmlChar *text,
                                         xmlChar *deflt, xmlChar *matches)
  : presets_widget_t(WIDGET_TYPE_TEXT, parseMatch(matches), key, text)
  , def(deflt)
{
}

presets_widget_text::~presets_widget_text()
{
  xmlFree(def);
}

presets_widget_combo::presets_widget_combo(xmlChar *key, xmlChar *text,
                                           xmlChar *deflt, xmlChar *matches,
                                           std::vector<std::string> &vals, std::vector<std::string> &dvals)
  : presets_widget_t(WIDGET_TYPE_COMBO, parseMatch(matches), key, text)
  , def(deflt)
#if __cplusplus >= 201103L
  , values(std::move(vals))
  , display_values(std::move(dvals))
#else
  , values(vals)
  , display_values(dvals)
#endif
{
}

presets_widget_combo::~presets_widget_combo()
{
  xmlFree(def);
}

std::vector<std::string> presets_widget_combo::split_string(const xmlChar *str, const char delimiter)
{
  std::vector<std::string> ret;

  if(!str)
    return ret;

  const char *c, *p = reinterpret_cast<const char *>(str);
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

presets_widget_key::presets_widget_key(xmlChar* key, xmlChar* val, xmlChar *matches)
  : presets_widget_t(WIDGET_TYPE_KEY, parseMatch(matches, MatchKeyValue_Force), key)
  , value(val)
{
}

presets_widget_key::~presets_widget_key()
{
  xmlFree(value);
}

presets_widget_checkbox::presets_widget_checkbox(xmlChar* key, xmlChar* text,
                                                 bool deflt, xmlChar *matches, xmlChar *von)
  : presets_widget_t(WIDGET_TYPE_CHECK, parseMatch(matches), key, text)
  , def(deflt)
  , value_on(von)
{
}

presets_widget_checkbox::~presets_widget_checkbox()
{
  xmlFree(value_on);
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

presets_item_visible::~presets_item_visible()
{
  xmlFree(name);
  xmlFree(icon);
}

presets_item::~presets_item()
{
  xmlFree(link);
}

presets_item_group::presets_item_group(const unsigned int types, presets_item_group *p,
                                       xmlChar *n, xmlChar *ic)
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
