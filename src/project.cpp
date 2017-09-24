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
#include "map.h"
#include "misc.h"
#include "osm_api.h"
#include "osm2go_platform.h"
#include "settings.h"
#include "statusbar.h"
#include "track.h"
#include "wms.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include <osm2go_cpp.h>
#include "osm2go_stl.h"

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

struct project_context_t {
  explicit project_context_t(appdata_t &a, project_t *p, gboolean n, const std::vector<project_t *> &j, GtkWidget *dlg);
  project_t * const project;
  settings_t * const settings;
  GtkWidget * const dialog;
  GtkWidget * const fsizehdr, * const fsize, * const diff_stat, * const diff_remove;
  GtkWidget * const desc, * const download;
  GtkWidget * const minlat, * const minlon, * const maxlat, * const maxlon;
  const gboolean is_new;
#ifdef SERVER_EDITABLE
  GtkWidget * const server;
#endif
  area_edit_t area_edit;
  const std::vector<project_t *> &projects;

  bool active_n_dirty() const;
};

/* create a left aligned label (normal ones are centered) */
static GtkWidget *gtk_label_left_new(const char *str = O2G_NULLPTR) {
  GtkWidget *label = gtk_label_new(str);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, .5f);
  return label;
}

static GtkWidget *pos_lat_label_new(pos_float_t lat) {
  char str[32];
  pos_lat_str(str, sizeof(str), lat);
  return gtk_label_new(str);
}

static GtkWidget *pos_lon_label_new(pos_float_t lon) {
  char str[32];
  pos_lon_str(str, sizeof(str), lon);
  return gtk_label_new(str);
}

project_context_t::project_context_t(appdata_t &a, project_t *p, gboolean n,
                                     const std::vector<project_t *> &j, GtkWidget *dlg)
  : project(p)
  , settings(a.settings)
  , dialog(dlg)
  , fsizehdr(gtk_label_left_new(_("Map data:")))
  , fsize(gtk_label_left_new())
  , diff_stat(gtk_label_left_new())
  , diff_remove(button_new_with_label(_("Undo all")))
  , desc(entry_new())
  , download(button_new_with_label(_("Download")))
  , minlat(pos_lat_label_new(project->min.lat))
  , minlon(pos_lon_label_new(project->min.lon))
  , maxlat(pos_lat_label_new(project->max.lat))
  , maxlon(pos_lon_label_new(project->max.lon))
  , is_new(n)
#ifdef SERVER_EDITABLE
  , server(entry_new())
#endif
  , area_edit(a, project->min, project->max, dlg)
  , projects(j)
{
}

struct select_context_t {
  select_context_t(appdata_t &a, GtkWidget *dial);
  ~select_context_t();

  appdata_t &appdata;
  map_state_t dummystate;
  std::vector<project_t *> projects;
  GtkWidget * const dialog;
  GtkWidget *list;
  GtkListStore *store;
};

static void pos_lat_label_set(GtkWidget *label, pos_float_t lat) {
  char str[32];
  pos_lat_str(str, sizeof(str), lat);
  gtk_label_set_text(GTK_LABEL(label), str);
}

static void pos_lon_label_set(GtkWidget *label, pos_float_t lon) {
  char str[32];
  pos_lon_str(str, sizeof(str), lon);
  gtk_label_set_text(GTK_LABEL(label), str);
}

static bool project_edit(select_context_t *scontext,
                             project_t *project, gboolean is_new);

/* ------------ project file io ------------- */

static std::string project_filename(const project_t *project) {
  return project->path + project->name + ".proj";
}

