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

#include "style.h"

#include "appdata.h"
#include "map.h"
#include "misc.h"
#include "settings.h"

#include <algorithm>
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string>
#include <strings.h>

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

static float parse_scale_max(xmlNodePtr cur_node) {
  float scale_max = xml_get_prop_float(cur_node, "scale-max");
  if (!isnan(scale_max))
    return scaledn_to_zoom(scale_max);
  else
    return 0.0f;
}

static void parse_style_node(xmlNode *a_node, xmlChar **fname, style_t *style) {
  xmlNode *cur_node = NULL, *sub_node = NULL;

  /* -------------- setup defaults -------------------- */
  /* (the defaults are pretty much the potlatch style) */
  style->area.border_width      = 2.0;
  style->area.color             = 0x00000060; // 37.5%
  style->area.zoom_max          = 0.1111;     // zoom factor above which an area is visible & selectable

  style->node.radius            = 4.0;
  style->node.border_radius     = 2.0;
  style->node.color             = 0x000000ff; // black with filling ...
  style->node.fill_color        = 0x008800ff; // ... in dark green
  style->node.show_untagged     = FALSE;
  style->node.zoom_max          = 0.4444;     // zoom factor above which a node is visible & selectable

  style->track.width            = 6.0;
  style->track.color            = 0x0000ff40; // blue
  style->track.gps_color        = 0x000080ff;

  style->way.width              = 3.0;
  style->way.color              = 0x606060ff; // grey
  style->way.zoom_max           = 0.2222;     // zoom above which it's visible & selectable

  style->highlight.width        = 3.0;
  style->highlight.color        = 0xffff0080;  // normal highlights are yellow
  style->highlight.node_color   = 0xff000080;  // node highlights are red
  style->highlight.touch_color  = 0x0000ff80;  // touchnode and
  style->highlight.arrow_color  = 0x0000ff80;  // arrows are blue
  style->highlight.arrow_limit  = 4.0;

  style->frisket.mult           = 3.0;
  style->frisket.color          = 0xffffffff;
  style->frisket.border.present = TRUE;
  style->frisket.border.width   = 6.0;
  style->frisket.border.color   = 0x00000099;

  style->icon.enable            = FALSE;
  style->icon.scale             = 1.0;    // icon size (multiplier)

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(fname != NULL && strcasecmp((char*)cur_node->name, "elemstyles") == 0) {
	*fname = xmlGetProp(cur_node, BAD_CAST "filename");

	/* ---------- node ------------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "node") == 0) {
	parse_color(cur_node, "color", &style->node.color);
	parse_color(cur_node, "fill-color", &style->node.fill_color);
        style->node.radius = xml_get_prop_float(cur_node, "radius");
        style->node.border_radius = xml_get_prop_float(cur_node, "border-radius");
        style->node.zoom_max = parse_scale_max(cur_node);

	style->node.show_untagged =
	  xml_get_prop_is(cur_node, "show-untagged", "true");

	/* ---------- icon ------------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "icon") == 0) {
        style->icon.scale = xml_get_prop_float(cur_node, "scale");
	char *prefix = (char*)xmlGetProp(cur_node, BAD_CAST "path-prefix");
	if(prefix) {
	  g_free(style->icon.path_prefix);
	  style->icon.path_prefix = prefix;
	}
	style->icon.enable = xml_get_prop_is(cur_node, "enable", "true");

	/* ---------- way ------------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "way") == 0) {
	parse_color(cur_node, "color", &style->way.color);
        style->way.width = xml_get_prop_float(cur_node, "width");
        style->way.zoom_max = parse_scale_max(cur_node);

	/* ---------- frisket --------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "frisket") == 0) {
        style->frisket.mult = xml_get_prop_float(cur_node, "mult");
	parse_color(cur_node, "color", &style->frisket.color);
	style->frisket.border.present = FALSE;

	for(sub_node = cur_node->children; sub_node; sub_node=sub_node->next) {
	  if(sub_node->type == XML_ELEMENT_NODE) {
	    if(strcasecmp((char*)sub_node->name, "border") == 0) {
	      style->frisket.border.present = TRUE;
              style->frisket.border.width = xml_get_prop_float(sub_node, "width");

	      parse_color(sub_node, "color", &style->frisket.border.color);
	    }
	  }
	}

	/* ---------- highlight ------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "highlight") == 0) {
	parse_color(cur_node, "color", &style->highlight.color);
	parse_color(cur_node, "node-color", &style->highlight.node_color);
	parse_color(cur_node, "touch-color", &style->highlight.touch_color);
	parse_color(cur_node, "arrow-color", &style->highlight.arrow_color);
        style->highlight.width = xml_get_prop_float(cur_node, "width");
        style->highlight.arrow_limit = xml_get_prop_float(cur_node, "arrow-limit");

	/* ---------- track ------------------------------------ */
      } else if(strcasecmp((char*)cur_node->name, "track") == 0) {
	parse_color(cur_node, "color", &style->track.color);
	parse_color(cur_node, "gps-color", &style->track.gps_color);
        style->track.width = xml_get_prop_float(cur_node, "width");

	/* ---------- area ------------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "area") == 0) {
	style->area.has_border_color =
	  parse_color(cur_node, "border-color", &style->area.border_color);
        style->area.border_width = xml_get_prop_float(cur_node,"border-width");
        style->area.zoom_max = parse_scale_max(cur_node);

	parse_color(cur_node, "color", &style->area.color);

	/* ---------- background ------------------------------- */
      } else if(strcasecmp((char*)cur_node->name, "background") == 0) {
	parse_color(cur_node, "color", &style->background.color);

      } else
	printf("  found unhandled style/%s\n", cur_node->name);
    }
  }

  if(!style->icon.path_prefix)
    style->icon.path_prefix = g_strdup("standard");
}

