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

#include "appdata.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

#ifdef ENABLE_BROWSER_INTERFACE
#ifndef USE_HILDON
#include <libgnome/gnome-url.h>
#else
#include <tablet-browser-interface.h>
#endif

typedef struct {
  appdata_t *appdata;
  char *link;
} www_context_t;

/* ---------- simple interface to the systems web browser ---------- */
static void on_info(GtkWidget *widget, www_context_t *context) {
#ifndef USE_HILDON
  /* taken from gnome-open, part of libgnome */
  GError *err = NULL;
  gnome_url_show(context->link, &err);
#else
  osso_rpc_run_with_defaults(context->appdata->osso_context, "osso_browser",
			     OSSO_BROWSER_OPEN_NEW_WINDOW_REQ, NULL,
			     DBUS_TYPE_STRING, context->link,
			     DBUS_TYPE_BOOLEAN, FALSE, DBUS_TYPE_INVALID);
#endif
}
#endif

/* --------------------- presets.xml parsing ----------------------- */

static gboolean xmlGetPropIs(xmlNode *node, char *prop, char *is) {
  char *prop_str = (char*)xmlGetProp(node, BAD_CAST prop);
  if(!prop_str) return FALSE;

  gboolean retval = FALSE;
  retval = (strcasecmp(prop_str, is) == 0);
  xmlFree(prop_str);
  return retval;
}

static presets_value_t *xmlGetPropValues(xmlNode *node, char *prop) {
  char *prop_str = (char*)xmlGetProp(node, BAD_CAST prop);
  if(!prop_str) return NULL;
  presets_value_t *value = NULL, **cur = &value;

  /* cut values strings */
  char *c, *p = prop_str;
  while((c = strchr(p, ','))) {

    *cur = g_new0(presets_value_t, 1);
    (*cur)->text = g_strndup(p, c-p);
    cur = &((*cur)->next);

    p = c+1;
  }

  /* attach remaining string as last value */
  *cur = g_new0(presets_value_t, 1);
  (*cur)->text = g_strdup(p);

  xmlFree(prop_str);
  return value;
}

char *josm_icon_name_adjust(char *name) {
  if(!name) return NULL;

  /* the icon loader uses names without extension */
  if(!strcasecmp(name+strlen(name)-4, ".png"))
    name[strlen(name)-4] = 0;

#ifdef JOSM_PATH_ADJUST
  if(strncmp(name, "presets/", strlen("presets/")) != 0) {
    if(strrchr(name, '/')) {
      char *new = g_strdup_printf("presets%s", strrchr(name, '/'));
      xmlFree(name);
      name = new;

      printf("icon path adjusted to %s\n", name);
    }
  }
#endif
  return name;
}

static int josm_type_bit(char *type) {
  struct { int bit; char *str; } types[] = {
    { PRESETS_TYPE_WAY,       "way"       },
    { PRESETS_TYPE_NODE,      "node"      },
    { PRESETS_TYPE_RELATION,  "relation"  },
    { PRESETS_TYPE_CLOSEDWAY, "closedway" },
    { 0,                      NULL        }};

  int i;
  for(i=0;types[i].bit;i++) {
    if(strcasecmp(types[i].str, type) == 0)
      return types[i].bit;
  }

  printf("WARNING: unexpected type %s\n", type);
  return 0;
}

/* parse a comma seperated list of types and set their bits */
static int josm_type_parse(char *type) {
  int type_mask = 0;

  if(!type) return PRESETS_TYPE_ALL;

  while(strchr(type, ',')) {
    *strchr(type, ',') = 0;

    type_mask |= josm_type_bit(type);
    type = type + strlen(type) + 1;
  }

  type_mask |= josm_type_bit(type);
  return type_mask;
}

