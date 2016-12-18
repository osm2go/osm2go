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

#include "project.h"

#include "appdata.h"
#include "area_edit.h"
#include "banner.h"
#include "diff.h"
#include "gps.h"
#include "list.h"
#include "misc.h"
#include "osm_api.h"
#include "statusbar.h"
#include "track.h"
#include "wms.h"

#include <glib/gstdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

typedef struct {
  project_t *project;
  settings_t *settings;
  GtkWidget *dialog, *fsize, *diff_stat, *diff_remove;
  GtkWidget *desc, *download;
  GtkWidget *minlat, *minlon, *maxlat, *maxlon;
  gboolean is_new;
#ifdef SERVER_EDITABLE
  GtkWidget *server;
#endif
  area_edit_t area_edit;
} project_context_t;

static gboolean project_edit(appdata_t *appdata, GtkWidget *parent,
                             project_t *project, gboolean is_new);


/* ------------ project file io ------------- */

static gboolean project_read(const char *project_file, project_t *project,
                             const char *defaultserver) {
  xmlDoc *doc = NULL;

  /* parse the file and get the DOM */
  if((doc = xmlReadFile(project_file, NULL, 0)) == NULL) {
    printf("error: could not parse file %s\n", project_file);
    return FALSE;
  }

  xmlNode *cur_node;
  for (cur_node = xmlDocGetRootElement(doc); cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcmp((char*)cur_node->name, "proj") == 0) {
        project->data_dirty = xml_get_prop_is(cur_node, "dirty", "true");

	xmlNode *node = cur_node->children;

	while(node != NULL) {
	  if(node->type == XML_ELEMENT_NODE) {
            xmlChar *str;

	    if(strcmp((char*)node->name, "desc") == 0) {
	      project->desc = xmlNodeListGetString(doc, node->children, 1);
	      printf("desc = %s\n", project->desc);

	    } else if(strcmp((char*)node->name, "server") == 0) {
	      str = xmlNodeListGetString(doc, node->children, 1);
              if(g_strcmp0(defaultserver, str) == 0) {
                project->server = defaultserver;
              } else {
                project->rserver = g_strdup(str);
                project->server = project->rserver;
              }
	      printf("server = %s\n", project->server);
	      xmlFree(str);

	    } else if(project->map_state &&
		      strcmp((char*)node->name, "map") == 0) {

	      if((str = xmlGetProp(node, BAD_CAST "zoom"))) {
		project->map_state->zoom = g_ascii_strtod(str, NULL);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "detail"))) {
		project->map_state->detail = g_ascii_strtod(str, NULL);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "scroll-offset-x"))) {
		project->map_state->scroll_offset.x = strtoul(str, NULL, 10);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "scroll-offset-y"))) {
		project->map_state->scroll_offset.y = strtoul(str, NULL, 10);
		xmlFree(str);
	      }

	    } else if(strcmp((char*)node->name, "wms") == 0) {

	      if((str = xmlGetProp(node, BAD_CAST "server"))) {
		project->wms_server = g_strdup(str);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "path"))) {
		project->wms_path = g_strdup(str);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "x-offset"))) {
		project->wms_offset.x = strtoul(str, NULL, 10);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "y-offset"))) {
		project->wms_offset.y = strtoul(str, NULL, 10);
		xmlFree(str);
	      }

	    } else if(strcmp((char*)node->name, "osm") == 0) {
	      str = xmlNodeListGetString(doc, node->children, 1);
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

	    } else if(strcmp((char*)node->name, "min") == 0) {
	      if((str = xmlGetProp(node, BAD_CAST "lat"))) {
		project->min.lat = g_ascii_strtod(str, NULL);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "lon"))) {
		project->min.lon = g_ascii_strtod(str, NULL);
		xmlFree(str);
	      }

	    } else if(strcmp((char*)node->name, "max") == 0) {
	      if((str = xmlGetProp(node, BAD_CAST "lat"))) {
		project->max.lat = g_ascii_strtod(str, NULL);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "lon"))) {
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

  return TRUE;
}

gboolean project_save(GtkWidget *parent, project_t *project) {
  char str[32];
  char *project_file = g_strconcat(project->path, project->name,
				 ".proj", NULL);

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

  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr node, root_node = xmlNewNode(NULL, BAD_CAST "proj");
  xmlNewProp(root_node, BAD_CAST "name", BAD_CAST project->name);
  if(project->data_dirty)
    xmlNewProp(root_node, BAD_CAST "dirty", BAD_CAST "true");

  xmlDocSetRootElement(doc, root_node);

  if(project->server)
    xmlNewChild(root_node, NULL, BAD_CAST "server",
		BAD_CAST project->server);

  if(project->desc)
    xmlNewChild(root_node, NULL, BAD_CAST "desc", project->desc);

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

  g_free(project_file);

  return TRUE;
}