/**
 * @brief parse a style definition file
 * @param appdata the global information structure
 * @param fullname absolute path of the file to read
 * @param fname location to store name of the object style XML file or NULL
 * @param name_only only parse the style name, leave all other fields empty
 * @return a new style pointer
 */
static style_t *style_parse(appdata_t *appdata, const std::string &fullname,
                            xmlChar **fname, gboolean name_only) {
  xmlDoc *doc = xmlReadFile(fullname.c_str(), NULL, 0);

  /* parse the file and get the DOM */
  if(doc == NULL) {
    xmlErrorPtr errP = xmlGetLastError();
    errorf(GTK_WIDGET(appdata->window),
	   _("Style parsing failed:\n\n"
	     "XML error while parsing style file\n"
	     "%s"), errP->message);

    return NULL;
  } else {
    /* Get the root element node */
    xmlNode *cur_node = NULL;
    style_t *style = NULL;

    for(cur_node = xmlDocGetRootElement(doc);
        cur_node; cur_node = cur_node->next) {
      if (cur_node->type == XML_ELEMENT_NODE) {
        if(strcasecmp((char*)cur_node->name, "style") == 0) {
          if(!style) {
            style = new style_t();
            style->name = (char*)xmlGetProp(cur_node, BAD_CAST "name");
            if(name_only)
              break;
            parse_style_node(cur_node, fname, style);
            style->iconP = &appdata->icon;
          }
        } else
	  printf("  found unhandled %s\n", cur_node->name);
      }
    }

    xmlFreeDoc(doc);
    return style;
  }
}

static style_t *style_load_fname(appdata_t *appdata, const std::string &filename) {
  xmlChar *fname = NULL;
  style_t *style = style_parse(appdata, filename, &fname, FALSE);

  printf("  elemstyle filename: %s\n", fname);
  style->elemstyles = josm_elemstyles_load((char*)fname);
  xmlFree(fname);

  return style;
}

style_t *style_load(appdata_t *appdata) {
  const char *name = appdata->settings->style;
  printf("Trying to load style %s\n", name);

  std::string fullname = find_file(name, ".style");

  if (fullname.empty()) {
    printf("style %s not found, trying %s instead\n", name, DEFAULT_STYLE);
    fullname = find_file(DEFAULT_STYLE ".style");
    if (fullname.empty()) {
      printf("  style not found, failed to find fallback style too\n");
      return NULL;
    }
  }

  printf("  style filename: %s\n", fullname.c_str());

  style_t *style = style_load_fname(appdata, fullname);

  return style;
}

static std::string style_basename(const std::string &name) {
  std::string::size_type pos = name.rfind("/");

  /* and cut off extension */
  std::string::size_type extpos = name.rfind(".");
  if(pos == name.npos)
    pos = 0;
  else
    pos++; // skip also the '/' itself

  return name.substr(pos, extpos - pos);
}

struct combo_add_styles {
  GtkWidget * const cbox;
  int cnt;
  int match;
  appdata_t * const appdata;
  combo_add_styles(GtkWidget *w, appdata_t *a) : cbox(w), cnt(0), match(-1), appdata(a) {};
  void operator()(const std::string &filename);
};