/* parse children of a given node for into *widget */
static presets_widget_t **parse_widgets(xmlNode *a_node, 
					presets_item_t *item,
					presets_widget_t **widget) {
  xmlNode *cur_node = NULL;

  for(cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if(cur_node->type == XML_ELEMENT_NODE) {

      if(strcasecmp((char*)cur_node->name, "space") == 0) {

	/* --------- space widget --------- */
	/* we are low on screen space on the handhelds, */
	/* so we just ignore extra spaces */
#ifndef USE_HILDON
	*widget = g_new0(presets_widget_t, 1);
	(*widget)->type = WIDGET_TYPE_SPACE;
#endif

      } else if(strcasecmp((char*)cur_node->name, "label") == 0) {

	/* --------- label widget --------- */
	*widget = g_new0(presets_widget_t, 1);
	(*widget)->type = WIDGET_TYPE_LABEL;
	(*widget)->text = (char*)xmlGetProp(cur_node, BAD_CAST "text");

	/* special handling of pre-<space/> separators */
	if(!(*widget)->text || (strcmp((*widget)->text, " ") == 0)) {
	  (*widget)->type = WIDGET_TYPE_SEPARATOR;
	  if((*widget)->text) xmlFree((*widget)->text);
	  (*widget)->text = NULL;
	}

	widget = &((*widget)->next);

      }
      else if(strcasecmp((char*)cur_node->name, "space") == 0) {
        // new-style separators
        *widget = g_new0(presets_widget_t, 1);
        (*widget)->type = WIDGET_TYPE_SEPARATOR;
	(*widget)->text = NULL;
	widget = &((*widget)->next);
      } 
      else if(strcasecmp((char*)cur_node->name, "text") == 0) {

	/* --------- text widget --------- */
	*widget = g_new0(presets_widget_t, 1);
	(*widget)->type = WIDGET_TYPE_TEXT;
	(*widget)->text = (char*)xmlGetProp(cur_node, BAD_CAST "text");
	(*widget)->key = (char*)xmlGetProp(cur_node, BAD_CAST "key");
	(*widget)->del_if_empty = xmlGetPropIs(cur_node, "delete_if_empty", "true");
	(*widget)->text_w.def = (char*)xmlGetProp(cur_node, BAD_CAST "default");
	widget = &((*widget)->next);

      } else if(strcasecmp((char*)cur_node->name, "combo") == 0) {

	/* --------- combo widget --------- */
	*widget = g_new0(presets_widget_t, 1);
	(*widget)->type = WIDGET_TYPE_COMBO;
	(*widget)->text = (char*)xmlGetProp(cur_node, BAD_CAST "text");
	(*widget)->key = (char*)xmlGetProp(cur_node, BAD_CAST "key");
	(*widget)->del_if_empty = xmlGetPropIs(cur_node, 
					       "delete_if_empty", "true");
	(*widget)->combo_w.def = (char*)xmlGetProp(cur_node, 
						   BAD_CAST "default");
	(*widget)->combo_w.values = xmlGetPropValues(cur_node, "values");
	widget = &((*widget)->next);

      } else if(strcasecmp((char*)cur_node->name, "key") == 0) {

	/* --------- invisible key widget --------- */
	*widget = g_new0(presets_widget_t, 1);
	(*widget)->type = WIDGET_TYPE_KEY;
	(*widget)->key = (char*)xmlGetProp(cur_node, BAD_CAST "key");
	(*widget)->key_w.value = (char*)xmlGetProp(cur_node, BAD_CAST "value");
	widget = &((*widget)->next);

      } else if(strcasecmp((char*)cur_node->name, "check") == 0) {

	/* --------- check widget --------- */
	*widget = g_new0(presets_widget_t, 1);
	(*widget)->type = WIDGET_TYPE_CHECK;
	(*widget)->text = (char*)xmlGetProp(cur_node, BAD_CAST "text");
	(*widget)->key = (char*)xmlGetProp(cur_node, BAD_CAST "key");
	(*widget)->del_if_empty = xmlGetPropIs(cur_node, 
					       "delete_if_empty", "true");
	(*widget)->check_w.def = xmlGetPropIs(cur_node, "default", "on");
	widget = &((*widget)->next);

      }
      else if (strcasecmp((char*)cur_node->name, "optional") == 0) {
        // Could be done as a fold-out box width twisties.
        // Or maybe as a separate dialog for small screens.
        // For now, just recurse and build up our current list.
        widget = parse_widgets(cur_node, item, widget);
      }

      else if (strcasecmp((char*)cur_node->name, "link") == 0) {

	/* --------- link is not a widget, but a property of item --------- */
	if(!item->link) {
	  item->link = (char*)xmlGetProp(cur_node, BAD_CAST "href");
	} else
	  printf("ignoring surplus link\n");
	
      } else 
	printf("found unhandled annotations/item/%s\n", cur_node->name);
    }
  }
  return widget;
}