static bool project_read(const std::string &project_file, project_t *project,
                             const std::string &defaultserver) {
  xmlDoc *doc = xmlReadFile(project_file.c_str(), O2G_NULLPTR, XML_PARSE_NONET);

  /* parse the file and get the DOM */
  if(doc == O2G_NULLPTR) {
    printf("error: could not parse file %s\n", project_file.c_str());
    return false;
  }

  xmlNode *cur_node;
  for (cur_node = xmlDocGetRootElement(doc); cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcmp(reinterpret_cast<const char *>(cur_node->name), "proj") == 0) {
        project->data_dirty = xml_get_prop_is(cur_node, "dirty", "true");
        project->isDemo = xml_get_prop_is(cur_node, "demo", "true");

	xmlNode *node = cur_node->children;

	while(node != O2G_NULLPTR) {
	  if(node->type == XML_ELEMENT_NODE) {
            xmlChar *str;

	    if(strcmp(reinterpret_cast<const char *>(node->name), "desc") == 0) {
              xmlChar *desc = xmlNodeListGetString(doc, node->children, 1);
              project->desc = reinterpret_cast<char *>(desc);
              printf("desc = %s\n", desc);
              xmlFree(desc);
	    } else if(strcmp(reinterpret_cast<const char *>(node->name), "server") == 0) {
	      str = xmlNodeListGetString(doc, node->children, 1);
              if(defaultserver == reinterpret_cast<char *>(str)) {
                project->server = defaultserver.c_str();
              } else {
                project->rserver = reinterpret_cast<char *>(str);
                project->server = project->rserver.c_str();
              }
	      printf("server = %s\n", project->server);
	      xmlFree(str);

            } else if(strcmp(reinterpret_cast<const char *>(node->name), "map") == 0) {

	      if((str = xmlGetProp(node, BAD_CAST "zoom"))) {
                project->map_state.zoom = g_ascii_strtod(reinterpret_cast<gchar *>(str), O2G_NULLPTR);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "detail"))) {
                project->map_state.detail = g_ascii_strtod(reinterpret_cast<gchar *>(str), O2G_NULLPTR);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "scroll-offset-x"))) {
                project->map_state.scroll_offset.x = strtoul(reinterpret_cast<gchar *>(str), O2G_NULLPTR, 10);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "scroll-offset-y"))) {
                project->map_state.scroll_offset.y = strtoul(reinterpret_cast<gchar *>(str), O2G_NULLPTR, 10);
		xmlFree(str);
	      }

            } else if(strcmp(reinterpret_cast<const char *>(node->name), "wms") == 0) {

	      if((str = xmlGetProp(node, BAD_CAST "server"))) {
		project->wms_server = reinterpret_cast<char *>(str);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "path"))) {
		project->wms_path = reinterpret_cast<char *>(str);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "x-offset"))) {
                project->wms_offset.x = strtoul(reinterpret_cast<char *>(str), O2G_NULLPTR, 10);
		xmlFree(str);
	      }
	      if((str = xmlGetProp(node, BAD_CAST "y-offset"))) {
                project->wms_offset.y = strtoul(reinterpret_cast<char *>(str), O2G_NULLPTR, 10);
		xmlFree(str);
	      }

            } else if(strcmp(reinterpret_cast<const char *>(node->name), "osm") == 0) {
	      str = xmlNodeListGetString(doc, node->children, 1);
	      printf("osm = %s\n", str);

	      /* make this a relative path if possible */
	      /* if the project path actually is a prefix of this, */
	      /* then just remove this prefix */
	      if(str[0] == '/' &&
                 strlen(reinterpret_cast<char *>(str)) > project->path.size() &&
                 !strncmp(reinterpret_cast<char *>(str), project->path.c_str(), project->path.size())) {

		project->osm = reinterpret_cast<char *>(str + project->path.size());
		printf("osm name converted to relative %s\n", project->osm.c_str());
	      } else
		project->osm = reinterpret_cast<char *>(str);

	      xmlFree(str);

            } else if(strcmp(reinterpret_cast<const char *>(node->name), "min") == 0) {
              project->min = xml_get_prop_pos(node);
            } else if(strcmp(reinterpret_cast<const char *>(node->name), "max") == 0) {
              project->max = xml_get_prop_pos(node);
	    }
	  }
	  node = node->next;
	}
      }
    }
  }

  xmlFreeDoc(doc);

  if(project->name.empty())
    return false;

  // no explicit filename was given, guess the default ones
  if(project->osm.empty()) {
    std::string fname = project->path + project->name + ".osm.gz";
    if(g_file_test(fname.c_str(), G_FILE_TEST_IS_REGULAR) != TRUE)
      fname.erase(fname.size() - 3);
    project->osm = fname.substr(project->path.size());
  }

  if(project->server == O2G_NULLPTR)
    project->server = defaultserver.c_str();

  return true;
}

bool project_t::save(GtkWidget *parent) {
  char str[32];
  const std::string &project_file = project_filename(this);

  printf("saving project to %s\n", project_file.c_str());

  /* check if project path exists */
  if(G_UNLIKELY(g_file_test(path.c_str(), G_FILE_TEST_IS_DIR) != TRUE)) {
    /* make sure project base path exists */
    if(G_UNLIKELY(g_mkdir_with_parents(path.c_str(), S_IRWXU) != 0)) {
      errorf(GTK_WIDGET(parent),
	     _("Unable to create project path %s"), path.c_str());
      return false;
    }
    fdguard nfd(path.c_str());
    if(G_UNLIKELY(!nfd.valid())) {
      errorf(GTK_WIDGET(parent), _("Unable to open project path %s"), path.c_str());
      return false;
    }
    dirfd.swap(nfd);
  }

  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr node, root_node = xmlNewNode(O2G_NULLPTR, BAD_CAST "proj");
  xmlNewProp(root_node, BAD_CAST "name", BAD_CAST name.c_str());
  if(data_dirty)
    xmlNewProp(root_node, BAD_CAST "dirty", BAD_CAST "true");
  if(isDemo)
    xmlNewProp(root_node, BAD_CAST "demo", BAD_CAST "true");

  xmlDocSetRootElement(doc, root_node);

  if(!rserver.empty()) {
    g_assert(server == rserver.c_str());
    xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "server", BAD_CAST server);
  }

  if(!desc.empty())
    xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "desc", BAD_CAST desc.c_str());

  const std::string defaultOsm = name + ".osm";
  if(G_UNLIKELY(osm != defaultOsm + ".gz" && osm != defaultOsm))
    xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "osm", BAD_CAST osm.c_str());

  node = xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "min", O2G_NULLPTR);
  xml_set_prop_pos(node, &min);

  node = xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "max", O2G_NULLPTR);
  xml_set_prop_pos(node, &max);

  node = xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "map", O2G_NULLPTR);
  g_ascii_formatd(str, sizeof(str), "%.04f", map_state.zoom);
  xmlNewProp(node, BAD_CAST "zoom", BAD_CAST str);
  g_ascii_formatd(str, sizeof(str), "%.04f", map_state.detail);
  xmlNewProp(node, BAD_CAST "detail", BAD_CAST str);
  snprintf(str, sizeof(str), "%d", map_state.scroll_offset.x);
  xmlNewProp(node, BAD_CAST "scroll-offset-x", BAD_CAST str);
  snprintf(str, sizeof(str), "%d", map_state.scroll_offset.y);
  xmlNewProp(node, BAD_CAST "scroll-offset-y", BAD_CAST str);

  if(wms_offset.x != 0 || wms_offset.y != 0 ||
     !wms_server.empty() || !wms_path.empty()) {
    node = xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "wms", O2G_NULLPTR);
    if(!wms_server.empty())
      xmlNewProp(node, BAD_CAST "server", BAD_CAST wms_server.c_str());
    if(!wms_path.empty())
      xmlNewProp(node, BAD_CAST "path", BAD_CAST wms_path.c_str());
    snprintf(str, sizeof(str), "%d", wms_offset.x);
    xmlNewProp(node, BAD_CAST "x-offset", BAD_CAST str);
    snprintf(str, sizeof(str), "%d", wms_offset.y);
    xmlNewProp(node, BAD_CAST "y-offset", BAD_CAST str);
  }

  xmlSaveFormatFileEnc(project_file.c_str(), doc, "UTF-8", 1);
  xmlFreeDoc(doc);

  return true;
}