/* ------------ freeing projects --------------------- */

void project_free(project_t *project) {
  if(!project) return;

  g_free(project->name);
  xmlFree(project->desc);
  g_free(project->rserver);

  g_free(project->wms_server);
  g_free(project->wms_path);

  g_free(project->path);
  g_free(project->osm);

  map_state_free(project->map_state);

  g_free(project);
}

/* ------------ project selection dialog ------------- */

/**
 * @brief check if a project with the given name exists
 * @param settings settings
 * @param name project name
 * @param filename where to store absolute filename if project exists or NULL
 * @returns if project exists
 */
gboolean project_exists(settings_t *settings, const char *name, gchar **filename) {
  gboolean ok = FALSE;
  gchar *fullname = g_strconcat(settings->base_path, name, "/", name, ".proj", NULL);

  /* check for project file */
  if(g_file_test(fullname, G_FILE_TEST_IS_REGULAR)) {
    if(filename) {
      *filename = fullname;
      fullname = NULL;
    }
    ok = TRUE;
  }

  g_free(fullname);

  return ok;
}

static GSList *project_scan(appdata_t *appdata) {
  GSList *projects = NULL;

  /* scan for projects */
  GDir *dir = g_dir_open(appdata->settings->base_path, 0, NULL);
  const char *name = NULL;
  while((name = g_dir_read_name(dir)) != NULL) {
    gchar *fullname;
    if(project_exists(appdata->settings, name, &fullname)) {
      printf("found project %s\n", name);

      /* try to read project and append it to chain */
      project_t *n = g_new0(project_t, 1);
      n->name = g_strdup(name);
      n->path = g_strconcat(appdata->settings->base_path, name, "/", NULL);

      if(project_read(fullname, n, appdata->settings->server))
        projects = g_slist_prepend(projects, n);
      else
        project_free(n);
      g_free(fullname);
    }
  }

  g_dir_close(dir);

  return projects;
}

typedef struct {
  appdata_t *appdata;
  GSList *projects;
  GtkWidget *dialog, *list;
} select_context_t;

enum {
  PROJECT_COL_NAME = 0,
  PROJECT_COL_STATUS,
  PROJECT_COL_DESCRIPTION,
  PROJECT_COL_DATA,
  PROJECT_NUM_COLS
};

static gboolean osm_file_exists(const project_t *project) {
  gboolean exists;

  if(project->name[0] == '/')
    exists = g_file_test(project->name, G_FILE_TEST_IS_REGULAR);
  else {
    gchar *full = g_strconcat(project->path, project->osm, NULL);
    exists = g_file_test(full, G_FILE_TEST_IS_REGULAR);
    g_free(full);
  }
  return exists;
}

static void view_selected(select_context_t *context, project_t *project) {
  /* check if the selected project also has a valid osm file */
  gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
      GTK_RESPONSE_ACCEPT,
      project && osm_file_exists(project));
}

static void
changed(GtkTreeSelection *selection, gpointer userdata) {
  select_context_t *context = (select_context_t*)userdata;

  GtkTreeModel *model = NULL;
  GtkTreeIter iter;

  gboolean sel = gtk_tree_selection_get_selected(selection, &model, &iter);
  if(sel) {
    project_t *project = NULL;
    gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &project, -1);

    view_selected(context, project);
  }

  list_button_enable(GTK_WIDGET(context->list), LIST_BUTTON_REMOVE, sel);
  list_button_enable(GTK_WIDGET(context->list), LIST_BUTTON_EDIT, sel);
}

/* get the currently selected project in the list, NULL if none */
static project_t *project_get_selected(GtkWidget *list) {
  project_t *project = NULL;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  g_assert(list_get_selected(list, &model, &iter));
  gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &project, -1);

  return project;
}

/* ------------------------- create a new project ---------------------- */

typedef struct {
  GtkWidget *dialog;
  settings_t *settings;
} name_callback_context_t;