static presets_item_t *parse_item(xmlDocPtr doc, xmlNode *a_node) {
  presets_item_t *item = g_new0(presets_item_t, 1);
  item->is_group = FALSE;

  /* ------ parse items own properties ------ */
  item->name = (char*)xmlGetProp(a_node, BAD_CAST "name");

  item->icon = 
    josm_icon_name_adjust((char*)xmlGetProp(a_node, BAD_CAST "icon"));

  item->type = 
    josm_type_parse((char*)xmlGetProp(a_node, BAD_CAST "type"));

  presets_widget_t **widget = &item->widget;
  parse_widgets(a_node, item, widget);
  return item;
}

static presets_item_t *parse_group(xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;

  presets_item_t *group = g_new0(presets_item_t, 1);
  group->is_group = TRUE;
  
  /* ------ parse groups own properties ------ */
  group->name = (char*)xmlGetProp(a_node, BAD_CAST "name");

  group->icon = 
    josm_icon_name_adjust((char*)xmlGetProp(a_node, BAD_CAST "icon"));

  group->type = 0;

  presets_item_t **preset = &group->group;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "item") == 0) {
	*preset = parse_item(doc, cur_node);
	if(*preset) {
	  group->type |= (*preset)->type;
	  preset = &((*preset)->next);
	}
      } else if(strcasecmp((char*)cur_node->name, "group") == 0) {
	*preset = parse_group(doc, cur_node);
	if(*preset) {
	  group->type |= (*preset)->type;
	  preset = &((*preset)->next);
	}
      } else if(strcasecmp((char*)cur_node->name, "separator") == 0) {
	*preset = g_new0(presets_item_t, 1);
	preset = &((*preset)->next);
      } else 
	printf("found unhandled annotations/group/%s\n", cur_node->name);
    }
  }  

  

  return group;
}

static presets_item_t *parse_annotations(xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;
  presets_item_t *presets = NULL, **preset = &presets;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "item") == 0) {
	*preset = parse_item(doc, cur_node);
	if(*preset) preset = &((*preset)->next);
      } else if(strcasecmp((char*)cur_node->name, "group") == 0) {
	*preset = parse_group(doc, cur_node);
	if(*preset) preset = &((*preset)->next);
      } else if(strcasecmp((char*)cur_node->name, "separator") == 0) {
	*preset = g_new0(presets_item_t, 1);
	preset = &((*preset)->next);
      } else 
	printf("found unhandled annotations/%s\n", cur_node->name);
    }
  }  
  return presets;
}

static presets_item_t *parse_doc(xmlDocPtr doc) {
  /* Get the root element node */
  xmlNode *cur_node = NULL;
  presets_item_t *presets = NULL; 

  for(cur_node = xmlDocGetRootElement(doc); 
      cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "annotations") == 0) {
	presets = parse_annotations(doc, cur_node);
      } else 
	printf("found unhandled %s\n", cur_node->name);
    }
  }
    
  xmlFreeDoc(doc);
  xmlCleanupParser();
  return presets;
}

