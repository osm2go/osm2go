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

#include "josm_presets.h"
#include "josm_presets_p.h"

#include "appdata.h"
#include "icon.h"
#include "misc.h"

#include <algorithm>
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifdef FREMANTLE
#include <hildon/hildon-picker-button.h>
#endif
#include <strings.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

#ifdef ENABLE_BROWSER_INTERFACE
static void on_info(GtkWidget *widget, gpointer context) {
  const char *link = (char*)g_object_get_data(G_OBJECT(widget), "link");
  open_url((appdata_t*)context, link);
}
#endif

/* --------------------- presets.xml parsing ----------------------- */

char *josm_icon_name_adjust(char *name) {
  if(!name) return NULL;

  /* the icon loader uses names without extension */
  if(!strcasecmp(name+strlen(name)-4, ".png"))
    name[strlen(name)-4] = 0;
  else if(!strcasecmp(name+strlen(name)-4, ".svg"))
    name[strlen(name)-4] = 0;

  return name;
}

static int josm_type_bit(const char *type, char sep) {
  const struct { int bit; const char *str; } types[] = {
    { PRESETS_TYPE_WAY,       "way"       },
    { PRESETS_TYPE_NODE,      "node"      },
    { PRESETS_TYPE_RELATION,  "relation"  },
    { PRESETS_TYPE_CLOSEDWAY, "closedway" },
    { 0,                      NULL        }};

  for(int i = 0; types[i].bit; i++) {
    const size_t tlen = strlen(types[i].str);
    if(strncmp(types[i].str, type, tlen) == 0 && type[tlen] == sep)
      return types[i].bit;
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

static void parse_widgets(xmlNode *a_node, presets_item *item,
                          const std::map<std::string, xmlNode *> &chunks);

/* parse children of a given node for into *widget */
static presets_widget_t *parse_widget(xmlNode *cur_node, presets_item *item,
                                      const std::map<std::string, xmlNode *> &chunks) {
  presets_widget_t *widget = 0;

  if(strcmp((char*)cur_node->name, "label") == 0) {
    xmlChar *text = xmlGetProp(cur_node, BAD_CAST "text");

    /* special handling of pre-<space/> separators */
    if(!text || (strcmp((char*)text, " ") == 0)) {
      widget = new presets_widget_t(WIDGET_TYPE_SEPARATOR);
      if(text)
        xmlFree(text);
    } else {
      /* --------- label widget --------- */
      widget = new presets_widget_t(WIDGET_TYPE_LABEL);
      widget->text = text;
    }

  } else if(strcmp((char*)cur_node->name, "space") == 0) {
#ifndef USE_HILDON
    // new-style separators
    widget = new presets_widget_t(WIDGET_TYPE_SEPARATOR);
#endif
  } else if(strcmp((char*)cur_node->name, "text") == 0) {

    /* --------- text widget --------- */
    widget = new presets_widget_t(WIDGET_TYPE_TEXT);
    widget->text = xmlGetProp(cur_node, BAD_CAST "text");
    widget->key = xmlGetProp(cur_node, BAD_CAST "key");
    widget->text_w.def = xmlGetProp(cur_node, BAD_CAST "default");

  } else if(strcmp((char*)cur_node->name, "combo") == 0) {

    /* --------- combo widget --------- */
    widget = new presets_widget_t(WIDGET_TYPE_COMBO);
    widget->text = xmlGetProp(cur_node, BAD_CAST "text");
    widget->key = xmlGetProp(cur_node, BAD_CAST "key");
    widget->combo_w.def = xmlGetProp(cur_node, BAD_CAST "default");
    widget->combo_w.values = xmlGetProp(cur_node, BAD_CAST "values");

  } else if(strcmp((char*)cur_node->name, "key") == 0) {

    /* --------- invisible key widget --------- */
    widget = new presets_widget_t(WIDGET_TYPE_KEY);
    widget->key = xmlGetProp(cur_node, BAD_CAST "key");
    widget->key_w.value = xmlGetProp(cur_node, BAD_CAST "value");

  } else if(strcmp((char*)cur_node->name, "check") == 0) {

    /* --------- check widget --------- */
    widget = new presets_widget_t(WIDGET_TYPE_CHECK);
    widget->text = xmlGetProp(cur_node, BAD_CAST "text");
    widget->key = xmlGetProp(cur_node, BAD_CAST "key");
    widget->check_w.def = xml_get_prop_is(cur_node, "default", "on");
  } else if(strcmp((char*)cur_node->name, "reference") == 0) {
    xmlChar *id = xmlGetProp(cur_node, BAD_CAST "ref");
    if(!id) {
      printf("found presets/item/reference without ref\n");
    } else {
      const std::map<std::string, xmlNode *>::const_iterator it =
          chunks.find(std::string(reinterpret_cast<char *>(id)));
      if(it == chunks.end()) {
        printf("found presets/item/reference without unresolved ref %s\n", id);
      } else {
        parse_widgets(it->second, item, chunks);
      }
      xmlFree(id);
    }

  } else
    printf("found unhandled presets/item/%s\n", cur_node->name);

  return widget;
}

/* parse children of a given node for into *widget */
static void parse_widgets(xmlNode *a_node, presets_item *item,
                          const std::map<std::string, xmlNode *> &chunks) {
  xmlNode *cur_node = NULL;
  std::vector<presets_widget_t *> ret;

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
        presets_widget_t *widget = parse_widget(cur_node, item, chunks);
        if(widget)
          item->widgets.push_back(widget);
      }
    }
  }
}

