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
#include "fdguard.h"
#include "list.h"
#include "map.h"
#include "misc.h"
#include "net_io.h"
#include "project.h"
#include "settings.h"
#include "xml_helpers.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#ifdef FREMANTLE
#include <hildon/hildon-gtk.h>
#include <hildon/hildon-pannable-area.h>
#endif
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <map>
#include <strings.h>
#include <vector>

#include "osm2go_annotations.h"
#include "osm2go_stl.h"
#include <osm2go_i18n.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

enum WmsImageFormat {
  WMS_FORMAT_JPG = (1<<0),
  WMS_FORMAT_JPEG = (1<<1),
  WMS_FORMAT_PNG = (1<<2),
  WMS_FORMAT_GIF = (1<<3)
};

struct charcmp {
  bool operator()(const char *a, const char *b) const {
    return g_strcmp0(a, b) < 0;
  }
};

typedef std::map<const char *, WmsImageFormat, charcmp> FormatMap;
static FormatMap ImageFormats;
static std::map<WmsImageFormat, const char *> ImageFormatExtensions;

static void initImageFormats()
{
  ImageFormats["image/png"] = WMS_FORMAT_PNG;
  ImageFormats["image/gif"] = WMS_FORMAT_GIF;
  ImageFormats["image/jpg"] = WMS_FORMAT_JPG;
  ImageFormats["image/jpeg"] = WMS_FORMAT_JPEG;

  ImageFormatExtensions[WMS_FORMAT_PNG] = "png";
  ImageFormatExtensions[WMS_FORMAT_GIF] = "gif";
  ImageFormatExtensions[WMS_FORMAT_JPG] = "jpg";
  ImageFormatExtensions[WMS_FORMAT_JPEG] = "jpg";
}

