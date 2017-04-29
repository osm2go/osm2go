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

#include "wms.h"

#include "appdata.h"
#include "list.h"
#include "map.h"
#include "misc.h"
#include "net_io.h"
#include "project.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <strings.h>
#include <vector>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

#define WMS_FORMAT_JPG  (1<<0)
#define WMS_FORMAT_JPEG (1<<1)
#define WMS_FORMAT_PNG  (1<<2)
#define WMS_FORMAT_GIF  (1<<3)

struct wms_llbbox_t {
  wms_llbbox_t() : valid(FALSE) {}
  pos_t min, max;
  gboolean valid;
};

struct wms_layer_t {
  explicit wms_layer_t(const std::string &t = std::string(),
                       const std::string &n = std::string(),
                       const std::string &s = std::string(),
                       bool epsg = false,
                       const wms_llbbox_t &x = wms_llbbox_t())
    : title(t), name(n), srs(s), epsg4326(epsg), llbbox(x) {}
  ~wms_layer_t();

  typedef std::vector<wms_layer_t *> list;

  std::string title;
  std::string name;
  std::string srs;
  bool epsg4326;
  wms_llbbox_t llbbox;

  list children;

  bool is_usable() const {
    return !name.empty() && epsg4326 && llbbox.valid;
  }
  static const char *EPSG4326() {
    return "EPSG:4326";
  }
};

struct wms_getmap_t {
  wms_getmap_t()
    : format(0) {}
  guint format;
};

struct wms_request_t {
  wms_getmap_t getmap;
};

struct wms_cap_t {
  wms_layer_t::list layers;
  wms_request_t request;
};

struct wms_t {
  wms_t(const std::string &s, const std::string &p)
    : server(s), path(p), width(0), height(0) {}
  ~wms_t();

  std::string server;
  std::string path;
  gint width, height;

  wms_cap_t cap;
};

static void wms_server_free(wms_server_t *wms_server) {
  g_free(wms_server->name);
  g_free(wms_server->server);
  g_free(wms_server->path);
  g_free(wms_server);
}

static gboolean xmlTextIs(xmlDocPtr doc, xmlNodePtr list, const char *str) {
  xmlChar *nstr = xmlNodeListGetString(doc, list, 1);
  if(!nstr) return FALSE;

  gboolean match = (strcmp(str, (char*)nstr) == 0);
  xmlFree(nstr);
  return match;
}

static gboolean wms_bbox_is_valid(pos_t *min, pos_t *max) {
  /* all four coordinates are valid? */
  if(isnan(min->lat)||isnan(min->lon)||isnan(max->lat)||isnan(max->lon))
    return FALSE;

  /* min/max span a useful range? */
  if(max->lat - min->lat < 0.1) return FALSE;
  if(max->lon - min->lon < 0.1) return FALSE;

  /* useful angles? */
  if(min->lat > 90.0  || min->lat < -90.0)  return FALSE;
  if(max->lat > 90.0  || max->lat < -90.0)  return FALSE;
  if(min->lon > 180.0 || min->lon < -180.0) return FALSE;
  if(max->lon > 180.0 || max->lon < -180.0) return FALSE;

  return TRUE;
}

static wms_layer_t *wms_cap_parse_layer(xmlDocPtr doc, xmlNode *a_node) {
  wms_layer_t *wms_layer = O2G_NULLPTR;
  xmlNode *cur_node = O2G_NULLPTR;

  wms_layer = new wms_layer_t();
  wms_layer->llbbox.min.lon = wms_layer->llbbox.min.lat = NAN;
  wms_layer->llbbox.max.lon = wms_layer->llbbox.max.lat = NAN;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "Layer") == 0) {
        wms_layer_t *children = wms_cap_parse_layer(doc, cur_node);
        if(children)
          wms_layer->children.push_back(children);
      } else if(strcasecmp((char*)cur_node->name, "Name") == 0) {
	xmlChar *str = xmlNodeListGetString(doc, cur_node->children, 1);
	wms_layer->name = reinterpret_cast<char *>(str);
	xmlFree(str);
      } else if(strcasecmp((char*)cur_node->name, "Title") == 0) {
	xmlChar *str = xmlNodeListGetString(doc, cur_node->children, 1);
	wms_layer->title = reinterpret_cast<char *>(str);
	xmlFree(str);
      } else if(strcasecmp((char*)cur_node->name, "SRS") == 0) {
	xmlChar *str = xmlNodeListGetString(doc, cur_node->children, 1);
        if(strcmp(reinterpret_cast<char *>(str), wms_layer_t::EPSG4326()) == 0)
          wms_layer->epsg4326 = true;
        else
          wms_layer->srs = reinterpret_cast<char *>(str);
        printf("SRS = %s\n", str);
	xmlFree(str);
      } else if(strcasecmp((char*)cur_node->name, "LatLonBoundingBox") == 0) {
	wms_layer->llbbox.min.lat = xml_get_prop_float(cur_node, "miny");
	wms_layer->llbbox.min.lon = xml_get_prop_float(cur_node, "minx");
	wms_layer->llbbox.max.lat = xml_get_prop_float(cur_node, "maxy");
	wms_layer->llbbox.max.lon = xml_get_prop_float(cur_node, "maxx");
      } else
	printf("found unhandled WMT_MS_Capabilities/Capability/Layer/%s\n",
	       cur_node->name);
    }
  }

  wms_layer->llbbox.valid = wms_bbox_is_valid(&wms_layer->llbbox.min,
					      &wms_layer->llbbox.max);

  printf("------------------- Layer: %s ---------------------------\n",
	 wms_layer->title.c_str());
  printf("Name: %s\n", wms_layer->name.c_str());
  printf("EPSG-4326: %s\n", wms_layer->epsg4326?"yes":"no");
  if(wms_layer->llbbox.valid)
    printf("LatLonBBox: %f/%f %f/%f\n",
	   wms_layer->llbbox.min.lat, wms_layer->llbbox.min.lon,
	   wms_layer->llbbox.max.lat, wms_layer->llbbox.max.lon);
  else
    printf("No/invalid LatLonBBox\n");

  return wms_layer;
}