static presets_item_t *parse_item(xmlNode *a_node, const std::map<std::string, xmlNode *> &chunks) {
  presets_item *item = new presets_item(josm_type_parse(xmlGetProp(a_node, BAD_CAST "type")));

  /* ------ parse items own properties ------ */
  item->name = xmlGetProp(a_node, BAD_CAST "name");

  item->icon = BAD_CAST
    josm_icon_name_adjust((char*)xmlGetProp(a_node, BAD_CAST "icon"));

  xmlChar *nl = xmlGetProp(a_node, BAD_CAST "preset_name_label");
  if(nl) {
    item->addEditName = (strcmp((char *)nl, "true") == 0);
    xmlFree(nl);
  }

  parse_widgets(a_node, item, chunks);
  return item;
}

static presets_item_t *parse_group(xmlDocPtr doc, xmlNode *a_node, presets_item_group *parent,
                                   const std::map<std::string, xmlNode *> &chunks) {
  xmlNode *cur_node = NULL;

  presets_item_group *group = new presets_item_group(presets_item_t::TY_GROUP, parent);

  /* ------ parse groups own properties ------ */
  group->name = xmlGetProp(a_node, BAD_CAST "name");

  group->icon = BAD_CAST
    josm_icon_name_adjust((char*)xmlGetProp(a_node, BAD_CAST "icon"));

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
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

static std::vector<presets_item_t *> parse_annotations(xmlDocPtr doc, xmlNode *a_node) {
  std::vector<presets_item_t *> presets;
  std::map<std::string, xmlNode *> chunks;

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
          if(chunks.find(id) != chunks.end())
            printf("ignoring presets/chunk duplicate id %s\n", id.c_str());
          else
            chunks[id] = cur_node;
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
        presets.push_back(preset);
    }
  }
  return presets;
}

static std::vector<presets_item_t *> parse_doc(xmlDocPtr doc) {
  std::vector<presets_item_t *> presets;

  for(xmlNode *cur_node = xmlDocGetRootElement(doc);
      cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcmp((char*)cur_node->name, "presets") == 0) {
	presets = parse_annotations(doc, cur_node);
      } else
	printf("found unhandled %s\n", cur_node->name);
    }
  }

  xmlFreeDoc(doc);
  return presets;
}

struct presets_items *josm_presets_load(void) {
  struct presets_items *presets = new presets_items();

  printf("Loading JOSM presets ...\n");

  const std::string &filename = find_file("defaultpresets.xml");
  if(filename.empty())
    return NULL;

  /* parse the file and get the DOM */
  xmlDoc *doc = NULL;
  if((doc = xmlReadFile(filename.c_str(), NULL, 0)) == NULL) {
    xmlErrorPtr errP = xmlGetLastError();
    printf("presets download failed: "
	   "XML error while parsing:\n"
	   "%s\n", errP->message);
  } else {
    printf("ok, parse doc tree\n");
    presets->items = parse_doc(doc);
  }

  return presets;
}

/* --------------------- the items dialog -------------------- */

static void attach_both(GtkWidget *table, GtkWidget *widget, gint y) {
  gtk_table_attach(GTK_TABLE(table), widget, 0,2,y,y+1,
                   static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
                   static_cast<GtkAttachOptions>(0), 0, 0);
}

static void attach_text(GtkWidget *table, char *text, gint y) {
  gtk_table_attach(GTK_TABLE(table), gtk_label_new(text), 0,1,y,y+1,
                   static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
                   static_cast<GtkAttachOptions>(0), 0, 0);
}

static void attach_right(GtkWidget *table, GtkWidget *widget, gint y) {
  gtk_table_attach(GTK_TABLE(table), widget, 1,2,y,y+1,
                   static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
                   static_cast<GtkAttachOptions>(0), 0, 0);
}

/**
 * @brief update the given tag with the newly entered value
 * @param widget the entry widget description
 * @param tags all tags of the object
 * @param ctag iterator of the tag to edit, tags.end() in case it does not yet exist
 * @param value the new value
 */
static bool store_value(presets_widget_t *widget, std::vector<tag_t *> &tags,
                        std::vector<tag_t *>::iterator ctag, const char *value) {
  bool changed = false;
  if(value && strlen(value)) {
    const char *chstr;
    if(ctag != tags.end()) {
      /* update the previous tag structure */
      g_assert(strcasecmp((*ctag)->key, (char*)widget->key) == 0);
      /* only update if the value actually changed */
      if(strcmp((*ctag)->value, value) != 0) {
        changed = true; /* mark as updated, actual change below */
        chstr = "updated";
      } else {
        chstr = "kept";
      }
    } else {
      /* no old entry, create a new one */
      tag_t *tag = g_new0(tag_t, 1);
      tag->update_key(reinterpret_cast<char *>(widget->key));
      /* value will be updated below */
      tags.push_back(tag);
      ctag = tags.end() - 1;
      changed = true;
      chstr = "new";
    }

    if(changed)
      (*ctag)->update_value(value);

    printf("%s key = %s, value = %s\n", chstr,
           widget->key, (*ctag)->value);
  } else if (ctag != tags.end()) {
    printf("removed key = %s, value = %s\n", widget->key, (*ctag)->value);
    tags.erase(ctag);
    osm_tag_free(*ctag);
    changed = true;
  } else
    printf("ignore empty key = %s\n", widget->key);

  return changed;
}