/* ------------ project selection dialog ------------- */

/**
 * @brief check if a project with the given name exists
 * @param settings settings
 * @param name project name
 * @param fullname where to store absolute filename if project exists
 * @returns if project exists
 */
static bool project_exists(const std::string &base_path, const char *name, std::string &fullname) {
  fullname = base_path + name + '/' + name + ".proj";

  /* check for project file */
  return g_file_test(fullname.c_str(), G_FILE_TEST_IS_REGULAR) == TRUE;
}

static std::vector<project_t *> project_scan(map_state_t &ms, const std::string &base_path, const std::string &server) {
  std::vector<project_t *> projects;

  /* scan for projects */
  DIR *dir = opendir(base_path.c_str());
  if(dir == O2G_NULLPTR)
    return projects;

  dirent *d;
  while((d = readdir(dir)) != O2G_NULLPTR) {
    if(d->d_type != DT_DIR && d->d_type != DT_UNKNOWN)
      continue;

    if(G_UNLIKELY(strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0))
      continue;

    std::string fullname;
    if(project_exists(base_path, d->d_name, fullname)) {
      printf("found project %s\n", d->d_name);

      /* try to read project and append it to chain */
      project_t *n = new project_t(ms, d->d_name, base_path);

      if(G_LIKELY(project_read(fullname, n, server)))
        projects.push_back(n);
      else
        delete n;
    }
  }

  closedir(dir);

  return projects;
}

enum {
  PROJECT_COL_NAME = 0,
  PROJECT_COL_STATUS,
  PROJECT_COL_DESCRIPTION,
  PROJECT_COL_DATA,
  PROJECT_NUM_COLS
};

/**
 * @brief check if OSM data is present for the given project
 * @param project the project to check
 * @return if OSM data file was found
 */
static bool osm_file_exists(const project_t *project) {
  struct stat st;
  return fstatat(project->dirfd, project->osm.c_str(), &st, 0) == 0 && S_ISREG(st.st_mode);
}

static void view_selected(GtkWidget *dialog, project_t *project) {
  /* check if the selected project also has a valid osm file */
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
                                    GTK_RESPONSE_ACCEPT,
                                    (project != O2G_NULLPTR && osm_file_exists(project)) ? TRUE : FALSE);
}

static void
changed(GtkTreeSelection *selection, gpointer userdata) {
  select_context_t *context = static_cast<select_context_t *>(userdata);

  GtkTreeModel *model = O2G_NULLPTR;
  GtkTreeIter iter;

  gboolean sel = gtk_tree_selection_get_selected(selection, &model, &iter);
  if(sel) {
    project_t *project = O2G_NULLPTR;
    gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &project, -1);

    view_selected(context->dialog, project);
  }

  list_button_enable(GTK_WIDGET(context->list), LIST_BUTTON_REMOVE, sel);
  list_button_enable(GTK_WIDGET(context->list), LIST_BUTTON_EDIT, sel);
}

/**
 * @brief get the currently selected project in the list
 * @param list the project list widget
 * @returns the project belonging to the currently selected entry
 *
 * This assumes there is a selection and a project associated to it.
 */
static project_t *project_get_selected(GtkWidget *list) {
  project_t *project = O2G_NULLPTR;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  g_assert_true(list_get_selected(list, &model, &iter));
  gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &project, -1);

  g_assert_nonnull(project);
  return project;
}

/* ------------------------- create a new project ---------------------- */

typedef struct {
  GtkWidget *dialog;
  settings_t *settings;
} name_callback_context_t;

static void callback_modified_name(GtkWidget *widget, name_callback_context_t *context) {
  const gchar *name = gtk_entry_get_text(GTK_ENTRY(widget));

  /* name must not contain some special chars */
  gboolean ok = FALSE;

  /* check if there's a name */
  if(name && strlen(name) > 0) {
    /* check if it consists of valid characters */
    if(strpbrk(name, "\\*?()\n\t\r") == O2G_NULLPTR) {
      /* check if such a project already exists */
      std::string fullname;
      if(!project_exists(context->settings->base_path, name, fullname))
	ok = TRUE;
    }
  }

  gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
				    GTK_RESPONSE_ACCEPT, ok);
}

static void project_close(appdata_t &appdata) {
  printf("closing current project\n");

  /* Save track and turn off the handler callback */
  track_save(appdata.project, appdata.track.track);
  track_clear(appdata);

  appdata.map->clear(MAP_LAYER_ALL);

  if(appdata.osm) {
    diff_save(appdata.project, appdata.osm);
    delete appdata.osm;
    appdata.osm = O2G_NULLPTR;
  }

  /* remember in settings that no project is open */
  appdata.settings->project.clear();

  /* update project file on disk */
  appdata.project->save(GTK_WIDGET(appdata.window));

  delete appdata.project;
  appdata.project = O2G_NULLPTR;
}

