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

#include "appdata.h"
#include "icon.h"
#include "misc.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#ifdef FREMANTLE
#include <hildon/hildon-picker-button.h>
#endif
#include <strings.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

typedef enum {
  WIDGET_TYPE_LABEL = 0,
  WIDGET_TYPE_SEPARATOR,
  WIDGET_TYPE_SPACE,
  WIDGET_TYPE_COMBO,
  WIDGET_TYPE_CHECK,
  WIDGET_TYPE_TEXT,
  WIDGET_TYPE_KEY
} presets_widget_type_t;

typedef struct presets_widget_t {
  presets_widget_type_t type;

  xmlChar *key, *text;

  union {
    /* a tag with an arbitrary text value */
    struct {
      xmlChar *def;
    } text_w;

    /* a combo box with pre-defined values */
    struct {
      xmlChar *def;
      xmlChar *values;
    } combo_w;

    /* a key is just a static key */
    struct {
      xmlChar *value;
    } key_w;

    /* single checkbox */
    struct {
      gboolean def;
    } check_w;

  };

  struct presets_widget_t *next;
} presets_widget_t;

struct presets_item_t {
  int type;
  xmlChar *name, *icon, *link;
  gboolean is_group;

  union {
    presets_widget_t *widget;
    struct presets_item_t *group;
  };

  struct presets_item_t *next;
};

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

  return name;
}

static int josm_type_bit(const char *type, char sep) {
  const struct { int bit; const char *str; } types[] = {
    { PRESETS_TYPE_WAY,       "way"       },
    { PRESETS_TYPE_NODE,      "node"      },
    { PRESETS_TYPE_RELATION,  "relation"  },
    { PRESETS_TYPE_CLOSEDWAY, "closedway" },
    { 0,                      NULL        }};

  int i;
  for(i=0;types[i].bit;i++) {
    const size_t tlen = strlen(types[i].str);
    if(strncmp(types[i].str, type, tlen) == 0 && type[tlen] == sep)
      return types[i].bit;
  }

  printf("WARNING: unexpected type %s\n", type);
  return 0;
}

/* parse a comma seperated list of types and set their bits */
static int josm_type_parse(xmlChar *xtype) {
  int type_mask = 0;
  const char *type = (const char*)xtype;

  if(!type) return PRESETS_TYPE_ALL;

  const char *ntype = strchr(type, ',');
  while(ntype) {
    type_mask |= josm_type_bit(type, ',');
    type = ntype + 1;
    ntype = strchr(type, ',');
  }

  type_mask |= josm_type_bit(type, '\0');
  xmlFree(xtype);
  return type_mask;
}