#ifdef USE_HILDON
static gint table_expose_event(GtkWidget *widget, GdkEventExpose *event,
			 gboolean *first) {

  if(*first) {
    guint border_width =
      gtk_container_get_border_width(GTK_CONTAINER(widget->parent));
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(widget->parent), GTK_SHADOW_NONE);

    gtk_widget_set_size_request(GTK_WIDGET(widget->parent), -1,
				widget->allocation.height +  2*border_width);
    *first = FALSE;
  }
  return FALSE;
}
#endif

static inline bool is_widget_interactive(const presets_widget_t *w) {
  return w->is_interactive();
}

static bool preset_combo_insert_value(GtkWidget *combo, const char *value,
                                          const char *preset)
{
  combo_box_append_text(combo, value);

  return (g_strcmp0(preset, value) == 0);
}

struct presets_context_t {
  appdata_t *appdata;
#ifndef FREMANTLE
  GtkWidget *menu;
#endif
#ifdef PICKER_MENU
  std::vector<presets_item_group *> submenus;
#endif
  tag_context_t *tag_context;
};

struct find_tag_functor {
  const char * const key;
  find_tag_functor(const xmlChar *k) : key(reinterpret_cast<const char *>(k)) {}
  bool operator()(const tag_t *tag) {
    return strcasecmp(tag->key, key) == 0;
  }
};

static void presets_item_dialog(presets_context_t *context,
                                const presets_item *item) {
  appdata_t *appdata = context->appdata;
  GtkWindow *parent = GTK_WINDOW(context->tag_context->dialog);

  GtkWidget *dialog = NULL;
  bool ok = false;

  printf("dialog for item %s\n", item->name);

  /* build dialog from items widget list */

  /* count total number of widgets and number of widgets that */
  /* have an interactive gui element. We won't show a dialog */
  /* at all if there's no interactive gui element at all */
  guint widget_cnt = item->widgets.size();
  std::vector<presets_widget_t *>::const_iterator itEnd = item->widgets.end();
  bool has_interactive_widget = std::find_if(item->widgets.begin(), itEnd,
                                             is_widget_interactive) != itEnd;

  /* allocate space for required number of gtk widgets */
  GtkWidget **gtk_widgets = (GtkWidget**)g_new0(GtkWidget, widget_cnt);
  std::vector<presets_widget_t *>::const_iterator it;

  if(has_interactive_widget)  {
    dialog =
      misc_dialog_new(MISC_DIALOG_NOSIZE,
		      (gchar*)item->name, parent,
		      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		      NULL);

#ifdef ENABLE_BROWSER_INTERFACE
    /* if a web link has been provided for this item install */
    /* a button for this */
    if(item->link) {
      GtkWidget *button = gtk_dialog_add_button(GTK_DIALOG(dialog), _
			("Info"), GTK_RESPONSE_HELP);
      g_object_set_data(G_OBJECT(button), "link", item->link);
      gtk_signal_connect(GTK_OBJECT(button), "clicked",
			 GTK_SIGNAL_FUNC(on_info), appdata);
    }
#endif

    /* special handling for the first label/separators */
    guint widget_skip = 0;  // number of initial widgets to skip
    it = item->widgets.begin();
    if(item->addEditName) {
      gchar *title = g_strdup_printf(_("Edit %s"), item->name);
      gtk_window_set_title(GTK_WINDOW(dialog), title);
      g_free(title);
    } else {
      // use the first label as title
      if(it != itEnd && ((*it)->type == WIDGET_TYPE_LABEL)) {
        gtk_window_set_title(GTK_WINDOW(dialog), (char*)(*it)->text);

        widget_skip++;   // this widget isn't part of the contents anymore
        it++;
      }
    }

    /* skip all following separators (and keys) */
    while(it != itEnd &&
          (((*it)->type == WIDGET_TYPE_SEPARATOR) ||
           ((*it)->type == WIDGET_TYPE_SPACE) ||
           ((*it)->type == WIDGET_TYPE_KEY))) {
      widget_skip++;   // this widget isn't part of the contents anymore
      it++;
    }

    /* create table of required size */
    GtkWidget *table = gtk_table_new(widget_cnt-widget_skip, 2, FALSE);

    for(widget_cnt = widget_skip; it != itEnd; it++, widget_cnt++) {
      /* check if there's a value with this key already */
      std::vector<tag_t *>::const_iterator otagIt = (*it)->key ?
                                                    std::find_if(context->tag_context->tags.begin(),
                                                                 context->tag_context->tags.end(),
                                                                 find_tag_functor((*it)->key)) :
                                                    context->tag_context->tags.end();
      tag_t *otag = otagIt != context->tag_context->tags.end() ? *otagIt : 0;
      const char *preset = otag ? otag->value : NULL;

      switch((*it)->type) {
      case WIDGET_TYPE_SEPARATOR:
	attach_both(table, gtk_hseparator_new(), widget_cnt-widget_skip);
	break;

      case WIDGET_TYPE_SPACE:
	/* space is just an empty label until we find something better */
	attach_both(table, gtk_label_new(" "), widget_cnt-widget_skip);
	break;

      case WIDGET_TYPE_LABEL: {
	attach_both(table, gtk_label_new((char*)(*it)->text), widget_cnt-widget_skip);
	break;

      case WIDGET_TYPE_COMBO:
#ifndef FREMANTLE
	attach_text(table, (char*)(*it)->text, widget_cnt-widget_skip);
#endif

	if(!preset && (*it)->combo_w.def)
	  preset = (char*)(*it)->combo_w.def;
	gtk_widgets[widget_cnt] = combo_box_new((char*)(*it)->text);
	combo_box_append_text(gtk_widgets[widget_cnt], _("<unset>"));
	const xmlChar *value = (*it)->combo_w.values;
	int active = 0;

	/* cut values strings */
	if(value) {
	  const char *c, *p = (char*)value;
	  int count = 1;
	  while((c = strchr(p, ','))) {
	    /* maximum length of an OSM value, shouldn't be reached anyway. */
	    char cur[256];
	    g_strlcpy(cur, p, sizeof(cur));
	    if(c - p < sizeof(cur))
	      cur[c - p] = '\0';
	    if (preset_combo_insert_value(gtk_widgets[widget_cnt], cur, preset)) {
	      active = count;
	      preset = NULL;
	    }

	    count++;
	    p = c + 1;
	  }
	  /* attach remaining string as last value */
	  if (preset_combo_insert_value(gtk_widgets[widget_cnt], p, preset))
	    active = count;
	}

	combo_box_set_active(gtk_widgets[widget_cnt], active);
#ifndef FREMANTLE
	attach_right(table, gtk_widgets[widget_cnt], widget_cnt-widget_skip);
#else
	attach_both(table, gtk_widgets[widget_cnt], widget_cnt-widget_skip);
#endif
	break;
      }

      case WIDGET_TYPE_CHECK:
	{ gboolean def = FALSE;
	  if(preset) def = ((strcasecmp(preset, "true") == 0) ||
			    (strcasecmp(preset, "yes") == 0));
	  else       def = (*it)->check_w.def;

	  gtk_widgets[widget_cnt] =
	    check_button_new_with_label((char*)(*it)->text);
	  check_button_set_active(gtk_widgets[widget_cnt], def);
#ifndef FREMANTLE
	  attach_right(table, gtk_widgets[widget_cnt], widget_cnt-widget_skip);
#else
	  attach_both(table, gtk_widgets[widget_cnt], widget_cnt-widget_skip);
#endif
      } break;

    case WIDGET_TYPE_TEXT:
      attach_text(table, (char*)(*it)->text, widget_cnt-widget_skip);

      if(!preset && (*it)->text_w.def)
        preset = (char*)(*it)->text_w.def;
      gtk_widgets[widget_cnt] = entry_new();
      if(preset)
	gtk_entry_set_text(GTK_ENTRY(gtk_widgets[widget_cnt]), preset);

      attach_right(table, gtk_widgets[widget_cnt], widget_cnt-widget_skip);
      break;

      default:
	break;
      }

      if(gtk_widgets[widget_cnt] && otag)
        g_object_set_data(G_OBJECT(gtk_widgets[widget_cnt]), "tag", otag);
    }

#ifndef USE_HILDON
    /* add widget to dialog */
    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), table);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 50);
