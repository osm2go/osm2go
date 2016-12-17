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
#include "list.h"
#include "misc.h"
#include "net_io.h"
#include "wms.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <strings.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

#define WMS_FORMAT_JPG  (1<<0)
#define WMS_FORMAT_JPEG (1<<1)
#define WMS_FORMAT_PNG  (1<<2)
#define WMS_FORMAT_GIF  (1<<3)

typedef struct {
  pos_t min, max;
  gboolean valid;
} wms_llbbox_t;

typedef struct wms_layer_t {
  gchar *title;
  gchar *name;
  gchar *srs;
  gboolean epsg4326, selected;
  wms_llbbox_t llbbox;

  struct wms_layer_t *children;
  struct wms_layer_t *next;
} wms_layer_t;

typedef struct {
  gulong format;
} wms_getmap_t;

typedef struct {
  wms_getmap_t *getmap;
} wms_request_t;

typedef struct {
  gchar *title;
} wms_service_t;

typedef struct {
  wms_layer_t *layer;
  wms_request_t *request;
} wms_cap_t;

typedef struct {
  gchar *server;
  gchar *path;
  gint width, height;

  wms_service_t *service;
  wms_cap_t *cap;
} wms_t;

gboolean xmlTextIs(xmlDocPtr doc, xmlNodePtr list, char *str) {
  xmlChar *nstr = xmlNodeListGetString(doc, list, 1);
  if(!nstr) return FALSE;

  gboolean match = (strcmp(str, (char*)nstr) == 0);
  xmlFree(nstr);
  return match;
}

float xmlGetPropFloat(xmlNode *node, char *prop) {
  xmlChar *prop_str = xmlGetProp(node, BAD_CAST prop);
  if(!prop_str) return NAN;

  float retval = g_ascii_strtod((gchar*)prop_str, NULL);
  xmlFree(prop_str);
  return retval;
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
  wms_layer_t *wms_layer = NULL;
  xmlNode *cur_node = NULL;

  wms_layer = g_new0(wms_layer_t, 1);
  wms_layer->llbbox.min.lon = wms_layer->llbbox.min.lat = NAN;
  wms_layer->llbbox.max.lon = wms_layer->llbbox.max.lat = NAN;

  wms_layer_t **children = &(wms_layer->children);

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "Layer") == 0) {
	*children = wms_cap_parse_layer(doc, cur_node);
	if(*children) children = &((*children)->next);
      } else if(strcasecmp((char*)cur_node->name, "Name") == 0) {
	xmlChar *str = xmlNodeListGetString(doc, cur_node->children, 1);
	wms_layer->name = g_strdup((gchar*)str);
	xmlFree(str);
      } else if(strcasecmp((char*)cur_node->name, "Title") == 0) {
	xmlChar *str = xmlNodeListGetString(doc, cur_node->children, 1);
	wms_layer->title = g_strdup((gchar*)str);
	xmlFree(str);
      } else if(strcasecmp((char*)cur_node->name, "SRS") == 0) {
	xmlChar *str = xmlNodeListGetString(doc, cur_node->children, 1);
	wms_layer->srs = g_strdup((gchar*)str);
	xmlFree(str);

	printf("SRS = %s\n", wms_layer->srs);

	if(strcmp(wms_layer->srs, "EPSG:4326") == 0)
	  wms_layer->epsg4326 = TRUE;

      } else if(strcasecmp((char*)cur_node->name, "LatLonBoundingBox") == 0) {
	wms_layer->llbbox.min.lat = xmlGetPropFloat(cur_node, "miny");
	wms_layer->llbbox.min.lon = xmlGetPropFloat(cur_node, "minx");
	wms_layer->llbbox.max.lat = xmlGetPropFloat(cur_node, "maxy");
	wms_layer->llbbox.max.lon = xmlGetPropFloat(cur_node, "maxx");
      } else
	printf("found unhandled WMT_MS_Capabilities/Capability/Layer/%s\n",
	       cur_node->name);
    }
  }

  wms_layer->llbbox.valid = wms_bbox_is_valid(&wms_layer->llbbox.min,
					      &wms_layer->llbbox.max);

  printf("------------------- Layer: %s ---------------------------\n",
	 wms_layer->title);
  printf("Name: %s\n", wms_layer->name);
  printf("EPSG-4326: %s\n", wms_layer->epsg4326?"yes":"no");
  if(wms_layer->llbbox.valid)
    printf("LatLonBBox: %f/%f %f/%f\n",
	   wms_layer->llbbox.min.lat, wms_layer->llbbox.min.lon,
	   wms_layer->llbbox.max.lat, wms_layer->llbbox.max.lon);
  else
    printf("No/invalid LatLonBBox\n");

  return wms_layer;
}

