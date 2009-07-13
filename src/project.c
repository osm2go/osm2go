/*
 * Copyright (C) 2008-2009 Till Harbaum <till@harbaum.org>.
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

/*
 * TODO:
 * - don't allow user to delete active project. force/request close before
 */

#include "appdata.h"
#include "banner.h"

#include <sys/stat.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

/* there shouldn't be a reason to changes the servers url */
#undef SERVER_EDITABLE

typedef struct {
  appdata_t *appdata;
  project_t *project;
  settings_t *settings;
  GtkWidget *dialog, *fsize, *diff_stat, *diff_remove;
  GtkWidget *desc;
  GtkWidget *minlat, *minlon, *maxlat, *maxlon;
#ifdef SERVER_EDITABLE
  GtkWidget *server;
#endif
  area_edit_t area_edit;
} project_context_t;

static gboolean project_edit(appdata_t *appdata, GtkWidget *parent, 
		      settings_t *settings, project_t *project,
		      gboolean enable_cancel);


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
	      if((str = (char*)xmlGetProp(node, BAD_CAST "detail"))) {
		project->map_state->detail = g_ascii_strtod(str, NULL);
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
	      printf("osm = %s\n", str);

	      /* make this a relative path if possible */
	      /* if the project path actually is a prefix of this, */
	      /* then just remove this prefix */
	      if((str[0] == '/') &&
		 (strlen(str) > strlen(project->path)) &&
		 !strncmp(str, project->path, strlen(project->path))) {

		project->osm = g_strdup(str + strlen(project->path));
		printf("osm name converted to relative %s\n", project->osm);
	      } else
		project->osm = g_strdup(str);

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

  if(project->server)
    node = xmlNewChild(root_node, NULL, BAD_CAST "server", 
		       BAD_CAST project->server);

  xmlNewChild(root_node, NULL, BAD_CAST "desc", BAD_CAST project->desc);
  xmlNewChild(root_node, NULL, BAD_CAST "osm", BAD_CAST project->osm);

  node = xmlNewChild(root_node, NULL, BAD_CAST "min", NULL);
  g_ascii_formatd(str, sizeof(str), LL_FORMAT, project->min.lat);
  xmlNewProp(node, BAD_CAST "lat", BAD_CAST str);
  g_ascii_formatd(str, sizeof(str), LL_FORMAT, project->min.lon);
  xmlNewProp(node, BAD_CAST "lon", BAD_CAST str);

  node = xmlNewChild(root_node, NULL, BAD_CAST "max", NULL);
  g_ascii_formatd(str, sizeof(str), LL_FORMAT, project->max.lat);
  xmlNewProp(node, BAD_CAST "lat", BAD_CAST str);
  g_ascii_formatd(str, sizeof(str), LL_FORMAT, project->max.lon);
  xmlNewProp(node, BAD_CAST "lon", BAD_CAST str);

  if(project->map_state) {
    node = xmlNewChild(root_node, NULL, BAD_CAST "map", BAD_CAST NULL);
    g_ascii_formatd(str, sizeof(str), "%.04f", project->map_state->zoom);
    xmlNewProp(node, BAD_CAST "zoom", BAD_CAST str);
    g_ascii_formatd(str, sizeof(str), "%.04f", project->map_state->detail);
    xmlNewProp(node, BAD_CAST "detail", BAD_CAST str);
    snprintf(str, sizeof(str), "%d", project->map_state->scroll_offset.x);
    xmlNewProp(node, BAD_CAST "scroll-offset-x", BAD_CAST str);
    snprintf(str, sizeof(str), "%d", project->map_state->scroll_offset.y);
    xmlNewProp(node, BAD_CAST "scroll-offset-y", BAD_CAST str);
  }

  node = xmlNewChild(root_node, NULL, BAD_CAST "wms", NULL);
  if(project->wms_server)
    xmlNewProp(node, BAD_CAST "server", BAD_CAST project->wms_server);
  if(project->wms_path)
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

gboolean project_exists(settings_t *settings, const char *name) {
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
  appdata_t *appdata;
  project_t *project;
  GtkWidget *dialog, *list;
  settings_t *settings;
} select_context_t;

enum {
  PROJECT_COL_NAME = 0,
  PROJECT_COL_STATUS,
  PROJECT_COL_DESCRIPTION,
  PROJECT_COL_DATA,
  PROJECT_NUM_COLS
};

static gboolean osm_file_exists(char *path, char *name) {
  gboolean exists = FALSE;

  if(name[0] == '/')
    exists = g_file_test(name, G_FILE_TEST_IS_REGULAR);
  else {
    char *full = g_strjoin(NULL, path, name, NULL);
    exists = g_file_test(full, G_FILE_TEST_IS_REGULAR);
    g_free(full);
  }
  return exists;
}

static void view_selected(select_context_t *context, project_t *project) {
  list_button_enable(context->list, LIST_BUTTON_REMOVE, project != NULL);
  list_button_enable(context->list, LIST_BUTTON_EDIT, project != NULL);

  /* check if the selected project also has a valid osm file */
  gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog), 
      GTK_RESPONSE_ACCEPT, 
      project && osm_file_exists(project->path, project->osm));
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
static project_t *project_get_selected(GtkWidget *list) {
  project_t *project = NULL;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  GtkTreeSelection *selection = list_get_selection(list);
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
  GtkTreeModel *model = list_get_model(context->list);
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
  GtkWidget *dialog = 
    misc_dialog_new(MISC_DIALOG_NOSIZE,  _("Project name"),
		    GTK_WINDOW(context->dialog),
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

  /* adjust default server stored in settings if required */
  if(strstr(context->settings->server, "0.5") != NULL) {
    strstr(context->settings->server, "0.5")[2] = '6';
    printf("adjusting server path in settings to 0.6\n");
  }

  /* use global server/access settings */
  project->server   = g_strdup(context->settings->server);

  /* build project osm file name */
  project->osm = g_strdup_printf("%s.osm", project->name);

  /* around the castle in karlsruhe, germany ... */
  project->min.lat = 49.005;  project->min.lon = 8.3911;
  project->max.lat = 49.023;  project->max.lon = 8.4185;

  /* create project file on disk */
  project_save(context->dialog, project);

  if(!project_edit(context->appdata, context->dialog,  
		   context->settings, project, TRUE)) {
    printf("new/edit cancelled!!\n");

    project_delete(context, project);

    project = NULL;
  }

  /* enable/disable edit/remove buttons */
  view_selected(context, project);

  return project;
}

// predecs
void project_get_status_icon_stock_id(project_t *project, gchar **stock_id);

static void on_project_new(GtkButton *button, gpointer data) {
  select_context_t *context = (select_context_t*)data;
  project_t **project = &context->project;
  *project = project_new(context);
  if(*project) {

    GtkTreeModel *model = list_get_model(context->list);

    GtkTreeIter iter;
    gchar *status_stock_id = NULL;
    project_get_status_icon_stock_id(*project, &status_stock_id);
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
		       PROJECT_COL_NAME,        (*project)->name,
		       PROJECT_COL_STATUS,      status_stock_id,
		       PROJECT_COL_DESCRIPTION, (*project)->desc,
		       PROJECT_COL_DATA,        *project,
		       -1);

    GtkTreeSelection *selection = list_get_selection(context->list);
    gtk_tree_selection_select_iter(selection, &iter);
  }
}

