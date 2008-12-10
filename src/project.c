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

#include <sys/stat.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

typedef struct {
  //  appdata_t *appdata;
  project_t *project;
  GtkWidget *dialog, *fsize, *diff_stat, *diff_remove;
  GtkWidget *desc, *server;
  GtkWidget *minlat, *minlon, *maxlat, *maxlon;
  area_edit_t area_edit;
} project_context_t;

/* ------------ project file io ------------- */

static gboolean project_read(appdata_t *appdata, 
	     char *project_file, project_t *project) {

  LIBXML_TEST_VERSION;

  xmlDoc *doc = NULL;
  xmlNode *root_element = NULL;

  /* parse the file and get the DOM */
  if((doc = xmlReadFile(project_file, NULL, 0)) == NULL) {
    printf("error: could not parse file %s\n", project_file);
    return FALSE;
  }
  
  /* Get the root element node */
  root_element = xmlDocGetRootElement(doc);

  xmlNode *cur_node = NULL;
  for (cur_node = root_element; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "proj") == 0) {
	char *str;
	
	if((str = (char*)xmlGetProp(cur_node, BAD_CAST "dirty"))) {
	  project->data_dirty = (strcasecmp(str, "true") == 0);
	  xmlFree(str);
	} else
	  project->data_dirty = FALSE;
	
	xmlNode *node = cur_node->children;
	
	while(node != NULL) {
	  if(node->type == XML_ELEMENT_NODE) {

	    if(strcasecmp((char*)node->name, "desc") == 0) {
	      str = (char*)xmlNodeListGetString(doc, node->children, 1);
	      project->desc = g_strdup(str);
	      printf("desc = %s\n", project->desc);
	      xmlFree(str);

	    } else if(strcasecmp((char*)node->name, "server") == 0) {
	      str = (char*)xmlNodeListGetString(doc, node->children, 1);
	      project->server = g_strdup(str);
	      printf("server = %s\n", project->server);
	      xmlFree(str);
	      
	    } else if(project->map_state && 
		      strcasecmp((char*)node->name, "map") == 0) {
	      if((str = (char*)xmlGetProp(node, BAD_CAST "zoom"))) {
		project->map_state->zoom = g_ascii_strtod(str, NULL);
		xmlFree(str);
	      } 
	      if((str = (char*)xmlGetProp(node, BAD_CAST "scroll-offset-x"))) {
		project->map_state->scroll_offset.x = strtoul(str, NULL, 10);
		xmlFree(str);
	      } 
	      if((str = (char*)xmlGetProp(node, BAD_CAST "scroll-offset-y"))) {
		project->map_state->scroll_offset.y = strtoul(str, NULL, 10);
		xmlFree(str);
	      } 

	    } else if(strcasecmp((char*)node->name, "wms") == 0) {

	      if((str = (char*)xmlGetProp(node, BAD_CAST "server"))) {
		project->wms_server = g_strdup(str);
		xmlFree(str);
	      } 
	      if((str = (char*)xmlGetProp(node, BAD_CAST "path"))) {
		project->wms_path = g_strdup(str);
		xmlFree(str);
	      } 
	      if((str = (char*)xmlGetProp(node, BAD_CAST "x-offset"))) {
		project->wms_offset.x = strtoul(str, NULL, 10);
		xmlFree(str);
	      } 
	      if((str = (char*)xmlGetProp(node, BAD_CAST "y-offset"))) {
		project->wms_offset.y = strtoul(str, NULL, 10);
		xmlFree(str);
	      } 
	      
	    } else if(strcasecmp((char*)node->name, "osm") == 0) {
	      str = (char*)xmlNodeListGetString(doc, node->children, 1);
	      project->osm = g_strdup(str);
	      printf("osm = %s\n", project->osm);
	      xmlFree(str);
	    } else if(strcasecmp((char*)node->name, "min") == 0) {
	      if((str = (char*)xmlGetProp(node, BAD_CAST "lat"))) {
		project->min.lat = g_ascii_strtod(str, NULL);
		xmlFree(str);
	      } 
	      if((str = (char*)xmlGetProp(node, BAD_CAST "lon"))) {
		project->min.lon = g_ascii_strtod(str, NULL);
		xmlFree(str);
	      }

	    } else if(strcasecmp((char*)node->name, "max") == 0) {
	      if((str = (char*)xmlGetProp(node, BAD_CAST "lat"))) {
		project->max.lat = g_ascii_strtod(str, NULL);
		xmlFree(str);
	      }
	      if((str = (char*)xmlGetProp(node, BAD_CAST "lon"))) {
		project->max.lon = g_ascii_strtod(str, NULL);
		xmlFree(str);
	      }
	    }
	  }
	  node = node->next;
	}
      }
    }
  }
  
  xmlFreeDoc(doc);
  xmlCleanupParser();

  /* ------------ set some default that may be missing ----------- */
  /* ------- e.g. from project files saved by old versions ------- */
  if(!project->wms_server)
    project->wms_server = g_strdup(appdata->settings->wms_server);

  if(!project->wms_path)
    project->wms_path = g_strdup(appdata->settings->wms_path);

  return TRUE;
}