static wms_getmap_t *wms_cap_parse_getmap(xmlDocPtr doc, xmlNode *a_node) {
  wms_getmap_t *wms_getmap = NULL;
  xmlNode *cur_node = NULL;

  wms_getmap = g_new0(wms_getmap_t, 1);

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "Format") == 0) {
	if(xmlTextIs(doc, cur_node->children, "image/png"))
	  wms_getmap->format |= WMS_FORMAT_PNG;
	if(xmlTextIs(doc, cur_node->children, "image/gif"))
	  wms_getmap->format |= WMS_FORMAT_GIF;
	if(xmlTextIs(doc, cur_node->children, "image/jpg"))
	  wms_getmap->format |= WMS_FORMAT_JPG;
	if(xmlTextIs(doc, cur_node->children, "image/jpeg"))
	  wms_getmap->format |= WMS_FORMAT_JPEG;
      } else
	printf("found unhandled "
	       "WMT_MS_Capabilities/Capability/Request/GetMap/%s\n",
	       cur_node->name);
    }
  }

  printf("Supported formats: %s%s%s\n",
	 (wms_getmap->format & WMS_FORMAT_PNG)?"png ":"",
	 (wms_getmap->format & WMS_FORMAT_GIF)?"gif ":"",
	 (wms_getmap->format & (WMS_FORMAT_JPG | WMS_FORMAT_JPEG))?"jpg ":"");
  return wms_getmap;
}

static wms_request_t *wms_cap_parse_request(xmlDocPtr doc, xmlNode *a_node) {
  wms_request_t *wms_request = NULL;
  xmlNode *cur_node = NULL;

  wms_request = g_new0(wms_request_t, 1);

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "GetMap") == 0) {
	wms_request->getmap = wms_cap_parse_getmap(doc, cur_node);
      } else
	printf("found unhandled WMT_MS_Capabilities/Capability/Request/%s\n",
	       cur_node->name);
    }
  }

  return wms_request;
}

static wms_cap_t *wms_cap_parse_cap(xmlDocPtr doc, xmlNode *a_node) {
  wms_cap_t *wms_cap = NULL;
  xmlNode *cur_node = NULL;

  wms_cap = g_new0(wms_cap_t, 1);
  wms_layer_t **layer = &(wms_cap->layer);

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "Request") == 0) {
	wms_cap->request = wms_cap_parse_request(doc, cur_node);
      } else if(strcasecmp((char*)cur_node->name, "Layer") == 0) {
	*layer = wms_cap_parse_layer(doc, cur_node);
	if(*layer) layer = &((*layer)->next);
      } else
	printf("found unhandled WMT_MS_Capabilities/Capability/%s\n",
	       cur_node->name);
    }
  }

  return wms_cap;
}

static wms_service_t *wms_cap_parse_service(xmlDocPtr doc, xmlNode *a_node) {
  wms_service_t *wms_service = NULL;
  xmlNode *cur_node = NULL;

  wms_service = g_new0(wms_service_t, 1);

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "title") == 0) {
	xmlChar *str = xmlNodeListGetString(doc, cur_node->children, 1);
	wms_service->title = g_strdup((gchar *)str);
	xmlFree(str);
      } else
	printf("found unhandled WMT_MS_Capabilities/Service/%s\n",
	       cur_node->name);
    }
  }

  printf("-- Service --\n");
  printf("Title: %s\n", wms_service->title);

  return wms_service;
}

static void wms_cap_parse(wms_t *wms, xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      if(strcasecmp((char*)cur_node->name, "Service") == 0) {
	if(!wms->service)
	  wms->service = wms_cap_parse_service(doc, cur_node);
      } else if(strcasecmp((char*)cur_node->name, "Capability") == 0) {
	if(!wms->cap)
	  wms->cap = wms_cap_parse_cap(doc, cur_node);
      } else
	printf("found unhandled WMT_MS_Capabilities/%s\n", cur_node->name);
    }
  }
}

