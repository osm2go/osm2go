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

#define WMS_FORMAT_JPG  (1<<0)
#define WMS_FORMAT_JPEG (1<<1)
#define WMS_FORMAT_PNG  (1<<2)
#define WMS_FORMAT_GIF  (1<<3)

typedef struct {
  pos_t min, max;
  gboolean valid;
} wms_llbbox_t;

typedef struct wms_layer_s {
  char *title;
  char *name;
  gboolean epsg4326, selected;
  wms_llbbox_t llbbox;

  struct wms_layer_s *children;
  struct wms_layer_s *next;
} wms_layer_t;

typedef struct {
  gulong format;
} wms_getmap_t;

typedef struct {
  wms_getmap_t *getmap;
} wms_request_t;

typedef struct {
  char *title;  
} wms_service_t;

typedef struct {
  wms_layer_t *layer;
  wms_request_t *request;
} wms_cap_t;

typedef struct {
  char *server;
  char *path;
  gint width, height;

  wms_service_t *service;
  wms_cap_t *cap;  
} wms_t;

gboolean xmlTextIs(xmlDocPtr doc, xmlNodePtr list, char *str) {
  char *nstr = (char*)xmlNodeListGetString(doc, list, 1);
  if(!nstr) return FALSE;

  printf("text = %s\n", nstr);

  gboolean match = (strcmp(str, nstr) == 0);  
  xmlFree(nstr);
  return match;
}

gboolean xmlPropIs(xmlNode *node, char *prop, char *str) {
  char *prop_str = (char*)xmlGetProp(node, BAD_CAST prop);
  if(!prop_str) return FALSE;

  gboolean match = (strcmp(prop_str, str) == 0);
  xmlFree(prop_str);
  return match;
}

float xmlGetPropFloat(xmlNode *node, char *prop) {
  char *prop_str = (char*)xmlGetProp(node, BAD_CAST prop);
  if(!prop_str) return NAN;

  float retval = g_ascii_strtod(prop_str, NULL);
  xmlFree(prop_str);
  return retval;
}

static gboolean wms_bbox_is_valid(pos_t *min, pos_t *max) {
  /* all four coordinates are valid? */
  if(isnan(min->lat)||isnan(min->lon)||isnan(max->lat)||isnan(max->lon))
    return FALSE;

  /* min/max span a useful range? */
  if(max->lat - min->lat < 1.0) return FALSE;
  if(max->lon - min->lon < 1.0) return FALSE;

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
  char *str = NULL;

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
	str = (char*)xmlNodeListGetString(doc, cur_node->children, 1);
	wms_layer->name = g_strdup(str);
	xmlFree(str);
      } else if(strcasecmp((char*)cur_node->name, "Title") == 0) {
	str = (char*)xmlNodeListGetString(doc, cur_node->children, 1);
	wms_layer->title = g_strdup(str);
	xmlFree(str);
      } else if(strcasecmp((char*)cur_node->name, "SRS") == 0) {
	if(xmlTextIs(doc, cur_node->children, "EPSG:4326"))
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
  char *str = NULL;

  wms_service = g_new0(wms_service_t, 1);

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "title") == 0) {
	str = (char*)xmlNodeListGetString(doc, cur_node->children, 1);
	wms_service->title = g_strdup(str);
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

  /*
   * Free the global variables that may
   * have been allocated by the parser.
   */
  xmlCleanupParser();
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

    if(layer->title) g_free(layer->title);
    if(layer->name)  g_free(layer->name);

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
  if(service->title)    g_free(service->title);
  g_free(service);
}

static void wms_free(wms_t *wms) {
  if(wms->server)  g_free(wms->server);
  if(wms->path)    g_free(wms->path);
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
		 gint depth, gboolean epsg4326, wms_llbbox_t *llbbox,
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
      (*c_layer)->epsg4326 = local_epsg4326;
      (*c_layer)->llbbox   = *local_llbbox;
      c_layer = &((*c_layer)->next);
    }

    wms_get_child_layers(layer->children, depth+1,
			 local_epsg4326, local_llbbox,
			 c_layer);

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
		 layer->epsg4326, llbbox, c_layer);

    layer = layer->next;
  }
  
  return r_layer;
}

static gboolean wms_server_dialog(appdata_t *appdata, wms_t *wms) {
  gboolean ok = FALSE;
  
  GtkWidget *dialog = gtk_dialog_new_with_buttons(_("WMS Setup"),
	  GTK_WINDOW(appdata->window), GTK_DIALOG_MODAL,
	  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
          NULL);

#ifdef USE_HILDON
  /* making the dialog a little wider makes it less "crowded" */
  gtk_window_set_default_size(GTK_WINDOW(dialog), 640, 50);
#else
  gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 50);