static void callback_modified_name(GtkWidget *widget, gpointer data) {
  name_callback_context_t *context = (name_callback_context_t*)data;

  const gchar *name = gtk_entry_get_text(GTK_ENTRY(widget));

  /* name must not contain some special chars */
  gboolean ok = FALSE;

  /* check if there's a name */
  if(name && strlen(name) > 0) {
    /* check if it consists of valid characters */
    if(strpbrk(name, "\\*?()\n\t\r") == NULL) {
      /* check if such a project already exists */
      if(!project_exists(context->settings, name, NULL))
	ok = TRUE;
    }
  }

  gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
				    GTK_RESPONSE_ACCEPT, ok);
}


static gboolean project_delete(select_context_t *context, project_t *project) {

  printf("deleting project \"%s\"\n", project->name);

  /* check if we are to delete the currently open project */
  if(context->appdata->project &&
     !strcmp(context->appdata->project->name, project->name)) {

    if(!yes_no_f(context->dialog, NULL, 0, 0,
		 _("Delete current project?"),
		 _("The project you are about to delete is the one "
		   "you are currently working on!\n\n"
		   "Do you want to delete it anyway?")))
      return FALSE;

    project_close(context->appdata);
  }

  /* remove entire directory from disk */
  GDir *dir = g_dir_open(project->path, 0, NULL);
  const char *name = NULL;
  do {
    if((name = g_dir_read_name(dir))) {
      char *fullname = g_strjoin("/", project->path, name, NULL);
      g_remove(fullname);
      g_free(fullname);
    }
  } while(name);
  g_dir_close(dir);

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
  context->projects = g_slist_remove(context->projects, project);

  /* free project structure */
  project_free(project);

  /* disable ok button button */
  view_selected(context, NULL);

  return TRUE;
}

static project_t *project_new(select_context_t *context) {
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

  name_callback_context_t name_context = { dialog, context->appdata->settings };
  GtkWidget *entry = entry_new();
  gtk_box_pack_start_defaults(GTK_BOX(hbox), entry);
  g_signal_connect(G_OBJECT(entry), "changed",
		   G_CALLBACK(callback_modified_name), &name_context);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox);

  /* don't allow user to click ok until a valid area has been specified */
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


  project->path = g_strconcat(
             context->appdata->settings->base_path, project->name, "/", NULL);
  project->desc = NULL;

  /* no data downloaded yet */
  project->data_dirty = TRUE;

  /* use global server/access settings */
  project->server   = g_strdup(context->appdata->settings->server);

  /* build project osm file name */
  project->osm = g_strconcat(project->name, ".osm", NULL);

  project->min.lat = NAN;  project->min.lon = NAN;
  project->max.lat = NAN;  project->max.lon = NAN;

  /* create project file on disk */
  if(!project_save(context->dialog, project)) {
    project_delete(context, project);

    project = NULL;
  } else if(!project_edit(context->appdata, context->dialog, project, TRUE)) {
    printf("new/edit cancelled!!\n");

    project_delete(context, project);

    project = NULL;
  }

  /* enable/disable edit/remove buttons */
  view_selected(context, project);

  return project;
}

/**
 * @brief check if OSM data is present for the given project
 * @param project the project to check
 * @return if OSM data file was found
 */
static gboolean project_osm_present(const project_t *project) {
  char *osm_name = g_strconcat(project->path, "/", project->name, ".osm", NULL);
  gboolean is_present = g_file_test(osm_name, G_FILE_TEST_EXISTS);
  g_free(osm_name);
  return is_present;
}

/**
 * @brief get icon for the given project
 * @param current the currently active project or NULL
 * @param project the project to check
 * @return the stock identifier
 */
static const gchar *
project_get_status_icon_stock_id(const project_t *current,
                                 const project_t *project) {
  /* is this the currently open project? */
  if(current && !strcmp(current->name, project->name))
    return GTK_STOCK_OPEN;
  else if(!project_osm_present(project))
    return GTK_STOCK_DIALOG_WARNING;
  else if(diff_present(project))
    return GTK_STOCK_PROPERTIES;
  else
    return GTK_STOCK_FILE;

    // TODO: check for outdatedness too. Which icon to use?
}

static void on_project_new(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  select_context_t *context = (select_context_t*)data;
  project_t *project = project_new(context);
  context->projects = g_slist_prepend(context->projects, project);
  if(project) {

    GtkTreeModel *model = list_get_model(context->list);

    GtkTreeIter iter;
    const gchar *status_stock_id = project_get_status_icon_stock_id(
                                         context->appdata->project, project);
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
		       PROJECT_COL_NAME,        project->name,
		       PROJECT_COL_STATUS,      status_stock_id,
		       PROJECT_COL_DESCRIPTION, project->desc,
		       PROJECT_COL_DATA,        project,
		       -1);

    GtkTreeSelection *selection = list_get_selection(context->list);
    gtk_tree_selection_select_iter(selection, &iter);
  }
}