presets_item_t *josm_presets_load(void) {
  presets_item_t *presets = NULL;

  printf("Loading JOSM presets ...\n");
    
  LIBXML_TEST_VERSION;

  char *filename = find_file("presets.xml");
  if(!filename) return NULL;
  
  /* parse the file and get the DOM */
  xmlDoc *doc = NULL;
  if((doc = xmlReadFile(filename, NULL, 0)) == NULL) {
    xmlErrorPtr errP = xmlGetLastError();
    printf("presets download failed: "
	   "XML error while parsing:\n"
	   "%s\n", errP->message);
  } else {
    printf("ok, parse doc tree\n");
    presets = parse_doc(doc);
  }

  g_free(filename);
  return presets;
}

/* --------------------- the items dialog -------------------- */

static void attach_both(GtkWidget *table, GtkWidget *widget, gint y) {
  gtk_table_attach(GTK_TABLE(table), widget, 0,2,y,y+1, 
		   GTK_EXPAND | GTK_FILL, 0,0,0);
}

static void attach_text(GtkWidget *table, char *text, gint y) {
  gtk_table_attach(GTK_TABLE(table), gtk_label_new(text), 0,1,y,y+1, 
		   GTK_EXPAND | GTK_FILL, 0,0,0);
}

static void attach_right(GtkWidget *table, GtkWidget *widget, gint y) {
  gtk_table_attach(GTK_TABLE(table), widget, 1,2,y,y+1, 
		   GTK_EXPAND | GTK_FILL, 0,0,0);
}