/* parse root element */
static void wms_cap_parse_root(wms_t *wms, xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      if(strcasecmp((char*)cur_node->name, "WMT_MS_Capabilities") == 0) {
	wms_cap_parse(wms, doc, cur_node);
      } else
	printf("found unhandled %s\n", cur_node->name);
    }
  }
}

static void wms_cap_parse_doc(wms_t *wms, xmlDocPtr doc) {
  /* Get the root element node */
  xmlNode *root_element = xmlDocGetRootElement(doc);

  wms_cap_parse_root(wms, doc, root_element);

  /*free the document */
  xmlFreeDoc(doc);
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

  wms->width = lmax.x - lmin.x;
  wms->height = lmax.y - lmin.y;

  if(wms->width > 2048)  wms->width = 2048;
  if(wms->height > 2048) wms->height = 2048;

  printf("WMS: required image size = %dx%d\n",
	 wms->width, wms->height);
}

/* --------------- freeing stuff ------------------- */

static void wms_layer_free(wms_layer_t *layer) {
  while(layer) {

    g_free(layer->title);
    g_free(layer->name);
    g_free(layer->srs);

    if(layer->children) wms_layer_free(layer->children);

    wms_layer_t *next = layer->next;
    g_free(layer);
    layer = next;
  }
}

static void wms_getmap_free(wms_getmap_t *getmap) {
  g_free(getmap);
}

static void wms_request_free(wms_request_t *request) {
  if(request->getmap) wms_getmap_free(request->getmap);
  g_free(request);
}

static void wms_cap_free(wms_cap_t *cap) {
  if(cap->layer)   wms_layer_free(cap->layer);
  if(cap->request) wms_request_free(cap->request);
  g_free(cap);
}

static void wms_service_free(wms_service_t *service) {
  g_free(service->title);
  g_free(service);
}

static void wms_free(wms_t *wms) {
  g_free(wms->server);
  g_free(wms->path);
  if(wms->cap)     wms_cap_free(wms->cap);
  if(wms->service) wms_service_free(wms->service);
  g_free(wms);
}

/* ---------------------- use ------------------- */

static gboolean wms_llbbox_fits(project_t *project, wms_llbbox_t *llbbox) {
  if(!llbbox || !llbbox->valid ||
     (project->min.lat < llbbox->min.lat) ||
     (project->min.lon < llbbox->min.lon) ||
     (project->max.lat > llbbox->max.lat) ||
     (project->max.lon > llbbox->max.lon))
    return FALSE;

  return TRUE;
}

static void wms_get_child_layers(wms_layer_t *layer,
	 gint depth, gboolean epsg4326, wms_llbbox_t *llbbox, const gchar *srs,
	 wms_layer_t **c_layer) {
  while(layer) {

    /* get a copy of the parents values for the current one ... */
    wms_llbbox_t *local_llbbox = llbbox;
    gboolean local_epsg4326 = epsg4326;

    /* ... and overwrite the inherited stuff with better local stuff */
    if(layer->llbbox.valid)                    local_llbbox = &layer->llbbox;
    if(layer->epsg4326)                        local_epsg4326 = TRUE;

    /* only named layers with useful bounding box are added to the list */
    if(local_llbbox && layer->name) {
      *c_layer = g_new0(wms_layer_t, 1);
      (*c_layer)->name     = g_strdup(layer->name);
      (*c_layer)->title    = g_strdup(layer->title);
      (*c_layer)->srs      = g_strdup(srs);
      (*c_layer)->epsg4326 = local_epsg4326;
      (*c_layer)->llbbox   = *local_llbbox;
      c_layer = &((*c_layer)->next);
    }

    wms_get_child_layers(layer->children, depth+1,
			 local_epsg4326, local_llbbox,
			 srs, c_layer);

    layer = layer->next;
  }
}

static wms_layer_t *wms_get_requestable_layers(wms_t *wms) {
  printf("\nSearching for usable layers\n");

  wms_layer_t *r_layer = NULL, **c_layer = &r_layer;

  wms_layer_t *layer = wms->cap->layer;
  while(layer) {
    wms_llbbox_t *llbbox = &layer->llbbox;
    if(llbbox && !llbbox->valid) llbbox = NULL;

    wms_get_child_layers(layer->children, 1,
	 layer->epsg4326, llbbox, layer->srs, c_layer);

    layer = layer->next;
  }

  return r_layer;
}