static bool project_delete(select_context_t *context, project_t *project) {

  printf("deleting project \"%s\"\n", project->name.c_str());

  /* check if we are to delete the currently open project */
  if(context->appdata.project &&
     context->appdata.project->name == project->name) {

    if(!yes_no_f(context->dialog, context->appdata, 0, 0,
		 _("Delete current project?"),
		 _("The project you are about to delete is the one "
		   "you are currently working on!\n\n"
		   "Do you want to delete it anyway?")))
      return false;

    project_close(context->appdata);
  }

  /* remove entire directory from disk */
  DIR *dir = opendir(project->path.c_str());
  if(G_LIKELY(dir != O2G_NULLPTR)) {
    int dfd = dirfd(dir);
    dirent *d;
    while ((d = readdir(dir)) != O2G_NULLPTR) {
      if(G_UNLIKELY(d->d_type == DT_DIR ||
                    (unlinkat(dfd, d->d_name, 0) == -1 && errno == EISDIR)))
        unlinkat(dfd, d->d_name, AT_REMOVEDIR);
    }
    closedir(dir);

    /* remove the projects directory */
    rmdir(project->path.c_str());
  }

  /* remove from view */
  GtkTreeIter iter;
  GtkTreeModel *model = GTK_TREE_MODEL(context->store);
  if(gtk_tree_model_get_iter_first(model, &iter)) {
    do {
      project_t *prj = O2G_NULLPTR;
      gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &prj, -1);
      if(prj && (prj == project)) {
	printf("found %s to remove\n", prj->name.c_str());
	/* and remove from store */
        gtk_list_store_remove(context->store, &iter);
        break;
      }
    } while(gtk_tree_model_iter_next(model, &iter));
  }

  /* de-chain entry from project list */
  const std::vector<project_t *>::iterator itEnd = context->projects.end();
  std::vector<project_t *>::iterator it = std::find(context->projects.begin(),
                                                    itEnd, project);
  if(it != itEnd)
    context->projects.erase(it);

  /* free project structure */
  delete project;

  /* disable ok button button */
  view_selected(context->dialog, O2G_NULLPTR);

  return true;
}

static project_t *project_new(select_context_t *context) {
  printf("creating project with default values\n");

  /* --------------  first choose a name for the project --------------- */
  GtkWidget *dialog =
    misc_dialog_new(MISC_DIALOG_NOSIZE,  _("Project name"),
		    GTK_WINDOW(context->dialog),
		    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		    O2G_NULLPTR);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 8);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), gtk_label_new(_("Name:")));

  name_callback_context_t name_context = { dialog, context->appdata.settings };
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
    return O2G_NULLPTR;
  }

  project_t *project = new project_t(context->dummystate, gtk_entry_get_text(GTK_ENTRY(entry)),
                                     context->appdata.settings->base_path);
  gtk_widget_destroy(dialog);

  /* no data downloaded yet */
  project->data_dirty = true;

  /* use global server/access settings */
  project->server = context->appdata.settings->server.c_str();

  /* build project osm file name */
  project->osm = project->name + ".osm";

  project->min.lat = NAN;  project->min.lon = NAN;
  project->max.lat = NAN;  project->max.lon = NAN;

  /* create project file on disk */
  if(!project->save(context->dialog)) {
    project_delete(context, project);

    project = O2G_NULLPTR;
  } else if(!project_edit(context, project, TRUE)) {
    printf("new/edit cancelled!!\n");

    project_delete(context, project);

    project = O2G_NULLPTR;
  }

  /* enable/disable edit/remove buttons */
  view_selected(context->dialog, project);

  return project;
}

/**
 * @brief get icon for the given project
 * @param current the currently active project or O2G_NULLPTR
 * @param project the project to check
 * @return the stock identifier
 */
static const gchar *
project_get_status_icon_stock_id(const project_t *current,
                                 const project_t *project) {
  /* is this the currently open project? */
  if(current && current->name == project->name)
    return GTK_STOCK_OPEN;
  else if(!osm_file_exists(project))
    return GTK_STOCK_DIALOG_WARNING;
  else if(diff_present(project))
    return GTK_STOCK_PROPERTIES;
  else
    return GTK_STOCK_FILE;

    // TODO: check for outdatedness too. Which icon to use?
}

static void on_project_new(select_context_t *context) {
  project_t *project = project_new(context);
  if(project) {
    context->projects.push_back(project);

    GtkTreeIter iter;
    const gchar *status_stock_id = project_get_status_icon_stock_id(
                                         context->appdata.project, project);
    gtk_list_store_append(GTK_LIST_STORE(context->store), &iter);
    gtk_list_store_set(GTK_LIST_STORE(context->store), &iter,
		       PROJECT_COL_NAME,        project->name.c_str(),
		       PROJECT_COL_STATUS,      status_stock_id,
		       PROJECT_COL_DESCRIPTION, project->desc.c_str(),
		       PROJECT_COL_DATA,        project,
		       -1);

    GtkTreeSelection *selection = list_get_selection(context->list);
    gtk_tree_selection_select_iter(selection, &iter);
  }
}

static void on_project_delete(select_context_t *context) {
  project_t *project = project_get_selected(context->list);

  if(!yes_no_f(context->dialog, context->appdata, 0, 0, _("Delete project?"),
               _("Do you really want to delete the project \"%s\"?"),
               project->name.c_str()))
    return;

  if(!project_delete(context, project))
    printf("unable to delete project\n");
}

static void on_project_edit(select_context_t *context) {
  project_t *project = project_get_selected(context->list);

  if(project_edit(context, project, FALSE)) {
    GtkTreeModel     *model;
    GtkTreeIter       iter;

    /* description etc. may have changed, so update list */
    GtkTreeSelection *selection = list_get_selection(context->list);
    g_assert_true(gtk_tree_selection_get_selected(selection, &model, &iter));

    //     gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &project, -1);
    const gchar *status_stock_id = project_get_status_icon_stock_id(
                                         context->appdata.project, project);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
		       PROJECT_COL_NAME, project->name.c_str(),
                       PROJECT_COL_STATUS, status_stock_id,
		       PROJECT_COL_DESCRIPTION, project->desc.c_str(),
		       -1);


    /* check if we have actually editing the currently open project */
    if(context->appdata.project &&
       context->appdata.project->name == project->name) {
      project_t *cur = context->appdata.project;

      printf("edited project was actually the active one!\n");

      /* update the currently active project also */

      /* update description */
      cur->desc = project->desc;

      // update OSM file, may have changed (gzip or not)
      cur->osm = project->osm;

      /* update server */
      if(project->server == context->appdata.settings->server) {
        cur->server = context->appdata.settings->server.c_str();
	cur->rserver.clear();
      } else {
        cur->rserver = project->server;
        cur->server = cur->rserver.c_str();
      }

      /* update coordinates */
      if((cur->min.lat != project->min.lat) ||
	 (cur->max.lat != project->max.lat) ||
	 (cur->min.lon != project->min.lon) ||
	 (cur->max.lon != project->max.lon)) {
        appdata_t &appdata = context->appdata;

	/* save modified coordinates */
        cur->min = project->min;
        cur->max = project->max;

	/* try to do this automatically */

	/* if we have valid osm data loaded: save state first */
        if(appdata.osm) {
	  /* redraw the entire map by destroying all map items  */
          diff_save(appdata.project, appdata.osm);
          appdata.map->clear(MAP_LAYER_ALL);

          delete appdata.osm;
	}

	/* and load the (hopefully) new file */
        appdata.osm = appdata.project->parse_osm(appdata.icons);
        diff_restore(appdata);
        appdata.map->paint();

	main_ui_enable(appdata);
      }
    }
  }

  /* enable/disable edit/remove buttons */
  view_selected(context->dialog, project);
}