static wms_getmap_t wms_cap_parse_getmap(xmlDocPtr doc, xmlNode *a_node) {
  wms_getmap_t wms_getmap;
  xmlNode *cur_node;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "Format") == 0) {
	if(xmlTextIs(doc, cur_node->children, "image/png"))
	  wms_getmap.format |= WMS_FORMAT_PNG;
	else if(xmlTextIs(doc, cur_node->children, "image/gif"))
	  wms_getmap.format |= WMS_FORMAT_GIF;
	else if(xmlTextIs(doc, cur_node->children, "image/jpg"))
	  wms_getmap.format |= WMS_FORMAT_JPG;
	else if(xmlTextIs(doc, cur_node->children, "image/jpeg"))
	  wms_getmap.format |= WMS_FORMAT_JPEG;
      } else
	printf("found unhandled "
	       "WMT_MS_Capabilities/Capability/Request/GetMap/%s\n",
	       cur_node->name);
    }
  }

  printf("Supported formats: %s%s%s\n",
	 (wms_getmap.format & WMS_FORMAT_PNG)?"png ":"",
	 (wms_getmap.format & WMS_FORMAT_GIF)?"gif ":"",
	 (wms_getmap.format & (WMS_FORMAT_JPG | WMS_FORMAT_JPEG))?"jpg ":"");
  return wms_getmap;
}

static wms_request_t wms_cap_parse_request(xmlDocPtr doc, xmlNode *a_node) {
  wms_request_t wms_request;
  xmlNode *cur_node;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "GetMap") == 0) {
	wms_request.getmap = wms_cap_parse_getmap(doc, cur_node);
      } else
	printf("found unhandled WMT_MS_Capabilities/Capability/Request/%s\n",
	       cur_node->name);
    }
  }

  return wms_request;
}

static bool wms_cap_parse_cap(xmlDocPtr doc, xmlNode *a_node, wms_cap_t *wms_cap) {
  xmlNode *cur_node;
  bool has_request = false;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "Request") == 0) {
	wms_cap->request = wms_cap_parse_request(doc, cur_node);
        has_request = true;
      } else if(strcasecmp((char*)cur_node->name, "Layer") == 0) {
        wms_layer_t *layer = wms_cap_parse_layer(doc, cur_node);
        if(layer)
          wms_cap->layers.push_back(layer);
      } else
	printf("found unhandled WMT_MS_Capabilities/Capability/%s\n",
	       cur_node->name);
    }
  }

  return has_request && !wms_cap->layers.empty();
}

static bool wms_cap_parse(wms_t *wms, xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = O2G_NULLPTR;
  bool has_service = false, has_cap = false;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      if(strcasecmp((char*)cur_node->name, "Service") == 0) {
        has_service = true;
      } else if(strcasecmp((char*)cur_node->name, "Capability") == 0) {
        if(!has_cap)
          has_cap = wms_cap_parse_cap(doc, cur_node, &wms->cap);
      } else
	printf("found unhandled WMT_MS_Capabilities/%s\n", cur_node->name);
    }
  }

  return has_service && has_cap;
}

/* parse root element */
static bool wms_cap_parse_root(wms_t *wms, xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = O2G_NULLPTR;
  bool ret = FALSE;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      if(strcasecmp((char*)cur_node->name, "WMT_MS_Capabilities") == 0) {
        ret = wms_cap_parse(wms, doc, cur_node);
      } else
	printf("found unhandled %s\n", cur_node->name);
    }
  }

  return ret;
}

static bool wms_cap_parse_doc(wms_t *wms, xmlDocPtr doc) {
  /* Get the root element node */
  xmlNode *root_element = xmlDocGetRootElement(doc);

  bool ret = wms_cap_parse_root(wms, doc, root_element);

  /*free the document */
  xmlFreeDoc(doc);

  return ret;
}

/* get pixel extent of image display */
void wms_setup_extent(project_t *project, wms_t *wms) {
  pos_t center;
  lpos_t lcenter, lmin, lmax;
  float scale;

  center.lat = (project->min.lat + project->max.lat)/2;
  center.lon = (project->min.lon + project->max.lon)/2;

  pos2lpos_center(&center, &lcenter);

  /* the scale is needed to accomodate for "streching" */
  /* by the mercartor projection */
  scale = cos(DEG2RAD(center.lat));

  pos2lpos_center(&project->min, &lmin);
  lmin.x -= lcenter.x;
  lmin.y -= lcenter.y;
  lmin.x *= scale;
  lmin.y *= scale;

  pos2lpos_center(&project->max, &lmax);
  lmax.x -= lcenter.x;
  lmax.y -= lcenter.y;
  lmax.x *= scale;
  lmax.y *= scale;

  wms->width = std::min(lmax.x - lmin.x, 2048);
  wms->height = std::min(lmax.y - lmin.y, 2048);

  printf("WMS: required image size = %dx%d\n",
	 wms->width, wms->height);
}

/* --------------- freeing stuff ------------------- */

static void wms_layer_free(wms_layer_t *layer) {
  delete layer;
}