/* parse children of a given node for into *widget */
static presets_widget_t **parse_widgets(xmlNode *a_node,
					presets_item_t *item,
					presets_widget_t **widget) {
  xmlNode *cur_node = NULL;

  for(cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if(cur_node->type == XML_ELEMENT_NODE) {

      if(strcmp((char*)cur_node->name, "label") == 0) {

	/* --------- label widget --------- */
	*widget = g_new0(presets_widget_t, 1);
	(*widget)->type = WIDGET_TYPE_LABEL;
	(*widget)->text = xmlGetProp(cur_node, BAD_CAST "text");

	/* special handling of pre-<space/> separators */
	if(!(*widget)->text || (strcmp((char*)(*widget)->text, " ") == 0)) {
	  (*widget)->type = WIDGET_TYPE_SEPARATOR;
	  if((*widget)->text) xmlFree((*widget)->text);
	  (*widget)->text = NULL;
	}

	widget = &((*widget)->next);

      }
      else if(strcmp((char*)cur_node->name, "space") == 0) {
#ifndef USE_HILDON
        // new-style separators
        *widget = g_new0(presets_widget_t, 1);
        (*widget)->type = WIDGET_TYPE_SEPARATOR;
	(*widget)->text = NULL;
	widget = &((*widget)->next);
#endif
      }
      else if(strcmp((char*)cur_node->name, "text") == 0) {

	/* --------- text widget --------- */
	*widget = g_new0(presets_widget_t, 1);
	(*widget)->type = WIDGET_TYPE_TEXT;
	(*widget)->text = xmlGetProp(cur_node, BAD_CAST "text");
	(*widget)->key = xmlGetProp(cur_node, BAD_CAST "key");
	(*widget)->text_w.def = xmlGetProp(cur_node, BAD_CAST "default");
	widget = &((*widget)->next);

      } else if(strcmp((char*)cur_node->name, "combo") == 0) {

	/* --------- combo widget --------- */
	*widget = g_new0(presets_widget_t, 1);
	(*widget)->type = WIDGET_TYPE_COMBO;
	(*widget)->text = xmlGetProp(cur_node, BAD_CAST "text");
	(*widget)->key = xmlGetProp(cur_node, BAD_CAST "key");
	(*widget)->combo_w.def = xmlGetProp(cur_node,
						   BAD_CAST "default");
	(*widget)->combo_w.values = xmlGetProp(cur_node, BAD_CAST "values");
	widget = &((*widget)->next);

      } else if(strcmp((char*)cur_node->name, "key") == 0) {

	/* --------- invisible key widget --------- */
	*widget = g_new0(presets_widget_t, 1);
	(*widget)->type = WIDGET_TYPE_KEY;
	(*widget)->key = xmlGetProp(cur_node, BAD_CAST "key");
	(*widget)->key_w.value = xmlGetProp(cur_node, BAD_CAST "value");
	widget = &((*widget)->next);

      } else if(strcmp((char*)cur_node->name, "check") == 0) {

	/* --------- check widget --------- */
	*widget = g_new0(presets_widget_t, 1);
	(*widget)->type = WIDGET_TYPE_CHECK;
	(*widget)->text = xmlGetProp(cur_node, BAD_CAST "text");
	(*widget)->key = xmlGetProp(cur_node, BAD_CAST "key");
	(*widget)->check_w.def = xml_get_prop_is(cur_node, "default", "on");
	widget = &((*widget)->next);

      }
      else if (strcmp((char*)cur_node->name, "optional") == 0) {
        // Could be done as a fold-out box width twisties.
        // Or maybe as a separate dialog for small screens.
        // For now, just recurse and build up our current list.
        widget = parse_widgets(cur_node, item, widget);
      }

      else if (strcmp((char*)cur_node->name, "link") == 0) {

	/* --------- link is not a widget, but a property of item --------- */
	if(!item->link) {
	  item->link = xmlGetProp(cur_node, BAD_CAST "href");
	} else
	  printf("ignoring surplus link\n");

      } else
	printf("found unhandled annotations/item/%s\n", cur_node->name);
    }
  }
  return widget;
}

static presets_item_t *parse_item(xmlNode *a_node) {
  presets_item_t *item = g_new0(presets_item_t, 1);
  item->is_group = FALSE;

  /* ------ parse items own properties ------ */
  item->name = xmlGetProp(a_node, BAD_CAST "name");

  item->icon = BAD_CAST
    josm_icon_name_adjust((char*)xmlGetProp(a_node, BAD_CAST "icon"));

  item->type =
    josm_type_parse(xmlGetProp(a_node, BAD_CAST "type"));

  presets_widget_t **widget = &item->widget;
  parse_widgets(a_node, item, widget);
  return item;
}

static presets_item_t *parse_group(xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;

  presets_item_t *group = g_new0(presets_item_t, 1);
  group->is_group = TRUE;

  /* ------ parse groups own properties ------ */
  group->name = xmlGetProp(a_node, BAD_CAST "name");

  group->icon = BAD_CAST
    josm_icon_name_adjust((char*)xmlGetProp(a_node, BAD_CAST "icon"));

  group->type = 0;

  presets_item_t **preset = &group->group;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcmp((char*)cur_node->name, "item") == 0) {
	*preset = parse_item(cur_node);
	if(*preset) {
	  group->type |= (*preset)->type;
	  preset = &((*preset)->next);
	}
      } else if(strcmp((char*)cur_node->name, "group") == 0) {
	*preset = parse_group(doc, cur_node);
	if(*preset) {
	  group->type |= (*preset)->type;
	  preset = &((*preset)->next);
	}
      } else if(strcmp((char*)cur_node->name, "separator") == 0) {
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
      if(strcmp((char*)cur_node->name, "item") == 0) {
	*preset = parse_item(cur_node);
	if(*preset) preset = &((*preset)->next);
      } else if(strcmp((char*)cur_node->name, "group") == 0) {
	*preset = parse_group(doc, cur_node);
	if(*preset) preset = &((*preset)->next);
      } else if(strcmp((char*)cur_node->name, "separator") == 0) {
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
      if(strcmp((char*)cur_node->name, "annotations") == 0) {
	presets = parse_annotations(doc, cur_node);
      } else
	printf("found unhandled %s\n", cur_node->name);
    }
  }

  xmlFreeDoc(doc);
  return presets;
}