static void
on_project_update_all(select_context_t *context)
{
  GtkTreeIter iter;
  GtkTreeModel *model = GTK_TREE_MODEL(context->store);
  if(gtk_tree_model_get_iter_first(model, &iter)) {
    do {
      project_t *prj = O2G_NULLPTR;
      gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &prj, -1);
      /* if the project was already downloaded do it again */
      if(prj && osm_file_exists(prj)) {
        printf("found %s to update\n", prj->name.c_str());
        if (!osm_download(GTK_WIDGET(context->dialog),
                     context->appdata.settings, prj))
          break;
      }
    } while(gtk_tree_model_iter_next(model, &iter));
  }
}

struct project_list_add {
  GtkListStore * const store;
  const project_t * const cur_proj;
  pos_t pos;
  bool check_pos;
  GtkTreeIter &seliter;
  gboolean &has_sel;
  project_list_add(GtkListStore *s, appdata_t &a, GtkTreeIter &l, gboolean &h)
    : store(s), cur_proj(a.project), pos(a.gps_state->get_pos()), check_pos(pos.valid())
    , seliter(l), has_sel(h) {}
  void operator()(const project_t *project);
};

void project_list_add::operator()(const project_t* project)
{
  GtkTreeIter iter;
  const gchar *status_stock_id = project_get_status_icon_stock_id(
                                         cur_proj, project);
  /* Append a row and fill in some data */
  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter,
                     PROJECT_COL_NAME,        project->name.c_str(),
                     PROJECT_COL_STATUS,      status_stock_id,
                     PROJECT_COL_DESCRIPTION, project->desc.c_str(),
                     PROJECT_COL_DATA,        project,
                     -1);

  /* decide if to select this project because it matches the current position */
  if(check_pos && position_in_rect(project->min, project->max, pos)) {
    seliter = iter;
    has_sel = TRUE;
    check_pos = false;
  }
}

/**
 * @brief create a widget to list the projects
 * @param context the context struct
 * @param has_sel if an item has been selected
 */
static GtkWidget *project_list_widget(select_context_t &context, gboolean &has_sel) {
  std::vector<list_view_column> columns;
  columns.push_back(list_view_column(_("Name"), 0));
  columns.push_back(list_view_column(_("State"), LIST_FLAG_STOCK_ICON));
  columns.push_back(list_view_column(_("Description"), LIST_FLAG_ELLIPSIZE));

  std::vector<list_button> buttons;
  buttons.push_back(list_button(_("_New"), G_CALLBACK(on_project_new)));
  buttons.push_back(list_button(_("_Edit"), G_CALLBACK(on_project_edit)));
  buttons.push_back(list_button(_("Remove"), G_CALLBACK(on_project_delete)));
  buttons.push_back(list_button(_("Update all"), GCallback(on_project_update_all)));

  /* build the store */
  context.store = gtk_list_store_new(PROJECT_NUM_COLS,
      G_TYPE_STRING,    // name
      G_TYPE_STRING,    // status
      G_TYPE_STRING,    // desc
      G_TYPE_POINTER);  // data

  context.list = list_new(LIST_HILDON_WITHOUT_HEADERS, 0,
                          &context, changed, buttons, columns, context.store);

  GtkTreeIter seliter;
  has_sel = FALSE;

  std::for_each(context.projects.begin(), context.projects.end(),
                project_list_add(context.store, context.appdata, seliter, has_sel));

  g_object_unref(context.store);

  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(context.store),
                                       PROJECT_COL_NAME, GTK_SORT_ASCENDING);

  if(has_sel)
    list_scroll(context.list, &seliter);

  return context.list;
}

std::string project_select(appdata_t &appdata) {
  std::string name;

  select_context_t context(appdata,
                    misc_dialog_new(MISC_DIALOG_MEDIUM,_("Project selection"),
                                    GTK_WINDOW(appdata.window),
                                    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                    O2G_NULLPTR));

  /* under fremantle the dialog does not have an "Open" button */
  /* as it's closed when a project is being selected */
  gtk_dialog_set_default_response(GTK_DIALOG(context.dialog),
				  GTK_RESPONSE_ACCEPT);

  gboolean has_sel;
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
			      project_list_widget(context, has_sel));

  /* don't all user to click ok until something is selected */
  gtk_dialog_set_response_sensitive(GTK_DIALOG(context.dialog),
				    GTK_RESPONSE_ACCEPT, has_sel);

  gtk_widget_show_all(context.dialog);
  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context.dialog)))
    name = project_get_selected(context.list)->name;

  return name;
}

/* ---------------------------------------------------- */