#else
#ifndef FREMANTLE_PANNABLE_AREA
    /* put it into a scrolled window */
    GtkWidget *scroll_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_win),
				   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll_win),
    					  table);
#else
    gtk_window_set_default_size(GTK_WINDOW(dialog), -1, 500);
    /* put view into a pannable area */
    GtkWidget *scroll_win = hildon_pannable_area_new();
    hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(scroll_win), table);
#endif

    gboolean first = TRUE;
    gtk_signal_connect(GTK_OBJECT(table), "expose_event",
		       G_CALLBACK(table_expose_event), &first);

    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), scroll_win);
#endif

    gtk_widget_show_all(dialog);

    /* run gtk_dialog_run, but continue if e.g. the help button was pressed */
    int result = -1;
    do
      result = gtk_dialog_run(GTK_DIALOG(dialog));
    while((result != GTK_RESPONSE_DELETE_EVENT) &&
	  (result != GTK_RESPONSE_ACCEPT) &&
	  (result != GTK_RESPONSE_REJECT));

    if(result == GTK_RESPONSE_ACCEPT)
      ok = true;

  } else
    ok = true;

  if(ok) {
    /* handle all children of the table */
    bool changed = false;
    it = item->widgets.begin();
    widget_cnt = 0;

    std::vector<tag_t *> &tags = context->tag_context->tags;
    for(; it != itEnd; it++, widget_cnt++) {
      tag_t *otag = gtk_widgets[widget_cnt] ?
                    static_cast<tag_t*>(g_object_get_data(G_OBJECT(gtk_widgets[widget_cnt]), "tag")) : 0;
      const std::vector<tag_t *>::iterator itEnd = tags.end();
      std::vector<tag_t *>::iterator ctag = otag ?
                                            std::find(tags.begin(), itEnd, otag) :
                                            itEnd; // the place to do the change
      const char *text;
      g_assert(!otag == (ctag == itEnd));

      switch((*it)->type) {
      case WIDGET_TYPE_COMBO: {
	g_assert(GTK_WIDGET_TYPE(gtk_widgets[widget_cnt]) == combo_box_type());

        text = combo_box_get_active_text(gtk_widgets[widget_cnt]);
	if(!strcmp(text, _("<unset>")))
	  text = NULL;

	break;
      }

      case WIDGET_TYPE_TEXT:
	g_assert(GTK_WIDGET_TYPE(gtk_widgets[widget_cnt]) == entry_type());

        text = gtk_entry_get_text(GTK_ENTRY(gtk_widgets[widget_cnt]));
	break;

      case WIDGET_TYPE_CHECK:
	g_assert(GTK_WIDGET_TYPE(gtk_widgets[widget_cnt]) == check_button_type());

        text = check_button_get_active(gtk_widgets[widget_cnt]) ? "yes" : NULL;
	break;

      case WIDGET_TYPE_KEY:
	g_assert(!gtk_widgets[widget_cnt]);
        g_assert(ctag == itEnd);
        ctag = std::find_if(tags.begin(), itEnd, find_tag_functor((*it)->key));

        text = reinterpret_cast<const char*>((*it)->key_w.value);
	break;

      default:
        continue;
      }

      changed |= store_value(*it, tags, ctag, text);
    }

    if(changed)
      context->tag_context->info_tags_replace();
  }

  g_free(gtk_widgets);

  if(has_interactive_widget)
    gtk_widget_destroy(dialog);
}