struct wms_llbbox_t {
  wms_llbbox_t() : valid(false) {}
  pos_t min, max;
  bool valid;
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

static bool wms_bbox_is_valid(const pos_t &min, const pos_t &max) {
  /* all four coordinates are valid? */
  if(unlikely(!min.valid() || !max.valid()))
    return false;

  /* min/max span a useful range? */
  if(unlikely(max.lat - min.lat < 0.1))
    return false;
  if(unlikely(max.lon - min.lon < 0.1))
    return false;

  return true;
}

static wms_layer_t *wms_cap_parse_layer(xmlDocPtr doc, xmlNode *a_node) {
  wms_layer_t *wms_layer = O2G_NULLPTR;
  xmlNode *cur_node = O2G_NULLPTR;

  wms_layer = new wms_layer_t();
  wms_layer->llbbox.min.lon = wms_layer->llbbox.min.lat = NAN;
  wms_layer->llbbox.max.lon = wms_layer->llbbox.max.lat = NAN;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Layer") == 0) {
        wms_layer_t *children = wms_cap_parse_layer(doc, cur_node);
        if(children)
          wms_layer->children.push_back(children);
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Name") == 0) {
	xmlChar *str = xmlNodeListGetString(doc, cur_node->children, 1);
	wms_layer->name = reinterpret_cast<char *>(str);
	xmlFree(str);
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Title") == 0) {
	xmlChar *str = xmlNodeListGetString(doc, cur_node->children, 1);
	wms_layer->title = reinterpret_cast<char *>(str);
	xmlFree(str);
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "SRS") == 0) {
	xmlChar *str = xmlNodeListGetString(doc, cur_node->children, 1);
        if(strcmp(reinterpret_cast<char *>(str), wms_layer_t::EPSG4326()) == 0)
          wms_layer->epsg4326 = true;
        else
          wms_layer->srs = reinterpret_cast<char *>(str);
        printf("SRS = %s\n", str);
	xmlFree(str);
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "LatLonBoundingBox") == 0) {
        wms_layer->llbbox.min = pos_t::fromXmlProperties(cur_node, "miny", "minx");
        wms_layer->llbbox.max = pos_t::fromXmlProperties(cur_node, "miny", "maxx");
      } else
	printf("found unhandled WMT_MS_Capabilities/Capability/Layer/%s\n",
	       cur_node->name);
    }
  }

  wms_layer->llbbox.valid = wms_bbox_is_valid(wms_layer->llbbox.min,
                                              wms_layer->llbbox.max);

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
      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Format") == 0) {
        xmlChar *nstr = xmlNodeListGetString(doc, cur_node->children, 1);

        const FormatMap::const_iterator it =
              ImageFormats.find(reinterpret_cast<char *>(nstr));
        if(it != ImageFormats.end())
          wms_getmap.format |= it->second;

        xmlFree(nstr);
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
      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "GetMap") == 0) {
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
      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Request") == 0) {
	wms_cap->request = wms_cap_parse_request(doc, cur_node);
        has_request = true;
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Layer") == 0) {
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

      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Service") == 0) {
        has_service = true;
      } else if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "Capability") == 0) {
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

      if(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "WMT_MS_Capabilities") == 0) {
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

  lcenter = center.toLpos();

  /* the scale is needed to accomodate for "streching" */
  /* by the mercartor projection */
  scale = cos(DEG2RAD(center.lat));

  lmin = project->min.toLpos();
  lmin.x -= lcenter.x;
  lmin.y -= lcenter.y;
  lmin.x *= scale;
  lmin.y *= scale;

  lmax = project->max.toLpos();
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

static void wms_layers_free(wms_layer_t::list &layers) {
  std::for_each(layers.begin(), layers.end(), std::default_delete<wms_layer_t>());
}

wms_layer_t::~wms_layer_t() {
  wms_layers_free(children);
}

wms_t::~wms_t() {
  wms_layers_free(cap.layers);
}

/* ---------------------- use ------------------- */

static bool wms_llbbox_fits(const project_t *project, const wms_llbbox_t &llbbox) {
  return ((project->min.lat >= llbbox.min.lat) &&
          (project->min.lon >= llbbox.min.lon) &&
          (project->max.lat <= llbbox.max.lat) &&
          (project->max.lon <= llbbox.max.lon));
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
  explicit requestable_layers_functor(wms_layer_t::list &c) : c_layer(c) {}
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

struct find_wms_functor {
  const char *name;
  explicit find_wms_functor(const char *n) : name(n) {}
  bool operator()(const wms_server_t *srv) {
    return srv->name == name;
  }
};

enum {
  WMS_SERVER_COL_NAME = 0,
  WMS_SERVER_COL_DATA,
  WMS_SERVER_NUM_COLS
};

struct wms_server_context_t {
  wms_server_context_t(appdata_t &a, wms_t *w, GtkWidget *d)
    : appdata(a), wms(w), dialog(d), list(O2G_NULLPTR), store(O2G_NULLPTR)
    , server_label(O2G_NULLPTR), path_label(O2G_NULLPTR) {}
  appdata_t &appdata;
  wms_t * const wms;
  GtkWidget * const dialog, *list;
  GtkListStore *store;
  GtkWidget *server_label, *path_label;

  /**
   * @brief select the server referenced in wms in the treeview
   * @returns the matching entry in the settings list
   */
  const wms_server_t *select_server() const;
};

static wms_server_t *get_selection(GtkTreeSelection *selection) {
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    wms_server_t *wms_server;
    gtk_tree_model_get(model, &iter, WMS_SERVER_COL_DATA, &wms_server, -1);
    assert(wms_server != O2G_NULLPTR);
    return wms_server;
  }

  return O2G_NULLPTR;
}

const wms_server_t *wms_server_context_t::select_server() const
{
  if(wms->server.empty() || wms->path.empty())
    return O2G_NULLPTR;

  /* if the projects settings match a list entry, then select this */

  GtkTreeSelection *selection = list_get_selection(list);

  /* walk the entire store to get all values */
  wms_server_t *server = O2G_NULLPTR;
  GtkTreeIter iter;

  bool valid = (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter) == TRUE);

  while(valid) {
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, WMS_SERVER_COL_DATA, &server, -1);
    assert(server != O2G_NULLPTR);

    if(wms->server == server->server &&
       wms->path == server->path) {
       gtk_tree_selection_select_iter(selection, &iter);
       return server;
    }

    valid = (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter) == TRUE);
  }

  return O2G_NULLPTR;
}