gboolean project_save(GtkWidget *parent, project_t *project) {
  char str[32];
  char *project_file = g_strdup_printf("%s%s.proj", 
		       project->path, project->name);

  printf("saving project to %s\n", project_file);

  /* check if project path exists */
  if(!g_file_test(project->path, G_FILE_TEST_IS_DIR)) {
    /* make sure project base path exists */
    if(g_mkdir_with_parents(project->path, S_IRWXU) != 0) {
      errorf(GTK_WIDGET(parent), 
	     _("Unable to create project path %s"), project->path);
      return FALSE;
    }
  }

  LIBXML_TEST_VERSION;

  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr node, root_node = xmlNewNode(NULL, BAD_CAST "proj");
  xmlNewProp(root_node, BAD_CAST "name", BAD_CAST project->name);
  if(project->data_dirty)
    xmlNewProp(root_node, BAD_CAST "dirty", BAD_CAST "true");

  xmlDocSetRootElement(doc, root_node);

  node = xmlNewChild(root_node, NULL, BAD_CAST "server", 
		     BAD_CAST project->server);

  xmlNewChild(root_node, NULL, BAD_CAST "desc", BAD_CAST project->desc);
  xmlNewChild(root_node, NULL, BAD_CAST "osm", BAD_CAST project->osm);

  node = xmlNewChild(root_node, NULL, BAD_CAST "min", NULL);
  g_ascii_dtostr(str, sizeof(str), project->min.lat);
  xmlNewProp(node, BAD_CAST "lat", BAD_CAST str);
  g_ascii_dtostr(str, sizeof(str), project->min.lon);
  xmlNewProp(node, BAD_CAST "lon", BAD_CAST str);

  node = xmlNewChild(root_node, NULL, BAD_CAST "max", NULL);
  g_ascii_dtostr(str, sizeof(str), project->max.lat);
  xmlNewProp(node, BAD_CAST "lat", BAD_CAST str);
  g_ascii_dtostr(str, sizeof(str), project->max.lon);
  xmlNewProp(node, BAD_CAST "lon", BAD_CAST str);

  if(project->map_state) {
    node = xmlNewChild(root_node, NULL, BAD_CAST "map", BAD_CAST NULL);
    g_ascii_dtostr(str, sizeof(str), project->map_state->zoom);
    xmlNewProp(node, BAD_CAST "zoom", BAD_CAST str);
    snprintf(str, sizeof(str), "%d", project->map_state->scroll_offset.x);
    xmlNewProp(node, BAD_CAST "scroll-offset-x", BAD_CAST str);
    snprintf(str, sizeof(str), "%d", project->map_state->scroll_offset.y);
    xmlNewProp(node, BAD_CAST "scroll-offset-y", BAD_CAST str);
  }

  node = xmlNewChild(root_node, NULL, BAD_CAST "wms", NULL);
  xmlNewProp(node, BAD_CAST "server", BAD_CAST project->wms_server);
  xmlNewProp(node, BAD_CAST "path", BAD_CAST project->wms_path);
  snprintf(str, sizeof(str), "%d", project->wms_offset.x);
  xmlNewProp(node, BAD_CAST "x-offset", BAD_CAST str);
  snprintf(str, sizeof(str), "%d", project->wms_offset.y);
  xmlNewProp(node, BAD_CAST "y-offset", BAD_CAST str);

  xmlSaveFormatFileEnc(project_file, doc, "UTF-8", 1);
  xmlFreeDoc(doc);
  xmlCleanupParser();

  g_free(project_file);

  return TRUE;
}

/* ------------ freeing projects --------------------- */

void project_free(project_t *project) {
  if(!project) return;

  if(project->name)       g_free(project->name);
  if(project->desc)       g_free(project->desc);
  if(project->server)     g_free(project->server);

  if(project->wms_server) g_free(project->wms_server);
  if(project->wms_path)   g_free(project->wms_path);
 
  if(project->path)       g_free(project->path);
  if(project->osm)        g_free(project->osm);

  map_state_free(project->map_state);

  g_free(project);
}