/* ------------------- the item list (popup menu) -------------- */

struct used_preset_functor {
  const tag_context_t * const tag_context;
  bool &is_interactive;
  bool &matches_all;
  used_preset_functor(const tag_context_t *c, bool &i, bool &m)
    : tag_context(c), is_interactive(i), matches_all(m) {}
  bool operator()(const presets_widget_t *w);
};

bool used_preset_functor::operator()(const presets_widget_t* w)
{
  if(w->type != WIDGET_TYPE_KEY) {
    is_interactive |= w->is_interactive();
    return false;
  }
  const tag_t t((char*) w->key, (char*) w->key_w.value);
  if(!osm_tag_key_and_value_present(tag_context->tags, &t))
    return true;

  matches_all = true;
  return false;
}

/**
 * @brief check if the currently active object uses this preset and the preset is interactive
 */
static bool preset_is_used(const presets_item_t *item, const presets_context_t *context)
{
  bool is_interactive = false;
  bool matches_all = false;
  used_preset_functor fc(context->tag_context, is_interactive, matches_all);
  if(std::find_if(item->widgets.begin(), item->widgets.end(), fc) != item->widgets.end())
    return false;

  return matches_all && is_interactive;
}

#ifndef PICKER_MENU
static void
cb_menu_item(GtkWidget *menu_item, gpointer data) {
  presets_context_t *context = (presets_context_t*)data;

  presets_item *item = static_cast<presets_item *>(g_object_get_data(G_OBJECT(menu_item), "item"));
  g_assert(item);

  presets_item_dialog(context, item);
}

static GtkWidget *create_menuitem(presets_context_t *context, const presets_item_visible *item)
{
  GtkWidget *menu_item;

  if(!item->icon)
    menu_item = gtk_menu_item_new_with_label((gchar*)item->name);
  else {
    menu_item = gtk_image_menu_item_new_with_label((gchar*)item->name);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
                                  icon_widget_load(&context->appdata->icon,
                                                   (char*)item->icon));
  }

  return menu_item;
}

struct build_menu_functor {
  presets_context_t * const context;
  GtkWidget * const menu;
  GtkWidget ** const matches;
  bool was_separator;
  bool was_item;
  build_menu_functor(presets_context_t *c, GtkWidget *m, GtkWidget **a)
    : context(c), menu(m), matches(a), was_separator(false), was_item(false) {}
  void operator()(presets_item_t *item);
};

static GtkWidget *build_menu(presets_context_t *context,
			     std::vector<presets_item_t *> &items, GtkWidget **matches) {
  build_menu_functor fc(context, gtk_menu_new(), matches);

  std::for_each(items.begin(), items.end(), fc);

  return fc.menu;
}

void build_menu_functor::operator()(presets_item_t *item)
{
  /* check if this presets entry is appropriate for the current item */
  if(item->type & context->tag_context->presets_type) {
    GtkWidget *menu_item;

    /* Show a separator if one was requested, but not if there was no item
     * before to prevent to show one as the first entry. */
    if(was_item && was_separator)
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    was_item = true;
    was_separator = false;

    menu_item = create_menuitem(context, static_cast<presets_item_visible *>(item));

    if(item->type & presets_item_t::TY_GROUP) {
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
                                build_menu(context,
                                           static_cast<presets_item_group *>(item)->items,
                                           matches));
    } else {
      g_object_set_data(G_OBJECT(menu_item), "item", item);
      g_signal_connect(menu_item, "activate",
                       GTK_SIGNAL_FUNC(cb_menu_item), context);

      if(preset_is_used(item, context)) {
        if(!*matches)
          *matches = gtk_menu_new();

          GtkWidget *used_item = create_menuitem(context, static_cast<presets_item_visible *>(item));
          g_object_set_data(G_OBJECT(used_item), "item", item);
          g_signal_connect(used_item, "activate",
                           GTK_SIGNAL_FUNC(cb_menu_item), context);
          gtk_menu_shell_append(GTK_MENU_SHELL(*matches), used_item);
      }
    }
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  } else if(item->type == presets_item_t::TY_SEPARATOR)
    /* Record that there was a separator. Do not immediately add it here to
     * prevent to show one as last entry. */
    was_separator = true;
}