static void wms_layers_free(wms_layer_t::list &layers) {
  std::for_each(layers.begin(), layers.end(), wms_layer_free);
}

wms_layer_t::~wms_layer_t() {
  wms_layers_free(children);
}

wms_t::~wms_t() {
  wms_layers_free(cap.layers);
}

/* ---------------------- use ------------------- */

static gboolean wms_llbbox_fits(const project_t *project, const wms_llbbox_t *llbbox) {
  if((project->min.lat < llbbox->min.lat) ||
     (project->min.lon < llbbox->min.lon) ||
     (project->max.lat > llbbox->max.lat) ||
     (project->max.lon > llbbox->max.lon))
    return FALSE;

  return TRUE;
}

struct child_layer_functor {
  gint depth;
  bool epsg4326;
  const wms_llbbox_t * const llbbox;
  const std::string &srs;
  wms_layer_t::list &clayers;
  child_layer_functor(gint d, bool e, const wms_llbbox_t *x, const std::string &s,
                      wms_layer_t::list &c)
    : depth(d), epsg4326(e), llbbox(x), srs(s), clayers(c) {}
  void operator()(const wms_layer_t *layer);
};

void child_layer_functor::operator()(const wms_layer_t *layer)
{
  /* get a copy of the parents values for the current one ... */
  const wms_llbbox_t *local_llbbox = llbbox;
  bool local_epsg4326 = epsg4326;

  /* ... and overwrite the inherited stuff with better local stuff */
  if(layer->llbbox.valid)                    local_llbbox = &layer->llbbox;
  local_epsg4326 |= layer->epsg4326;

  /* only named layers with useful bounding box are added to the list */
  if(local_llbbox && !layer->name.empty()) {
    wms_layer_t *c_layer = new wms_layer_t(layer->title, layer->name,
                                           local_epsg4326 ? std::string() : srs,
                                           local_epsg4326, *local_llbbox);
    clayers.push_back(c_layer);
  }

  std::for_each(layer->children.begin(), layer->children.end(),
                child_layer_functor(depth + 1, local_epsg4326,
                                    local_llbbox, srs, clayers));
}

struct requestable_layers_functor {
  wms_layer_t::list &c_layer;
  requestable_layers_functor(wms_layer_t::list &c) : c_layer(c) {}
  void operator()(const wms_layer_t *layer);
};

void requestable_layers_functor::operator()(const wms_layer_t* layer)
{
  const wms_llbbox_t *llbbox = &layer->llbbox;
  if(!llbbox->valid)
    llbbox = O2G_NULLPTR;

  std::for_each(layer->children.begin(), layer->children.end(),
                child_layer_functor(1, layer->epsg4326, llbbox, layer->srs,
                                    c_layer));
}

enum {
  WMS_SERVER_COL_NAME = 0,
  WMS_SERVER_COL_DATA,
  WMS_SERVER_NUM_COLS
};

struct wms_server_context_t {
  wms_server_context_t(appdata_t *a, wms_t *w, GtkWidget *d)
    : appdata(a), wms(w), dialog(d), list(O2G_NULLPTR), store(O2G_NULLPTR)
    , server_label(O2G_NULLPTR), path_label(O2G_NULLPTR) {}
  appdata_t * const appdata;
  wms_t * const wms;
  GtkWidget * const dialog, *list;
  GtkListStore *store;
  GtkWidget *server_label, *path_label;
};

static wms_server_t *get_selection(GtkWidget *list) {
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  selection = list_get_selection(list);
  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    wms_server_t *wms_server;
    gtk_tree_model_get(model, &iter, WMS_SERVER_COL_DATA, &wms_server, -1);
    return(wms_server);
  }

  return O2G_NULLPTR;
}

static void wms_server_selected(wms_server_context_t *context,
				wms_server_t *selected) {

  if(!selected && !context->wms->server.empty() && !context->wms->path.empty()) {
    /* if the projects settings match a list entry, then select this */

    GtkTreeSelection *selection = list_get_selection(context->list);

    /* walk the entire store to get all values */
    wms_server_t *server = O2G_NULLPTR;
    GtkTreeIter iter;

    bool valid =
      (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(context->store), &iter) != TRUE);

    while(valid && !selected) {
      gtk_tree_model_get(GTK_TREE_MODEL(context->store), &iter,
			 WMS_SERVER_COL_DATA, &server, -1);
      g_assert_nonnull(server);

      if(context->wms->server == server->server &&
         context->wms->path == server->path) {
	gtk_tree_selection_select_iter(selection, &iter);
	selected = server;
      }

      valid = (gtk_tree_model_iter_next(GTK_TREE_MODEL(context->store), &iter) != TRUE);
    }
  }

  list_button_enable(context->list, LIST_BUTTON_REMOVE, selected != O2G_NULLPTR);
  list_button_enable(context->list, LIST_BUTTON_EDIT, selected != O2G_NULLPTR);

  /* user can click ok if a entry is selected or if both fields are */
  /* otherwise valid */
  if(selected) {
    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
		      GTK_RESPONSE_ACCEPT, TRUE);

    gtk_label_set_text(GTK_LABEL(context->server_label), selected->server);
    gtk_label_set_text(GTK_LABEL(context->path_label), selected->path);
  } else {
    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
	      GTK_RESPONSE_ACCEPT, !context->wms->server.empty() && !context->wms->path.empty());

    if(!context->wms->server.empty())
      gtk_label_set_text(GTK_LABEL(context->server_label),
			 context->wms->server.c_str());
    else
      gtk_label_set_text(GTK_LABEL(context->server_label),
			 "");
    if(!context->wms->path.empty())
      gtk_label_set_text(GTK_LABEL(context->path_label),
			 context->wms->path.c_str());
    else
      gtk_label_set_text(GTK_LABEL(context->path_label),
			 "");
  }
}