#endif

  GtkWidget *label;
  GtkWidget *table = gtk_table_new(2, 2, FALSE);  // x, y

  label = gtk_label_new(_("Server:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach(GTK_TABLE(table),  label, 0, 1, 0, 1, 0,0,0,0);
  GtkWidget *server_entry = gtk_entry_new();
  HILDON_ENTRY_NO_AUTOCAP(server_entry);
  gtk_entry_set_text(GTK_ENTRY(server_entry), wms->server);
  gtk_table_attach_defaults(GTK_TABLE(table), server_entry, 1, 2, 0, 1);

  label = gtk_label_new(_("Path:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach(GTK_TABLE(table),  label, 0, 1, 1, 2, 0,0,0,0);
  GtkWidget *path_entry = gtk_entry_new();
  HILDON_ENTRY_NO_AUTOCAP(path_entry);
  gtk_entry_set_text(GTK_ENTRY(path_entry), wms->path);
  gtk_table_attach_defaults(GTK_TABLE(table), path_entry, 1, 2, 1, 2);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), table);
  gtk_widget_show_all(dialog);
  
  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog))) {
    wms->server = g_strdup(gtk_entry_get_text(GTK_ENTRY(server_entry)));
    wms->path   = g_strdup(gtk_entry_get_text(GTK_ENTRY(path_entry)));
    ok = TRUE;
  }

  gtk_widget_destroy(dialog);

  return ok;
}

enum {
  LAYER_COL_TITLE = 0,
  LAYER_COL_SELECTED,
  LAYER_COL_FITS,
  LAYER_COL_DATA,
  LAYER_NUM_COLS
};

static void
layer_toggled(GtkCellRendererToggle *cell, const gchar *path_str,
	      GtkListStore *store) {
  GtkTreePath *path;
  GtkTreeIter iter;

  path = gtk_tree_path_new_from_string(path_str);
  gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path);

  /* get current enabled flag */
  gboolean enabled;
  gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 
		     LAYER_COL_SELECTED, &enabled, -1);

  /* change it and store it */
  enabled = !enabled;
  gtk_list_store_set(store, &iter, LAYER_COL_SELECTED, enabled, -1);

  /* and store it in the layer itself */
  wms_layer_t *layer = NULL;
  gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, LAYER_COL_DATA, &layer, -1);
  layer->selected = enabled;

  /* walk the entire store to get all values */
  if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 
		       LAYER_COL_SELECTED, &enabled, -1);

    while(gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter) &&
	  !enabled) 
      gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 
			 LAYER_COL_SELECTED, &enabled, -1);
  }

  GtkWidget *dialog = g_object_get_data(G_OBJECT(store), "dialog");
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), 
				    GTK_RESPONSE_ACCEPT, enabled);
  
  gtk_tree_path_free(path);
}

static GtkWidget *wms_layer_widget(appdata_t *appdata, wms_layer_t *layer, 
				   GtkWidget *dialog) {
  GtkWidget *view = gtk_tree_view_new();

  /* build the store */
  GtkListStore *store = gtk_list_store_new(LAYER_NUM_COLS, 
      G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_POINTER);

  g_object_set_data(G_OBJECT(store), "dialog", dialog);

  /* --- "selected" column --- */
  GtkCellRenderer *renderer = gtk_cell_renderer_toggle_new();
  g_signal_connect(renderer, "toggled", G_CALLBACK(layer_toggled), store);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
	-1, _(""), renderer, 
        "active", LAYER_COL_SELECTED, 
        "activatable", LAYER_COL_FITS, 
	NULL);

  /* --- "Title" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
		 _("Title"), renderer, 
		 "text", LAYER_COL_TITLE, 
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
	       LAYER_COL_SELECTED, FALSE,
	       LAYER_COL_TITLE, layer->title,
	       LAYER_COL_FITS, fits,
	       LAYER_COL_DATA, layer,
	       -1);
    layer = layer->next;
  }
  
  g_object_unref(store);

  /* put it into a scrolled window */
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), 
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), 
				      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scrolled_window), view);

  return scrolled_window;
}


static gboolean wms_layer_dialog(appdata_t *appdata, wms_layer_t *layer) {
  gboolean ok = FALSE;

  GtkWidget *dialog = gtk_dialog_new_with_buttons(_("WMS layer selection"),
	  GTK_WINDOW(appdata->window), GTK_DIALOG_MODAL,
	  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
          NULL);

  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), 
				    GTK_RESPONSE_ACCEPT, FALSE);

#ifdef USE_HILDON
  /* making the dialog a little wider makes it less "crowded" */
  gtk_window_set_default_size(GTK_WINDOW(dialog), 640, 300);
#else
  gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 200);