/* ------------ project selection dialog ------------- */

static char *project_fullname(settings_t *settings, const char *name) {
  return g_strdup_printf("%s%s/%s.proj", settings->base_path, name, name);
}

static gboolean project_exists(settings_t *settings, const char *name) {
  gboolean ok = FALSE;
  char *fulldir = g_strdup_printf("%s%s", settings->base_path, name);

  if(g_file_test(fulldir, G_FILE_TEST_IS_DIR)) {

    /* check for project file */
    char *fullname = project_fullname(settings, name);

    if(g_file_test(fullname, G_FILE_TEST_IS_REGULAR)) 
      ok = TRUE;

    g_free(fullname);
  }      
  g_free(fulldir);

  return ok;
}

static project_t *project_scan(appdata_t *appdata) {
  project_t *projects = NULL, **current = &projects;

  /* scan for projects */
  GDir *dir = g_dir_open(appdata->settings->base_path, 0, NULL);
  const char *name = NULL;
  do {
    if((name = g_dir_read_name(dir))) {
      if(project_exists(appdata->settings, name)) {
	printf("found project %s\n", name);
	
	/* try to read project and append it to chain */
	*current = g_new0(project_t, 1);
	(*current)->name = g_strdup(name);
	(*current)->path = g_strdup_printf("%s%s/", 
			  appdata->settings->base_path, name);
	
	char *fullname = project_fullname(appdata->settings, name);
	if(project_read(appdata, fullname, *current))
	  current = &((*current)->next);
	else {
	  g_free(*current);
	  *current = NULL;
	}
	g_free(fullname);
      }
    }
  } while(name);
  
  g_dir_close(dir);

  return projects;
}

typedef struct {
  project_t *project;
  GtkWidget *dialog, *view;
  GtkWidget *but_new, *but_edit, *but_remove;
  settings_t *settings;
#ifdef USE_HILDON
  dbus_mm_pos_t *mmpos;
  osso_context_t *osso_context;  
#endif
} select_context_t;

enum {
  PROJECT_COL_NAME = 0,
  PROJECT_COL_DESCRIPTION,
  PROJECT_COL_DATA,
  PROJECT_NUM_COLS
};

static void view_selected(select_context_t *context, project_t *project) {
  gtk_widget_set_sensitive(context->but_remove, project != NULL);
  gtk_widget_set_sensitive(context->but_edit, project != NULL);

  /* check if the selected project also has a valid osm file */
  gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog), 
      GTK_RESPONSE_ACCEPT, 
      project && g_file_test(project->osm, G_FILE_TEST_IS_REGULAR));
}

static gboolean
view_selection_func(GtkTreeSelection *selection, GtkTreeModel *model,
		     GtkTreePath *path, gboolean path_currently_selected,
		     gpointer userdata) {
  select_context_t *context = (select_context_t*)userdata;
  GtkTreeIter iter;
    
  if(gtk_tree_model_get_iter(model, &iter, path)) {
    project_t *project = NULL;
    gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &project, -1);
    g_assert(gtk_tree_path_get_depth(path) == 1);

    view_selected(context, project);
  }
  
  return TRUE; /* allow selection state to change */
}

/* get the currently selected project in the list, NULL if none */
static project_t *project_get_selected(GtkWidget *view) {
  project_t *project = NULL;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  GtkTreeSelection *selection = 
    gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
  g_assert(gtk_tree_selection_get_selected(selection, &model, &iter));
  gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &project, -1);

  return project;
}

/* ------------------------- create a new project ---------------------- */

/* returns true of str contains one of the characters in chars */
static gboolean strchrs(char *str, char *chars) {
  while(*chars) {
    char *p = str;
    while(*p) {
      if(*p == *chars)
	return TRUE;

      p++;
    }
    chars++;
  }
  return FALSE;
}

typedef struct {
  GtkWidget *dialog;
  settings_t *settings;
} name_callback_context_t;

static void callback_modified_name(GtkWidget *widget, gpointer data) {
  name_callback_context_t *context = (name_callback_context_t*)data;

  char *name = (char*)gtk_entry_get_text(GTK_ENTRY(widget));

  /* name must not contain some special chars */
  gboolean ok = FALSE;

  /* check if there's a name */
  if(name && strlen(name) > 0) {
    /* check if it consists of valid characters */
    if(!strchrs(name, "\\*?()\n\t\r")) {
      /* check if such a project already exists */
      if(!project_exists(context->settings, name))
	ok = TRUE;
    }
  }

  gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog), 
				    GTK_RESPONSE_ACCEPT, ok);
}