static void on_project_delete(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  select_context_t *context = (select_context_t*)data;
  project_t *project = project_get_selected(context->list);

  gboolean r = yes_no_f(context->dialog, NULL, 0, 0, _("Delete project?"),
                        _("Do you really want to delete the "
                          "project \"%s\"?"), project->name);
  if (!r)
    return;

  if(!project_delete(context, project))
    printf("unable to delete project\n");
}

static void on_project_edit(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  select_context_t *context = (select_context_t*)data;
  project_t *project = project_get_selected(context->list);
  g_assert(project);

  if(project_edit(context->appdata, context->dialog, project, FALSE)) {
    GtkTreeModel     *model;
    GtkTreeIter       iter;

    /* description etc. may have changed, so update list */
    GtkTreeSelection *selection = list_get_selection(context->list);
    g_assert(gtk_tree_selection_get_selected(selection, &model, &iter));

    //     gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &project, -1);
    const gchar *status_stock_id = project_get_status_icon_stock_id(
                                         context->appdata->project, project);
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
      if(project->desc) cur->desc = xmlStrdup(project->desc);

      /* update server */
      if(cur->rserver) {
        g_free(cur->rserver);
        cur->rserver = NULL;
      }
      if(g_strcmp0(project->server, context->appdata->settings->server) == 0) {
        cur->server = context->appdata->settings->server;
      } else {
        cur->rserver = g_strdup(project->server);
        cur->server = cur->rserver;
      }

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
	  map_clear(appdata, MAP_LAYER_ALL);

	  osm_free(appdata->osm);
	  appdata->osm = NULL;
	}

	/* and load the (hopefully) new file */
	appdata->osm = osm_parse(appdata->project->path,
                                 appdata->project->osm,
                                 &appdata->icon);
	diff_restore(appdata, appdata->project, appdata->osm);
	map_paint(appdata);

	main_ui_enable(appdata);
      }
    }
  }

  /* enable/disable edit/remove buttons */
  view_selected(context, project);
}

static void
on_project_update_all(G_GNUC_UNUSED GtkButton *button, gpointer data)
{
  select_context_t *context = (select_context_t*)data;
  GtkTreeIter iter;
  GtkTreeModel *model = list_get_model(context->list);
  if(gtk_tree_model_get_iter_first(model, &iter)) {
    do {
      project_t *prj = NULL;
      gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &prj, -1);
      /* if the project was already downloaded do it again */
      if(prj && project_osm_present(prj)) {
        printf("found %s to update\n", prj->name);
        if (!osm_download(GTK_WIDGET(context->dialog),
                     context->appdata->settings, prj))
          break;
      }
    } while(gtk_tree_model_iter_next(model, &iter));
  }
}

/**
 * @brief create a widget to list the projects
 * @param context the context struct
 * @param has_sel if an item has been selected
 */
static GtkWidget *project_list_widget(select_context_t *context, gboolean *has_sel) {
  context->list = list_new(LIST_HILDON_WITHOUT_HEADERS);

  list_override_changed_event(context->list, changed, context);

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

  GtkTreeIter iter, seliter;
  GSList *cur;
  gboolean check_pos;
  pos_t pos;
  *has_sel = FALSE;

  check_pos = gps_get_pos(context->appdata, &pos, NULL);

  /* there are too many context variables, so no foreach here */
  for(cur = context->projects; cur; cur = g_slist_next(cur)) {
    project_t *project = (project_t *)cur->data;
    const gchar *status_stock_id = project_get_status_icon_stock_id(
                                         context->appdata->project, project);
    /* Append a row and fill in some data */
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
	       PROJECT_COL_NAME,        project->name,
               PROJECT_COL_STATUS,      status_stock_id,
	       PROJECT_COL_DESCRIPTION, project->desc,
	       PROJECT_COL_DATA,        project,
	       -1);

    /* decide if to select this project because it matches the current position */
    if(check_pos && osm_position_within_bounds_ll(&project->min, &project->max, &pos)) {
      seliter = iter;
      *has_sel = TRUE;
      check_pos = FALSE;
    }
  }

  list_set_store(context->list, store);
  g_object_unref(store);

  list_set_static_buttons(context->list, LIST_BTN_NEW | LIST_BTN_WIDE | LIST_BTN_WIDE4,
	  G_CALLBACK(on_project_new), G_CALLBACK(on_project_edit),
	  G_CALLBACK(on_project_delete), context);

  list_set_user_buttons(context->list,
			LIST_BUTTON_USER0, _("Update all"), on_project_update_all,
			0);

  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
                                       PROJECT_COL_NAME, GTK_SORT_ASCENDING);

  if(*has_sel)
    list_scroll(context->list, &seliter);

  return context->list;
}