#else // PICKER_MENU

struct group_member_used {
  const presets_context_t * const context;
  group_member_used(const presets_context_t *c) : context(c) {}
  bool operator()(const presets_item_t *item);
};

static bool preset_group_is_used(const presets_item_group *item, const presets_context_t *context)
{
  g_assert(item->type & presets_item_t::TY_GROUP);
  return std::find_if(item->items.begin(), item->items.end(),
                      group_member_used(context)) != item->items.end();
}

bool group_member_used::operator()(const presets_item_t *item)
{
  if(item->type & presets_item_t::TY_GROUP)
    return preset_group_is_used(static_cast<const presets_item_group *>(item), context);
  else
    return preset_is_used(item, context);
}

enum {
  PRESETS_PICKER_COL_ICON = 0,
  PRESETS_PICKER_COL_NAME,
  PRESETS_PICKER_COL_ITEM_PTR,
  PRESETS_PICKER_COL_SUBMENU_ICON,
  PRESETS_PICKER_COL_SUBMENU_PTR,
  PRESETS_PICKER_NUM_COLS
};

static GtkWidget *presets_picker(presets_context_t *context,
                                 const std::vector<presets_item_t *> &items,
                                 bool scan_for_recent);
static GtkWidget *preset_picker_recent(presets_context_t *context);

static void remove_sub(presets_item_group *sub_item) {
  if(sub_item->widget) {
    gtk_widget_destroy(sub_item->widget);
    sub_item->widget = 0;
  }
}

/**
 * @brief remove all child pickers
 */
static void remove_subs(presets_context_t *context, presets_item_group *sub_item) {
  std::vector<presets_item_group *> &oldsubs = context->submenus;
  std::vector<presets_item_group *>::iterator it =
             std::find(oldsubs.begin(), oldsubs.end(), sub_item);
  g_assert(it != oldsubs.end());
  std::for_each(++it, oldsubs.end(), remove_sub);
  oldsubs.erase(it, oldsubs.end());
}

static void
on_presets_picker_selected(GtkTreeSelection *selection, gpointer data) {
  presets_context_t *context = (presets_context_t*)data;

#ifdef FREMANTLE
  /* try to prevent inital selection */
  if(!g_object_get_data(G_OBJECT(selection), "setup_done")) {
    gtk_tree_selection_unselect_all (selection);
    g_object_set_data(G_OBJECT(selection), "setup_done", (gpointer)TRUE);
    return;
  }
#endif

  GtkTreeIter   iter;
  GtkTreeModel *model;

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    presets_item_visible *item = 0;
    presets_item_group *sub_item = 0;
    gtk_tree_model_get(model, &iter,
		       PRESETS_PICKER_COL_SUBMENU_PTR, &sub_item,
		       PRESETS_PICKER_COL_ITEM_PTR, &item,
		       -1);

    printf("clicked on %s, submenu = %p\n", item ? (char*)item->name : "''", sub_item);

    GtkWidget * const view =
      GTK_WIDGET(gtk_tree_selection_get_tree_view(selection));

    if(sub_item || (!item && !sub_item)) {
      /* check if this already had a submenu */
      GtkWidget *sub;
      if(context->submenus.empty()) {
        // check if "Uses Presets" is shown
        sub = GTK_WIDGET(g_object_get_data(G_OBJECT(view), "sub"));
        if(sub) {
          g_object_set_data(G_OBJECT(view), "sub", 0);
          gtk_widget_destroy(sub);
        }
      }

      if(sub_item) {
        /* normal submenu */

        // the current list of submenus may or may not have common anchestors with this one
        if(sub_item->widget) {
         // this item is already visible, so it must be in the list, just drop all childs
         remove_subs(context, sub_item);
        } else {
          // this item is not currently visible
          if(sub_item->parent) {
            // the parent item has to be visible, otherwise this could not have been clicked
            remove_subs(context, sub_item->parent);
          } else {
            // this is a top level menu, so everything currently shown can be removed
            std::for_each(context->submenus.begin(), context->submenus.end(), remove_sub);
            context->submenus.clear();
          }

          sub = presets_picker(context, sub_item->items, false);
          sub_item->widget = sub;
          g_object_set_data(G_OBJECT(sub), "sub_item", (gpointer)sub_item);
        }
        context->submenus.push_back(sub_item);
      } else {
        /* used presets submenu */
        // this is always on top level, so all old submenu entries can be removed
        std::for_each(context->submenus.begin(), context->submenus.end(), remove_sub);
        context->submenus.clear();
        sub = preset_picker_recent(context);
        g_object_set_data(G_OBJECT(view), "sub", (gpointer)sub);
      }

      /* views parent is a scrolled window whichs parent in turn is the hbox */
      GtkWidget *hbox = view->parent->parent;

      gtk_box_pack_start_defaults(GTK_BOX(hbox), sub);
      gtk_widget_show_all(sub);
    } else {
      /* save item pointer in dialog */
      g_object_set_data(G_OBJECT(gtk_widget_get_toplevel(view)),
			"item", (gpointer)item);

      /* and request closing of menu */
      gtk_dialog_response(GTK_DIALOG(gtk_widget_get_toplevel(view)),
			  GTK_RESPONSE_ACCEPT);
    }
  }
}

