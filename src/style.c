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

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

static void xml_get_prop_float(xmlNode *node, char *prop, float *value) {
  char *str = (char*)xmlGetProp(node, BAD_CAST prop);
  if(str) {
    *value = strtod(str, NULL);
    xmlFree(str);
  }
}

static int xml_get_prop_opaque(xmlNode *node, char *prop) {
  float opaque = 100;
  xml_get_prop_float(node, prop, &opaque);
  if(opaque > 255) opaque = 255;
  return(0xff & (int)(0.5 + opaque * 2.55));
}

static gboolean xml_prop_is(xmlNode *node, char *prop, char *str) {
  char *prop_str = (char*)xmlGetProp(node, BAD_CAST prop);
  if(!prop_str) return FALSE;

  gboolean match = (strcasecmp(prop_str, str) == 0);
  xmlFree(prop_str);
  return match;
}

static style_t *parse_style(xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL, *sub_node = NULL;
  style_t *style = g_new0(style_t, 1);

  style->name = (char*)xmlGetProp(a_node, BAD_CAST "name");

  /* -------------- setup defaults -------------------- */
  /* (the defaults are pretty much the potlatch style) */
  style->background.color       = 0xffffff;   // white

  style->area.border_width      = 2.0;
  style->area.opaque            = 0x60;       // 37.5%
  style->area.zoom_max          = 0.1111;     // zoom factor above which an area is visible & selectable

  style->node.radius            = 4.0;
  style->node.border_radius     = 2.0;
  style->node.color             = 0x000000;   // black
  style->node.has_fill_color    = TRUE;       // is filled ...
  style->node.fill_color        = 0x008800;   // ... in dark green
  style->node.show_untagged     = FALSE;
  style->node.zoom_max          = 0.4444;     // zoom factor above which a node is visible & selectable

  style->track.width            = 6.0;
  style->track.color            = 0x0000ff40; // blue
  style->track.gps_color        = 0x000080;

  style->way.width              = 3.0;
  style->way.color              = 0x606060;   // grey
  style->way.zoom_max           = 0.2222;       // zoom above which it's visible & selectable

  style->highlight.width        = 3.0;
  style->highlight.color        = 0xffff0080;
  style->highlight.node_color   = 0xff000080;
  style->highlight.touch_color  = 0x0000ff80;
  style->highlight.arrow_color  = 0x0000ff80;
  style->highlight.arrow_limit  = 4.0;

  style->frisket.mult           = 3.0;
  style->frisket.opaque         = 0xff;
  style->frisket.border.present = TRUE;
  style->frisket.border.width   = 6.0;
  style->frisket.border.color   = 0x00000099;

  style->icon.enable            = FALSE;
  style->icon.scale             = 1.0;    // icon size (multiplier)
  style->icon.path_prefix       = g_strdup("standard");

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "elemstyles") == 0) {
	style->elemstyles_filename = 
	  (char*)xmlGetProp(cur_node, BAD_CAST "filename");

	/* ---------- node ------------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "node") == 0) {
	parse_color(cur_node, "color", &style->node.color);
	style->node.has_fill_color = 
	  parse_color(cur_node, "fill-color", &style->node.fill_color);
	xml_get_prop_float(cur_node, "radius", &style->node.radius);
	xml_get_prop_float(cur_node, "border-radius", 
			   &style->node.border_radius);
        float scale_max = 0;
	xml_get_prop_float(cur_node, "scale-max", &scale_max);
	if (scale_max > 0)
	  style->node.zoom_max = scaledn_to_zoom(scale_max);
	else
	  style->node.zoom_max = 0;

	style->node.show_untagged = 
	  xml_prop_is(cur_node, "show-untagged", "true");

	/* ---------- icon ------------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "icon") == 0) {
	xml_get_prop_float(cur_node, "scale", &style->icon.scale);
        style->icon.path_prefix = (char*)xmlGetProp(cur_node, BAD_CAST "path-prefix");
	style->icon.enable = xml_prop_is(cur_node, "enable", "true");

	/* ---------- way ------------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "way") == 0) {
	parse_color(cur_node, "color", &style->way.color);
	xml_get_prop_float(cur_node, "width", &style->way.width);
	float scale_max = 0;
	xml_get_prop_float(cur_node, "scale-max", &scale_max);
	if (scale_max > 0)
	  style->way.zoom_max = scaledn_to_zoom(scale_max);
	else
	  style->way.zoom_max = 0;

	/* ---------- frisket --------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "frisket") == 0) {
	xml_get_prop_float(cur_node, "mult", &style->frisket.mult);
	style->frisket.opaque = xml_get_prop_opaque(cur_node, "opaque");
	style->frisket.border.present = FALSE;

	for(sub_node = cur_node->children; sub_node; sub_node=sub_node->next) {
	  if(sub_node->type == XML_ELEMENT_NODE) {
	    if(strcasecmp((char*)sub_node->name, "border") == 0) {
	      style->frisket.border.present = TRUE;
	      xml_get_prop_float(sub_node, "width", 
				 &style->frisket.border.width);

	      gboolean color_set = 
		parse_color(sub_node, "color", &style->frisket.border.color);
	      int opaque = xml_get_prop_opaque(sub_node, "opaque");
	      if(color_set)
		style->frisket.border.color = 
		  (style->frisket.border.color<<8) | opaque;
	    }
	  }
	}

	/* ---------- highlight ------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "highlight") == 0) {
	gboolean color_set = 
	  parse_color(cur_node, "color", &style->highlight.color);
	gboolean node_color_set = 
	  parse_color(cur_node, "node-color", &style->highlight.node_color);
	gboolean touch_color_set = 
	  parse_color(cur_node, "touch-color", &style->highlight.touch_color);
	gboolean arrow_color_set = 
	  parse_color(cur_node, "arrow-color", &style->highlight.arrow_color);
	xml_get_prop_float(cur_node, "width", &style->highlight.width);
	xml_get_prop_float(cur_node, "arrow-limit", 
			   &style->highlight.arrow_limit);

	int op = xml_get_prop_opaque(cur_node, "opaque");
	if(color_set) 
	  style->highlight.color = (style->highlight.color << 8) | op;
	if(node_color_set) 
	  style->highlight.node_color =(style->highlight.node_color << 8)|op;
	if(touch_color_set) 
	  style->highlight.touch_color =(style->highlight.touch_color << 8)|op;
	if(arrow_color_set)
	  style->highlight.arrow_color =(style->highlight.arrow_color << 8)|op;

	/* ---------- track ------------------------------------ */
      } else if(strcasecmp((char*)cur_node->name, "track") == 0) {
	gboolean color_set = 
	  parse_color(cur_node, "color", &style->track.color);
	parse_color(cur_node, "gps-color", &style->track.gps_color);
	xml_get_prop_float(cur_node, "width", &style->track.width);

	int opaque = xml_get_prop_opaque(cur_node, "opaque");
	if(color_set) style->track.color = (style->track.color<<8) | opaque;

	/* ---------- area ------------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "area") == 0) {
	style->area.has_border_color = 
	  parse_color(cur_node, "border-color", &style->area.border_color);
	xml_get_prop_float(cur_node,"border-width", &style->area.border_width);
    float scale_max = 0;
	xml_get_prop_float(cur_node, "scale-max", &scale_max);
    if (scale_max > 0)
	  style->area.zoom_max = scaledn_to_zoom(scale_max);
	else
      style->area.zoom_max = 0;

	style->area.opaque = xml_get_prop_opaque(cur_node, "opaque");

	/* ---------- background ------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "background") == 0) {
	parse_color(cur_node, "color", &style->background.color);

      } else
	printf("  found unhandled style/%s\n", cur_node->name);
    }
  }  
  return style;
}

static style_t *parse_doc(xmlDocPtr doc) {
  /* Get the root element node */
  xmlNode *cur_node = NULL;
  style_t *style = NULL; 

  for(cur_node = xmlDocGetRootElement(doc); 
      cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "style") == 0) {
	if(!style)
	  style = parse_style(doc, cur_node);
      } else 
	printf("  found unhandled %s\n", cur_node->name);
    }
  }
    
  xmlFreeDoc(doc);
  xmlCleanupParser();
  return style;
}