static tag_t **store_value(presets_widget_t *widget, tag_t **ctag, 
			   char *value) {
  if((value && strlen(value)) || !widget->del_if_empty) {
    *ctag = g_new0(tag_t, 1);
    (*ctag)->key = g_strdup(widget->key);
    (*ctag)->value = g_strdup(value?value:"");
	  
    printf("key = %s, value = %s\n", 
	   widget->key, (*ctag)->value);
    
    ctag = &((*ctag)->next);
  } else
    printf("ignore empty key = %s\n", widget->key);

  return ctag;
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

static tag_t *presets_item_dialog(appdata_t *appdata, GtkWindow *parent, 
		     presets_item_t *item, tag_t *orig_tag) {
  GtkWidget *dialog = NULL;
  gboolean ok = FALSE;
  tag_t *tag = NULL, **ctag = &tag;
#ifdef ENABLE_BROWSER_INTERFACE
  www_context_t *www_context = NULL;
#endif

  printf("dialog for item %s\n", item->name);

  /* build dialog from items widget list */
  presets_widget_t *widget = item->widget;

  /* count total number of widgets and number of widgets that */
  /* have an interactive gui element. We won't show a dialog */
  /* at all if there's no interactive gui element at all */
  gint widget_cnt = 0, interactive_widget_cnt = 0;
  while(widget) {
    if((widget->type != WIDGET_TYPE_LABEL) &&
       (widget->type != WIDGET_TYPE_SEPARATOR) &&
       (widget->type != WIDGET_TYPE_KEY))
      interactive_widget_cnt++;

    widget_cnt++;
    widget = widget->next;
  }

  /* allocate space for required number of gtk widgets */
  GtkWidget **gtk_widgets = (GtkWidget**)g_new0(GtkWidget, widget_cnt);

  if(interactive_widget_cnt)  {
    dialog = 
      misc_dialog_new(MISC_DIALOG_NOSIZE,
		      item->name, parent,
		      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
		      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		      NULL);

#ifdef ENABLE_BROWSER_INTERFACE
    /* if a web link has been provided for this item install */
    /* a button for this */
    if(item->link) {
      www_context = g_new0(www_context_t, 1);
      www_context->link = item->link;
      www_context->appdata = appdata;
      
      GtkWidget *button = gtk_dialog_add_button(GTK_DIALOG(dialog), _
			("Info"), GTK_RESPONSE_HELP);
      gtk_signal_connect(GTK_OBJECT(button), "clicked",
			 GTK_SIGNAL_FUNC(on_info), (gpointer)www_context);
    }
#endif

    /* special handling for the first label/separators */
    guint widget_skip = 0;  // number of initial widgets to skip
    widget = item->widget;
    if(widget && (widget->type == WIDGET_TYPE_LABEL)) {
      gtk_window_set_title(GTK_WINDOW(dialog), widget->text);

      widget_skip++;   // this widget isn't part of the contents anymore
      widget = widget->next;

      /* skip all following separators (and keys) */
      while(widget && 
	    ((widget->type == WIDGET_TYPE_SEPARATOR) ||
	     (widget->type == WIDGET_TYPE_SPACE) ||
	     (widget->type == WIDGET_TYPE_KEY))) {
	widget_skip++;   // this widget isn't part of the contents anymore
	widget = widget->next;
      }
    }

    /* create table of required size */
    GtkWidget *table = gtk_table_new(widget_cnt-widget_skip, 2, FALSE);
  
    widget_cnt = widget_skip;
    while(widget) {
      /* check if there's a value with this key already */
      char *preset = osm_tag_get_by_key(orig_tag, widget->key);

      switch(widget->type) {
      case WIDGET_TYPE_SEPARATOR:
	attach_both(table, gtk_hseparator_new(), widget_cnt-widget_skip);
	break;

      case WIDGET_TYPE_SPACE:
	/* space is just an empty label until we find something better */
	attach_both(table, gtk_label_new(" "), widget_cnt-widget_skip);
	break;

      case WIDGET_TYPE_LABEL:
	attach_both(table, gtk_label_new(widget->text), widget_cnt-widget_skip);
	break;
	
      case WIDGET_TYPE_COMBO:
	attach_text(table, widget->text, widget_cnt-widget_skip);

	if(!preset && widget->combo_w.def) preset = widget->combo_w.def;
	gtk_widgets[widget_cnt] = gtk_combo_box_new_text();
	presets_value_t *value = widget->combo_w.values;
	int count = 0, active = -1;
	while(value) {
	  if(active < 0 && preset && strcmp(preset, value->text)==0)
	    active = count;
	  
	  gtk_combo_box_append_text(GTK_COMBO_BOX(gtk_widgets[widget_cnt]), 
				    value->text);
	  value = value->next;
	  count++;
	}
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(gtk_widgets[widget_cnt]), 
				 active);
	attach_right(table, gtk_widgets[widget_cnt], widget_cnt-widget_skip);
	break;
	
      case WIDGET_TYPE_CHECK:
	{ gboolean def = FALSE;
	  if(preset) def = ((strcasecmp(preset, "true") == 0) ||
			    (strcasecmp(preset, "yes") == 0));
	  else       def = widget->check_w.def;

	  gtk_widgets[widget_cnt] = 
	    gtk_check_button_new_with_label(widget->text);
	  gtk_toggle_button_set_active(
	    GTK_TOGGLE_BUTTON(gtk_widgets[widget_cnt]), def);
	  attach_right(table, gtk_widgets[widget_cnt], widget_cnt-widget_skip);
      } break;
      
    case WIDGET_TYPE_TEXT:
      attach_text(table, widget->text, widget_cnt-widget_skip);
      
      if(!preset && widget->text_w.def) preset = widget->text_w.def;
      gtk_widgets[widget_cnt] = gtk_entry_new();
      if(preset)
	gtk_entry_set_text(GTK_ENTRY(gtk_widgets[widget_cnt]), preset);

      attach_right(table, gtk_widgets[widget_cnt], widget_cnt-widget_skip);
      break;
      
      default:
	break;
      }
      
      widget_cnt++;
      widget = widget->next;
    }
    
#ifndef USE_HILDON
    /* add widget to dialog */
    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), table);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 50);