static void on_project_delete(GtkButton *button, gpointer data) {
  select_context_t *context = (select_context_t*)data;
  project_t *project = project_get_selected(context->list);

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
  project_t *project = project_get_selected(context->list);
  g_assert(project);

  if(project_edit(context->appdata, context->dialog, 
		  context->settings, project, FALSE)) {
    GtkTreeModel     *model;
    GtkTreeIter       iter;

    /* description etc. may have changed, so update list */
    GtkTreeSelection *selection = list_get_selection(context->list);
    g_assert(gtk_tree_selection_get_selected(selection, &model, &iter));

    //     gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &project, -1);
    gchar *status_stock_id = NULL;
    project_get_status_icon_stock_id(project, &status_stock_id);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, 
		       PROJECT_COL_NAME, project->name, 
                       PROJECT_COL_STATUS, status_stock_id,
		       PROJECT_COL_DESCRIPTION, project->desc, 
		       -1);

    
    /* check if we have actually editing the currently open project */
    if(context->appdata->project && 
       !strcmp(context->appdata->project->name, project->name)) {
      project_t *cur = context->appdata->project;

      printf("edited project was actually the active one!\n");

      /* update the currently active project also */

      /* update description */
      if(cur->desc) { free(cur->desc); cur->desc = NULL; }
      if(project->desc) cur->desc = g_strdup(project->desc);

      /* update server */
      if(cur->server) { free(cur->server); cur->server = NULL; }
      if(project->server) cur->server = g_strdup(project->server);

      /* update coordinates */
      if((cur->min.lat != project->min.lat) ||
	 (cur->max.lat != project->max.lat) ||
	 (cur->min.lon != project->min.lon) ||
	 (cur->max.lon != project->max.lon)) {
	appdata_t *appdata = context->appdata;

	/* save modified coordinates */
	cur->min.lat = project->min.lat;
	cur->max.lat = project->max.lat;
	cur->min.lon = project->min.lon;
	cur->max.lon = project->max.lon;

	/* try to do this automatically */

	/* if we have valid osm data loaded: save state first */
	if(appdata->osm) {
	  /* redraw the entire map by destroying all map items  */
	  diff_save(appdata->project, appdata->osm);
	  map_clear(appdata, MAP_LAYER_OBJECTS_ONLY);
	  osm_free(&appdata->icon, appdata->osm);

	  appdata->osm = NULL;
	}

	/* and load the (hopefully) new file */
	appdata->osm = osm_parse(appdata->project->path, 
				 appdata->project->osm);
	diff_restore(appdata, appdata->project, appdata->osm);
	map_paint(appdata);
	
	main_ui_enable(appdata);
      }
    }
  }

  /* enable/disable edit/remove buttons */
  view_selected(context, project);
}