static void
wms_server_changed(GtkTreeSelection *selection, gpointer userdata) {
  wms_server_context_t *context = static_cast<wms_server_context_t *>(userdata);

  GtkTreeModel *model = O2G_NULLPTR;
  GtkTreeIter iter;

  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    wms_server_t *wms_server = O2G_NULLPTR;

    gtk_tree_model_get(model, &iter, WMS_SERVER_COL_DATA, &wms_server, -1);
    wms_server_selected(context, wms_server);
  }
}

static void on_server_remove(G_GNUC_UNUSED GtkWidget *but, wms_server_context_t *context) {
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  selection = list_get_selection(context->list);
  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    wms_server_t *server = O2G_NULLPTR;
    gtk_tree_model_get(model, &iter, WMS_SERVER_COL_DATA, &server, -1);

    g_assert_nonnull(server);

    /* de-chain */
    printf("de-chaining server %s\n", server->name);
    wms_server_t **prev = &context->appdata->settings->wms_server;
    while(*prev != server) prev = &((*prev)->next);
    *prev = server->next;

    /* free tag itself */
    wms_server_free(server);

    /* and remove from store */
    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
  }

  wms_server_selected(context, O2G_NULLPTR);
}

static void callback_modified_name(GtkWidget *widget, gpointer data) {
  settings_t *settings = (settings_t*)data;

  const gchar *name = gtk_entry_get_text(GTK_ENTRY(widget));

  /* name must not contain some special chars */
  gboolean ok = TRUE;

  /* search all entries except the last (which is the one we are editing) */
  wms_server_t *server = settings->wms_server;
  while(server && server->next) {
    if(strcasecmp(server->name, (char*)name) == 0) {
      ok = FALSE;
      break;
    }
    server = server->next;
  }

  GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
  /* toplevel is a dialog only of dialog has been realized */
  if(GTK_IS_DIALOG(toplevel))
    gtk_dialog_set_response_sensitive(
		      GTK_DIALOG(gtk_widget_get_toplevel(widget)),
		      GTK_RESPONSE_ACCEPT, ok);
}

/* edit url and path of a given wms server entry */
gboolean wms_server_edit(wms_server_context_t *context, gboolean edit_name,
			 wms_server_t *wms_server) {
  GtkWidget *dialog =
    misc_dialog_new(MISC_DIALOG_WIDE, _("Edit WMS Server"),
		    GTK_WINDOW(context->dialog),
		    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		    O2G_NULLPTR);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog),
				  GTK_RESPONSE_ACCEPT);

  GtkWidget *label, *name, *server, *path;
  GtkWidget *table = gtk_table_new(2, 3, FALSE);

  gtk_table_attach(GTK_TABLE(table),
		   label = gtk_label_new(_("Name:")), 0, 1, 0, 1,
		   GTK_FILL, static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),
		    name = entry_new(), 1, 2, 0, 1);
  gtk_entry_set_activates_default(GTK_ENTRY(name), TRUE);
  HILDON_ENTRY_NO_AUTOCAP(name);
  gtk_widget_set_sensitive(GTK_WIDGET(name), edit_name);
  g_signal_connect(G_OBJECT(name), "changed",
		   G_CALLBACK(callback_modified_name), context->appdata->settings);

  gtk_table_attach(GTK_TABLE(table),
		   label = gtk_label_new(_("Server:")), 0, 1, 1, 2,
		   GTK_FILL, static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),
		    server = entry_new(), 1, 2, 1, 2);
  gtk_entry_set_activates_default(GTK_ENTRY(server), TRUE);
  HILDON_ENTRY_NO_AUTOCAP(server);

  gtk_table_attach(GTK_TABLE(table),
		   label = gtk_label_new(_("Path:")), 0, 1, 2, 3,
		   GTK_FILL, static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),
		    path = entry_new(), 1, 2, 2, 3);
  gtk_entry_set_activates_default(GTK_ENTRY(path), TRUE);
  HILDON_ENTRY_NO_AUTOCAP(path);

  gtk_entry_set_text(GTK_ENTRY(name), wms_server->name);
  gtk_entry_set_text(GTK_ENTRY(server), wms_server->server);
  gtk_entry_set_text(GTK_ENTRY(path), wms_server->path);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), table);

  gtk_widget_show_all(dialog);

  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog))) {
    if(edit_name) {
      GtkTreeSelection *selection;
      GtkTreeModel     *model;
      GtkTreeIter       iter;
      selection = list_get_selection(context->list);
      gtk_tree_selection_get_selected(selection, &model, &iter);
      gtk_list_store_set(context->store, &iter,
			 WMS_SERVER_COL_NAME, wms_server->name,
			 -1);

      g_free(wms_server->name);
      wms_server->name = g_strdup(gtk_entry_get_text(GTK_ENTRY(name)));
    }

    g_free(wms_server->server);
    wms_server->server = g_strdup(gtk_entry_get_text(GTK_ENTRY(server)));
    g_free(wms_server->path);
    wms_server->path = g_strdup(gtk_entry_get_text(GTK_ENTRY(path)));
    printf("setting %s/%s\n", wms_server->server, wms_server->path);

    /* set texts below */
    gtk_label_set_text(GTK_LABEL(context->server_label), wms_server->server);
    gtk_label_set_text(GTK_LABEL(context->path_label), wms_server->path);

    gtk_widget_destroy(dialog);

    return TRUE;
  }

  gtk_widget_destroy(dialog);
  return FALSE;
}