#if GLIB_CHECK_VERSION(2,28,0)
static void project_free_notify(gpointer data) {
#else
static void project_free_notify(gpointer data, gpointer user_data) {
#endif
  project_free((project_t*)data);
}

static char *project_select(appdata_t *appdata) {
  char *name = NULL;

  select_context_t context = { 0 };
  context.appdata = appdata;
  context.projects = project_scan(appdata);

  /* create project selection dialog */
  context.dialog =
    misc_dialog_new(MISC_DIALOG_MEDIUM,_("Project selection"),
	  GTK_WINDOW(appdata->window),
	  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
          GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
          NULL);

  /* under fremantle the dialog does not have an "Open" button */
  /* as it's closed when a project is being selected */
  gtk_dialog_set_default_response(GTK_DIALOG(context.dialog),
				  GTK_RESPONSE_ACCEPT);

  gboolean has_sel;
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
			      project_list_widget(&context, &has_sel));

  /* don't all user to click ok until something is selected */
  gtk_dialog_set_response_sensitive(GTK_DIALOG(context.dialog),
				    GTK_RESPONSE_ACCEPT, has_sel);

  gtk_widget_show_all(context.dialog);
  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context.dialog)))
    name = g_strdup(project_get_selected(context.list)->name);

  gtk_widget_destroy(context.dialog);

  /* free all entries */
#if GLIB_CHECK_VERSION(2,28,0)
  g_slist_free_full(context.projects, project_free_notify);
#else
  g_slist_foreach(context.projects, project_free_notify, NULL);
  g_slist_free(context.projects);
#endif

  return name;
}

/* ---------------------------------------------------- */
  /* no idea if that check is correct, but this way it works both for the N900 and my desktop */
#if !GLIB_CHECK_VERSION(2,24,2)
typedef struct stat GStatBuf;
#endif

/* return file length or -1 on error */
static GStatBuf file_info(const project_t *project) {
  GStatBuf st;
  int r;

  if (project->osm[0] == '/') {
    r = g_stat(project->osm, &st);
  } else {
    char *str = g_strconcat(project->path, project->osm, NULL);
    r = g_stat(str, &st);
    g_free(str);
  }

  if(r != 0)
    memset(&st, 0, sizeof(st));

  return st;
}

static void project_filesize(project_context_t *context) {
  char *str = NULL;
  gchar *gstr = NULL;
  const project_t * const project = context->project;

  printf("Checking size of %s\n", project->osm);

  if(!osm_file_exists(project)) {
    GdkColor color;
    gdk_color_parse("red", &color);
    gtk_widget_modify_fg(context->fsize, GTK_STATE_NORMAL, &color);

    str = _("Not downloaded!");

    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
				      GTK_RESPONSE_ACCEPT, !context->is_new);

  } else {
    gtk_widget_modify_fg(context->fsize, GTK_STATE_NORMAL, NULL);

    if(!project->data_dirty) {
      GStatBuf st = file_info(project);
      struct tm loctime;
      localtime_r(&st.st_mtim.tv_sec, &loctime);
      char time_str[32];
      strftime(time_str, sizeof(time_str), "%x %X", &loctime);

      gstr = g_strdup_printf(_("%"G_GOFFSET_FORMAT" bytes present\nfrom %s"),
			    (goffset)st.st_size, time_str);
      str = gstr;
    } else
      str = _("Outdated, please download!");

    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
		      GTK_RESPONSE_ACCEPT, !context->is_new ||
				      !project->data_dirty);
  }

  if(str) {
    gtk_label_set_text(GTK_LABEL(context->fsize), str);
    g_free(gstr);
  }
}