enum {
  WMS_SERVER_COL_NAME = 0,
  WMS_SERVER_COL_DATA,
  WMS_SERVER_NUM_COLS
};

typedef struct {
  appdata_t *appdata;
  wms_t *wms;
  GtkWidget *dialog, *list;
  GtkListStore *store;
  GtkWidget *server_label, *path_label;
} wms_server_context_t;

static wms_server_t *get_selection(wms_server_context_t *context) {
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  selection = list_get_selection(context->list);
  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    wms_server_t *wms_server;
    gtk_tree_model_get(model, &iter, WMS_SERVER_COL_DATA, &wms_server, -1);
    return(wms_server);
  }

  return NULL;
}

static void wms_server_selected(wms_server_context_t *context,
				wms_server_t *selected) {

  if(!selected && context->wms->server && context->wms->path) {
    /* if the projects settings match a list entry, then select this */

    GtkTreeSelection *selection = list_get_selection(context->list);

    /* walk the entire store to get all values */
    wms_server_t *server = NULL;
    GtkTreeIter iter;

    gboolean valid =
      gtk_tree_model_get_iter_first(GTK_TREE_MODEL(context->store), &iter);

    while(valid && !selected) {
      gtk_tree_model_get(GTK_TREE_MODEL(context->store), &iter,
			 WMS_SERVER_COL_DATA, &server, -1);
      g_assert(server);

      if((strcmp(server->server, context->wms->server) == 0) &&
	 (strcmp(server->path, context->wms->path) == 0)) {
	gtk_tree_selection_select_iter(selection, &iter);
	selected = server;
      }

      valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(context->store), &iter);
    }
  }

  list_button_enable(context->list, LIST_BUTTON_REMOVE, selected != NULL);
  list_button_enable(context->list, LIST_BUTTON_EDIT, selected != NULL);

  /* user can click ok if a entry is selected or if both fields are */
  /* otherwise valid */
  if(selected) {
    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
		      GTK_RESPONSE_ACCEPT, TRUE);

    gtk_label_set_text(GTK_LABEL(context->server_label), selected->server);
    gtk_label_set_text(GTK_LABEL(context->path_label), selected->path);
  } else {
    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
	      GTK_RESPONSE_ACCEPT, context->wms->server && context->wms->path);

    if(context->wms->server)
      gtk_label_set_text(GTK_LABEL(context->server_label),
			 context->wms->server);
    else
      gtk_label_set_text(GTK_LABEL(context->server_label),
			 "");
    if(context->wms->path)
      gtk_label_set_text(GTK_LABEL(context->path_label),
			 context->wms->path);
    else
      gtk_label_set_text(GTK_LABEL(context->path_label),
			 "");
  }
}

static void
wms_server_changed(GtkTreeSelection *selection, gpointer userdata) {
  wms_server_context_t *context = (wms_server_context_t*)userdata;

  GtkTreeModel *model = NULL;
  GtkTreeIter iter;

  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    wms_server_t *wms_server = NULL;

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
    wms_server_t *server = NULL;
    gtk_tree_model_get(model, &iter, WMS_SERVER_COL_DATA, &server, -1);

    g_assert(server);

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

  wms_server_selected(context, NULL);
}