/* user clicked "edit..." button in the wms server list */
static void on_server_edit(G_GNUC_UNUSED GtkWidget *but, wms_server_context_t *context) {
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  selection = list_get_selection(context->list);
  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    wms_server_t *server = O2G_NULLPTR;
    gtk_tree_model_get(model, &iter, WMS_SERVER_COL_DATA, &server, -1);
    g_assert_nonnull(server);

    wms_server_edit(context, FALSE, server);
  }
}

/* user clicked "add..." button in the wms server list */
static void on_server_add(G_GNUC_UNUSED GtkWidget *but, wms_server_context_t *context) {

  /* attach a new server item to the chain */
  wms_server_t **prev = &context->appdata->settings->wms_server;
  while(*prev) prev = &(*prev)->next;

  *prev = g_new0(wms_server_t, 1);
  (*prev)->name   = g_strdup("<service name>");
  (*prev)->server = g_strdup("<server url>");
  (*prev)->path   = g_strdup("<path in server>");

  GtkTreeModel *model = list_get_model(context->list);

  GtkTreeIter iter;
  gtk_list_store_append(GTK_LIST_STORE(model), &iter);
  gtk_list_store_set(GTK_LIST_STORE(model), &iter,
		     WMS_SERVER_COL_NAME, (*prev)->name,
		     WMS_SERVER_COL_DATA, *prev,
		     -1);

  GtkTreeSelection *selection = list_get_selection(context->list);
  gtk_tree_selection_select_iter(selection, &iter);

  if(!wms_server_edit(context, TRUE, *prev)) {
    /* user has cancelled request. remove newly added item */
    printf("user clicked cancel\n");

    wms_server_free(*prev);
    *prev = O2G_NULLPTR;

    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
  } else
    /* update name from edit result */
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
		       WMS_SERVER_COL_NAME, (*prev)->name,
		       -1);

  wms_server_selected(context, *prev);
}

/* widget to select a wms server from a list */
static GtkWidget *wms_server_widget(wms_server_context_t *context) {

  context->list = list_new(LIST_HILDON_WITHOUT_HEADERS);

  list_override_changed_event(context->list, wms_server_changed, context);

  list_set_columns(context->list,
		   _("Name"), WMS_SERVER_COL_NAME, LIST_FLAG_ELLIPSIZE,
		   O2G_NULLPTR);

  /* build and fill the store */
  context->store = gtk_list_store_new(WMS_SERVER_NUM_COLS,
	        G_TYPE_STRING, G_TYPE_POINTER);

  list_set_store(context->list, context->store);

  GtkTreeIter iter;
  wms_server_t *wms_server = context->appdata->settings->wms_server;
  while(wms_server) {
    /* Append a row and fill in some data */
    gtk_list_store_append(context->store, &iter);
    gtk_list_store_set(context->store, &iter,
		       WMS_SERVER_COL_NAME, wms_server->name,
		       WMS_SERVER_COL_DATA, wms_server,
		       -1);

    wms_server = wms_server->next;
  }

  g_object_unref(context->store);

  list_set_static_buttons(context->list, 0,
	  G_CALLBACK(on_server_add), G_CALLBACK(on_server_edit),
	  G_CALLBACK(on_server_remove), context);

  return context->list;
}

static gboolean wms_server_dialog(appdata_t *appdata, wms_t *wms) {
  gboolean ok = FALSE;

  wms_server_context_t context(appdata, wms,
    misc_dialog_new(MISC_DIALOG_MEDIUM, _("WMS Server Selection"),
		    GTK_WINDOW(appdata->window),
		    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		    O2G_NULLPTR));

  /* server selection box */
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
			      wms_server_widget(&context));

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
		     gtk_hseparator_new(), FALSE, FALSE, 0);

  GtkWidget *label;
  GtkWidget *table = gtk_table_new(2, 2, FALSE);  // x, y
  gtk_table_set_col_spacing(GTK_TABLE(table), 0, 10);

  label = gtk_label_new(_("Server:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL,
                   static_cast<GtkAttachOptions>(0), 0, 0);
  context.server_label = gtk_label_new(O2G_NULLPTR);
  gtk_label_set_ellipsize(GTK_LABEL(context.server_label),
			  PANGO_ELLIPSIZE_MIDDLE);
  gtk_misc_set_alignment(GTK_MISC(context.server_label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), context.server_label,
			    1, 2, 0, 1);

  label = gtk_label_new(_("Path:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL,
                   static_cast<GtkAttachOptions>(0), 0, 0);
  context.path_label = gtk_label_new(O2G_NULLPTR);
  gtk_label_set_ellipsize(GTK_LABEL(context.path_label),
			  PANGO_ELLIPSIZE_MIDDLE);
  gtk_misc_set_alignment(GTK_MISC(context.path_label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), context.path_label,
			    1, 2, 1, 2);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
		     table, FALSE, FALSE, 0);

  wms_server_selected(&context, O2G_NULLPTR);

  gtk_widget_show_all(context.dialog);

  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context.dialog))) {
    wms_server_t *server = get_selection(context.list);
    if(server) {
      /* fetch parameters from selected entry */
      printf("WMS: using %s\n", server->name);
      wms->server = server->server;
      wms->path = server->path;
      ok = TRUE;
    } else {
      if(!wms->server.empty() && !wms->path.empty())
	ok = TRUE;
    }
  }

  gtk_widget_destroy(context.dialog);

  return ok;
}

enum {
  LAYER_COL_TITLE = 0,
  LAYER_COL_FITS,
  LAYER_COL_DATA,
  LAYER_NUM_COLS
};