static style_t *style_parse(appdata_t *appdata, char *fullname) {
  style_t *style = NULL;
  
  xmlDoc *doc = NULL;
    
  LIBXML_TEST_VERSION;
    
  /* parse the file and get the DOM */
  if((doc = xmlReadFile(fullname, NULL, 0)) == NULL) {
    xmlErrorPtr errP = xmlGetLastError();
    errorf(GTK_WIDGET(appdata->window), 
	   _("Style parsing failed:\n\n"
	     "XML error while parsing style file\n"
	     "%s"), errP->message);
    
    return NULL;
  } else {
    style = parse_doc(doc);
    style->iconP = &appdata->icon;
  }

  return style;
}

style_t *style_load(appdata_t *appdata, char *name) {
  printf("Trying to load style %s\n", name);

  char *filename = g_strdup_printf("%s.style", name);
  char *fullname = find_file(filename);
  g_free(filename);

  if (!fullname) {
    printf("style %s not found, trying %s instead\n", name, DEFAULT_STYLE);
    filename = g_strdup_printf("%s.style", DEFAULT_STYLE);
    fullname = find_file(filename);
    g_free(filename);
    if (!fullname) {
      printf("  style not found, failed to find fallback style too\n");
      return NULL;
    }
  }

  printf("  style filename: %s\n", fullname);

  style_t *style = style_parse(appdata, fullname);
  g_free(fullname);

  printf("  elemstyle filename: %s\n", style->elemstyles_filename);
  elemstyle_t *elemstyles = 
    josm_elemstyles_load(style->elemstyles_filename);
  xmlFree(style->elemstyles_filename);
  style->elemstyles = elemstyles;

  return style;
}