static void wms_server_selected(wms_server_context_t *context,
                                const wms_server_t *selected) {
  list_button_enable(context->list, LIST_BUTTON_REMOVE, selected != O2G_NULLPTR);
  list_button_enable(context->list, LIST_BUTTON_EDIT, selected != O2G_NULLPTR);

  /* user can click ok if a entry is selected or if both fields are */
  /* otherwise valid */
  if(selected) {
    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
		      GTK_RESPONSE_ACCEPT, TRUE);

    gtk_label_set_text(GTK_LABEL(context->server_label), selected->server.c_str());
    gtk_label_set_text(GTK_LABEL(context->path_label), selected->path.c_str());
  } else {
    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
	      GTK_RESPONSE_ACCEPT, !context->wms->server.empty() && !context->wms->path.empty());

    gtk_label_set_text(GTK_LABEL(context->server_label), context->wms->server.c_str());
    gtk_label_set_text(GTK_LABEL(context->path_label), context->wms->path.c_str());
  }
}

static void
wms_server_changed(GtkTreeSelection *selection, gpointer userdata) {
  wms_server_context_t *context = static_cast<wms_server_context_t *>(userdata);

  wms_server_t *wms_server = get_selection(selection);
  if(wms_server != O2G_NULLPTR)
    wms_server_selected(context, wms_server);
}

static void on_server_remove(wms_server_context_t *context) {
  GtkTreeSelection *selection = list_get_selection(context->list);
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    wms_server_t *server = O2G_NULLPTR;
    gtk_tree_model_get(model, &iter, WMS_SERVER_COL_DATA, &server, -1);

    assert(server != O2G_NULLPTR);

    /* de-chain */
    printf("de-chaining server %s\n", server->name.c_str());
    std::vector<wms_server_t *> &servers = context->appdata.settings->wms_server;
    const std::vector<wms_server_t *>::iterator itEnd = servers.end();
    std::vector<wms_server_t *>::iterator it = std::find(servers.begin(), itEnd, server);
    assert(it != itEnd);

    /* free tag itself */
    delete *it;
    servers.erase(it);

    /* and remove from store */
    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
  }

  wms_server_selected(context, context->select_server());
}

static void callback_modified_name(GtkWidget *widget, settings_t *settings) {
  const gchar *name = gtk_entry_get_text(GTK_ENTRY(widget));

  /* search all entries except the last (which is the one we are editing) */
  std::vector<wms_server_t *> &servers = settings->wms_server;
  const std::vector<wms_server_t *>::iterator itEnd = servers.end() - 1;
  std::vector<wms_server_t *>::iterator it = std::find_if(servers.begin(), itEnd,
                                                          find_wms_functor(name));

  gboolean ok = it == itEnd ? TRUE : FALSE;

  GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
  /* toplevel is a dialog only of dialog has been realized */
  if(GTK_IS_DIALOG(toplevel))
    gtk_dialog_set_response_sensitive(
		      GTK_DIALOG(gtk_widget_get_toplevel(widget)),
		      GTK_RESPONSE_ACCEPT, ok);
}