#ifndef FREMANTLE
/* we handle these events on our own in order to implement */
/* a very direct selection mechanism (multiple selections usually */
/* require the control key to be pressed). This interferes with */
/* fremantle finger scrolling, but fortunately the fremantle */
/* default behaviour already is what we want. */
static gboolean on_view_clicked(GtkWidget *widget, GdkEventButton *event,
				G_GNUC_UNUSED gpointer user_data) {
  if(event->window == gtk_tree_view_get_bin_window(GTK_TREE_VIEW(widget))) {
    GtkTreePath *path;

    if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
		     event->x, event->y, &path, O2G_NULLPTR, O2G_NULLPTR, O2G_NULLPTR)) {
      GtkTreeSelection *sel =
	gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

      if(!gtk_tree_selection_path_is_selected(sel, path))
	gtk_tree_selection_select_path(sel, path);
      else
	gtk_tree_selection_unselect_path(sel, path);
    }
    return TRUE;
  }
  return FALSE;
}
#endif

struct selected_context {
  appdata_t * const appdata;
  wms_layer_t::list selected;
  selected_context(appdata_t *a) : appdata(a) {}
  void operator()(const wms_layer_t *layer);
};

static void changed(GtkTreeSelection *sel, gpointer user_data) {
  /* we need to know what changed in order to let the user acknowlege it! */
  wms_layer_t::list * const selected = static_cast<wms_layer_t::list *>(user_data);

  /* get view from selection ... */
  GtkTreeView *view = gtk_tree_selection_get_tree_view(sel);
  g_assert_nonnull(view);

  /* ... and get model from view */
  GtkTreeModel *model = gtk_tree_view_get_model(view);
  g_assert_nonnull(model);

  /* walk the entire store */
  GtkTreeIter iter;
  gboolean ok = gtk_tree_model_get_iter_first(model, &iter);
  selected->clear();
  while(ok) {
    wms_layer_t *layer = O2G_NULLPTR;

    gtk_tree_model_get(model, &iter, LAYER_COL_DATA, &layer, -1);
    g_assert_nonnull(layer);

    if(gtk_tree_selection_iter_is_selected(sel, &iter) == TRUE)
      selected->push_back(layer);

    ok = gtk_tree_model_iter_next(model, &iter);
  }

  GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(view));
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
                                    GTK_RESPONSE_ACCEPT,
                                    selected->empty() ? FALSE : TRUE);
}

struct fitting_layers_functor {
  GtkListStore * const store;
  const project_t * const project;
  fitting_layers_functor(GtkListStore *s, const project_t *p) : store(s), project(p) {}
  void operator()(const wms_layer_t *layer);
};

void fitting_layers_functor::operator()(const wms_layer_t *layer)
{
  GtkTreeIter iter;
  gboolean fits = layer->llbbox.valid && wms_llbbox_fits(project, &layer->llbbox);

  /* Append a row and fill in some data */
  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter,
                     LAYER_COL_TITLE, layer->title.c_str(),
                     LAYER_COL_FITS, fits,
                     LAYER_COL_DATA, layer,
                     -1);
}

static GtkWidget *wms_layer_widget(selected_context *context, const wms_layer_t::list &layers,
				   G_GNUC_UNUSED GtkWidget *dialog) {

#ifndef FREMANTLE_PANNABLE_AREA
  GtkWidget *view = gtk_tree_view_new();
#else
  GtkWidget *view = hildon_gtk_tree_view_new(HILDON_UI_MODE_EDIT);
#endif

  /* change list mode to "multiple" */
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

#ifndef FREMANTLE
  /* catch views button-press event for our custom handling */
  g_signal_connect(view, "button-press-event",
		   G_CALLBACK(on_view_clicked), O2G_NULLPTR);
#endif

  /* build the store */
  GtkListStore *store = gtk_list_store_new(LAYER_NUM_COLS,
      G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER);

  /* --- "Title" column --- */
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, O2G_NULLPTR );
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
		 _("Title"), renderer,
		 "text", LAYER_COL_TITLE,
		 "sensitive", LAYER_COL_FITS,
		 O2G_NULLPTR);

  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(view), column, -1);

  gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));

  std::for_each(layers.begin(), layers.end(),
                fitting_layers_functor(store, context->appdata->project));

  g_object_unref(store);

  g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(changed), &context->selected);

#ifndef FREMANTLE_PANNABLE_AREA
  /* put it into a scrolled window */
  GtkWidget *scrolled_window = gtk_scrolled_window_new(O2G_NULLPTR, O2G_NULLPTR);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window),
				      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scrolled_window), view);
  return scrolled_window;
#else
  /* put view into a pannable area */
  GtkWidget *pannable_area = hildon_pannable_area_new();
  gtk_container_add(GTK_CONTAINER(pannable_area), view);
  return pannable_area;
#endif
}


static gboolean wms_layer_dialog(selected_context *ctx, const wms_layer_t::list &layer) {
  gboolean ok = FALSE;

  GtkWidget *dialog =
    misc_dialog_new(MISC_DIALOG_LARGE, _("WMS layer selection"),
		    GTK_WINDOW(ctx->appdata->window),
		    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		    O2G_NULLPTR);

  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
				    GTK_RESPONSE_ACCEPT, FALSE);

  /* layer list */
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox),
		      wms_layer_widget(ctx, layer, dialog));


  gtk_widget_show_all(dialog);

  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    ok = TRUE;

  gtk_widget_destroy(dialog);

  return ok;
}

static bool layer_is_usable(const wms_layer_t *layer) {
  return layer->is_usable();
}