void combo_add_styles::operator()(const std::string &filename)
{
  printf("  file: %s\n", filename.c_str());

  style_t *style = style_parse(appdata, filename, NULL, TRUE);
  printf("    name: %s\n", style->name);
  combo_box_append_text(cbox, style->name);

  const std::string &basename = style_basename(filename);
  if(strcmp(basename.c_str(), appdata->settings->style) == 0)
    match = cnt;

  delete style;

  cnt++;
}

GtkWidget *style_select_widget(appdata_t *appdata) {
  const std::vector<std::string> &chain = file_scan(".style");

  /* there must be at least one style, otherwise */
  /* the program wouldn't be running */
  g_assert(!chain.empty());

  GtkWidget *cbox = combo_box_new(_("Style"));

  /* fill combo box with presets */
  combo_add_styles cas(cbox, appdata);
  std::for_each(chain.begin(), chain.end(), cas);

  if(cas.match >= 0)
    combo_box_set_active(cbox, cas.match);

  return cbox;
}

struct style_find {
  appdata_t * const appdata;
  const char * const name;
  style_find(appdata_t *a, const char *n) : appdata(a), name(n) {}
  bool operator()(const std::string &filename);
};

bool style_find::operator()(const std::string &filename)
{
  style_t *style = style_parse(appdata, filename, NULL, TRUE);

  bool match = (strcmp(style->name, name) == 0);
  delete style;

  return match;
}

void style_change(appdata_t *appdata, const char *name) {
  const std::vector<std::string> &chain = file_scan(".style");

  const std::vector<std::string>::const_iterator it =
      std::find_if(chain.begin(), chain.end(), style_find(appdata, name));

  g_assert(it != chain.end());
  const std::string &new_style = style_basename(*it);

  /* check if style has really been changed */
  if(appdata->settings->style &&
     !strcmp(appdata->settings->style, new_style.c_str())) {
    return;
  }

  style_t *nstyle = style_load_fname(appdata, *it);
  if (nstyle == NULL) {
    errorf(GTK_WIDGET(appdata->window),
           _("Error loading style %s"), it->c_str());
    return;
  }

  g_free(appdata->settings->style);
  appdata->settings->style = g_strdup(new_style.c_str());

  map_clear(appdata, MAP_LAYER_OBJECTS_ONLY);
  /* let gtk clean up first */
  while(gtk_events_pending()) {
    putchar('.');
    gtk_main_iteration();
  }

  delete appdata->map->style;
  appdata->map->style = nstyle;

  /* canvas background may have changed */
  canvas_set_background(appdata->map->canvas,
			appdata->map->style->background.color);

  map_paint(appdata);
}

#ifndef FREMANTLE
/* in fremantle this happens inside the submenu handling since this button */
/* is actually placed inside the submenu there */
void style_select(GtkWidget *parent, appdata_t *appdata) {

  printf("select style\n");

  /* ------------------ style dialog ---------------- */
  GtkWidget *dialog =
    misc_dialog_new(MISC_DIALOG_NOSIZE,_("Select style"),
		    GTK_WINDOW(parent),
		    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		    NULL);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

  GtkWidget *cbox = style_select_widget(appdata);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 8);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), gtk_label_new(_("Style:")));

  gtk_box_pack_start_defaults(GTK_BOX(hbox), cbox);
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox);

  gtk_widget_show_all(dialog);

  if(GTK_RESPONSE_ACCEPT != gtk_dialog_run(GTK_DIALOG(dialog))) {
    printf("user clicked cancel\n");
    gtk_widget_destroy(dialog);
    return;
  }

  const char *ptr = combo_box_get_active_text(cbox);
  printf("user clicked ok on %s\n", ptr);

  gtk_widget_destroy(dialog);

  style_change(appdata, ptr);
}
#endif

style_t::style_t()
  : iconP(0)
  , name(0)
{
  memset(&icon, 0, sizeof(icon));
  memset(&track, 0, sizeof(track));
  memset(&way, 0, sizeof(way));
  memset(&area, 0, sizeof(area));
  memset(&frisket, 0, sizeof(frisket));
  memset(&node, 0, sizeof(node));
  memset(&highlight, 0, sizeof(highlight));

  background.color = 0xffffffff; // white
}

style_t::~style_t()
{
  printf("freeing style\n");

  josm_elemstyles_free(elemstyles);

  g_free(name);
  g_free(icon.path_prefix);
}

//vim:et:ts=8:sw=2:sts=2:ai