gboolean project_osm_present(project_t *project) {
  char *osm_name = g_strdup_printf("%s/%s.osm", project->path, project->name);
  gboolean is_present = g_file_test(osm_name, G_FILE_TEST_EXISTS);
  g_free(osm_name);
  return is_present;
}

void project_get_status_icon_stock_id(project_t *project, gchar **stock_id) {
    *stock_id = (! project_osm_present(project)) ? GTK_STOCK_DIALOG_WARNING
         : diff_present(project) ? GTK_STOCK_PROPERTIES
         : GTK_STOCK_FILE;
    // TODO: check for outdatedness too. Which icon to use?
}

static GtkWidget *project_list_widget(select_context_t *context) {
  context->list = list_new(LIST_HILDON_WITHOUT_HEADERS);

  list_set_selection_function(context->list, view_selection_func, context);

  list_set_columns(context->list,
	   _("Name"), PROJECT_COL_NAME, 0,
	   _("State"), PROJECT_COL_STATUS, LIST_FLAG_STOCK_ICON,
	   _("Description"), PROJECT_COL_DESCRIPTION, LIST_FLAG_ELLIPSIZE,
	   NULL);
		   

  /* build the store */
  GtkListStore *store = gtk_list_store_new(PROJECT_NUM_COLS, 
      G_TYPE_STRING,    // name
      G_TYPE_STRING,    // status
      G_TYPE_STRING,    // desc
      G_TYPE_POINTER);  // data

  GtkTreeIter iter;
  project_t *project = context->project;
  while(project) {
    gchar *status_stock_id = NULL;
    project_get_status_icon_stock_id(project, &status_stock_id);
    /* Append a row and fill in some data */
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
	       PROJECT_COL_NAME,        project->name,
               PROJECT_COL_STATUS,      status_stock_id,
	       PROJECT_COL_DESCRIPTION, project->desc,
	       PROJECT_COL_DATA,        project,
	       -1);
    project = project->next;
  }
  
  list_set_store(context->list, store);
  g_object_unref(store);

  list_set_static_buttons(context->list, TRUE, G_CALLBACK(on_project_new),
	  G_CALLBACK(on_project_edit), G_CALLBACK(on_project_delete), context);

  return context->list;
}

static char *project_select(appdata_t *appdata) {
  char *name = NULL;

  select_context_t *context = g_new0(select_context_t, 1);
  context->appdata = appdata;
  context->settings = appdata->settings;
  context->project = project_scan(appdata);

  /* create project selection dialog */
  context->dialog = 
    misc_dialog_new(MISC_DIALOG_MEDIUM,_("Project selection"),
	  GTK_WINDOW(appdata->window),
	  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
          GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
          NULL);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context->dialog)->vbox), 
			      project_list_widget(context));

  /* don't all user to click ok until something is selected */
  gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog), 
				    GTK_RESPONSE_ACCEPT, FALSE);

  gtk_widget_show_all(context->dialog);
  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context->dialog))) 
    name = g_strdup(project_get_selected(context->list)->name);

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
static gsize file_length(char *path, char *name) {
  char *str = NULL;

  if(name[0] == '/') str = g_strdup(name);
  else               str = g_strjoin(NULL, path, name, NULL);

  GMappedFile *gmap = g_mapped_file_new(str, FALSE, NULL);
  g_free(str);

  if(!gmap) return -1;
  gsize size = g_mapped_file_get_length(gmap); 
  g_mapped_file_free(gmap);
  return size;
}