presets_item_t *josm_presets_load(void) {
  presets_item_t *presets = NULL;

  printf("Loading JOSM presets ...\n");

  gchar *filename = find_file("presets.xml", NULL, NULL);
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

static gboolean store_value(presets_widget_t *widget,
                            tag_t **ctag, const char *value, tag_t *otag) {
  gboolean changed = FALSE;
  if(value && strlen(value)) {
    const char *chstr;
    tag_t *tag;
    if(otag) {
      /* update the previous tag structure */
      tag = otag;
      g_assert(strcasecmp(otag->key, (char*)widget->key) == 0);
      /* only update if the value actually changed */
      if(strcmp(otag->value, value) != 0) {
        changed = TRUE; /* mark as updated, actual change below */
        chstr = "updated ";
      } else {
        chstr = "kept ";
      }
    } else {
      /* no old entry, create a new one */
      tag = g_new0(tag_t, 1);
      osm_tag_update_key(tag, (char*)widget->key);
      /* value will be updated below */
      *ctag = tag;
      changed = TRUE;
      chstr = "new ";
    }

    if(changed)
      osm_tag_update_value(tag, value);

    printf("%skey = %s, value = %s\n", chstr,
           widget->key, tag->value);
  } else if (otag) {
    g_free(otag->value);
    otag->value = NULL; /* mark this entry as deleted */
    changed = TRUE;
    printf("removed key = %s\n", widget->key);
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

static gboolean is_widget_interactive(const presets_widget_t *w)
{
  switch(w->type) {
  case WIDGET_TYPE_LABEL:
  case WIDGET_TYPE_SEPARATOR:
  case WIDGET_TYPE_SPACE:
  case WIDGET_TYPE_KEY:
    return FALSE;
  default:
    return TRUE;
  }
}

static gboolean preset_combo_insert_value(GtkWidget *combo, const char *value,
                                          const char *preset)
{
  combo_box_append_text(combo, value);

  return (g_strcmp0(preset, value) == 0) ? TRUE : FALSE;
}

typedef struct {
  appdata_t *appdata;
#ifndef FREMANTLE
  GtkWidget *menu;
#endif
  tag_context_t *tag_context;
} presets_context_t;

static void presets_item_dialog(presets_context_t *context,
                                const presets_item_t *item) {
  appdata_t *appdata = context->appdata;
  GtkWindow *parent = GTK_WINDOW(context->tag_context->dialog);
  tag_t **orig_tag = context->tag_context->tag;

  GtkWidget *dialog = NULL;
  gboolean ok = FALSE;

  printf("dialog for item %s\n", item->name);

  /* build dialog from items widget list */
  presets_widget_t *widget = item->widget;

  /* count total number of widgets and number of widgets that */
  /* have an interactive gui element. We won't show a dialog */
  /* at all if there's no interactive gui element at all */
  gint widget_cnt = 0, interactive_widget_cnt = 0;
  while(widget) {
    if(is_widget_interactive(widget))
      interactive_widget_cnt++;

    widget_cnt++;
    widget = widget->next;
  }

  /* allocate space for required number of gtk widgets */
  GtkWidget **gtk_widgets = (GtkWidget**)g_new0(GtkWidget, widget_cnt);

  if(interactive_widget_cnt)  {
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
    widget = item->widget;
    if(widget && (widget->type == WIDGET_TYPE_LABEL)) {
      gtk_window_set_title(GTK_WINDOW(dialog), (char*)widget->text);

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
      tag_t *otag = osm_tag_find(*orig_tag, (char*)widget->key);
      const char *preset = otag ? otag->value : NULL;

      switch(widget->type) {
      case WIDGET_TYPE_SEPARATOR:
	attach_both(table, gtk_hseparator_new(), widget_cnt-widget_skip);
	break;

      case WIDGET_TYPE_SPACE:
	/* space is just an empty label until we find something better */
	attach_both(table, gtk_label_new(" "), widget_cnt-widget_skip);
	break;

      case WIDGET_TYPE_LABEL:
	attach_both(table, gtk_label_new((char*)widget->text), widget_cnt-widget_skip);
	break;

      case WIDGET_TYPE_COMBO:
#ifndef FREMANTLE
	attach_text(table, (char*)widget->text, widget_cnt-widget_skip);
#endif

	if(!preset && widget->combo_w.def)
	  preset = (char*)widget->combo_w.def;
	gtk_widgets[widget_cnt] = combo_box_new((char*)widget->text);
	combo_box_append_text(gtk_widgets[widget_cnt], _("<unset>"));
	const xmlChar *value = widget->combo_w.values;
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

      case WIDGET_TYPE_CHECK:
	{ gboolean def = FALSE;
	  if(preset) def = ((strcasecmp(preset, "true") == 0) ||
			    (strcasecmp(preset, "yes") == 0));
	  else       def = widget->check_w.def;

	  gtk_widgets[widget_cnt] =
	    check_button_new_with_label((char*)widget->text);
	  check_button_set_active(gtk_widgets[widget_cnt], def);
#ifndef FREMANTLE
	  attach_right(table, gtk_widgets[widget_cnt], widget_cnt-widget_skip);
#else
	  attach_both(table, gtk_widgets[widget_cnt], widget_cnt-widget_skip);
#endif
      } break;

    case WIDGET_TYPE_TEXT:
      attach_text(table, (char*)widget->text, widget_cnt-widget_skip);

      if(!preset && widget->text_w.def)
        preset = (char*)widget->text_w.def;
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

      widget_cnt++;
      widget = widget->next;
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
      ok = TRUE;

  } else
    ok = TRUE;

  if(ok) {
    /* handle all children of the table */
    gboolean changed = FALSE;
    widget = item->widget;
    widget_cnt = 0;
    tag_t **last = orig_tag;
    while (*last)
      last = &(*last)->next;

    while(widget) {
      tag_t *otag = gtk_widgets[widget_cnt] ? g_object_get_data(G_OBJECT(gtk_widgets[widget_cnt]), "tag") : NULL;
      switch(widget->type) {
      case WIDGET_TYPE_COMBO:
	g_assert(GTK_WIDGET_TYPE(gtk_widgets[widget_cnt]) == combo_box_type());

	const char *text = combo_box_get_active_text(gtk_widgets[widget_cnt]);
	if(!strcmp(text, _("<unset>")))
	  text = NULL;

	changed |= store_value(widget, last, text, otag);
	break;

      case WIDGET_TYPE_TEXT:
	g_assert(GTK_WIDGET_TYPE(gtk_widgets[widget_cnt]) == entry_type());

	changed |= store_value(widget, last, gtk_entry_get_text(
		     GTK_ENTRY(gtk_widgets[widget_cnt])), otag);
	break;

      case WIDGET_TYPE_CHECK:
	g_assert(GTK_WIDGET_TYPE(gtk_widgets[widget_cnt]) == check_button_type());

	changed |= store_value(widget, last,
                 check_button_get_active(gtk_widgets[widget_cnt])?"yes":NULL, otag);
	break;

      case WIDGET_TYPE_KEY:
	g_assert(!gtk_widgets[widget_cnt]);
	g_assert(!otag);
	otag = osm_tag_find(*orig_tag, (char*)widget->key);

	changed |= store_value(widget, last, (char*)widget->key_w.value, otag);
	break;

      default:
	break;
      }

      if(*last)
        last = &(*last)->next;

      widget_cnt++;
      widget = widget->next;
    }

    if(changed) {
      tag_t **last = orig_tag;
      while (*last) {
        tag_t *tmp = *last;
        /* marked as deleted by store_value() */
        if(tmp->value == NULL) {
          *last = tmp->next;
          g_free(tmp);
        } else {
          last = &tmp->next;
        }
      }
      info_tags_replace(context->tag_context);
    }
  }

  g_free(gtk_widgets);

  if(interactive_widget_cnt)
    gtk_widget_destroy(dialog);
}

/* ------------------- the item list (popup menu) -------------- */

/**
 * @brief check if the currently active object uses this preset and the preset is interactive
 */
static gboolean preset_is_used(const presets_item_t *item, const presets_context_t *context)
{
  gboolean matches_all = FALSE;
  gboolean is_interactive = FALSE;
  const presets_widget_t *w = item->widget;
  for(w = item->widget; w; w = w->next) {
    if(w->type != WIDGET_TYPE_KEY) {
      is_interactive |= is_widget_interactive(w);
      continue;
    }
    const tag_t t = {
      .key = (char*) w->key,
      .value = (char*) w->key_w.value
    };
    if(osm_tag_key_and_value_present(*(context->tag_context->tag), &t))
      matches_all = TRUE;
    else
      return FALSE;
  }

  return matches_all && is_interactive;
}

#ifndef PICKER_MENU
static void
cb_menu_item(GtkWidget *menu_item, gpointer data) {
  presets_context_t *context = (presets_context_t*)data;

  presets_item_t *item = g_object_get_data(G_OBJECT(menu_item), "item");
  g_assert(item);

  presets_item_dialog(context, item);
}

static GtkWidget *create_menuitem(presets_context_t *context, presets_item_t *item)
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

static GtkWidget *build_menu(presets_context_t *context,
			     presets_item_t *item, GtkWidget **matches) {
  GtkWidget *menu = gtk_menu_new();
  gboolean was_separator = FALSE;
  /* avoid showing separators at the top of a menu */
  gboolean was_item = FALSE;

  for(; item; item = item->next) {
    /* check if this presets entry is appropriate for the current item */
    if(item->type & context->tag_context->presets_type && item->name) {
      GtkWidget *menu_item;

      /* Show a separator if one was requested, but not if there was no item
       * before to prevent to show one as the first entry. */
      if(was_item && was_separator)
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
      was_item = TRUE;
      was_separator = FALSE;

      menu_item = create_menuitem(context, item);

      if(item->is_group) {
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
                                  build_menu(context, item->group, matches));
      } else {
        g_object_set_data(G_OBJECT(menu_item), "item", item);
        g_signal_connect(menu_item, "activate",
                         GTK_SIGNAL_FUNC(cb_menu_item), context);

        if(preset_is_used(item, context)) {
          if(!*matches)
            *matches = gtk_menu_new();

          GtkWidget *used_item = create_menuitem(context, item);
          g_object_set_data(G_OBJECT(used_item), "item", item);
          g_signal_connect(used_item, "activate",
                           GTK_SIGNAL_FUNC(cb_menu_item), context);
          gtk_menu_shell_append(GTK_MENU_SHELL(*matches), used_item);
        }
      }

      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    } else if(!item->name)
      /* Record that there was a separator. Do not immediately add it here to
       * prevent to show one as last entry. */
      was_separator = TRUE;
  }

  return menu;
}
#else // PICKER_MENU

static gboolean preset_group_is_used(const presets_item_t *item, const presets_context_t *context)
{
  g_assert(item->is_group);
  const presets_item_t *child;
  for (child = item->group; child; child = child->next) {
    if(child->is_group) {
      if(preset_group_is_used(child, context))
        return TRUE;
    } else if(preset_is_used(child, context))
      return TRUE;
  }

  return FALSE;
}

enum {
  PRESETS_PICKER_COL_ICON = 0,
  PRESETS_PICKER_COL_NAME,
  PRESETS_PICKER_COL_ITEM_PTR,
  PRESETS_PICKER_COL_SUBMENU_ICON,
  PRESETS_PICKER_COL_SUBMENU_PTR,
  PRESETS_PICKER_NUM_COLS
};

static GtkWidget *presets_picker(presets_context_t *context, presets_item_t *item, gboolean scan_for_recent);
static GtkWidget *preset_picker_recent(presets_context_t *context);

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
    presets_item_t *item = NULL, *sub_item = NULL;
    gtk_tree_model_get(model, &iter,
		       PRESETS_PICKER_COL_SUBMENU_PTR, &sub_item,
		       PRESETS_PICKER_COL_ITEM_PTR, &item,
		       -1);

    printf("clicked on %s, submenu = %p\n", item ? (char*)item->name : "''", sub_item);

    GtkWidget *view =
      GTK_WIDGET(gtk_tree_selection_get_tree_view(selection));

    if(sub_item || (!item && !sub_item)) {
      /* check if this already had a submenu */
      GtkWidget *sub = GTK_WIDGET(g_object_get_data(G_OBJECT(view), "sub"));
      g_assert(sub);

      gtk_widget_destroy(sub);

      /* views parent is a scrolled window whichs parent in turn is the hbox */
      GtkWidget *hbox = view->parent->parent;

      if(sub_item) {
        /* normal submenu */
        sub = presets_picker(context, sub_item, FALSE);
      } else {
        /* used presets submenu */
        sub = preset_picker_recent(context);
      }
      gtk_box_pack_start_defaults(GTK_BOX(hbox), sub);
      gtk_widget_show_all(sub);
      g_object_set_data(G_OBJECT(view), "sub", (gpointer)sub);
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

static GtkTreeIter preset_insert_item(const presets_item_t *item, icon_t **icons,
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

static void insert_recent_items(const presets_item_t *first, presets_context_t *context,
                                GtkListStore *store) {
  const presets_item_t *preset;
  for(preset = first; preset; preset = preset->next) {
    if(preset->is_group)
      insert_recent_items(preset->group, context, store);
    else if(preset_is_used(preset, context))
      preset_insert_item(preset, &context->appdata->icon, store);
  }
}

static GtkWidget *preset_picker_recent(presets_context_t *context) {
  GtkTreeView *view;
  GtkListStore *store = presets_picker_store(&view);

  insert_recent_items(context->appdata->presets, context, store);

  return presets_picker_embed(view, store, context);
}

static GtkWidget *
presets_picker(presets_context_t *context, presets_item_t *item, gboolean scan_for_recent) {
  GtkTreeView *view;
  GtkListStore *store = presets_picker_store(&view);

  gboolean show_recent = FALSE;
  GdkPixbuf *subicon = icon_load(&context->appdata->icon,
                                 "submenu_arrow");
  for(; item; item = item->next) {
    /* check if this presets entry is appropriate for the current item */
    if(!(item->type & context->tag_context->presets_type))
      continue;

    if(!item->name)
      continue;

    GtkTreeIter iter = preset_insert_item(item, &context->appdata->icon, store);

    /* mark submenues as such */
    if(item->is_group) {
      gtk_list_store_set(store, &iter,
			 PRESETS_PICKER_COL_SUBMENU_PTR,  item->group,
			 PRESETS_PICKER_COL_SUBMENU_ICON, subicon,
			 -1);
      if(scan_for_recent) {
        show_recent = preset_group_is_used(item, context);
        scan_for_recent = !show_recent;
      }
    } else if(scan_for_recent) {
      show_recent = preset_is_used(item, context);
      scan_for_recent = !show_recent;
    }
  }
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
      context->menu = build_menu(context, context->appdata->presets, &matches);
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

    GtkWidget *root = presets_picker(context, context->appdata->presets, TRUE);
    gtk_box_pack_start_defaults(GTK_BOX(hbox), root);

    GtkWidget *sub = gtk_label_new(NULL);
    gtk_box_pack_start_defaults(GTK_BOX(hbox), sub);

    g_object_set_data(G_OBJECT(gtk_bin_get_child(GTK_BIN(root))),
		      "sub", (gpointer)sub);

    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox);

    gtk_widget_show_all(dialog);
    presets_item_t *item = NULL;
    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
      item = (presets_item_t*)g_object_get_data(G_OBJECT(dialog), "item");

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

static void free_widget(presets_widget_t *widget) {
  if(widget->key) xmlFree(widget->key);
  if(widget->text) xmlFree(widget->text);

  switch(widget->type) {
  case WIDGET_TYPE_TEXT:
    if(widget->text_w.def) xmlFree(widget->text_w.def);
    break;

  case WIDGET_TYPE_COMBO:
    if(widget->combo_w.def) xmlFree(widget->combo_w.def);
    if(widget->combo_w.values) xmlFree(widget->combo_w.values);
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
  if(item->link) xmlFree(item->link);

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