static void callback_modified_name(GtkWidget *widget, gpointer data) {
  wms_server_context_t *context = (wms_server_context_t*)data;

  const gchar *name = gtk_entry_get_text(GTK_ENTRY(widget));

  /* name must not contain some special chars */
  gboolean ok = TRUE;

  /* search all entries except the last (which is the one we are editing) */
  wms_server_t *server = context->appdata->settings->wms_server;
  while(server && server->next) {
    if(strcasecmp(server->name, (char*)name) == 0) ok = FALSE;
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
		    NULL);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog),
				  GTK_RESPONSE_ACCEPT);

  GtkWidget *label, *name, *server, *path;
  GtkWidget *table = gtk_table_new(2, 3, FALSE);

  gtk_table_attach(GTK_TABLE(table),
		   label = gtk_label_new(_("Name:")), 0, 1, 0, 1,
		   GTK_FILL, 0, 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),
		    name = entry_new(), 1, 2, 0, 1);
  gtk_entry_set_activates_default(GTK_ENTRY(name), TRUE);
  HILDON_ENTRY_NO_AUTOCAP(name);
  gtk_widget_set_sensitive(GTK_WIDGET(name), edit_name);
  g_signal_connect(G_OBJECT(name), "changed",
		   G_CALLBACK(callback_modified_name), context);

  gtk_table_attach(GTK_TABLE(table),
		   label = gtk_label_new(_("Server:")), 0, 1, 1, 2,
		   GTK_FILL, 0, 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),
		    server = entry_new(), 1, 2, 1, 2);
  gtk_entry_set_activates_default(GTK_ENTRY(server), TRUE);
  HILDON_ENTRY_NO_AUTOCAP(server);

  gtk_table_attach(GTK_TABLE(table),
		   label = gtk_label_new(_("Path:")), 0, 1, 2, 3,
		   GTK_FILL, 0, 0, 0);
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
    wms_server_t *server = NULL;
    gtk_tree_model_get(model, &iter, WMS_SERVER_COL_DATA, &server, -1);
    g_assert(server);

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
    *prev = NULL;

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
		   NULL);

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

  wms_server_context_t *context = g_new0(wms_server_context_t, 1);
  context->appdata = appdata;
  context->wms = wms;

  context->dialog =
    misc_dialog_new(MISC_DIALOG_MEDIUM, _("WMS Server Selection"),
		    GTK_WINDOW(appdata->window),
		    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		    NULL);

  /* server selection box */
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context->dialog)->vbox),
			      wms_server_widget(context));

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context->dialog)->vbox),
		     gtk_hseparator_new(), FALSE, FALSE, 0);

  GtkWidget *label;
  GtkWidget *table = gtk_table_new(2, 2, FALSE);  // x, y
  gtk_table_set_col_spacing(GTK_TABLE(table), 0, 10);

  label = gtk_label_new(_("Server:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach(GTK_TABLE(table),  label, 0, 1, 0, 1, GTK_FILL, 0,0,0);
  context->server_label = gtk_label_new(NULL);
  gtk_label_set_ellipsize(GTK_LABEL(context->server_label),
			  PANGO_ELLIPSIZE_MIDDLE);
  gtk_misc_set_alignment(GTK_MISC(context->server_label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), context->server_label,
			    1, 2, 0, 1);

  label = gtk_label_new(_("Path:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach(GTK_TABLE(table),  label, 0, 1, 1, 2, GTK_FILL, 0,0,0);
  context->path_label = gtk_label_new(NULL);
  gtk_label_set_ellipsize(GTK_LABEL(context->path_label),
			  PANGO_ELLIPSIZE_MIDDLE);
  gtk_misc_set_alignment(GTK_MISC(context->path_label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), context->path_label,
			    1, 2, 1, 2);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context->dialog)->vbox),
		     table, FALSE, FALSE, 0);


  wms_server_selected(context, NULL);

  gtk_widget_show_all(context->dialog);

  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context->dialog))) {
    wms_server_t *server = get_selection(context);
    if(server) {
      /* fetch parameters from selected entry */
      printf("WMS: using %s\n", server->name);
      g_free(wms->server);
      wms->server = g_strdup(server->server);
      g_free(wms->path);
      wms->path = g_strdup(server->path);
      ok = TRUE;
    } else {
      if(wms->server && wms->path)
	ok = TRUE;

    }
  }

  gtk_widget_destroy(context->dialog);

  g_free(context);
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
		     event->x, event->y, &path, NULL, NULL, NULL)) {
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

static void changed(GtkTreeSelection *sel, G_GNUC_UNUSED gpointer user_data) {
  /* we need to know what changed in order to let the user acknowlege it! */

  /* get view from selection ... */
  GtkTreeView *view = gtk_tree_selection_get_tree_view(sel);
  g_assert(view);

  /* ... and get model from view */
  GtkTreeModel *model = gtk_tree_view_get_model(view);
  g_assert(model);

  /* walk the entire store */
  GtkTreeIter iter;
  gboolean one = FALSE, ok = gtk_tree_model_get_iter_first(model, &iter);
  while(ok) {
    wms_layer_t *layer = NULL;

    gtk_tree_model_get(model, &iter, LAYER_COL_DATA, &layer, -1);
    g_assert(layer);

    layer->selected = gtk_tree_selection_iter_is_selected(sel, &iter);
    if(layer->selected) one = TRUE;

    ok = gtk_tree_model_iter_next(model, &iter);
  }

  GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(view));
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
				    GTK_RESPONSE_ACCEPT, one);
}