/* edit url and path of a given wms server entry */
bool wms_server_edit(wms_server_context_t *context, gboolean edit_name,
                     wms_server_t *wms_server) {
  GtkWidget *dialog =
    misc_dialog_new(MISC_DIALOG_WIDE, _("Edit WMS Server"),
		    GTK_WINDOW(context->dialog),
		    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		    O2G_NULLPTR);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog),
				  GTK_RESPONSE_ACCEPT);

  GtkWidget *label = gtk_label_new(_("Name:"));
  GtkWidget *name = entry_new(EntryFlagsNoAutoCap);
  GtkWidget *table = gtk_table_new(2, 3, FALSE);

  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
		   GTK_FILL, static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), name, 1, 2, 0, 1);
  gtk_entry_set_activates_default(GTK_ENTRY(name), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(name), edit_name);
  g_signal_connect(G_OBJECT(name), "changed",
		   G_CALLBACK(callback_modified_name), context->appdata.settings);

  label = gtk_label_new(_("Server:"));
  GtkWidget *server = entry_new(EntryFlagsNoAutoCap);
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
		   GTK_FILL, static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), server, 1, 2, 1, 2);
  gtk_entry_set_activates_default(GTK_ENTRY(server), TRUE);

  label = gtk_label_new(_("Path:"));
  GtkWidget *path = entry_new(EntryFlagsNoAutoCap);
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
		   GTK_FILL, static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), path, 1, 2, 2, 3);
  gtk_entry_set_activates_default(GTK_ENTRY(path), TRUE);

  gtk_entry_set_text(GTK_ENTRY(name), wms_server->name.c_str());
  gtk_entry_set_text(GTK_ENTRY(server), wms_server->server.c_str());
  gtk_entry_set_text(GTK_ENTRY(path), wms_server->path.c_str());

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), table);

  gtk_widget_show_all(dialog);

  const bool ret = (GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)));
  if(ret) {
    if(edit_name)
      wms_server->name = gtk_entry_get_text(GTK_ENTRY(name));

    wms_server->server = gtk_entry_get_text(GTK_ENTRY(server));
    wms_server->path = gtk_entry_get_text(GTK_ENTRY(path));
    printf("setting %s/%s\n", wms_server->server.c_str(), wms_server->path.c_str());

    /* set texts below */
    gtk_label_set_text(GTK_LABEL(context->server_label), wms_server->server.c_str());
    gtk_label_set_text(GTK_LABEL(context->path_label), wms_server->path.c_str());
  }

  gtk_widget_destroy(dialog);
  return ret;
}

/* user clicked "edit..." button in the wms server list */
static void on_server_edit(wms_server_context_t *context) {
  wms_server_t *server = get_selection(list_get_selection(context->list));
  assert(server != O2G_NULLPTR);

  wms_server_edit(context, FALSE, server);
}

struct store_fill_functor {
  GtkListStore *store;
  explicit store_fill_functor(GtkListStore *s) : store(s) {}
  GtkTreeIter operator()(const wms_server_t *srv);
};

GtkTreeIter store_fill_functor::operator()(const wms_server_t *srv)
{
  GtkTreeIter iter;

  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter,
                     WMS_SERVER_COL_NAME, srv->name.c_str(),
                     WMS_SERVER_COL_DATA, srv,
                     -1);

  return iter;
}

/* user clicked "add..." button in the wms server list */
static void on_server_add(wms_server_context_t *context) {

  wms_server_t *newserver = new wms_server_t();
  newserver->name   = "<service name>";
  // in case the project has a server set, but the global list is empty,
  // fill the data of the project server
  if(context->appdata.settings->wms_server.empty() &&
     !context->appdata.project->wms_server.empty()) {
    newserver->server = context->appdata.project->wms_server;
    newserver->path   = context->appdata.project->wms_path;
  } else {
    newserver->server = "<server url>";
    newserver->path   = "<path in server>";
  }

  if(!wms_server_edit(context, TRUE, newserver)) {
    /* user has cancelled request. remove newly added item */
    printf("user clicked cancel\n");

    delete newserver;
  } else {
    /* attach a new server item to the chain */
    context->appdata.settings->wms_server.push_back(newserver);

    GtkTreeIter iter = store_fill_functor(context->store)(newserver);

    GtkTreeSelection *selection = list_get_selection(context->list);
    gtk_tree_selection_select_iter(selection, &iter);

    wms_server_selected(context, newserver);
  }
}