/* a project may currently be open. "unsaved changes" then also */
/* means that the user may have unsaved changes */
static gboolean project_active_n_dirty(project_context_t *context) {

  if(!context->area_edit.appdata->osm) return FALSE;

  if(context->area_edit.appdata->project &&
     !strcmp(context->area_edit.appdata->project->name,
	     context->project->name)) {

    printf("editing the currently open project\n");

    return(!diff_is_clean(context->area_edit.appdata->osm, TRUE));
  }

  return FALSE;
}

void project_diffstat(project_context_t *context) {
  const char *str;

  if(diff_present(context->project) || project_active_n_dirty(context)) {
    /* this should prevent the user from changing the area */
    str = _("unsaved changes pending");
  } else
    str = _("no pending changes");

  gtk_label_set_text(GTK_LABEL(context->diff_stat), str);
}

static gboolean
project_pos_is_valid(project_t *project) {
  return(!isnan(project->min.lat) &&
	 !isnan(project->min.lon) &&
	 !isnan(project->max.lat) &&
	 !isnan(project->max.lon));
}

static void on_edit_clicked(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  project_context_t *context = (project_context_t*)data;
  project_t * const project = context->project;

  if(diff_present(project) || project_active_n_dirty(context))
    messagef(context->dialog,
	     _("Pending changes"),
	     _("You have pending changes in this project.\n\n"
	       "Changing the area may cause pending changes to be "
	       "lost if they are outside the updated area."));

  if(area_edit(&context->area_edit)) {
    printf("coordinates changed!!\n");

    /* the wms layer isn't usable with new coordinates */
    wms_remove_file(project);

    pos_lon_label_set(context->minlat, project->min.lat);
    pos_lon_label_set(context->minlon, project->min.lon);
    pos_lon_label_set(context->maxlat, project->max.lat);
    pos_lon_label_set(context->maxlon, project->max.lon);

    gboolean pos_valid = project_pos_is_valid(project);
    gtk_widget_set_sensitive(context->download, pos_valid);

    /* (re-) download area */
    if (pos_valid)
    {
      if(osm_download(GTK_WIDGET(context->dialog),
	      context->area_edit.appdata->settings, project))
         project->data_dirty = FALSE;
    }
    project_filesize(context);
  }
}

static void on_download_clicked(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  project_context_t *context = (project_context_t*)data;
  project_t * const project = context->project;

  printf("download %s\n", project->osm);

  if(osm_download(context->dialog, context->settings, project))
    project->data_dirty = FALSE;
  else
    printf("download failed\n");

  project_filesize(context);
}