static GtkWidget *wms_layer_widget(appdata_t *appdata, wms_layer_t *layer,
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
		   G_CALLBACK(on_view_clicked), NULL);
#endif

  /* build the store */
  GtkListStore *store = gtk_list_store_new(LAYER_NUM_COLS,
      G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER);

  /* --- "Title" column --- */
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
		 _("Title"), renderer,
		 "text", LAYER_COL_TITLE,
		 "sensitive", LAYER_COL_FITS,
		 NULL);

  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(view), column, -1);

  gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));

  GtkTreeIter iter;
  while(layer) {
    gboolean fits = wms_llbbox_fits(appdata->project, &layer->llbbox);

    /* Append a row and fill in some data */
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
	       LAYER_COL_TITLE, layer->title,
	       LAYER_COL_FITS, fits,
	       LAYER_COL_DATA, layer,
	       -1);
    layer = layer->next;
  }

  g_object_unref(store);

  g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(changed), layer);

#ifndef FREMANTLE_PANNABLE_AREA
  /* put it into a scrolled window */
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
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


static gboolean wms_layer_dialog(appdata_t *appdata, wms_layer_t *layer) {
  gboolean ok = FALSE;

  GtkWidget *dialog =
    misc_dialog_new(MISC_DIALOG_LARGE, _("WMS layer selection"),
		    GTK_WINDOW(appdata->window),
		    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		    NULL);

  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
				    GTK_RESPONSE_ACCEPT, FALSE);

  /* layer list */
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox),
		      wms_layer_widget(appdata, layer, dialog));


  gtk_widget_show_all(dialog);

  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    ok = TRUE;

  gtk_widget_destroy(dialog);

  return ok;
}

static gboolean wms_one_layer_is_usable(project_t *project,
					wms_layer_t *layer) {
  gboolean ok = FALSE;

  while(layer) {
   if(layer->name && layer->epsg4326 && layer->llbbox.valid)
     ok = TRUE;

    printf("----- Layer \"%s\" -----\n", layer->title);
    printf("Name: %s\n", layer->name);
    printf("epsg4326: %s\n", layer->epsg4326?"yes":"no");
    if(layer->llbbox.valid) {
      printf("llbbox: %f/%f %f/%f\n",
	     layer->llbbox.min.lat, layer->llbbox.min.lon,
	     layer->llbbox.max.lat, layer->llbbox.max.lon);

      printf("llbbox fits project: %s\n",
	     wms_llbbox_fits(project, &layer->llbbox)?"yes":"no");
    } else
      printf("llbbox: none/invalid\n");

    layer = layer->next;
  }

  return ok;
}