gboolean project_delete(select_context_t *context, project_t *project) {

  /* remove entire directory from disk */
  GDir *dir = g_dir_open(project->path, 0, NULL);
  const char *name = NULL;
  do {
    if((name = g_dir_read_name(dir))) {
      char *fullname = g_strdup_printf("%s/%s", project->path, name);
      g_remove(fullname);
      g_free(fullname);
    }
  } while(name);

  /* remove the projects directory */
  g_remove(project->path);

  /* remove from view */
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(context->view));
  gboolean deleted = FALSE;
  if(gtk_tree_model_get_iter_first(model, &iter)) {
    do {
      project_t *prj = NULL;
      gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &prj, -1);
      if(prj && (prj == project)) {
	printf("found %s to remove\n", prj->name);
	/* and remove from store */
	gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
	deleted = TRUE;
      }
    } while(!deleted && gtk_tree_model_iter_next(model, &iter));
  }

  /* de-chain entry from project list */
  project_t **project_list = &context->project;
  while(*project_list) {
    if(*project_list == project) 
      *project_list = (*project_list)->next;
    else
      project_list = &((*project_list)->next);
  }

  /* free project structure */
  project_free(project);

  /* disable edit/remove buttons */
  view_selected(context, NULL);

  return TRUE;
}

project_t *project_new(select_context_t *context) {
  printf("creating project with default values\n");

  /* --------------  first choose a name for the project --------------- */
  GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Project name"),
	  GTK_WINDOW(context->dialog), GTK_DIALOG_MODAL,
	  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
	  NULL);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 8);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), gtk_label_new(_("Name:")));

  name_callback_context_t name_context = { dialog, context->settings };
  GtkWidget *entry = gtk_entry_new();
  //  gtk_entry_set_text(GTK_ENTRY(entry), "<enter name>");
  gtk_box_pack_start_defaults(GTK_BOX(hbox), entry);
  g_signal_connect(G_OBJECT(entry), "changed",
		   G_CALLBACK(callback_modified_name), &name_context);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox);

  /* don't all user to click ok until something useful has been entered */
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), 
				    GTK_RESPONSE_ACCEPT, FALSE);

  gtk_widget_show_all(dialog);
  if(GTK_RESPONSE_ACCEPT != gtk_dialog_run(GTK_DIALOG(dialog))) {
    gtk_widget_destroy(dialog);
    return NULL;
  }

  project_t *project = g_new0(project_t, 1);
  project->name = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
  gtk_widget_destroy(dialog);


  project->path = g_strdup_printf("%s%s/", 
             context->settings->base_path, project->name);
  project->desc = g_strdup(_("<project description>"));

  /* no data downloaded yet */
  project->data_dirty = TRUE;

  /* use global server/access settings */
  project->server   = g_strdup(context->settings->server);
  
  /* dito for wms settings */
  project->wms_server = g_strdup(context->settings->wms_server);
  project->wms_path   = g_strdup(context->settings->wms_path);
  
  /* build project osm file name */
  project->osm = g_strdup_printf("%s%s.osm", project->path, project->name);

  /* around the castle in karlsruhe, germany ... */
  project->min.lat = 49.005;  project->min.lon = 8.3911;
  project->max.lat = 49.023;  project->max.lon = 8.4185;

  /* create project file on disk */
  project_save(context->dialog, project);

#ifdef USE_HILDON
  if(!project_edit(context->dialog, project, context->mmpos, 
		   context->osso_context))
#else
  if(!project_edit(context->dialog, project))
#endif
  {
    printf("edit cancelled!!\n");

    project_delete(context, project);

    project = NULL;
  }

  return project;
}

static void on_project_new(GtkButton *button, gpointer data) {
  select_context_t *context = (select_context_t*)data;
  project_t **project = &context->project;
  *project = project_new(context);
  if(*project) {

    GtkTreeModel *model = 
      gtk_tree_view_get_model(GTK_TREE_VIEW(context->view));

    GtkTreeIter iter;
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
		       PROJECT_COL_NAME,        (*project)->name,
		       PROJECT_COL_DESCRIPTION, (*project)->desc,
		       PROJECT_COL_DATA,        *project,
		       -1);

    GtkTreeSelection *selection = 
      gtk_tree_view_get_selection(GTK_TREE_VIEW(context->view));
    gtk_tree_selection_select_iter(selection, &iter);
  }
}