static void on_diff_remove_clicked(G_GNUC_UNUSED GtkButton *button, gpointer data) {
  project_context_t *context = (project_context_t*)data;
  const project_t * const project = context->project;

  printf("clicked diff remove\n");

  if(yes_no_f(context->dialog, NULL, 0, 0, _("Discard changes?"),
	      _("Do you really want to discard your changes? This will "
		"permanently undo all changes you have made so far and which "
		"you did not upload yet."))) {
    appdata_t *appdata = context->area_edit.appdata;

    diff_remove(project);

    /* if this is the currently open project, we need to undo */
    /* the map changes as well */

    if(appdata->project &&
       !strcmp(appdata->project->name, project->name)) {

      printf("undo all on current project: delete map changes as well\n");

      /* just reload the map */
      map_clear(appdata, MAP_LAYER_OBJECTS_ONLY);
      osm_free(appdata->osm);
      appdata->osm = osm_parse(appdata->project->path, appdata->project->osm, &appdata->icon);
      map_paint(appdata);
    }

    /* update button/label state */
    project_diffstat(context);
    gtk_widget_set_sensitive(context->diff_remove,  FALSE);
  }
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
project_edit(appdata_t *appdata, GtkWidget *parent,
	     project_t *project, gboolean is_new) {
  gboolean ok = FALSE;

  if(project_check_demo(parent, project))
    return ok;

  /* ------------ project edit dialog ------------- */

  project_context_t context = { 0 };
  context.project = project;
  context.area_edit.settings = context.settings = appdata->settings;
  context.area_edit.appdata = appdata;
  context.is_new = is_new;
  context.area_edit.min = &project->min;
  context.area_edit.max = &project->max;

  /* cancel is enabled for "new" projects only */
  if(is_new) {
    char *str = g_strdup_printf(_("New project - %s"), project->name);

    context.area_edit.parent =
      context.dialog = misc_dialog_new(MISC_DIALOG_WIDE, str,
				GTK_WINDOW(parent),
				GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
				GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
    g_free(str);
  } else {
    char *str = g_strdup_printf(_("Edit project - %s"), project->name);

    context.area_edit.parent =
      context.dialog = misc_dialog_new(MISC_DIALOG_WIDE, str,
				GTK_WINDOW(parent),
				GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
    g_free(str);
  }

  gtk_dialog_set_default_response(GTK_DIALOG(context.dialog),
				  GTK_RESPONSE_ACCEPT);

  GtkWidget *label;
  GtkWidget *table = gtk_table_new(5, 5, FALSE);  // x, y
  gtk_table_set_col_spacing(GTK_TABLE(table), 0, 8);
  gtk_table_set_col_spacing(GTK_TABLE(table), 3, 8);

  label = gtk_label_left_new(_("Description:"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 0, 1);
  context.desc = entry_new();
  gtk_entry_set_activates_default(GTK_ENTRY(context.desc), TRUE);
  if(project->desc)
    gtk_entry_set_text(GTK_ENTRY(context.desc), (gchar*)project->desc);
  gtk_table_attach_defaults(GTK_TABLE(table),  context.desc, 1, 5, 0, 1);
  gtk_table_set_row_spacing(GTK_TABLE(table), 0, 4);

  label = gtk_label_left_new(_("Latitude:"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 1, 2);
  context.minlat = pos_lat_label_new(project->min.lat);
  gtk_table_attach_defaults(GTK_TABLE(table), context.minlat, 1, 2, 1, 2);
  label = gtk_label_new(_("to"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 2, 3, 1, 2);
  context.maxlat = pos_lon_label_new(project->max.lat);
  gtk_table_attach_defaults(GTK_TABLE(table), context.maxlat, 3, 4, 1, 2);

  label = gtk_label_left_new(_("Longitude:"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 2, 3);
  context.minlon = pos_lat_label_new(project->min.lon);
  gtk_table_attach_defaults(GTK_TABLE(table), context.minlon, 1, 2, 2, 3);
  label = gtk_label_new(_("to"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 2, 3, 2, 3);
  context.maxlon = pos_lon_label_new(project->max.lon);
  gtk_table_attach_defaults(GTK_TABLE(table), context.maxlon, 3, 4, 2, 3);

  GtkWidget *edit = button_new_with_label(_("Edit"));
  gtk_signal_connect(GTK_OBJECT(edit), "clicked",
  		     (GtkSignalFunc)on_edit_clicked, &context);
  gtk_table_attach(GTK_TABLE(table), edit, 4, 5, 1, 3,
		   GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,0,0);

  gtk_table_set_row_spacing(GTK_TABLE(table), 2, 4);

#ifdef SERVER_EDITABLE
  label = gtk_label_left_new(_("Server:"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 3, 4);
  context.server = entry_new();
  gtk_entry_set_activates_default(GTK_ENTRY(context.server), TRUE);
  HILDON_ENTRY_NO_AUTOCAP(context.server);
  gtk_entry_set_text(GTK_ENTRY(context.server), project->server);
  gtk_table_attach_defaults(GTK_TABLE(table),  context.server, 1, 4, 3, 4);

  gtk_table_set_row_spacing(GTK_TABLE(table), 3, 4);
#endif

  label = gtk_label_left_new(_("Map data:"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 4, 5);
  context.fsize = gtk_label_left_new(_(""));
  project_filesize(&context);
  gtk_table_attach_defaults(GTK_TABLE(table), context.fsize, 1, 4, 4, 5);
  context.download = button_new_with_label(_("Download"));
  gtk_signal_connect(GTK_OBJECT(context.download), "clicked",
		     (GtkSignalFunc)on_download_clicked, &context);
  gtk_widget_set_sensitive(context.download, project_pos_is_valid(project));

  gtk_table_attach_defaults(GTK_TABLE(table), context.download, 4, 5, 4, 5);

  gtk_table_set_row_spacing(GTK_TABLE(table), 4, 4);

  label = gtk_label_left_new(_("Changes:"));
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 5, 6);
  context.diff_stat = gtk_label_left_new(_(""));
  project_diffstat(&context);
  gtk_table_attach_defaults(GTK_TABLE(table), context.diff_stat, 1, 4, 5, 6);
  context.diff_remove = button_new_with_label(_("Undo all"));
  if(!diff_present(project) && !project_active_n_dirty(&context))
    gtk_widget_set_sensitive(context.diff_remove,  FALSE);
  gtk_signal_connect(GTK_OBJECT(context.diff_remove), "clicked",
		     (GtkSignalFunc)on_diff_remove_clicked, &context);
  gtk_table_attach_defaults(GTK_TABLE(table), context.diff_remove, 4, 5, 5, 6);

  /* ---------------------------------------------------------------- */

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
			      table);

  /* disable "ok" if there's no valid file downloaded */
  if(is_new)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(context.dialog),
		    GTK_RESPONSE_ACCEPT,
		    osm_file_exists(project));

  gtk_widget_show_all(context.dialog);

  /* the return value may actually be != ACCEPT, but only if the editor */
  /* is run for a new project which is completely removed afterwards if */
  /* cancel has been selected */
  ok = (GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context.dialog)));

  /* transfer values from edit dialog into project structure */

  /* fetch values from dialog */
  xmlFree(project->desc);
  const gchar *ndesc = gtk_entry_get_text(GTK_ENTRY(context.desc));
  if(ndesc && strlen(ndesc))
    project->desc = xmlStrdup(BAD_CAST ndesc);
  else
    project->desc = NULL;

#ifdef SERVER_EDITABLE
  g_free(context.project->server);
  context.project->server = g_strdup(gtk_entry_get_text(
				       GTK_ENTRY(context.server)));
  if(strlen(context.project->server) == 0) {
    g_free(context.project->server);
    context.project->server = NULL;
  }
#endif

  /* save project */
  project_save(context.dialog, project);

  gtk_widget_destroy(context.dialog);

  return ok;
}

gboolean project_open(appdata_t *appdata, const char *name) {
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
  project->path = g_strconcat(
		  appdata->settings->base_path, name, "/", NULL);
  project->name = g_strdup(name);

  char *project_file = g_strconcat(project->path, name, ".proj", NULL);

  printf("project file = %s\n", project_file);
  if(!g_file_test(project_file, G_FILE_TEST_IS_REGULAR)) {
    printf("requested project file doesn't exist\n");
    project_free(project);
    g_free(project_file);
    return FALSE;
  }

  if(!project_read(project_file, project, appdata->settings->server)) {
    printf("error reading project file\n");
    project_free(project);
    g_free(project_file);
    return FALSE;
  }

  g_free(project_file);

  /* --------- project structure ok: load its OSM file --------- */
  appdata->project = project;

  printf("project_open: loading osm %s\n", project->osm);
  appdata->osm = osm_parse(project->path, project->osm, &appdata->icon);
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
  track_clear(appdata);

  map_clear(appdata, MAP_LAYER_ALL);

  if(appdata->osm) {
    osm_free(appdata->osm);
    appdata->osm = NULL;
  }

  /* remember in settings that no project is open */
  if(appdata->settings->project)
  {
    g_free(appdata->settings->project);
    appdata->settings->project = NULL;
  }

  /* update project file on disk */
  project_save(GTK_WIDGET(appdata->window), appdata->project);

  project_free(appdata->project);
  appdata->project = NULL;

  return TRUE;
}

#define _PROJECT_LOAD_BUF_SIZ 64

gboolean project_load(appdata_t *appdata, const char *name) {
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
      osm_free(appdata->osm);
      appdata->osm = NULL;
    }

    snprintf(banner_txt, _PROJECT_LOAD_BUF_SIZ,
	     _("Error opening %s"), proj_name);
    banner_busy_stop(appdata);
    banner_show_info(appdata, banner_txt);

    g_free(proj_name);
    return FALSE;
  }

  if(!appdata->window) {
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
      osm_free(appdata->osm);
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
  if(!appdata->window) goto fail;

  diff_restore(appdata, appdata->project, appdata->osm);

  /* prepare colors etc, draw data and adjust scroll/zoom settings */
  banner_busy_tick();
  if(!appdata->window) goto fail;

  map_init(appdata);

  /* restore a track */
  banner_busy_tick();
  if(!appdata->window) goto fail;

  track_clear(appdata);
  if(track_restore(appdata))
    map_track_draw(appdata->map, appdata->track.track);

  /* finally load a background if present */
  banner_busy_tick();
  if(!appdata->window) goto fail;
  wms_load(appdata);

  /* save the name of the project for the perferences */
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

 fail:
  printf("project loading interrupted by user\n");

  if(appdata->project) {
    project_free(appdata->project);
    appdata->project = NULL;
  }

  if(appdata->osm) {
    osm_free(appdata->osm);
    appdata->osm = NULL;
  }

  g_free(proj_name);

  return FALSE;
}