#else
    /* put it into a scrolled window */
    GtkWidget *scroll_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_win), 
				   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll_win), 
    					  table);

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
      ok = TRUE;

  } else
    ok = TRUE;
  
  if(ok) {
    /* handle all children of the table */
    widget = item->widget;
    widget_cnt = 0;
    while(widget) {
      switch(widget->type) {
      case WIDGET_TYPE_COMBO:
	g_assert(GTK_WIDGET_TYPE(gtk_widgets[widget_cnt]) == 
		 GTK_TYPE_COMBO_BOX);

	ctag = store_value(widget, ctag, gtk_combo_box_get_active_text(
		GTK_COMBO_BOX(gtk_widgets[widget_cnt])));
	break;

      case WIDGET_TYPE_TEXT:
	g_assert(GTK_WIDGET_TYPE(gtk_widgets[widget_cnt]) == 
		 GTK_TYPE_ENTRY);

	ctag = store_value(widget, ctag, (char*)gtk_entry_get_text(
		     GTK_ENTRY(gtk_widgets[widget_cnt])));
	break;

      case WIDGET_TYPE_CHECK:
	g_assert(GTK_WIDGET_TYPE(gtk_widgets[widget_cnt]) == 
		 GTK_TYPE_CHECK_BUTTON);

	ctag = store_value(widget, ctag,
                 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			   gtk_widgets[widget_cnt]))?"yes":
		    (widget->del_if_empty?NULL:"no"));
	break;

      case WIDGET_TYPE_KEY:
	g_assert(!gtk_widgets[widget_cnt]);

	ctag = store_value(widget, ctag, widget->key_w.value);
	break;

      default:
	break;
      }
      
      widget_cnt++;
      widget = widget->next;
    }
  }

  g_free(gtk_widgets);

  if(interactive_widget_cnt)
    gtk_widget_destroy(dialog);

#ifdef ENABLE_BROWSER_INTERFACE
  if(www_context)
    g_free(www_context);
#endif

  return tag;
}

/* ------------------- the item list (popup menu) -------------- */ 

typedef struct {
  appdata_t *appdata;
  GtkWidget *menu;
  tag_context_t *tag_context;
} presets_context_t;

static void 
cb_menu_item(GtkMenuItem *menu_item, gpointer data) {
  presets_context_t *context = (presets_context_t*)data;

  presets_item_t *item = g_object_get_data(G_OBJECT(menu_item), "item");
  g_assert(item);

  tag_t *tag = 
    presets_item_dialog(context->appdata, 
			GTK_WINDOW(context->tag_context->dialog), item,
			*context->tag_context->tag);

  if(tag) {
    tag_context_t *tag_context = context->tag_context;

    /* add new tags to the old list and replace entries with the same key */

    while(tag) {
#if 0
      printf("current:\n");
      tag_t *mdst = &tag_context->tag;
      while(mdst) {
	printf("%s: %s\n", msdt
#endif      

      tag_t *next = tag->next;
      tag->next = NULL;

      tag_t **dst = tag_context->tag;
      gboolean replaced = FALSE;
      while(*dst && !replaced) {
	if(strcasecmp((*dst)->key, tag->key) == 0) {
	  g_free((*dst)->value);
	  (*dst)->value = g_strdup(tag->value);
	  replaced = TRUE;
	}
	dst = &(*dst)->next;
      }

      /* if nothing was replaced, then just append new tag */
      if(!replaced) 
	*dst = tag;
      else
	osm_tag_free(tag);
      
      tag = next;
    }

#if 0
    /* free existing tags */
    osm_tags_free(*tag_context->tag);

    /* attach new tags */
    *tag_context->tag = tag;
#endif

    info_tags_replace(tag_context);
  }
}

static GtkWidget *build_menu(presets_context_t *context, 
			     presets_item_t *item) {
  GtkWidget *menu = gtk_menu_new();
  
  while(item) {
    GtkWidget *menu_item;

    /* check if this presets entry is appropriate for the current item */
    if(item->type & context->tag_context->presets_type) {

      if(item->name) {
	if(!item->icon)
	  menu_item = gtk_menu_item_new_with_label(item->name);
	else {
	  menu_item = gtk_image_menu_item_new_with_label(item->name);
	  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), 
			icon_widget_load(&context->appdata->icon, item->icon));
	}
	
	if(item->is_group) 
	  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), 
				    build_menu(context, item->group));
	else {
	  g_object_set_data(G_OBJECT(menu_item), "item", item);
	  g_signal_connect(menu_item, "activate", 
			   GTK_SIGNAL_FUNC(cb_menu_item), context);
	}
      } else
	menu_item = gtk_separator_menu_item_new();

      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);      
    }

    item = item->next;
  }

  return menu;
}