static void project_filesize(project_context_t *context) {
  char *str = O2G_NULLPTR;
  gchar *gstr = O2G_NULLPTR;
  const project_t * const project = context->project;

  printf("Checking size of %s\n", project->osm.c_str());

  struct stat st;
  bool stret = fstatat(project->dirfd, project->osm.c_str(), &st, 0) == 0 &&
               S_ISREG(st.st_mode);
  if(!stret && errno == ENOENT) {
    GdkColor color;
    gdk_color_parse("red", &color);
    gtk_widget_modify_fg(context->fsize, GTK_STATE_NORMAL, &color);

    str = _("Not downloaded!");

    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
				      GTK_RESPONSE_ACCEPT, !context->is_new);

  } else {
    gtk_widget_modify_fg(context->fsize, GTK_STATE_NORMAL, O2G_NULLPTR);

    if(!project->data_dirty) {
      if(stret) {
        struct tm loctime;
        localtime_r(&st.st_mtim.tv_sec, &loctime);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%x %X", &loctime);

        if(project->osm.size() > 3 && strcmp(project->osm.c_str() + project->osm.size() - 3, ".gz") == 0) {
          gstr = g_strdup_printf(_("%" G_GOFFSET_FORMAT " bytes present\nfrom %s"),
                                 static_cast<goffset>(st.st_size), time_str);
          gtk_label_set_text(GTK_LABEL(context->fsizehdr), _("Map data:\n(compressed)"));
        } else {
          gstr = g_strdup_printf(_("%" G_GOFFSET_FORMAT " bytes present\nfrom %s"),
                                 static_cast<goffset>(st.st_size), time_str);
          gtk_label_set_text(GTK_LABEL(context->fsizehdr), _("Map data:"));
        }
        str = gstr;
      } else {
        str = _("Error testing data file");
      }
    } else
      str = _("Outdated, please download!");

    gboolean en = (!context->is_new || !project->data_dirty) ? TRUE : FALSE;
    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
                                      GTK_RESPONSE_ACCEPT, en);
  }

  if(str) {
    gtk_label_set_text(GTK_LABEL(context->fsize), str);
    g_free(gstr);
  }
}

/* a project may currently be open. "unsaved changes" then also */
/* means that the user may have unsaved changes */
bool project_context_t::active_n_dirty() const {

  if(!area_edit.appdata.osm)
    return false;

  if(area_edit.appdata.project &&
     area_edit.appdata.project->name == project->name) {

    printf("editing the currently open project\n");

    return !diff_is_clean(area_edit.appdata.osm, true);
  }

  return false;
}

static void project_diffstat(project_context_t &context) {
  const char *str;

  if(diff_present(context.project) || context.active_n_dirty()) {
    /* this should prevent the user from changing the area */
    str = _("unsaved changes pending");
  } else
    str = _("no pending changes");

  gtk_label_set_text(GTK_LABEL(context.diff_stat), str);
}

static gboolean
project_pos_is_valid(const project_t *project) {
  return (project->min.valid() && project->max.valid()) ? TRUE : FALSE;
}

struct projects_to_bounds {
  std::vector<pos_bounds> &pbounds;
  explicit projects_to_bounds(std::vector<pos_bounds> &b) : pbounds(b) {}
  void operator()(const project_t *project);
};

void projects_to_bounds::operator()(const project_t* project)
{
  if (!project_pos_is_valid(project))
    return;

  pbounds.push_back(pos_bounds(project->min, project->max));
}

static void on_edit_clicked(project_context_t *context) {
  project_t * const project = context->project;

  if(diff_present(project) || context->active_n_dirty())
    messagef(context->dialog,
	     _("Pending changes"),
	     _("You have pending changes in this project.\n\n"
	       "Changing the area may cause pending changes to be "
	       "lost if they are outside the updated area."));

  context->area_edit.other_bounds.clear();
  std::for_each(context->projects.begin(), context->projects.end(),
                projects_to_bounds(context->area_edit.other_bounds));

  if(context->area_edit.run()) {
    printf("coordinates changed!!\n");

    /* the wms layer isn't usable with new coordinates */
    wms_remove_file(*project);

    pos_lat_label_set(context->minlat, project->min.lat);
    pos_lon_label_set(context->minlon, project->min.lon);
    pos_lat_label_set(context->maxlat, project->max.lat);
    pos_lon_label_set(context->maxlon, project->max.lon);

    gboolean pos_valid = project_pos_is_valid(project);
    gtk_widget_set_sensitive(context->download, pos_valid);

    /* (re-) download area */
    if (pos_valid)
    {
      if(osm_download(GTK_WIDGET(context->dialog),
	      context->area_edit.appdata.settings, project))
         project->data_dirty = false;
    }
    project_filesize(context);
  }
}

static void on_download_clicked(project_context_t *context) {
  project_t * const project = context->project;

  printf("download %s\n", project->osm.c_str());

  if(osm_download(context->dialog, context->settings, project))
    project->data_dirty = false;
  else
    printf("download failed\n");

  project_filesize(context);
}

static void on_diff_remove_clicked(project_context_t *context) {
  const project_t * const project = context->project;

  printf("clicked diff remove\n");

  appdata_t &appdata = context->area_edit.appdata;
  if(yes_no_f(context->dialog, appdata, 0, 0, _("Discard changes?"),
	      _("Do you really want to discard your changes? This will "
		"permanently undo all changes you have made so far and which "
		"you did not upload yet."))) {
    diff_remove(project);

    /* if this is the currently open project, we need to undo */
    /* the map changes as well */

    if(appdata.project && appdata.project->name == project->name) {

      printf("undo all on current project: delete map changes as well\n");

      /* just reload the map */
      appdata.map->clear(MAP_LAYER_OBJECTS_ONLY);
      delete appdata.osm;
      appdata.osm = appdata.project->parse_osm(appdata.icons);
      appdata.map->paint();
    }

    /* update button/label state */
    project_diffstat(*context);
    gtk_widget_set_sensitive(context->diff_remove,  FALSE);
  }
}