void project_filesize(project_context_t *context) {
  char *str = NULL;

  printf("Checking size of %s\n", context->project->osm);

  if(!osm_file_exists(context->project->path, context->project->osm)) {
    GdkColor color;
    gdk_color_parse("red", &color);
    gtk_widget_modify_fg(context->fsize, GTK_STATE_NORMAL, &color);

    str = g_strdup(_("Not downloaded!"));
  } else {
    gtk_widget_modify_fg(context->fsize, GTK_STATE_NORMAL, NULL);

    if(!context->project->data_dirty)
      str = g_strdup_printf(_("%d bytes present"), 
			    file_length(context->project->path,
					context->project->osm));
    else
      str = g_strdup_printf(_("Outdated, please download!"));
  }

  if(str) {
    gtk_label_set_text(GTK_LABEL(context->fsize), str); 
    g_free(str);
  }
}

void project_diffstat(project_context_t *context) {
  char *str = NULL;

  if(diff_present(context->project)) {
    /* this should prevent the user from changing the area */
    str = g_strdup(_("unsaved changes pending"));
  } else
    str = g_strdup(_("no pending changes"));

  gtk_label_set_text(GTK_LABEL(context->diff_stat), str); 
  g_free(str);
}

static void on_edit_clicked(GtkButton *button, gpointer data) {
  project_context_t *context = (project_context_t*)data;

  if(diff_present(context->project)) {
    if(!yes_no_f(context->dialog, NULL, 0, 0,
		 _("Discard pending changes?"),
		 _("You have pending changes in this project. Changing "
		   "the area will discard these changes.\n\nDo you want to "
		   "discard all your changes?")))
      return;

    diff_remove(context->project);
    project_diffstat(context);
    gtk_widget_set_sensitive(context->diff_remove,  FALSE);
  }

  if(area_edit(&context->area_edit)) {
    printf("coordinates changed!!\n");

    pos_lon_label_set(context->minlat, context->project->min.lat);
    pos_lon_label_set(context->minlon, context->project->min.lon);
    pos_lon_label_set(context->maxlat, context->project->max.lat);
    pos_lon_label_set(context->maxlon, context->project->max.lon);

    /* (re-) download area */
    if(osm_download(GTK_WIDGET(context->dialog), 
		    context->appdata->settings, context->project))
       context->project->data_dirty = FALSE;
    
    project_filesize(context);
  }
}

static void on_download_clicked(GtkButton *button, gpointer data) {
  project_context_t *context = (project_context_t*)data;

  printf("download %s\n", context->project->osm);

  if(osm_download(context->dialog, context->settings, context->project)) {
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
	       _("Do you really want to discard your changes? This "
		 "permanently undo all changes you've made so far and which "
		 "you didn't upload yet."));
      
  gtk_window_set_title(GTK_WINDOW(dialog), _("Discard changes?"));
  
  /* set the active flag again if the user answered "no" */
  if(GTK_RESPONSE_YES == gtk_dialog_run(GTK_DIALOG(dialog))) {
    diff_remove(context->project);
    project_diffstat(context);
    gtk_widget_set_sensitive(context->diff_remove,  FALSE);
  }
  
  gtk_widget_destroy(dialog);
}

gboolean project_check_demo(GtkWidget *parent, project_t *project) {
  if(!project->server) 
    messagef(parent, "Demo project", 
	     "This is a preinstalled demo project. This means that the "
	     "basic project parameters cannot be changed and no data can "
	     "be up- or downloaded via the OSM servers.\n\n" 
	     "Please setup a new project to do these things.");
    
  return !project->server;
}

/* create a left aligned label (normal ones are centered) */
static GtkWidget *gtk_label_left_new(char *str) {
  GtkWidget *label = gtk_label_new(str);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, .5f);
  return label;
}