static gint button_press(GtkWidget *widget, GdkEventButton *event, 
			 gpointer data) {
  presets_context_t *context = (presets_context_t*)data;

  if(event->type == GDK_BUTTON_PRESS) {
    printf("button press %d %d\n", event->button, event->time);

    gtk_menu_popup(GTK_MENU(context->menu), NULL, NULL, NULL, NULL,
		   event->button, event->time);

    /* Tell calling code that we have handled this event; the buck
     * stops here. */
    return TRUE;
  }
  return FALSE;
}

static gint on_button_destroy(GtkWidget *widget, gpointer data) {
  presets_context_t *context = (presets_context_t*)data;

  printf("freeing preset button context\n");
  gtk_widget_destroy(context->menu);
  g_free(context);

  return FALSE;
}

GtkWidget *josm_presets_select(appdata_t *appdata, 
			       tag_context_t *tag_context) {
  presets_context_t *context = g_new0(presets_context_t, 1);
  context->appdata = appdata;
  context->tag_context = tag_context;

  context->menu = build_menu(context, appdata->presets);
  gtk_widget_show_all( GTK_WIDGET(context->menu) );

  GtkWidget *but = gtk_button_new_with_label(_("Presets"));
  gtk_widget_set_events(but, GDK_EXPOSURE_MASK);
  gtk_widget_add_events(but, GDK_BUTTON_PRESS_MASK);
  gtk_signal_connect(GTK_OBJECT(but), "button-press-event",
		     (GtkSignalFunc)button_press, context);

  gtk_signal_connect(GTK_OBJECT(but), "destroy",
		     (GtkSignalFunc)on_button_destroy, context);

  return but;
}

/* ----------------------- cleaning up --------------------- */

static void free_values(presets_value_t *value) {
  while(value) {
    presets_value_t *next = value->next;
    if(value->text) g_free(value->text);
    g_free(value);
    value = next;
  }

}

static void free_widget(presets_widget_t *widget) {
  if(widget->key) xmlFree(widget->key);
  if(widget->text) xmlFree(widget->text);

  switch(widget->type) {
  case WIDGET_TYPE_TEXT:
    if(widget->text_w.def) xmlFree(widget->text_w.def);
    break;

  case WIDGET_TYPE_COMBO:
    if(widget->combo_w.def) xmlFree(widget->combo_w.def);
    if(widget->combo_w.values) free_values(widget->combo_w.values);
    break;

  case WIDGET_TYPE_KEY:
    if(widget->key_w.value) xmlFree(widget->key_w.value);
    break;

  default:
    break;
  }

  g_free(widget);
}

static void free_widgets(presets_widget_t *widget) {
  while(widget) {
    presets_widget_t *next = widget->next;
    free_widget(widget);
    widget = next;
  }
}

static void free_items(presets_item_t *item);
static void free_item(presets_item_t *item) {
  if(item->name) xmlFree(item->name);
  if(item->icon) xmlFree(item->icon);

  if(item->is_group)
    free_items(item->group);
  else
    free_widgets(item->widget);

  g_free(item);
}

static void free_items(presets_item_t *item) {
  while(item) {
    presets_item_t *next = item->next;
    free_item(item);
    item = next;
  }
}

void josm_presets_free(presets_item_t *presets) {
  free_items(presets);
}

// vim:et:ts=8:sw=2:sts=2:ai