void style_free(style_t *style) {
  if(!style) return;

  printf("freeing style\n");

  if(style->elemstyles)
    josm_elemstyles_free(style->elemstyles);

  if(style->name) g_free(style->name);

  if (style->icon.path_prefix) g_free(style->icon.path_prefix);

  g_free(style);
}

static char *style_basename(char *name) {
  char *retval = name;

  if(strrchr(name, '/'))
    retval = strrchr(name, '/') + 1;

  /* create a local copy */
  retval = g_strdup(retval);

  /* and cut off extension */
  if(strrchr(retval, '.'))
    *strrchr(retval, '.') = 0;

  return retval;
}

void style_select(GtkWidget *parent, appdata_t *appdata) {
  file_chain_t *chain = file_scan("*.style");

  printf("select style\n");

  /* there must be at least one style, otherwise */
  /* the program wouldn't be running */
  g_assert(chain);

  /* ------------------ style dialog ---------------- */
  GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Select style"),
	  GTK_WINDOW(parent), GTK_DIALOG_MODAL,
	  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
	  NULL);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 8);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), gtk_label_new(_("Style:")));

  GtkWidget *cbox = NULL;
  cbox = gtk_combo_box_new_text();

  /* fill combo box with presets */
  int cnt = 0, match = -1;
  file_chain_t *lchain = chain;
  while(lchain) {
    printf("  file: %s\n", lchain->name);
    
    style_t *style = style_parse(appdata, lchain->name);
    printf("    name: %s\n", style->name);
    gtk_combo_box_append_text(GTK_COMBO_BOX(cbox), style->name);

    char *basename = style_basename(lchain->name);
    if(strcmp(basename, appdata->settings->style) == 0) match = cnt;
    g_free(basename);
    
    xmlFree(style->elemstyles_filename);
    style->elemstyles_filename = NULL;
    style_free(style);
    
    lchain = lchain->next;
    cnt++;
  }

  if(match >= 0)
    gtk_combo_box_set_active(GTK_COMBO_BOX(cbox), match);
  
  gtk_box_pack_start_defaults(GTK_BOX(hbox), cbox);
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox);

  gtk_widget_show_all(dialog);

  if(GTK_RESPONSE_ACCEPT != gtk_dialog_run(GTK_DIALOG(dialog))) {
    printf("user clicked cancel\n");
    gtk_widget_destroy(dialog);
    return;
  }
  
  char *ptr = gtk_combo_box_get_active_text(GTK_COMBO_BOX(cbox));
  printf("user clicked ok on %s\n", ptr);  

  while(chain) {
    file_chain_t *next = chain->next;
    style_t *style = style_parse(appdata, chain->name);

    if(strcmp(style->name, ptr) == 0) {
      if(appdata->settings->style)
	g_free(appdata->settings->style);

      appdata->settings->style = style_basename(chain->name);
    }

    xmlFree(style->elemstyles_filename);
    style->elemstyles_filename = NULL;
    style_free(style);

    g_free(chain);
    chain = next;
  }

  gtk_widget_destroy(dialog);

  map_clear(appdata, MAP_LAYER_OBJECTS_ONLY);
  /* let gtk clean up first */
  while(gtk_events_pending()) {
    putchar('.');
    gtk_main_iteration();
  }

  style_free(appdata->map->style);
  appdata->map->style = style_load(appdata, appdata->settings->style);

  /* canvas background may have changed */
  g_object_set(G_OBJECT(appdata->map->canvas), "background-color-rgb", 
	       appdata->map->style->background.color, NULL);

  map_paint(appdata);
}