void wms_import(appdata_t *appdata) {
  if(!appdata->project) {
    errorf(GTK_WIDGET(appdata->window),
	   _("Need an open project to derive WMS coordinates"));
    return;
  }

  /* this cancels any wms adjustment in progress */
  if(appdata->map->action.type == MAP_ACTION_BG_ADJUST)
    map_action_cancel(appdata->map);

  wms_t wms(appdata->project->wms_server, appdata->project->wms_path);

  /* reset any background adjustments in the project ... */
  appdata->project->wms_offset.x = 0;
  appdata->project->wms_offset.y = 0;

  /* ... as well as in the map */
  appdata->map->bg.offset.x = 0;
  appdata->map->bg.offset.y = 0;

  /* get server from dialog */
  if(!wms_server_dialog(appdata, &wms))
    return;

  /* ------------- copy values back into project ---------------- */
  if(wms.server != appdata->project->wms_server)
    appdata->project->wms_server = wms.server;

  if(wms.path != appdata->project->wms_path)
    appdata->project->wms_path = wms.path;

  /* ----------- request capabilities -------------- */
  bool path_contains_qm = wms.path.find('?') != wms.path.npos;
  bool path_ends_with_special =
    (wms.path[wms.path.size()- 1] == '?') ||
    (wms.path[wms.path.size() - 1] == '&');

  /* if there's already a question mark, then add further */
  /* parameters using the &, else use the ? */
  const char *append_char = path_ends_with_special?"":(path_contains_qm?"&":"?");

  std::string url;
  url.resize(256); // make enough room that most URLs will need no reallocation
  url = wms.server + wms.path + append_char +
                                 "SERVICE=wms"
                                 "&VERSION=1.1.1"
                                 "&REQUEST=GetCapabilities";

  char *cap = O2G_NULLPTR;
  net_io_download_mem(GTK_WIDGET(appdata->window), appdata->settings,
		      url.c_str(), &cap);

  /* ----------- parse capabilities -------------- */

  bool parse_success = false;
  if(!cap) {
    errorf(GTK_WIDGET(appdata->window),
	   _("WMS download failed:\n\n"
	     "GetCapabilities failed"));
  } else {
    xmlDoc *doc = O2G_NULLPTR;

    /* parse the file and get the DOM */
    if((doc = xmlReadMemory(cap, strlen(cap), O2G_NULLPTR, O2G_NULLPTR, 0)) == O2G_NULLPTR) {
      xmlErrorPtr errP = xmlGetLastError();
      errorf(GTK_WIDGET(appdata->window),
	     _("WMS download failed:\n\n"
	       "XML error while parsing capabilities:\n"
	       "%s"), errP->message);
    } else {
      printf("ok, parse doc tree\n");

      parse_success = wms_cap_parse_doc(&wms, doc);
    }

    g_free(cap);
  }

  /* ------------ basic checks ------------- */

  if(!parse_success) {
    errorf(GTK_WIDGET(appdata->window), _("Incomplete/unexpected reply!"));
    return;
  }

  if(!wms.cap.request.getmap.format) {
    errorf(GTK_WIDGET(appdata->window), _("No supported image format found."));
    return;
  }

  /* ---------- evaluate layers ----------- */
  printf("\nSearching for usable layers\n");

  wms_layer_t::list layers;
  requestable_layers_functor fc(layers);

  std::for_each(wms.cap.layers.begin(), wms.cap.layers.end(), fc);
  bool at_least_one_ok = std::find_if(layers.begin(), layers.end(), layer_is_usable) !=
                         layers.end();

  if(!at_least_one_ok) {
    errorf(GTK_WIDGET(appdata->window),
	   _("Server provides no data in the required format!\n\n"
	     "(epsg4326 and LatLonBoundingBox are mandatory for osm2go)"));
#if 0
    wms_layers_free(layers);
    return;
#endif
  }

  selected_context ctx(appdata);

  if(!wms_layer_dialog(&ctx, layers)) {
    wms_layers_free(layers);
    return;
  }

  /* --------- build getmap request ----------- */

  /* get required image size */
  wms_setup_extent(appdata->project, &wms);

  /* start building url */
  url.erase(url.size() - strlen("GetCapabilities"));
  url += "GetMap&LAYERS=";

  /* append layers */
  const wms_layer_t::list::const_iterator selEnd = ctx.selected.end();
  wms_layer_t::list::const_iterator selIt = ctx.selected.begin();
  if(selIt != selEnd) {
    url += (*selIt)->name;
    for(++selIt; selIt != selEnd; selIt++) {
      url += ',';
      url += (*selIt)->name;
    }
  }

  /* uses epsg4326 if possible */
#if __cplusplus >= 201103L
  const std::string srss = std::move(layers.front()->srs);
#else
  std::string srss;
  srss.swap(layers.front()->srs);
#endif
  const char *srs = srss.empty() ? wms_layer_t::EPSG4326() : srss.c_str();

  wms_layers_free(layers);

  /* append styles entry */
  url += "&STYLES=";

  if(ctx.selected.size() > 1)
    url += std::string(ctx.selected.size() - 1, ',');

  /* and append rest */
  gchar minlon[G_ASCII_DTOSTR_BUF_SIZE], minlat[G_ASCII_DTOSTR_BUF_SIZE];
  gchar maxlon[G_ASCII_DTOSTR_BUF_SIZE], maxlat[G_ASCII_DTOSTR_BUF_SIZE];

  /* build strings of min and max lat and lon to be used in url */
  g_ascii_formatd(minlon, sizeof(minlon), LL_FORMAT,
		  appdata->project->min.lon);
  g_ascii_formatd(minlat, sizeof(minlat), LL_FORMAT,
		  appdata->project->min.lat);
  g_ascii_formatd(maxlon, sizeof(maxlon), LL_FORMAT,
		  appdata->project->max.lon);
  g_ascii_formatd(maxlat, sizeof(maxlat), LL_FORMAT,
		  appdata->project->max.lat);

  /* find preferred supported video format */
  gint format = 0;
  while(!(wms.cap.request.getmap.format & (1<<format)))
    format++;

  const char *formats[] = { "image/jpg", "image/jpeg",
			    "image/png", "image/gif" };
  char buf[64];
  sprintf(buf, "&WIDTH=%d&HEIGHT=%d&FORMAT=", wms.width, wms.height);

  /* build complete url */
  const char *parts[] = { "&SRS=", srs, "&BBOX=", minlon, ",", minlat, ",",
                          maxlon, ",", maxlat, buf, formats[format], O2G_NULLPTR };
  for(int i = 0; parts[i]; i++)
    url += parts[i];

  const char *exts[] = { "jpg", "jpg", "png", "gif" };
  const std::string filename = std::string(appdata->project->path) + "wms." +
                               exts[format] + "&reaspect=false";

  /* remove any existing image before */
  wms_remove(appdata);

  if(!net_io_download_file(GTK_WIDGET(appdata->window), appdata->settings,
                           url.c_str(), filename.c_str(), O2G_NULLPTR))
    return;

  /* there should be a matching file on disk now */
  map_set_bg_image(appdata->map, filename.c_str());

  gint x = appdata->osm->bounds->min.x + appdata->map->bg.offset.x;
  gint y = appdata->osm->bounds->min.y + appdata->map->bg.offset.y;
  canvas_image_move(appdata->map->bg.item, x, y,
		    appdata->map->bg.scale.x, appdata->map->bg.scale.y);

  gtk_widget_set_sensitive(appdata->menuitems[MENU_ITEM_WMS_CLEAR], TRUE);
  gtk_widget_set_sensitive(appdata->menuitems[MENU_ITEM_WMS_ADJUST], TRUE);
}