static gboolean 
project_edit(appdata_t *appdata, GtkWidget *parent, settings_t *settings, 
	     project_t *project, gboolean enable_cancel) {
  gboolean ok = FALSE;

  if(project_check_demo(parent, project))
    return ok;

  /* ------------ project edit dialog ------------- */
  
  project_context_t *context = g_new0(project_context_t, 1);
  context->appdata = appdata;
  context->project = project;
  context->area_edit.settings = context->settings = settings;
  
  context->area_edit.min = &project->min;
  context->area_edit.max = &project->max;
#ifdef USE_HILDON
  context->area_edit.mmpos = &appdata->mmpos;
  context->area_edit.osso_context = appdata->osso_context;
#endif

  /* cancel is enabled for "new" projects only */
  if(enable_cancel) {
    char *str = g_strdup_printf(_("New project - %s"), project->name);

    context->area_edit.parent = 
      context->dialog = misc_dialog_new(MISC_DIALOG_WIDE, str,
				GTK_WINDOW(parent),
				GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
				GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
    g_free(str);
  } else {
    char *str = g_strdup_printf(_("Edit project - %s"), project->name);

    context->area_edit.parent = 
      context->dialog = misc_dialog_new(MISC_DIALOG_WIDE, str,
				GTK_WINDOW(parent),
				GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    g_free(str);
  }

  GtkWidget *download, *label;
  GtkWidget *table = gtk_table_new(5, 5, FALSE);  // x, y
  gtk_table_set_col_spacing(GTK_TABLE(table), 0, 8);
  gtk_table_set_col_spacing(GTK_TABLE(table), 3, 8);

  label = gtk_label_left_new(_("Description:"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 0, 1);
  context->desc = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(context->desc), project->desc);
  gtk_table_attach_defaults(GTK_TABLE(table),  context->desc, 1, 4, 0, 1);
  gtk_table_set_row_spacing(GTK_TABLE(table), 0, 4);

  label = gtk_label_left_new(_("Latitude:"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 1, 2);
  context->minlat = pos_lat_label_new(project->min.lat);
  gtk_table_attach_defaults(GTK_TABLE(table), context->minlat, 1, 2, 1, 2);
  label = gtk_label_new(_("to"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 2, 3, 1, 2);
  context->maxlat = pos_lon_label_new(project->max.lat);
  gtk_table_attach_defaults(GTK_TABLE(table), context->maxlat, 3, 4, 1, 2);

  label = gtk_label_left_new(_("Longitude:"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 2, 3);
  context->minlon = pos_lat_label_new(project->min.lon);
  gtk_table_attach_defaults(GTK_TABLE(table), context->minlon, 1, 2, 2, 3);
  label = gtk_label_new(_("to"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 2, 3, 2, 3);
  context->maxlon = pos_lon_label_new(project->max.lon);
  gtk_table_attach_defaults(GTK_TABLE(table), context->maxlon, 3, 4, 2, 3);

  GtkWidget *edit = gtk_button_new_with_label(_("Edit"));
  gtk_signal_connect(GTK_OBJECT(edit), "clicked",
  		     (GtkSignalFunc)on_edit_clicked, context);
  gtk_table_attach(GTK_TABLE(table), edit, 4, 5, 1, 3, 
		   GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,0,0);

  gtk_table_set_row_spacing(GTK_TABLE(table), 2, 4);

#ifdef SERVER_EDITABLE
  label = gtk_label_left_new(_("Server:"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 3, 4);
  context->server = gtk_entry_new();
  HILDON_ENTRY_NO_AUTOCAP(context->server);
  gtk_entry_set_text(GTK_ENTRY(context->server), project->server);
  gtk_table_attach_defaults(GTK_TABLE(table),  context->server, 1, 4, 3, 4);

  gtk_table_set_row_spacing(GTK_TABLE(table), 3, 4);
#endif

  label = gtk_label_left_new(_("Map data:"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 4, 5);
  context->fsize = gtk_label_left_new(_(""));
  project_filesize(context);
  gtk_table_attach_defaults(GTK_TABLE(table), context->fsize, 1, 4, 4, 5);
  download = gtk_button_new_with_label(_("Download"));
  gtk_signal_connect(GTK_OBJECT(download), "clicked",
		     (GtkSignalFunc)on_download_clicked, context);
  gtk_table_attach_defaults(GTK_TABLE(table), download, 4, 5, 4, 5);

  gtk_table_set_row_spacing(GTK_TABLE(table), 4, 4);

  label = gtk_label_left_new(_("Changes:"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 5, 6);
  context->diff_stat = gtk_label_left_new(_(""));
  project_diffstat(context);
  gtk_table_attach_defaults(GTK_TABLE(table), context->diff_stat, 1, 4, 5, 6);
  context->diff_remove = gtk_button_new_with_label(_("Undo all"));
  if(!diff_present(project)) 
    gtk_widget_set_sensitive(context->diff_remove,  FALSE);
  gtk_signal_connect(GTK_OBJECT(context->diff_remove), "clicked",
		     (GtkSignalFunc)on_diff_remove_clicked, context);
  gtk_table_attach_defaults(GTK_TABLE(table), context->diff_remove, 4, 5, 5, 6);
  
  /* ---------------------------------------------------------------- */

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context->dialog)->vbox), 
			      table);
  gtk_widget_show_all(context->dialog);

  /* the return value may actually be != ACCEPT, but only if the editor */
  /* is run for a new project which is completely removed afterwards if */
  /* cancel has been selected */
  ok = (GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context->dialog)));

  /* transfer values from edit dialog into project structure */
  
  /* fetch values from dialog */
  if(context->project->desc) g_free(context->project->desc);
  context->project->desc = g_strdup(gtk_entry_get_text(
			      GTK_ENTRY(context->desc)));
#ifdef SERVER_EDITABLE
  if(context->project->server) g_free(context->project->server);
  context->project->server = g_strdup(gtk_entry_get_text(
				       GTK_ENTRY(context->server)));
#endif

  /* save project */
  project_save(context->dialog, project);

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
    project->map_state = map_state_new();	      
  }

  map_state_reset(project->map_state);
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

  printf("project_open: loading osm %s\n", project->osm);
  appdata->osm = osm_parse(project->path, project->osm);
  if(!appdata->osm) {
    printf("OSM parsing failed\n");
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

  /* Save track and turn off the handler callback */
  track_save(appdata->project, appdata->track.track);
  track_clear(appdata, appdata->track.track);
  appdata->track.track = NULL;

  map_clear(appdata, MAP_LAYER_ALL);

  if(appdata->osm) {
    osm_free(&appdata->icon, appdata->osm);
    appdata->osm = NULL;
  }

  /* update project file on disk */
  project_save(GTK_WIDGET(appdata->window), appdata->project);

  project_free(appdata->project);
  appdata->project = NULL;

  return TRUE;
}

#define _PROJECT_LOAD_BUF_SIZ 64

gboolean project_load(appdata_t *appdata, char *name) {
  char *proj_name = NULL;

  if(!name) {
    /* make user select a project */
    proj_name = project_select(appdata);
    if(!proj_name) {
      printf("no project selected\n");
      return FALSE;
    }
  }
  else {
    proj_name = g_strdup(name);
  }

  char banner_txt[_PROJECT_LOAD_BUF_SIZ];
  memset(banner_txt, 0, _PROJECT_LOAD_BUF_SIZ);

  snprintf(banner_txt, _PROJECT_LOAD_BUF_SIZ, _("Loading %s"), proj_name);
  banner_busy_start(appdata, TRUE, banner_txt);

  /* close current project */
  banner_busy_tick();
  if(appdata->project) 
    project_close(appdata);

  /* open project itself */
  banner_busy_tick();
  if(!project_open(appdata, proj_name)) {
    printf("error opening requested project\n");

    if(appdata->project) {
      project_free(appdata->project);
      appdata->project = NULL;
    }

    if(appdata->osm) {
      osm_free(&appdata->icon, appdata->osm);
      appdata->osm = NULL;
    }

    snprintf(banner_txt, _PROJECT_LOAD_BUF_SIZ, 
	     _("Error opening %s"), proj_name);
    banner_busy_stop(appdata);
    banner_show_info(appdata, banner_txt);

    g_free(proj_name);
    return FALSE;
  }    

  /* check if OSM data is valid */
  banner_busy_tick();
  if(!osm_sanity_check(GTK_WIDGET(appdata->window), appdata->osm)) {
    printf("project/osm sanity checks failed, unloading project\n");

    if(appdata->project) {
      project_free(appdata->project);
      appdata->project = NULL;
    }

    if(appdata->osm) {
      osm_free(&appdata->icon, appdata->osm);
      appdata->osm = NULL;
    }

    snprintf(banner_txt, _PROJECT_LOAD_BUF_SIZ, 
	     _("Error opening %s"), proj_name);
    banner_busy_stop(appdata);
    banner_show_info(appdata, banner_txt);

    g_free(proj_name);
    return FALSE;
  }

  /* load diff possibly preset */
  banner_busy_tick();
  diff_restore(appdata, appdata->project, appdata->osm);

  /* prepare colors etc, draw data and adjust scroll/zoom settings */
  banner_busy_tick();
  map_init(appdata);

  /* restore a track */
  banner_busy_tick();
  appdata->track.track = track_restore(appdata, appdata->project);
  if(appdata->track.track)
    map_track_draw(appdata->map, appdata->track.track);

  /* finally load a background if present */
  banner_busy_tick();
  wms_load(appdata);

  /* save the name of the project for the perferences */
  if(appdata->settings->project)
    g_free(appdata->settings->project);
  appdata->settings->project = g_strdup(appdata->project->name);

  banner_busy_stop(appdata);

#if 0
  snprintf(banner_txt, _PROJECT_LOAD_BUF_SIZ, _("Loaded %s"), proj_name);
  banner_show_info(appdata, banner_txt);
#endif

  statusbar_set(appdata, NULL, 0);

  g_free(proj_name);
  return TRUE;
}

/* ------------------- project setup wizard ----------------- */

struct wizard_s;

typedef struct wizard_page_s {
  const gchar *title;
  GtkWidget* (*setup)(struct wizard_page_s *page);
  void (*update)(struct wizard_page_s *page);
  GtkAssistantPageType type;
  gboolean complete;
  /* everything before here is initialized statically */

  struct wizard_s *wizard;
  GtkWidget *widget;
  gint index;

  union {
    struct {
      GtkWidget *check[3];
      GtkWidget *label[3];
    } source_selection;

  } state;

} wizard_page_t;

typedef struct wizard_s {
  gboolean running;

  int page_num;
  wizard_page_t *page;
  appdata_t *appdata;
  guint handler_id;
  GtkWidget *assistant;
} wizard_t;


static gint on_assistant_destroy(GtkWidget *widget, wizard_t *wizard) {
  printf("destroy callback\n");
  wizard->running = FALSE;
  return FALSE;
}

static void on_assistant_cancel(GtkWidget *widget, wizard_t *wizard) {
  printf("cancel callback\n");
  wizard->running = FALSE;
}

static void on_assistant_close(GtkWidget *widget, wizard_t *wizard) {
  printf("close callback\n");
  wizard->running = FALSE;
}

static GtkWidget *wizard_text(const char *text) {
  GtkTextBuffer *buffer = gtk_text_buffer_new(NULL);
  gtk_text_buffer_set_text(buffer, text, -1);

#ifndef USE_HILDON_TEXT_VIEW
  GtkWidget *view = gtk_text_view_new_with_buffer(buffer);
#else
  GtkWidget *view = hildon_text_view_new();
  hildon_text_view_set_buffer(HILDON_TEXT_VIEW(view), buffer);
#endif

  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 2 );
  gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 2 );
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE );

  return view;
}

/* ---------------- page 1: intro ----------------- */
static GtkWidget *wizard_create_intro_page(wizard_page_t *page) {
  static const char *text = 
    "This wizard will guide you through the setup of a new project.\n\n"
    "An osm2go project covers a certain area of the world as seen "
    "by openstreetmap.org. The wizard will help you downloading "
    "the data describing that area and will enable you to make changes "
    "to it using osm2go.";

  return wizard_text(text);
}

/* ---------------- page 2: source selection ----------------- */
static gboolean gtk_widget_get_sensitive(GtkWidget *widget) {
  GValue is_sensitive= { 0, };
  g_value_init(&is_sensitive, G_TYPE_BOOLEAN);
  g_object_get_property(G_OBJECT(widget), "sensitive", &is_sensitive);
  return g_value_get_boolean(&is_sensitive);
}

static void wizard_update_source_selection_page(wizard_page_t *page) {

  gboolean gps_on = page->wizard->appdata && 
    page->wizard->appdata->settings && 
    page->wizard->appdata->settings->enable_gps;
  gboolean gps_fix = gps_on && gps_get_pos(page->wizard->appdata, NULL, NULL);

  gtk_widget_set_sensitive(page->state.source_selection.check[0], gps_fix);
  if(gps_fix) 
    gtk_label_set_text(GTK_LABEL(page->state.source_selection.label[0]),
		       "(GPS has a valid position)");
  else if(gps_on)
    gtk_label_set_text(GTK_LABEL(page->state.source_selection.label[0]),
		       "(GPS has no valid position)");
  else
    gtk_label_set_text(GTK_LABEL(page->state.source_selection.label[0]),
		       "(GPS is disabled)");

#ifndef USE_HILDON
  gtk_widget_set_sensitive(page->state.source_selection.check[1], FALSE);
  gtk_label_set_text(GTK_LABEL(page->state.source_selection.label[1]),
		       "(Maemo Mapper not available)");

#endif

  /* check if the user selected something that is actually selectable */
  /* only allow him to continue then */
  gboolean sel_ok = FALSE;
  int i;
  for(i=0;i<3;i++) {
    if(gtk_toggle_button_get_active(
	    GTK_TOGGLE_BUTTON(page->state.source_selection.check[i])))
      sel_ok = gtk_widget_get_sensitive(page->state.source_selection.check[i]);
  }

  /* set page to "completed" if a valid entry is selected */
  gtk_assistant_set_page_complete(
               GTK_ASSISTANT(page->wizard->assistant), page->widget, sel_ok);
}

/* the user has changed the selected source, update dialog */
static void on_wizard_source_selection_toggled(GtkToggleButton *togglebutton,
					    gpointer user_data) {
  if(gtk_toggle_button_get_active(togglebutton)) 
    wizard_update_source_selection_page((wizard_page_t*)user_data);
}

static GtkWidget *wizard_create_source_selection_page(wizard_page_t *page) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start_defaults(GTK_BOX(vbox), 
	      wizard_text("Please choose how to determine the area you "
			  "are planning to work on."));

  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
  GtkWidget *vbox2 = gtk_vbox_new(FALSE, 0);

  /* add selection buttons */
  int i;
  for(i=0;i<3;i++) {
    static const char *labels[] = {
      "Use current GPS position",
      "Get from Maemo Mapper",
      "Specify area manually"
    };

    page->state.source_selection.check[i] = 
      gtk_radio_button_new_with_label_from_widget(
	      i?GTK_RADIO_BUTTON(page->state.source_selection.check[0]):NULL, 
	      _(labels[i]));
    g_signal_connect(G_OBJECT(page->state.source_selection.check[i]), 
	"toggled", G_CALLBACK(on_wizard_source_selection_toggled), page);
    gtk_box_pack_start(GTK_BOX(vbox2), page->state.source_selection.check[i], 
		       TRUE, TRUE, 2);
    page->state.source_selection.label[i] = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(vbox2), page->state.source_selection.label[i], 
		       TRUE, TRUE, 2);
  }

  gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, FALSE, 0); 
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0); 
  return vbox;
}