static void on_project_delete(GtkButton *button, gpointer data) {
  select_context_t *context = (select_context_t*)data;
  project_t *project = project_get_selected(context->view);

  char *str = g_strdup_printf(_("Do you really want to delete the "
				"project \"%s\"?"), project->name);
  GtkWidget *dialog = gtk_message_dialog_new(
	     GTK_WINDOW(context->dialog),
	     GTK_DIALOG_DESTROY_WITH_PARENT,
	     GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, str);
  g_free(str);

  gtk_window_set_title(GTK_WINDOW(dialog), _("Delete project?"));
      
  /* set the active flag again if the user answered "no" */
  if(GTK_RESPONSE_NO == gtk_dialog_run(GTK_DIALOG(dialog))) {
    gtk_widget_destroy(dialog);
    return;
  }

  gtk_widget_destroy(dialog);

  if(!project_delete(context, project)) 
    printf("unable to delete project\n");
}

static void on_project_edit(GtkButton *button, gpointer data) {
  select_context_t *context = (select_context_t*)data;
  project_t *project = project_get_selected(context->view);
  g_assert(project);
#ifdef USE_HILDON
  if(project_edit(context->dialog, project, 
		  context->mmpos, context->osso_context))
#else
  if(project_edit(context->dialog, project))
#endif
  {
    GtkTreeModel     *model;
    GtkTreeIter       iter;

    /* description may have changed, so update list */
    GtkTreeSelection *selection = 
      gtk_tree_view_get_selection(GTK_TREE_VIEW(context->view));
    g_assert(gtk_tree_selection_get_selected(selection, &model, &iter));

    //     gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &project, -1);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, 
		       PROJECT_COL_NAME, project->name, 
		       PROJECT_COL_DESCRIPTION, project->desc, 
		       -1);

    
  }
}

static GtkWidget *project_list_widget(select_context_t *context) {
  GtkWidget *vbox = gtk_vbox_new(FALSE,3);
  context->view = gtk_tree_view_new();

  gtk_tree_selection_set_select_function(
	 gtk_tree_view_get_selection(GTK_TREE_VIEW(context->view)), 
	 view_selection_func, 
	 context, NULL);

  /* --- "Name" column --- */
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(context->view),
	-1, _("Name"), renderer, "text", PROJECT_COL_NAME, NULL);

  /* --- "Description" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
	 _("Description"), renderer, "text", PROJECT_COL_DESCRIPTION, NULL);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(context->view), column, -1);

  /* build the store */
  GtkListStore *store = gtk_list_store_new(PROJECT_NUM_COLS, 
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

  GtkTreeIter iter;
  project_t *project = context->project;
  while(project) {

    /* Append a row and fill in some data */
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
	       PROJECT_COL_NAME,        project->name,
	       PROJECT_COL_DESCRIPTION, project->desc,
	       PROJECT_COL_DATA,        project,
	       -1);
    project = project->next;
  }
  
  gtk_tree_view_set_model(GTK_TREE_VIEW(context->view), GTK_TREE_MODEL(store));
  g_object_unref(store);

  /* put it into a scrolled window */
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), 
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), 
				      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scrolled_window), context->view);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), scrolled_window);

  /* ------- button box ------------ */

  GtkWidget *hbox = gtk_hbox_new(TRUE,3);
  context->but_new = gtk_button_new_with_label(_("New..."));
  gtk_box_pack_start_defaults(GTK_BOX(hbox), context->but_new);
  gtk_signal_connect(GTK_OBJECT(context->but_new), "clicked",
  		     GTK_SIGNAL_FUNC(on_project_new), context);

  context->but_edit = gtk_button_new_with_label(_("Edit..."));
  gtk_widget_set_sensitive(context->but_edit, FALSE);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), context->but_edit);
  gtk_signal_connect(GTK_OBJECT(context->but_edit), "clicked",
    	     GTK_SIGNAL_FUNC(on_project_edit), context);

  context->but_remove = gtk_button_new_with_label(_("Remove"));
  gtk_widget_set_sensitive(context->but_remove, FALSE);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), context->but_remove);
  gtk_signal_connect(GTK_OBJECT(context->but_remove), "clicked",
  		     GTK_SIGNAL_FUNC(on_project_delete), context);

  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  return vbox;
}