/* widget to select a wms server from a list */
static GtkWidget *wms_server_widget(wms_server_context_t *context) {
  std::vector<list_button> buttons;
  buttons.push_back(list_button(_("_Add"), G_CALLBACK(on_server_add)));
  buttons.push_back(list_button(_("_Edit"), G_CALLBACK(on_server_edit)));
  buttons.push_back(list_button(_("Remove"), G_CALLBACK(on_server_remove)));

  context->store = gtk_list_store_new(WMS_SERVER_NUM_COLS,
	        G_TYPE_STRING, G_TYPE_POINTER);

  context->list = list_new(LIST_HILDON_WITHOUT_HEADERS, 0, context,
                           wms_server_changed, buttons,
                           std::vector<list_view_column>(1, list_view_column(_("Name"), LIST_FLAG_ELLIPSIZE)),
                           context->store);

  const std::vector<wms_server_t *> &servers = context->appdata.settings->wms_server;
  std::for_each(servers.begin(), servers.end(), store_fill_functor(context->store));

  g_object_unref(context->store);

  return context->list;
}

static bool wms_server_dialog(appdata_t &appdata, wms_t *wms) {
  bool ok = false;

  wms_server_context_t context(appdata, wms,
    misc_dialog_new(MISC_DIALOG_MEDIUM, _("WMS Server Selection"),
		    GTK_WINDOW(appdata.window),
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

  wms_server_selected(&context, context.select_server());

  gtk_widget_show_all(context.dialog);

  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context.dialog))) {
    const wms_server_t *server = get_selection(list_get_selection(context.list));
    if(server) {
      /* fetch parameters from selected entry */
      printf("WMS: using %s\n", server->name.c_str());
      wms->server = server->server;
      wms->path = server->path;
      ok = true;
    } else {
      ok = !wms->server.empty() && !wms->path.empty();
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
static gboolean on_view_clicked(GtkWidget *widget, GdkEventButton *event, gpointer) {
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
  appdata_t &appdata;
  wms_layer_t::list selected;
  explicit selected_context(appdata_t &a) : appdata(a) {}
  void operator()(const wms_layer_t *layer);
};

static void changed(GtkTreeSelection *sel, gpointer user_data) {
  /* we need to know what changed in order to let the user acknowlege it! */
  wms_layer_t::list * const selected = static_cast<wms_layer_t::list *>(user_data);

  /* get view from selection ... */
  GtkTreeView *view = gtk_tree_selection_get_tree_view(sel);
  assert(view != O2G_NULLPTR);

  /* ... and get model from view */
  GtkTreeModel *model = gtk_tree_view_get_model(view);
  assert(model != O2G_NULLPTR);

  /* walk the entire store */
  GtkTreeIter iter;
  gboolean ok = gtk_tree_model_get_iter_first(model, &iter);
  selected->clear();
  while(ok) {
    wms_layer_t *layer = O2G_NULLPTR;

    gtk_tree_model_get(model, &iter, LAYER_COL_DATA, &layer, -1);
    assert(layer != O2G_NULLPTR);

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
  bool fits = layer->llbbox.valid && wms_llbbox_fits(project, layer->llbbox);

  /* Append a row and fill in some data */
  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter,
                     LAYER_COL_TITLE, layer->title.c_str(),
                     LAYER_COL_FITS, fits ? TRUE : FALSE,
                     LAYER_COL_DATA, layer,
                     -1);
}

static GtkWidget *wms_layer_widget(selected_context *context, const wms_layer_t::list &layers,
                                   GtkWidget *) {

#ifndef FREMANTLE
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
                fitting_layers_functor(store, context->appdata.project));

  g_object_unref(store);

  g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(changed), &context->selected);

#ifndef FREMANTLE
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
		    GTK_WINDOW(ctx->appdata.window),
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

static bool setBgImage(appdata_t &appdata, const std::string &filename) {
  bool ret = appdata.map->set_bg_image(filename);
  if(ret) {
    appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_WMS_CLEAR, true);
    appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_WMS_ADJUST, true);
  }
  return ret;
}