static GtkListStore *presets_picker_store(GtkTreeView **view) {
  GtkCellRenderer *renderer;

#ifndef FREMANTLE
  *view = GTK_TREE_VIEW(gtk_tree_view_new());
#else
  *view = GTK_TREE_VIEW(hildon_gtk_tree_view_new(HILDON_UI_MODE_EDIT));
#endif

  gtk_tree_view_set_headers_visible(*view, FALSE);

  /* --- "Icon" column --- */
  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_insert_column_with_attributes(*view,
      -1, "Icon", renderer, "pixbuf", PRESETS_PICKER_COL_ICON, NULL);

  /* --- "Name" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
		 "Name", renderer, "text", PRESETS_PICKER_COL_NAME, NULL);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(*view, column, -1);

  /* --- "submenu icon" column --- */
  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_insert_column_with_attributes(*view,
      -1, "Submenu Icon", renderer, "pixbuf",
      PRESETS_PICKER_COL_SUBMENU_ICON, NULL);

  return gtk_list_store_new(PRESETS_PICKER_NUM_COLS,
			     GDK_TYPE_PIXBUF,
			     G_TYPE_STRING,
			     G_TYPE_POINTER,
			     GDK_TYPE_PIXBUF,
			     G_TYPE_POINTER
			     );
}

static GtkWidget *presets_picker_embed(GtkTreeView *view, GtkListStore *store,
                                       presets_context_t *context) {
  gtk_tree_view_set_model(view, GTK_TREE_MODEL(store));
  g_object_unref(store);

  /* Setup the selection handler */
  GtkTreeSelection *select = gtk_tree_view_get_selection(view);
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT (select), "changed",
		    G_CALLBACK (on_presets_picker_selected), context);

  gtk_tree_selection_unselect_all(select);

  /* put this inside a scrolled view */
  GtkWidget *c;
#ifndef FREMANTLE_PANNABLE_AREA
  c = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(c),
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
#else
  c = hildon_pannable_area_new();
#endif
  gtk_container_add(GTK_CONTAINER(c), GTK_WIDGET(view));
  return c;
}

static GtkTreeIter preset_insert_item(const presets_item_visible *item, icon_t **icons,
                                      GtkListStore *store) {
  /* icon load can cope with NULL as name (returns NULL then) */
  GdkPixbuf *icon = icon_load(icons, (char*)item->icon);

  /* Append a row and fill in some data */
  GtkTreeIter iter;
  gtk_list_store_append(store, &iter);

  gtk_list_store_set(store, &iter,
		     PRESETS_PICKER_COL_ICON, icon,
		     PRESETS_PICKER_COL_NAME, item->name,
		     PRESETS_PICKER_COL_ITEM_PTR, item,
		     -1);

  return iter;
}

struct insert_recent_items {
  const presets_context_t * const context;
  GtkListStore * const store;
  insert_recent_items(const presets_context_t *c, GtkListStore *s) : context(c), store(s) {}
  void operator()(const presets_item_t *preset);
};

void insert_recent_items::operator()(const presets_item_t *preset)
{
  if(preset->type & presets_item_t::TY_GROUP) {
    const presets_item_group *gr = static_cast<const presets_item_group *>(preset);
    std::for_each(gr->items.begin(), gr->items.end(),
                  insert_recent_items(context, store));
  } else if(preset_is_used(preset, context))
    preset_insert_item(static_cast<const presets_item_visible *>(preset),
                       &context->appdata->icon, store);
}

static GtkWidget *preset_picker_recent(presets_context_t *context) {
  GtkTreeView *view;
  insert_recent_items fc(context, presets_picker_store(&view));

  const std::vector<presets_item_t *> &items = context->appdata->presets->items;
  std::for_each(items.begin(), items.end(), fc);

  return presets_picker_embed(view, fc.store, context);
}

struct picker_add_functor {
  presets_context_t * const context;
  GtkListStore * const store;
  GdkPixbuf * const subicon;
  bool &show_recent;
  bool scan_for_recent;
  picker_add_functor(presets_context_t *c, GtkListStore *s, GdkPixbuf *i, bool r, bool &w)
    : context(c), store(s), subicon(i), show_recent(w), scan_for_recent(r) {}
  void operator()(const presets_item_t *item);
};

void picker_add_functor::operator()(const presets_item_t* item)
{
  /* check if this presets entry is appropriate for the current item */
  if(!(item->type & context->tag_context->presets_type))
    return;

  const presets_item_visible * const itemv = static_cast<typeof(itemv)>(item);

  if(!itemv->name)
    return;

  GtkTreeIter iter = preset_insert_item(itemv, &context->appdata->icon, store);

  /* mark submenues as such */
  if(item->type & presets_item_t::TY_GROUP) {
    gtk_list_store_set(store, &iter,
                       PRESETS_PICKER_COL_SUBMENU_PTR,  item,
                       PRESETS_PICKER_COL_SUBMENU_ICON, subicon, -1);
    if(scan_for_recent) {
      show_recent = preset_group_is_used(static_cast<const presets_item_group *>(itemv), context);
      scan_for_recent = !show_recent;
    }
  } else if(scan_for_recent) {
    show_recent = preset_is_used(itemv, context);
    scan_for_recent = !show_recent;
  }
}

/**
 * @brief create a picker list for preset items
 * @param context the tag context
 * @param items the list of presets to show
 * @param scan_for_recent if a "Used Presets" subentry should be created
 *
 * This will create a view with the given items. This is just one column in the
 * presets view, every submenu will get it's own view created by a call to this
 * function.
 */