char *project_select(appdata_t *appdata) {
  char *name = NULL;

  select_context_t *context = g_new0(select_context_t, 1);
#ifdef USE_HILDON
  context->mmpos = &appdata->mmpos;
  context->osso_context = appdata->osso_context;
#endif
  context->settings = appdata->settings;
  context->project = project_scan(appdata);

  /* create project selection dialog */
  context->dialog = gtk_dialog_new_with_buttons(_("Project selection"),
	  GTK_WINDOW(appdata->window), GTK_DIALOG_MODAL,
	  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
          GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
          NULL);

#ifdef USE_HILDON
  gtk_window_set_default_size(GTK_WINDOW(context->dialog), 500, 300);
#else
  gtk_window_set_default_size(GTK_WINDOW(context->dialog), 400, 200);
#endif

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context->dialog)->vbox), 
			      project_list_widget(context));

  /* don't all user to click ok until something is selected */
  gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog), 
				    GTK_RESPONSE_ACCEPT, FALSE);

  gtk_widget_show_all(context->dialog);
  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context->dialog))) 
    name = g_strdup(project_get_selected(context->view)->name);

  gtk_widget_destroy(context->dialog);

  /* free all entries */
  project_t *project = context->project;
  while(project) {
    project_t *next = project->next;
    project_free(project);
    project = next;
  }

  g_free(context);

  return name;
}

/* ---------------------------------------------------- */

/* return file length or -1 on error */
static gsize file_length(char *name) {
  GMappedFile *gmap = g_mapped_file_new(name, FALSE, NULL);
  if(!gmap) return -1;
  gsize size = g_mapped_file_get_length(gmap); 
  g_mapped_file_free(gmap);
  return size;
}

void project_filesize(project_context_t *context) {
  char *str = NULL;

  printf("Checking size of %s\n", context->project->osm);

  if(!g_file_test(context->project->osm, G_FILE_TEST_IS_REGULAR)) {
    GdkColor color;
    gdk_color_parse("red", &color);
    gtk_widget_modify_fg(context->fsize, GTK_STATE_NORMAL, &color);

    str = g_strdup(_("Not downloaded!"));
    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog), 
				      GTK_RESPONSE_ACCEPT, 0);
  } else {
    gtk_widget_modify_fg(context->fsize, GTK_STATE_NORMAL, NULL);

    if(!context->project->data_dirty)
      str = g_strdup_printf(_("%d bytes present"), 
			    file_length(context->project->osm));
    else
      str = g_strdup_printf(_("Outdated, please download!"));

    /* project also must not be dirty to proceed */
    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog), 
			GTK_RESPONSE_ACCEPT, !context->project->data_dirty);
  }

  if(str) {
    gtk_label_set_text(GTK_LABEL(context->fsize), str); 
    g_free(str);
  }
}

void project_diffstat(project_context_t *context) {
  char *str = NULL;

  if(diff_present(context->project))
    str = g_strdup(_("present"));
  else
    str = g_strdup(_("not present"));

  gtk_label_set_text(GTK_LABEL(context->diff_stat), str); 
  g_free(str);
}

static void project_update(project_context_t *context) {

  /* fetch values from dialog */
  if(context->project->desc) g_free(context->project->desc);
  context->project->desc = g_strdup(gtk_entry_get_text(
                                       GTK_ENTRY(context->desc)));
  
  if(context->project->server) g_free(context->project->server);
  context->project->server = g_strdup(gtk_entry_get_text(
				       GTK_ENTRY(context->server)));
}

static void on_edit_clicked(GtkButton *button, gpointer data) {
  project_context_t *context = (project_context_t*)data;

  if(area_edit(&context->area_edit)) {
    printf("coordinates changed!!\n");

    pos_lon_label_set(context->minlat, context->project->min.lat);
    pos_lon_label_set(context->minlon, context->project->min.lon);
    pos_lon_label_set(context->maxlat, context->project->max.lat);
    pos_lon_label_set(context->maxlon, context->project->max.lon);
  }
}

static void on_download_clicked(GtkButton *button, gpointer data) {
  project_context_t *context = (project_context_t*)data;

  project_update(context);

  printf("download %s\n", context->project->osm);

  if(osm_download(context->dialog, context->project)) {
    context->project->data_dirty = FALSE;
    project_filesize(context);
  } else
    printf("download failed\n"); 
}

static void on_diff_remove_clicked(GtkButton *button, gpointer data) {
  project_context_t *context = (project_context_t*)data;

  printf("clicked diff remove\n");

  GtkWidget *dialog = gtk_message_dialog_new(
	     GTK_WINDOW(context->dialog),
	     GTK_DIALOG_DESTROY_WITH_PARENT,
	     GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
	       _("Do you really want to remove the diff file? This "
		 "will delete all changes you've made so far and which "
		 "you didn't upload yet."));
      
  gtk_window_set_title(GTK_WINDOW(dialog), _("Remove diff?"));
  
  /* set the active flag again if the user answered "no" */
  if(GTK_RESPONSE_YES == gtk_dialog_run(GTK_DIALOG(dialog))) {
    diff_remove(context->project);
    project_diffstat(context);
    gtk_widget_set_sensitive(context->diff_remove,  FALSE);
  }
  
  gtk_widget_destroy(dialog);
}