void wms_import(appdata_t *appdata) {
  if(!appdata->project) {
    errorf(GTK_WIDGET(appdata->window),
	   _("Need an open project to derive WMS coordinates"));
    return;
  }

  /* this cancels any wms adjustment in progress */
  if(appdata->map->action.type == MAP_ACTION_BG_ADJUST)
    map_action_cancel(appdata);

  wms_t *wms = g_new0(wms_t,1);
  wms->server = g_strdup(appdata->project->wms_server);
  wms->path   = g_strdup(appdata->project->wms_path);

  /* reset any background adjustments in the project ... */
  if((appdata->project->wms_offset.x != 0)||
     (appdata->project->wms_offset.y != 0)) {

    appdata->project->wms_offset.x = 0;
    appdata->project->wms_offset.y = 0;
  }

  /* ... as well as in the map */
  appdata->map->bg.offset.x = 0;
  appdata->map->bg.offset.y = 0;

  /* get server from dialog */
  if(!wms_server_dialog(appdata, wms)) {
    wms_free(wms);
    return;
  }

  /* ------------- copy values back into project ---------------- */
  if(!appdata->project->wms_server ||
     strcmp(appdata->project->wms_server, wms->server) != 0) {
    g_free(appdata->project->wms_server);
    appdata->project->wms_server = g_strdup(wms->server);
  }

  if(!appdata->project->wms_path ||
     strcmp(appdata->project->wms_path, wms->path) != 0) {
    g_free(appdata->project->wms_path);
    appdata->project->wms_path = g_strdup(wms->path);
  }

  /* ----------- request capabilities -------------- */
  gboolean path_contains_qm = (strchr(wms->path, '?') != NULL);
  gboolean path_ends_with_special =
    (wms->path[strlen(wms->path)-1] == '?') ||
    (wms->path[strlen(wms->path)-1] == '&');

  /* if there's already a question mark, then add further */
  /* parameters using the &, else use the ? */
  const char *append_char = path_ends_with_special?"":(path_contains_qm?"&":"?");

  gchar *url = g_strdup_printf("%s%s"
			      "%sSERVICE=wms"
			      //			      "&WMTVER=1.1.1"
			      "&VERSION=1.1.1"
			      "&REQUEST=GetCapabilities",
			      wms->server, wms->path, append_char);

  char *cap = NULL;
  net_io_download_mem(GTK_WIDGET(appdata->window), appdata->settings,
		      (char*)url, &cap);
  g_free(url);

  /* ----------- parse capabilities -------------- */

  if(!cap) {
    errorf(GTK_WIDGET(appdata->window),
	   _("WMS download failed:\n\n"
	     "GetCapabilities failed"));
  } else {
    xmlDoc *doc = NULL;

    /* parse the file and get the DOM */
    if((doc = xmlReadMemory(cap, strlen(cap), NULL, NULL, 0)) == NULL) {
      xmlErrorPtr errP = xmlGetLastError();
      errorf(GTK_WIDGET(appdata->window),
	     _("WMS download failed:\n\n"
	       "XML error while parsing capabilities:\n"
	       "%s"), errP->message);
    } else {
      printf("ok, parse doc tree\n");

      wms_cap_parse_doc(wms, doc);
    }

    g_free(cap);
  }

  /* ------------ basic checks ------------- */

  if(!wms->cap || !wms->service || !wms->cap->layer ||
     !wms->cap->request || !wms->cap->request->getmap) {
    errorf(GTK_WIDGET(appdata->window), _("Incomplete/unexpected reply!"));
    wms_free(wms);
    return;
  }

  if(!wms->cap->request->getmap->format) {
    errorf(GTK_WIDGET(appdata->window), _("No supported image format found."));
    wms_free(wms);
    return;
  }

  /* ---------- evaluate layers ----------- */

  wms_layer_t *layer = wms_get_requestable_layers(wms);
  gboolean at_least_one_ok = wms_one_layer_is_usable(appdata->project, layer);

  if(!at_least_one_ok) {
    errorf(GTK_WIDGET(appdata->window),
	   _("Server provides no data in the required format!\n\n"
	     "(epsg4326 and LatLonBoundingBox are mandatory for osm2go)"));
#if 0
    wms_layer_free(layer);
    wms_free(wms);
    return;
#endif
  }

  if(!wms_layer_dialog(appdata, layer)) {
    wms_layer_free(layer);
    wms_free(wms);
    return;
  }

  /* --------- build getmap request ----------- */

  /* get required image size */
  wms_setup_extent(appdata->project, wms);

  /* start building url */
  url = g_strdup_printf("%s%s"
			"%sSERVICE=wms"
			//			"&WMTVER=1.1.1"
			"&VERSION=1.1.1"
			"&REQUEST=GetMap"
			"&LAYERS=",
			wms->server, wms->path, append_char);

  /* append layers */
  gchar *old;
  wms_layer_t *t = layer;
  gint cnt = 0;
  while(t) {
    if(t->selected) {
      old = url;
      url = g_strconcat(url, (!cnt)?"":",", t->name, NULL);
      g_free(old);
      cnt++;
    }
    t = t->next;
  }

  /* uses epsg4326 if possible */
  gchar *srs = NULL;
  if(layer->epsg4326)
    srs = g_strdup("EPSG:4326");
  else if(layer->srs)
    srs = g_strdup(layer->srs);

  wms_layer_free(layer);

  /* append styles entry */
  old = url;
  url = g_strconcat(url, "&STYLES=", NULL);
  g_free(old);

  while(--cnt) {
    old = url;
    url = g_strconcat(url, ",", NULL);
    g_free(old);
  }

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
  while(!(wms->cap->request->getmap->format & (1<<format)))
    format++;

  const char *formats[] = { "image/jpg", "image/jpeg",
			    "image/png", "image/gif" };

  /* build complete url */
  old = url;

  url = g_strdup_printf("%s&SRS=%s&BBOX=%s,%s,%s,%s"
			"&WIDTH=%d&HEIGHT=%d&FORMAT=%s"
			"&reaspect=false", url, srs,
			minlon, minlat, maxlon, maxlat, wms->width,
			wms->height, formats[format]);
  g_free(srs);
  g_free(old);

  const char *exts[] = { "jpg", "jpg", "png", "gif" };
  gchar *filename = g_strjoin("/wms.", appdata->project->path,
				   exts[format], NULL);

  /* remove any existing image before */
  wms_remove(appdata);

  if(!net_io_download_file(GTK_WIDGET(appdata->window), appdata->settings,
			   (char*)url, filename, NULL)) {
    g_free(filename);
    g_free(url);
    wms_free(wms);
    return;
  }

  /* there should be a matching file on disk now */
  map_set_bg_image(appdata->map, filename);

  gint x = appdata->osm->bounds->min.x + appdata->map->bg.offset.x;
  gint y = appdata->osm->bounds->min.y + appdata->map->bg.offset.y;
  canvas_image_move(appdata->map->bg.item, x, y,
		    appdata->map->bg.scale.x, appdata->map->bg.scale.y);


  g_free(filename);
  g_free(url);

  /* --------- free wms structure -----------------*/
  wms_free(wms);

  gtk_widget_set_sensitive(appdata->menu_item_wms_clear, TRUE);
  gtk_widget_set_sensitive(appdata->menu_item_wms_adjust, TRUE);
}