bool project_t::check_demo(GtkWidget *parent) const {
  if(isDemo)
    messagef(parent, _("Demo project"),
             _("This is a preinstalled demo project. This means that the "
               "basic project parameters cannot be changed and no data can "
               "be up- or downloaded via the OSM servers.\n\n"
               "Please setup a new project to do these things."));

  return isDemo;
}

static bool
project_edit(select_context_t *scontext, project_t *project, gboolean is_new) {
  GtkWidget *parent = scontext->dialog;

  if(project->check_demo(parent))
    return false;

  /* ------------ project edit dialog ------------- */

  GtkWidget *dialog;
  /* cancel is enabled for "new" projects only */
  if(is_new) {
    char *str = g_strdup_printf(_("New project - %s"), project->name.c_str());

    dialog = misc_dialog_new(MISC_DIALOG_WIDE, str,
                             GTK_WINDOW(parent),
                             GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                             GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, O2G_NULLPTR);
    g_free(str);
  } else {
    char *str = g_strdup_printf(_("Edit project - %s"), project->name.c_str());

    dialog = misc_dialog_new(MISC_DIALOG_WIDE, str,
                             GTK_WINDOW(parent),
                             GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, O2G_NULLPTR);
    g_free(str);
  }

  project_context_t context(scontext->appdata, project, is_new, scontext->projects, dialog);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog),
				  GTK_RESPONSE_ACCEPT);

  GtkWidget *table = gtk_table_new(5, 5, FALSE);  // x, y
  gtk_table_set_col_spacing(GTK_TABLE(table), 0, 8);
  gtk_table_set_col_spacing(GTK_TABLE(table), 3, 8);

  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_left_new(_("Description:")), 0, 1, 0, 1);
  gtk_entry_set_activates_default(GTK_ENTRY(context.desc), TRUE);
  if(!project->desc.empty())
    gtk_entry_set_text(GTK_ENTRY(context.desc), project->desc.c_str());
  gtk_table_attach_defaults(GTK_TABLE(table),  context.desc, 1, 5, 0, 1);
  gtk_table_set_row_spacing(GTK_TABLE(table), 0, 4);

  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_left_new(_("Latitude:")), 0, 1, 1, 2);
  gtk_table_attach_defaults(GTK_TABLE(table), context.minlat, 1, 2, 1, 2);
  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("to")), 2, 3, 1, 2);
  gtk_table_attach_defaults(GTK_TABLE(table), context.maxlat, 3, 4, 1, 2);

  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_left_new(_("Longitude:")), 0, 1, 2, 3);
  gtk_table_attach_defaults(GTK_TABLE(table), context.minlon, 1, 2, 2, 3);
  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("to")), 2, 3, 2, 3);
  gtk_table_attach_defaults(GTK_TABLE(table), context.maxlon, 3, 4, 2, 3);

  GtkWidget *edit = button_new_with_label(_("Edit"));
  g_signal_connect_swapped(GTK_OBJECT(edit), "clicked",
                           G_CALLBACK(on_edit_clicked), &context);
  gtk_table_attach(GTK_TABLE(table), edit, 4, 5, 1, 3,
                   static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
                   static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),0,0);

  gtk_table_set_row_spacing(GTK_TABLE(table), 2, 4);

#ifdef SERVER_EDITABLE
  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_left_new(_("Server:")), 0, 1, 3, 4);
  gtk_entry_set_activates_default(GTK_ENTRY(context.server), TRUE);
  HILDON_ENTRY_NO_AUTOCAP(context.server);
  gtk_entry_set_text(GTK_ENTRY(context.server), project->server);
  gtk_table_attach_defaults(GTK_TABLE(table),  context.server, 1, 4, 3, 4);

  gtk_table_set_row_spacing(GTK_TABLE(table), 3, 4);
#endif

  gtk_table_attach_defaults(GTK_TABLE(table), context.fsizehdr, 0, 1, 4, 5);
  project_filesize(&context);
  gtk_table_attach_defaults(GTK_TABLE(table), context.fsize, 1, 4, 4, 5);
  g_signal_connect_swapped(GTK_OBJECT(context.download), "clicked",
                           G_CALLBACK(on_download_clicked), &context);
  gtk_widget_set_sensitive(context.download, project_pos_is_valid(project));

  gtk_table_attach_defaults(GTK_TABLE(table), context.download, 4, 5, 4, 5);

  gtk_table_set_row_spacing(GTK_TABLE(table), 4, 4);

  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_left_new(_("Changes:")), 0, 1, 5, 6);
  project_diffstat(context);
  gtk_table_attach_defaults(GTK_TABLE(table), context.diff_stat, 1, 4, 5, 6);
  if(!diff_present(project) && !context.active_n_dirty())
    gtk_widget_set_sensitive(context.diff_remove,  FALSE);
  g_signal_connect_swapped(GTK_OBJECT(context.diff_remove), "clicked",
                           G_CALLBACK(on_diff_remove_clicked), &context);
  gtk_table_attach_defaults(GTK_TABLE(table), context.diff_remove, 4, 5, 5, 6);

  /* ---------------------------------------------------------------- */

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
			      table);

  /* disable "ok" if there's no valid file downloaded */
  if(is_new)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(context.dialog),
                                      GTK_RESPONSE_ACCEPT,
                                      osm_file_exists(project) ? TRUE : FALSE);

  gtk_widget_show_all(context.dialog);

  /* the return value may actually be != ACCEPT, but only if the editor */
  /* is run for a new project which is completely removed afterwards if */
  /* cancel has been selected */
  bool ok = (GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context.dialog)));

  /* transfer values from edit dialog into project structure */

  /* fetch values from dialog */
  const gchar *ndesc = gtk_entry_get_text(GTK_ENTRY(context.desc));
  if(ndesc && strlen(ndesc))
    project->desc = ndesc;
  else
    project->desc.clear();