gboolean project_edit(GtkWidget *parent, project_t *project POS_PARM) {
  gboolean ok = FALSE;

  /* ------------ project edit dialog ------------- */
  
  project_context_t *context = g_new0(project_context_t, 1);
  context->project = project;
  
  context->area_edit.min = &project->min;
  context->area_edit.max = &project->max;
#ifdef USE_HILDON
  context->area_edit.mmpos = mmpos;
  context->area_edit.osso_context = osso_context;
#endif

  char *str = g_strdup_printf(_("Project - %s"), project->name);
  context->area_edit.parent = 
    context->dialog = gtk_dialog_new_with_buttons(str,
	  GTK_WINDOW(parent), GTK_DIALOG_MODAL,
	  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
          NULL);
  g_free(str);

#ifdef USE_HILDON
  /* making the dialog a little wider makes it less "crowded" */
  gtk_window_set_default_size(GTK_WINDOW(context->dialog), 640, 100);
#else
  gtk_window_set_default_size(GTK_WINDOW(context->dialog), 400, 100);
#endif

  GtkWidget *download, *label;
  GtkWidget *table = gtk_table_new(4, 6, FALSE);  // x, y

  label = gtk_label_new(_("Description:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 0, 1);
  context->desc = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(context->desc), project->desc);
  gtk_table_attach_defaults(GTK_TABLE(table),  context->desc, 1, 4, 0, 1);

  gtk_table_set_row_spacing(GTK_TABLE(table), 0, 4);

  label = gtk_label_new(_("Latitude"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 1, 2, 1, 2);
  label = gtk_label_new(_("Longitude"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 2, 3, 1, 2);

  label = gtk_label_new(_("Min:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 2, 3);
  context->minlat = pos_lat_label_new(project->min.lat);
  gtk_table_attach_defaults(GTK_TABLE(table), context->minlat, 1, 2, 2, 3);
  context->minlon = pos_lon_label_new(project->min.lon);
  gtk_table_attach_defaults(GTK_TABLE(table), context->minlon, 2, 3, 2, 3);

  label = gtk_label_new(_("Max:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 3, 4);
  context->maxlat = pos_lat_label_new(project->max.lat);
  gtk_table_attach_defaults(GTK_TABLE(table), context->maxlat, 1, 2, 3, 4);
  context->maxlon = pos_lon_label_new(project->max.lon);
  gtk_table_attach_defaults(GTK_TABLE(table), context->maxlon, 2, 3, 3, 4);

  GtkWidget *edit = gtk_button_new_with_label(_("Edit..."));
  gtk_signal_connect(GTK_OBJECT(edit), "clicked",
  		     (GtkSignalFunc)on_edit_clicked, context);
  gtk_table_attach(GTK_TABLE(table), edit, 3, 4, 2, 4, 
		   GTK_EXPAND | GTK_FILL,0,0,0);

  gtk_table_set_col_spacing(GTK_TABLE(table), 0, 4);
  gtk_table_set_row_spacing(GTK_TABLE(table), 3, 4);

  label = gtk_label_new(_("Server:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 4, 5);
  context->server = gtk_entry_new();
  HILDON_ENTRY_NO_AUTOCAP(context->server);
  gtk_entry_set_text(GTK_ENTRY(context->server), project->server);
  gtk_table_attach_defaults(GTK_TABLE(table),  context->server, 1, 4, 4, 5);

  label = gtk_label_new(_("OSM file:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 5, 6);
  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
  context->fsize = gtk_label_new(_(""));
  gtk_misc_set_alignment(GTK_MISC(context->fsize), 0.f, 0.5f);
  project_filesize(context);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), context->fsize);
  download = gtk_button_new_with_label(_("Download..."));
  gtk_signal_connect(GTK_OBJECT(download), "clicked",
		     (GtkSignalFunc)on_download_clicked, context);
  gtk_box_pack_start(GTK_BOX(hbox), download, FALSE, FALSE, 0);
  gtk_table_attach_defaults(GTK_TABLE(table), hbox, 1, 4, 5, 6);

  label = gtk_label_new(_("Diff file:"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 6, 7);
  hbox = gtk_hbox_new(FALSE, 0);
  context->diff_stat = gtk_label_new(_(""));
  gtk_misc_set_alignment(GTK_MISC(context->diff_stat), 0.f, 0.5f);
  project_diffstat(context);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), context->diff_stat);
  context->diff_remove = gtk_button_new_with_label(_("Remove..."));
  if(!diff_present(project)) 
    gtk_widget_set_sensitive(context->diff_remove,  FALSE);
  gtk_signal_connect(GTK_OBJECT(context->diff_remove), "clicked",
		     (GtkSignalFunc)on_diff_remove_clicked, context);
  gtk_box_pack_start(GTK_BOX(hbox), context->diff_remove, FALSE, FALSE, 0);
  gtk_table_attach_defaults(GTK_TABLE(table), hbox, 1, 4, 6, 7);
  

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context->dialog)->vbox), 
			      table);
  gtk_widget_show_all(context->dialog);

  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context->dialog))) {
    ok = TRUE;

    /* transfer values from edit dialog into project structure */
    project_update(context);

    /* save project */
    project_save(context->dialog, project);
  }

  gtk_widget_destroy(context->dialog);
  g_free(context);

  return ok;
}

gboolean project_open(appdata_t *appdata, char *name) {
  project_t *project = g_new0(project_t, 1);

  /* link to map state if a map already exists */
  if(appdata->map) {
    printf("Project: Using map state\n");
    project->map_state = appdata->map->state;
  } else {
    printf("Project: Creating new map_state\n");
    project->map_state = g_new0(map_state_t,1);	      
  }
  project->map_state->refcount++;	

  /* build project path */
  project->path = g_strdup_printf("%s%s/", 
		  appdata->settings->base_path, name);
  project->name = g_strdup(name);

  char *project_file = g_strdup_printf("%s%s.proj", project->path, name);

  printf("project file = %s\n", project_file);
  if(!g_file_test(project_file, G_FILE_TEST_IS_REGULAR)) {
    printf("requested project file doesn't exist\n");
    project_free(project);
    g_free(project_file);
    return FALSE;
  }

  if(!project_read(appdata, project_file, project)) {
    printf("error reading project file\n");
    project_free(project);
    g_free(project_file);
    return FALSE;
  }

  g_free(project_file);

  /* --------- project structure ok: load its OSM file --------- */
  appdata->project = project;

  printf("project_load: loading osm\n");
  appdata->osm = osm_parse(project->osm);
  if(!appdata->osm) {
    errorf(GTK_WIDGET(appdata->window), _("Parsing of OSM data failed!"));
    return FALSE;
  }

  printf("parsing ok\n");

  return TRUE;
}

gboolean project_close(appdata_t *appdata) {
  if(!appdata->project) return FALSE;
  
  printf("closing current project\n");

  /* redraw the entire map by destroying all map items and redrawing them */
  if(appdata->osm)
    diff_save(appdata->project, appdata->osm);

  map_clear(appdata, MAP_LAYER_ALL);

  if(appdata->osm) {
    osm_free(&appdata->icon, appdata->osm);
    appdata->osm = NULL;
  }

  project_free(appdata->project);
  appdata->project = NULL;

  return TRUE;
}

gboolean project_load(appdata_t *appdata, char *name) {
  char *proj_name = NULL;

  if(!name) {
    /* make user select a project */
    proj_name = project_select(appdata);
    if(!proj_name) {
      printf("no project selected\n");
      return FALSE;
    }
  } else
    proj_name = g_strdup(name);

  /* close current project */
  if(appdata->project) 
    project_close(appdata);

  /* open project itself */
  if(!project_open(appdata, proj_name)) {
    printf("error opening requested project\n");
    g_free(proj_name);
    return FALSE;
  }    

  g_free(proj_name);

  /* check if OSM data is valid */
  if(!osm_sanity_check(GTK_WIDGET(appdata->window), appdata->osm)) {
    printf("project/osm sanity checks failed, unloading project\n");
    project_free(appdata->project);
    return FALSE;
  }

  /* load diff possibly preset */
  diff_restore(appdata, appdata->project, appdata->osm);
  
  /* prepare colors etc, draw data and adjust scroll/zoom settings */
  map_init(appdata);

  /* restore a track */
  appdata->track.track = track_restore(appdata, appdata->project);
  if(appdata->track.track)
    map_track_draw(appdata->map, appdata->track.track);

  /* finally load a background if present */
  wms_load(appdata);

  /* save the name of the project for the perferences */
  if(appdata->settings->project)
    g_free(appdata->settings->project);
  appdata->settings->project = g_strdup(appdata->project->name);

  return TRUE;
}