static const char *wms_exts[] = { "png", "gif", "jpg", NULL };
/* this must be the longest one */
#define DUMMYEXT wms_exts[0]

/* try to load an existing image into map */
void wms_load(appdata_t *appdata) {
  int i;
  gchar *filename = g_strjoin("/wms.", appdata->project->path, DUMMYEXT, NULL);
  gchar *ext = filename + strlen(filename) - strlen(DUMMYEXT);

  for(i = 0; wms_exts[i]; i++) {
    memcpy(ext, wms_exts[i], strlen(wms_exts[i]));

    if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
      appdata->map->bg.offset.x = appdata->project->wms_offset.x;
      appdata->map->bg.offset.y = appdata->project->wms_offset.y;

      map_set_bg_image(appdata->map, (char*)filename);

      /* restore image to saved position */
      gint x = appdata->osm->bounds->min.x + appdata->map->bg.offset.x;
      gint y = appdata->osm->bounds->min.y + appdata->map->bg.offset.y;
      canvas_image_move(appdata->map->bg.item, x, y,
			appdata->map->bg.scale.x, appdata->map->bg.scale.y);

      gtk_widget_set_sensitive(appdata->menu_item_wms_clear, TRUE);
      gtk_widget_set_sensitive(appdata->menu_item_wms_adjust, TRUE);

      break;
    }
  }
  g_free(filename);
}

void wms_remove_file(project_t *project) {
  int i;
  gchar *filename = g_strjoin("/wms.", project->path, DUMMYEXT, NULL);
  gchar *ext = filename + strlen(filename) - strlen(DUMMYEXT);

  for(i = 0; wms_exts[i]; i++) {
    memcpy(ext, wms_exts[i], strlen(wms_exts[i]));

    if(g_file_test(filename, G_FILE_TEST_EXISTS))
      g_remove(filename);
  }

  g_free(filename);
}

void wms_remove(appdata_t *appdata) {

  /* this cancels any wms adjustment in progress */
  if(appdata->map->action.type == MAP_ACTION_BG_ADJUST)
    map_action_cancel(appdata);

  gtk_widget_set_sensitive(appdata->menu_item_wms_clear, FALSE);
  gtk_widget_set_sensitive(appdata->menu_item_wms_adjust, FALSE);

  map_remove_bg_image(appdata->map);

  wms_remove_file(appdata->project);
}

struct server_preset_s {
  const gchar *name, *server, *path;
} default_servers[] = {
  { "Open Geospatial Consortium Web Services", "http://ows.terrestris.de", "/osm/service?" },
  /* add more servers here ... */
  { NULL, NULL, NULL }
};

wms_server_t *wms_server_get_default(void) {
  wms_server_t *server = NULL, **cur = &server;
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

void wms_server_free(wms_server_t *wms_server) {
  g_free(wms_server->name);
  g_free(wms_server->server);
  g_free(wms_server->path);
  g_free(wms_server);
}

void wms_servers_free(wms_server_t *wms_server) {
  while(wms_server) {
    wms_server_t *next = wms_server->next;
    wms_server_free(wms_server);
    wms_server = next;
  }
}