static const char *wms_exts[] = { "png", "gif", "jpg", O2G_NULLPTR };
/* this must be the longest one */
#define DUMMYEXT wms_exts[0]

/* try to load an existing image into map */
void wms_load(appdata_t *appdata) {
  int i;
  std::string filename = appdata->project->path + "/wms.";
  filename.resize(filename.size() + strlen(DUMMYEXT));
  const std::string::size_type extpos = filename.size();

  for(i = 0; wms_exts[i]; i++) {
    filename.erase(extpos);
    filename += wms_exts[i];

    if(g_file_test(filename.c_str(), G_FILE_TEST_EXISTS)) {
      appdata->map->bg.offset.x = appdata->project->wms_offset.x;
      appdata->map->bg.offset.y = appdata->project->wms_offset.y;

      map_set_bg_image(appdata->map, filename.c_str());

      /* restore image to saved position */
      gint x = appdata->osm->bounds->min.x + appdata->map->bg.offset.x;
      gint y = appdata->osm->bounds->min.y + appdata->map->bg.offset.y;
      canvas_image_move(appdata->map->bg.item, x, y,
			appdata->map->bg.scale.x, appdata->map->bg.scale.y);

      gtk_widget_set_sensitive(appdata->menuitems[MENU_ITEM_WMS_CLEAR], TRUE);
      gtk_widget_set_sensitive(appdata->menuitems[MENU_ITEM_WMS_ADJUST], TRUE);

      break;
    }
  }
}

void wms_remove_file(project_t *project) {
  int i;
  std::string filename = project->path + "/wms.";
  filename.resize(filename.size() + strlen(DUMMYEXT));
  const std::string::size_type extpos = filename.size();

  for(i = 0; wms_exts[i]; i++) {
    filename.erase(extpos);
    filename += wms_exts[i];

    if(g_file_test(filename.c_str(), G_FILE_TEST_EXISTS))
      g_remove(filename.c_str());
  }
}

void wms_remove(appdata_t *appdata) {

  /* this cancels any wms adjustment in progress */
  if(appdata->map->action.type == MAP_ACTION_BG_ADJUST)
    map_action_cancel(appdata->map);

  gtk_widget_set_sensitive(appdata->menuitems[MENU_ITEM_WMS_CLEAR], FALSE);
  gtk_widget_set_sensitive(appdata->menuitems[MENU_ITEM_WMS_ADJUST], FALSE);

  map_remove_bg_image(appdata->map);

  wms_remove_file(appdata->project);
}

struct server_preset_s {
  const gchar *name, *server, *path;
} default_servers[] = {
  { "Open Geospatial Consortium Web Services", "http://ows.terrestris.de", "/osm/service?" },
  /* add more servers here ... */
  { O2G_NULLPTR, O2G_NULLPTR, O2G_NULLPTR }
};

wms_server_t *wms_server_get_default(void) {
  wms_server_t *server = O2G_NULLPTR, **cur = &server;
  struct server_preset_s *preset = default_servers;

  while(preset->name) {
    *cur = g_new0(wms_server_t, 1);
    (*cur)->name = g_strdup(preset->name);
    (*cur)->server = g_strdup(preset->server);
    (*cur)->path = g_strdup(preset->path);
    cur = &(*cur)->next;
    preset++;
  }

  return server;
}

void wms_servers_free(wms_server_t *wms_server) {
  while(wms_server) {
    wms_server_t *next = wms_server->next;
    wms_server_free(wms_server);
    wms_server = next;
  }
}