#endif

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
    appdata->project->dirty = TRUE;
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
  if(strcmp(appdata->project->wms_server, wms->server) != 0) {
    g_free(appdata->project->wms_server);
    appdata->project->wms_server = g_strdup(wms->server);
    appdata->project->dirty = TRUE;
  }

  if(strcmp(appdata->project->wms_path, wms->path) != 0) {
    g_free(appdata->project->wms_path);
    appdata->project->wms_path = g_strdup(wms->path);
    appdata->project->dirty = TRUE;
  }

  /* ----------- request capabilities -------------- */
  gboolean path_contains_qm = (strchr(wms->path, '?') != NULL);
  gboolean path_ends_with_special = 
    (wms->path[strlen(wms->path)-1] == '?') ||
    (wms->path[strlen(wms->path)-1] == '&');

  /* if there's already a question mark, then add further */
  /* parameters using the &, else use the ? */
  char *append_char = path_ends_with_special?"":(path_contains_qm?"&":"?");

  char *url = g_strdup_printf("%s%s"
			      "%sSERVICE=wms" 
			      "&WMTVER=1.1.1"
			      "&REQUEST=GetCapabilities",
			      wms->server, wms->path, append_char);

  char *cap = net_io_download_mem(GTK_WIDGET(appdata->window), url);
  g_free(url);

  /* ----------- parse capabilities -------------- */

  if(!cap) {
    errorf(GTK_WIDGET(appdata->window), 
	   _("WMS download failed:\n\n"
	     "GetCapabilities failed"));
  } else {
    xmlDoc *doc = NULL;
    
    LIBXML_TEST_VERSION;
    
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
	   _("Server provides no data in the required format!\n"
	     "(epsg4326 and LatLonBoundingBox are mandatory for osm2go)"));
    wms_layer_free(layer);
    wms_free(wms);
    return;
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
			"&WMTVER=1.1.1"
			"&REQUEST=GetMap"
			"&LAYERS=",
			wms->server, wms->path, append_char);

  /* append layers */
  char *old;
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
  char minlon[G_ASCII_DTOSTR_BUF_SIZE], minlat[G_ASCII_DTOSTR_BUF_SIZE];
  char maxlon[G_ASCII_DTOSTR_BUF_SIZE], maxlat[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr(minlon, sizeof(minlon), appdata->project->min.lon);
  g_ascii_dtostr(minlat, sizeof(minlat), appdata->project->min.lat);
  g_ascii_dtostr(maxlon, sizeof(maxlon), appdata->project->max.lon);
  g_ascii_dtostr(maxlat, sizeof(maxlat), appdata->project->max.lat);

  gint format = 0;
  while(!(wms->cap->request->getmap->format & (1<<format)))
    format++;

  const char *formats[] = { "image/jpg", "image/jpeg", 
			    "image/png", "image/gif" };

  old = url;
  url = g_strdup_printf("%s&SRS=EPSG:4326&BBOX=%s,%s,%s,%s"
			"&WIDTH=%d&HEIGHT=%d&FORMAT=%s&reaspect=false", url,
			minlon, minlat, maxlon, maxlat, wms->width, 
			wms->height, formats[format]);
  g_free(old);

  const char *exts[] = { "jpg", "jpg", "png", "gif" };
  char *filename = g_strdup_printf("%s/wms.%s", appdata->project->path, 
				   exts[format]);


  /* remove any existing image before */
  wms_remove(appdata);

  if(!net_io_download_file(GTK_WIDGET(appdata->window), url, filename)) {
    g_free(filename);
    g_free(url);
    wms_free(wms);
    return;
  }

  /* there should be a matching file on disk now */
  map_set_bg_image(appdata->map, filename);

  g_free(filename);
  g_free(url);

  /* --------- free wms structure -----------------*/
  wms_free(wms);

  gtk_widget_set_sensitive(appdata->menu_item_wms_clear, TRUE);
  gtk_widget_set_sensitive(appdata->menu_item_wms_adjust, TRUE);
}

/* try to load an existing image into map */
void wms_load(appdata_t *appdata) {
  const char *exts[] = { "png", "gif", "jpg", "" };
  int i=0;

  while(exts[i][0]) {
    char *filename = g_strdup_printf("%s/wms.%s", appdata->project->path, 
				     exts[i]);

    if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
      appdata->map->bg.offset.x = appdata->project->wms_offset.x;
      appdata->map->bg.offset.y = appdata->project->wms_offset.y;

      map_set_bg_image(appdata->map, filename);

      /* restore image to saved position */
      gint x = appdata->osm->bounds->min.x + appdata->map->bg.offset.x;
      gint y = appdata->osm->bounds->min.y + appdata->map->bg.offset.y;
      canvas_image_move(appdata->map->bg.item, x, y, 
			appdata->map->bg.scale.x, appdata->map->bg.scale.y);

      g_free(filename);

      gtk_widget_set_sensitive(appdata->menu_item_wms_clear, TRUE);
      gtk_widget_set_sensitive(appdata->menu_item_wms_adjust, TRUE);

      return;
    }
    g_free(filename);
    i++;
  }
}

void wms_remove(appdata_t *appdata) {
  const char *exts[] = { "png", "gif", "jpg", "" };
  int i=0;

  /* this cancels any wms adjustment in progress */
  if(appdata->map->action.type == MAP_ACTION_BG_ADJUST)
    map_action_cancel(appdata);

  gtk_widget_set_sensitive(appdata->menu_item_wms_clear, FALSE);
  gtk_widget_set_sensitive(appdata->menu_item_wms_adjust, FALSE);

  map_remove_bg_image(appdata->map);

  while(exts[i][0]) {
    char *filename = g_strdup_printf("%s/wms.%s", appdata->project->path, 
				     exts[i]);

    if(g_file_test(filename, G_FILE_TEST_EXISTS)) 
      g_remove(filename);

    g_free(filename);
    i++;
  }
}