#ifdef SERVER_EDITABLE
  context.project->rserver = gtk_entry_get_text(GTK_ENTRY(context.server));
  if(context.project->rserver.empty() ||
     context.project->rserver == scontext->appdata.settings->server) {
    context.project->rserver.clear();
    context.project->server = scontext->appdata.settings->server.c_str();
  } else {
    context.project->server = context.project->rserver.c_str();
  }
#endif

  project->save(context.dialog);

  gtk_widget_destroy(context.dialog);

  return ok;
}

static bool project_open(appdata_t &appdata, const std::string &name) {
  project_t *project;
  std::string project_file;

  g_assert_false(name.empty());
  if(G_UNLIKELY(name.find('/') != std::string::npos)) {
    // load with absolute or relative path, usually only done for demo
    project_file = name;
    std::string::size_type sl = name.rfind('/');
    std::string pname = name.substr(sl + 1);
    if(G_LIKELY(pname.substr(pname.size() - 5) == ".proj"))
      pname.erase(pname.size() - 5);
    // usually that ends in /foo/foo.proj
    if(name.substr(sl - pname.size() - 1, pname.size() + 1) == '/' + pname)
      sl -= pname.size();
    project = new project_t(appdata.map_state, pname, name.substr(0, sl));
  } else {
    project = new project_t(appdata.map_state, name, appdata.settings->base_path);

    project_file = project_filename(project);
  }
  project->map_state.reset();

  printf("project file = %s\n", project_file.c_str());
  if(G_UNLIKELY(!project_read(project_file, project, appdata.settings->server))) {
    printf("error reading project file\n");
    delete project;
    return false;
  }

  /* --------- project structure ok: load its OSM file --------- */
  appdata.project = project;

  printf("project_open: loading osm %s\n", project->osm.c_str());
  appdata.osm = project->parse_osm(appdata.icons);
  if(!appdata.osm) {
    printf("OSM parsing failed\n");
    return false;
  }

  printf("parsing ok\n");

  return true;
}

bool project_load(appdata_t &appdata, const std::string &name) {
  char banner_txt[64];
  snprintf(banner_txt, sizeof(banner_txt), _("Loading %s"), name.c_str());
  banner_busy_start(appdata, banner_txt);

  /* close current project */
  osm2go_platform::process_events();

  if(appdata.project)
    project_close(appdata);

  /* open project itself */
  osm2go_platform::process_events();

  if(G_UNLIKELY(!project_open(appdata, name))) {
    printf("error opening requested project\n");

    delete appdata.project;
    appdata.project = O2G_NULLPTR;

    delete appdata.osm;
    appdata.osm = O2G_NULLPTR;

    snprintf(banner_txt, sizeof(banner_txt),
	     _("Error opening %s"), name.c_str());
    banner_busy_stop(appdata);
    banner_show_info(appdata, banner_txt);

    return false;
  }

  if(!appdata.window)
    return false;

  /* check if OSM data is valid */
  osm2go_platform::process_events();
  const char *errmsg = appdata.osm->sanity_check();
  if(G_UNLIKELY(errmsg != O2G_NULLPTR)) {
    errorf(GTK_WIDGET(appdata.window), "%s", errmsg);
    printf("project/osm sanity checks failed, unloading project\n");

    delete appdata.project;
    appdata.project = O2G_NULLPTR;

    delete appdata.osm;
    appdata.osm = O2G_NULLPTR;

    snprintf(banner_txt, sizeof(banner_txt),
	     _("Error opening %s"), name.c_str());
    banner_busy_stop(appdata);
    banner_show_info(appdata, banner_txt);

    return false;
  }

  /* load diff possibly preset */
  osm2go_platform::process_events();
  if(!appdata.window) goto fail;

  diff_restore(appdata);

  /* prepare colors etc, draw data and adjust scroll/zoom settings */
  osm2go_platform::process_events();
  if(!appdata.window) goto fail;

  appdata.map->init();

  /* restore a track */
  osm2go_platform::process_events();
  if(!appdata.window) goto fail;

  track_clear(appdata);
  if(track_restore(appdata))
    appdata.map->track_draw(appdata.settings->trackVisibility, *appdata.track.track);

  /* finally load a background if present */
  osm2go_platform::process_events();
  if(!appdata.window) goto fail;
  wms_load(appdata);

  /* save the name of the project for the perferences */
  appdata.settings->project = appdata.project->name;

  banner_busy_stop(appdata);

#if 0
  snprintf(banner_txt, sizeof(banner_txt), _("Loaded %s"), name.c_str());
  banner_show_info(appdata, banner_txt);
#endif

  appdata.statusbar->set(O2G_NULLPTR, false);

  return true;

 fail:
  printf("project loading interrupted by user\n");

  if(appdata.project) {
    delete appdata.project;
    appdata.project = O2G_NULLPTR;
  }

  if(appdata.osm) {
    delete appdata.osm;
    appdata.osm = O2G_NULLPTR;
  }

  return false;
}

osm_t *project_t::parse_osm(icon_t &icons) const {
  return osm_t::parse(path, osm, icons);
}

project_t::project_t(map_state_t &ms, const std::string &n, const std::string &base_path)
  : server(O2G_NULLPTR)
  , map_state(ms)
  , name(n)
  , path(base_path +  name + '/')
  , data_dirty(false)
  , isDemo(false)
  , dirfd(path.c_str())
{
  memset(&wms_offset, 0, sizeof(wms_offset));
  memset(&min, 0, sizeof(min));
  memset(&max, 0, sizeof(max));
}

project_t::~project_t()
{
}

select_context_t::select_context_t(appdata_t &a, GtkWidget *dial)
  : appdata(a)
  , projects(project_scan(dummystate, appdata.settings->base_path, appdata.settings->server))
  , dialog(dial)
  , list(O2G_NULLPTR)
{
}

select_context_t::~select_context_t()
{
  std::for_each(projects.begin(), projects.end(), std::default_delete<project_t>());

  gtk_widget_destroy(dialog);
}