static GtkWidget *
presets_picker(presets_context_t *context, const std::vector<presets_item_t *> &items,
                bool scan_for_recent) {
  GtkTreeView *view;
  GtkListStore *store = presets_picker_store(&view);

  bool show_recent = false;
  GdkPixbuf *subicon = icon_load(&context->appdata->icon,
                                 "submenu_arrow");
  picker_add_functor fc(context, store, subicon, scan_for_recent, show_recent);

  std::for_each(items.begin(), items.end(), fc);

  if(show_recent) {
    GtkTreeIter     iter;

    /* Append a row and fill in some data */
    gtk_list_store_prepend(store, &iter);
    gtk_list_store_set(store, &iter,
		       PRESETS_PICKER_COL_NAME, _("Used presets"),
		       PRESETS_PICKER_COL_SUBMENU_ICON, subicon,
		       -1);
  }

  icon_free(&context->appdata->icon, subicon);

  return presets_picker_embed(view, store, context);
}
#endif

static gint button_press(GtkWidget *widget, GdkEventButton *event,
			 gpointer data) {
  presets_context_t *context = (presets_context_t*)data;

  if(event->type == GDK_BUTTON_PRESS) {
    printf("button press %d %d\n", event->button, event->time);

#ifndef PICKER_MENU
  (void)widget;
    if (!context->menu) {
      GtkWidget *matches = NULL;
      context->menu = build_menu(context, context->appdata->presets->items, &matches);
      if(matches) {
        GtkWidget *menu_item = gtk_menu_item_new_with_label(_("Used presets"));

        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), matches);
        gtk_menu_shell_prepend(GTK_MENU_SHELL(context->menu), gtk_separator_menu_item_new());
        gtk_menu_shell_prepend(GTK_MENU_SHELL(context->menu), menu_item);
      }
    }
    gtk_widget_show_all( GTK_WIDGET(context->menu) );

    gtk_menu_popup(GTK_MENU(context->menu), NULL, NULL, NULL, NULL,
		   event->button, event->time);
#else
    /* popup our special picker like menu */
    GtkWidget *dialog =
      gtk_dialog_new_with_buttons(_("Presets"),
		  GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(widget))),
				  GTK_DIALOG_MODAL,
          GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
	  NULL);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 480);

    /* create root picker */
    GtkWidget *hbox = gtk_hbox_new(TRUE, 0);

    GtkWidget *root = presets_picker(context, context->appdata->presets->items, true);
    gtk_box_pack_start_defaults(GTK_BOX(hbox), root);

    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox);

    gtk_widget_show_all(dialog);
    presets_item *item = NULL;
    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
      item = static_cast<presets_item *>(g_object_get_data(G_OBJECT(dialog), "item"));

    gtk_widget_destroy(dialog);

    if(item)
      presets_item_dialog(context, item);
#endif

    /* Tell calling code that we have handled this event; the buck
     * stops here. */
    return TRUE;
  }
  return FALSE;
}

static gint on_button_destroy(G_GNUC_UNUSED GtkWidget *widget, gpointer data) {
  presets_context_t *context = (presets_context_t*)data;

#ifndef FREMANTLE
  printf("freeing preset button context\n");
  if (context->menu)
    gtk_widget_destroy(context->menu);
#endif

  g_free(context);

  return FALSE;
}

GtkWidget *josm_build_presets_button(appdata_t *appdata,
			       tag_context_t *tag_context) {
  presets_context_t *context = g_new0(presets_context_t, 1);
  context->appdata = appdata;
  context->tag_context = tag_context;

  GtkWidget *but = button_new_with_label(_("Presets"));
  gtk_widget_set_events(but, GDK_EXPOSURE_MASK);
  gtk_widget_add_events(but, GDK_BUTTON_PRESS_MASK);
  gtk_signal_connect(GTK_OBJECT(but), "button-press-event",
		     (GtkSignalFunc)button_press, context);

  gtk_signal_connect(GTK_OBJECT(but), "destroy",
		     (GtkSignalFunc)on_button_destroy, context);

  return but;
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

presets_widget_t::presets_widget_t(presets_widget_type_t t)
  : type(t)
  , key(0)
  , text(0)
{
  // largest subentry
  memset(&combo_w, 0, sizeof(combo_w));
}

presets_widget_t::~presets_widget_t()
{
  if(key)
    xmlFree(key);
  if(text)
    xmlFree(text);

  switch(type) {
  case WIDGET_TYPE_TEXT:
    if(text_w.def)
      xmlFree(text_w.def);
    break;

  case WIDGET_TYPE_COMBO:
    if(combo_w.def)
      xmlFree(combo_w.def);
    if(combo_w.values)
      xmlFree(combo_w.values);
    break;

  case WIDGET_TYPE_KEY:
    if(key_w.value)
      xmlFree(key_w.value);
    break;

  default:
    break;
  }
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

presets_item_t::~presets_item_t()
{
  std::for_each(widgets.begin(), widgets.end(), free_widget);
}

presets_item_group::~presets_item_group()
{
  std::for_each(items.begin(), items.end(), free_item);
}

presets_items::~presets_items()
{
  std::for_each(items.begin(), items.end(), free_item);
}

// vim:et:ts=8:sw=2:sts=2:ai