/* this is called once a second while the wizard is running and can be used */
/* to update pages etc */
static gboolean wizard_update(gpointer data) {
  wizard_t *wizard = (wizard_t*)data;
  gint page = gtk_assistant_get_current_page(GTK_ASSISTANT(wizard->assistant));

  if(wizard->page[page].update)
 ;   //    wizard->page[page].update(&wizard->page[page]);
  else
    printf("nothing to animate on page %d\n", page);

  return TRUE;
}

void project_wizard(appdata_t *appdata) {
  wizard_page_t page[] = {
    { "Introduction",           wizard_create_intro_page, NULL,
      GTK_ASSISTANT_PAGE_INTRO,    TRUE},
    { "Area source selection",  wizard_create_source_selection_page,
      wizard_update_source_selection_page,
      GTK_ASSISTANT_PAGE_CONTENT,  FALSE},
    { "Click the Check Button", NULL, NULL,
      GTK_ASSISTANT_PAGE_CONTENT,  FALSE},
    { "Click the Button",       NULL, NULL,
      GTK_ASSISTANT_PAGE_PROGRESS, FALSE},
    { "Confirmation",           NULL, NULL,
      GTK_ASSISTANT_PAGE_CONFIRM,  TRUE},
  };
  
  wizard_t wizard = {
    TRUE,

    /* the pages themselves */
    sizeof(page) / sizeof(wizard_page_t), page,
    appdata, 0, NULL
  };

  wizard.assistant = gtk_assistant_new();
  gtk_widget_set_size_request(wizard.assistant, 450, 300);

  /* Add five pages to the GtkAssistant dialog. */
  int i;
  for (i = 0; i < wizard.page_num; i++) {
    wizard.page[i].wizard = &wizard;

    if(wizard.page[i].setup)
      wizard.page[i].widget = 
	wizard.page[i].setup(&wizard.page[i]);
    else {
      char *str = g_strdup_printf("Page %d", i);
      wizard.page[i].widget = gtk_label_new(str);
      g_free(str);
    }

    page[i].index = gtk_assistant_append_page(GTK_ASSISTANT(wizard.assistant),
					      wizard.page[i].widget);

    gtk_assistant_set_page_title(GTK_ASSISTANT(wizard.assistant),
                                  wizard.page[i].widget, wizard.page[i].title);
    gtk_assistant_set_page_type(GTK_ASSISTANT(wizard.assistant),
                                  wizard.page[i].widget, wizard.page[i].type);

    /* Set the introduction and conclusion pages as complete so they can be
     * incremented or closed. */
    gtk_assistant_set_page_complete(GTK_ASSISTANT(wizard.assistant),
                     wizard.page[i].widget, wizard.page[i].complete);

    if(wizard.page[i].update)
      wizard.page[i].update(&wizard.page[i]);
  }

  /* install handler for timed updates */
  wizard.handler_id = gtk_timeout_add(1000, wizard_update, &wizard);

  /* make it a modal subdialog of the main window */
  gtk_window_set_modal(GTK_WINDOW(wizard.assistant), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(wizard.assistant), 
			       GTK_WINDOW(appdata->window));

  gtk_widget_show_all(wizard.assistant);

  g_signal_connect(G_OBJECT(wizard.assistant), "destroy",
		   G_CALLBACK(on_assistant_destroy), &wizard);

  g_signal_connect(G_OBJECT(wizard.assistant), "cancel",
		   G_CALLBACK(on_assistant_cancel), &wizard);

  g_signal_connect(G_OBJECT(wizard.assistant), "close",
		   G_CALLBACK(on_assistant_close), &wizard);

  do {
    if(gtk_events_pending()) 
      gtk_main_iteration();
    else 
      usleep(1000);

  } while(wizard.running);

  gtk_timeout_remove(wizard.handler_id);

  gtk_widget_destroy(wizard.assistant);
}


// vim:et:ts=8:sw=2:sts=2:ai