static bool layer_is_usable(const wms_layer_t *layer) {
  return layer->is_usable();
}

struct find_format_reverse_functor {
  const unsigned int mask;
  explicit find_format_reverse_functor(unsigned int m) : mask(m) {}
  bool operator()(const std::pair<const char *, WmsImageFormat> &p) {
    return p.second & mask;
  }
};

void wms_import(appdata_t &appdata) {
  if(!appdata.project) {
    errorf(appdata.window, _("Need an open project to derive WMS coordinates"));
    return;
  }

  /* this cancels any wms adjustment in progress */
  if(appdata.map->action.type == MAP_ACTION_BG_ADJUST)
    map_action_cancel(appdata.map);

  wms_t wms(appdata.project->wms_server, appdata.project->wms_path);

  /* reset any background adjustments in the project ... */
  appdata.project->wms_offset.x = 0;
  appdata.project->wms_offset.y = 0;

  /* ... as well as in the map */
  appdata.map->bg.offset.x = 0;
  appdata.map->bg.offset.y = 0;

  /* get server from dialog */
  if(!wms_server_dialog(appdata, &wms))
    return;

  /* ------------- copy values back into project ---------------- */
  appdata.project->wms_server = wms.server;
  appdata.project->wms_path = wms.path;

  /* ----------- request capabilities -------------- */
  /* nothing has to be done if the last character of path is already a valid URL delimiter */
  const char lastCh = wms.path[wms.path.size() - 1];
  const char *append_char = (lastCh == '?' || lastCh == '&') ? "" :
  /* if there's already a question mark, then add further */
  /* parameters using the &, else use the ? */
                            (wms.path.find('?') != std::string::npos ? "&" : "?");

  std::string url;
  url.reserve(256); // make enough room that most URLs will need no reallocation
  url = wms.server + wms.path + append_char +
                                 "SERVICE=wms"
                                 "&VERSION=1.1.1"
                                 "&REQUEST=GetCapabilities";

  char *cap = O2G_NULLPTR;
  size_t caplen;
  net_io_download_mem(appdata.window, url, &cap, caplen);

  /* ----------- parse capabilities -------------- */
  if(unlikely(ImageFormats.empty()))
    initImageFormats();

  bool parse_success = false;
  if(!cap) {
    errorf(appdata.window, _("WMS download failed:\n\nGetCapabilities failed"));
  } else {
    xmlDoc *doc = O2G_NULLPTR;

    /* parse the file and get the DOM */
    if((doc = xmlReadMemory(cap, caplen, O2G_NULLPTR, O2G_NULLPTR, XML_PARSE_NONET)) == O2G_NULLPTR) {
      xmlErrorPtr errP = xmlGetLastError();
      errorf(appdata.window, _("WMS download failed:\n\n"
             "XML error while parsing capabilities:\n%s"), errP->message);
    } else {
      printf("ok, parse doc tree\n");

      parse_success = wms_cap_parse_doc(&wms, doc);
    }

    g_free(cap);
  }

  /* ------------ basic checks ------------- */

  if(!parse_success) {
    errorf(appdata.window, _("Incomplete/unexpected reply!"));
    return;
  }

  if(!wms.cap.request.getmap.format) {
    errorf(appdata.window, _("No supported image format found."));
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
    errorf(appdata.window, _("Server provides no data in the required format!\n\n"
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
  wms_setup_extent(appdata.project, &wms);

  /* start building url */
  url.erase(url.size() - strlen("Capabilities")); // Keep "Get"
  url += "Map&LAYERS="; // reuse "Get" from "GetCapabilities"

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

  /* build strings of min and max lat and lon to be used in url */
  std::string mincoords = appdata.project->min.print(',');
  std::string maxcoords = appdata.project->max.print(',');

  /* find preferred supported video format */
  const FormatMap::const_iterator itEnd = ImageFormats.end();
  FormatMap::const_iterator it = std::find_if(std::cbegin(ImageFormats), itEnd,
                                              find_format_reverse_functor(wms.cap.request.getmap.format));
  assert(it != itEnd);

  char buf[64];
  sprintf(buf, "&WIDTH=%d&HEIGHT=%d&FORMAT=", wms.width, wms.height);

  /* build complete url */
  const std::array<const char *, 9> parts = { {
                          "&SRS=", srs, "&BBOX=", mincoords.c_str(), ",",
                          maxcoords.c_str(), buf, it->first, "&reaspect=false"
                          } };
  for(unsigned int i = 0; i < parts.size(); i++)
    url += parts[i];

  const std::string filename = std::string(appdata.project->path) + "wms." +
                               ImageFormatExtensions[it->second];

  /* remove any existing image before */
  wms_remove(appdata);

  if(!net_io_download_file(appdata.window, url, filename, O2G_NULLPTR))
    return;

  /* there should be a matching file on disk now */
  setBgImage(appdata, filename);
}

/* try to load an existing image into map */
void wms_load(appdata_t &appdata) {
  if(unlikely(ImageFormatExtensions.empty()))
    initImageFormats();

  const std::map<WmsImageFormat, const char *>::const_iterator itEnd = ImageFormatExtensions.end();
  std::map<WmsImageFormat, const char *>::const_iterator it = ImageFormatExtensions.begin();

  std::string filename = appdata.project->path + "/wms.";
  const std::string::size_type extpos = filename.size();

  for(; it != itEnd; it++) {
    filename.erase(extpos);
    filename += it->second;

    appdata.map->bg.offset.x = appdata.project->wms_offset.x;
    appdata.map->bg.offset.y = appdata.project->wms_offset.y;

    if(setBgImage(appdata, filename))
      break;
  }
}

void wms_remove_file(project_t &project) {
  if(unlikely(ImageFormatExtensions.empty()))
    initImageFormats();

  fdguard dirfd(project.path.c_str());
  if(unlikely(!dirfd.valid()))
    return;

  std::string filename = "wms.";
  const std::string::size_type extpos = filename.size();

  const std::map<WmsImageFormat, const char *>::const_iterator itEnd = ImageFormatExtensions.end();
  for(std::map<WmsImageFormat, const char *>::const_iterator it = ImageFormatExtensions.begin();
      it != itEnd; it++) {
    filename.erase(extpos);
    filename += it->second;

    unlinkat(project.dirfd, filename.c_str(), 0);
  }
}

void wms_remove(appdata_t &appdata) {

  /* this cancels any wms adjustment in progress */
  if(appdata.map->action.type == MAP_ACTION_BG_ADJUST)
    map_action_cancel(appdata.map);

  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_WMS_CLEAR, false);
  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_WMS_ADJUST, false);

  appdata.map->remove_bg_image();

  wms_remove_file(*appdata.project);
}

struct server_preset_s {
  const char *name, *server, *path;
};

static const std::array<struct server_preset_s, 1> default_servers = { {
  { "Open Geospatial Consortium Web Services", "http://ows.terrestris.de", "/osm/service?" }
  /* add more servers here ... */
} };

std::vector<wms_server_t *> wms_server_get_default(void) {
  std::vector<wms_server_t *> servers;

  for(unsigned int i = 0; i < default_servers.size(); i++) {
    wms_server_t *cur = new wms_server_t();
    cur->name = default_servers[i].name;
    cur->server = default_servers[i].server;
    cur->path = default_servers[i].path;
    servers.push_back(cur);
  }

  return servers;
}
