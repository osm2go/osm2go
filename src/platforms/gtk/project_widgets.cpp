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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "project.h"
#include "project_p.h"

#include "appdata.h"
#include "area_edit.h"
#include "diff.h"
#include "gps_state.h"
#include "list.h"
#include "map.h"
#include "notifications.h"
#include "osm_api.h"
#include "settings.h"
#include "uicontrol.h"
#include "wms.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include "osm2go_platform.h"
#include "osm2go_platform_gtk.h"
#include "osm2go_stl.h"

namespace {

struct project_context_t {
  explicit project_context_t(appdata_t &a, project_t *p, bool n, const std::vector<project_t *> &j, GtkWidget *dlg);
  project_context_t() O2G_DELETED_FUNCTION;
  project_context_t(const project_context_t &) O2G_DELETED_FUNCTION;
  project_context_t &operator=(const project_context_t &) O2G_DELETED_FUNCTION;
#if __cplusplus >= 201103L
  project_context_t(project_context_t &&) = delete;
  project_context_t &operator=(project_context_t &&) = delete;
#endif
  ~project_context_t()
  { delete area_edit; }

  project_t * const project;
  appdata_t &appdata;
  GtkWidget * const dialog;
  GtkWidget * const fsizehdr, * const fsize, * const diff_stat, * const diff_remove;
  GtkWidget * const desc, * const download;
  GtkWidget * const minlat, * const minlon, * const maxlat, * const maxlon;
  const bool is_new;
#ifdef SERVER_EDITABLE
  GtkWidget * const server;
#endif
  area_edit_t *area_edit;
  const std::vector<project_t *> &projects;
};

void pos_lat_label_set(GtkWidget *label, pos_float_t lat)
{
  char str[32];
  pos_lat_str(str, sizeof(str), lat);
  gtk_label_set_text(GTK_LABEL(label), str);
}

void pos_lon_label_set(GtkWidget *label, pos_float_t lon)
{
  char str[32];
  pos_lon_str(str, sizeof(str), lon);
  gtk_label_set_text(GTK_LABEL(label), str);
}

void boundsUpdated(project_context_t *context)
{
  const pos_area &bounds = context->project->bounds;

  pos_lat_label_set(context->minlat, bounds.min.lat);
  pos_lon_label_set(context->minlon, bounds.min.lon);
  pos_lat_label_set(context->maxlat, bounds.max.lat);
  pos_lon_label_set(context->maxlon, bounds.max.lon);

  bool pos_valid =  bounds.valid();
  gtk_widget_set_sensitive(context->download, pos_valid ? TRUE : FALSE);
}

/* create a left aligned label (normal ones are centered) */
GtkWidget *gtk_label_left_new(const char *str = nullptr)
{
  GtkWidget *label = gtk_label_new(str);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, .5f);
  return label;
}

}

project_context_t::project_context_t(appdata_t &a, project_t *p, bool n,
                                     const std::vector<project_t *> &j, GtkWidget *dlg)
  : project(p)
  , appdata(a)
  , dialog(dlg)
  , fsizehdr(gtk_label_left_new(_("Map data:")))
  , fsize(gtk_label_left_new())
  , diff_stat(gtk_label_left_new())
  , diff_remove(osm2go_platform::button_new_with_label(_("Undo all")))
  , desc(osm2go_platform::entry_new())
  , download(osm2go_platform::button_new_with_label(_("Download")))
  , minlat(gtk_label_new(nullptr))
  , minlon(gtk_label_new(nullptr))
  , maxlat(gtk_label_new(nullptr))
  , maxlon(gtk_label_new(nullptr))
  , is_new(n)
#ifdef SERVER_EDITABLE
  , server(entry_new(osm2go_platform::EntryFlagsNoAutoCap))
#endif
  , area_edit(nullptr)
  , projects(j)
{
}

namespace {

struct select_context_t {
  select_context_t(appdata_t &a, GtkWidget *dial);
  select_context_t() O2G_DELETED_FUNCTION;
  select_context_t(const select_context_t &) O2G_DELETED_FUNCTION;
  select_context_t &operator=(const select_context_t &) O2G_DELETED_FUNCTION;
#if __cplusplus >= 201103L
  select_context_t(select_context_t &&) = delete;
  select_context_t &operator=(select_context_t &&) = delete;
#endif
  ~select_context_t();

  appdata_t &appdata;
  std::vector<project_t *> projects;
  GtkWidget * const dialog;
  GtkTreeModelFilter *filter;
  GtkWidget *list;
  std::unique_ptr<GtkListStore, g_object_deleter> store;
};

bool project_edit(select_context_t *scontext, project_t *project, bool is_new);

/* ------------ project selection dialog ------------- */

enum {
  PROJECT_COL_NAME = 0,
  PROJECT_COL_STATUS,
  PROJECT_COL_DESCRIPTION,
  PROJECT_COL_DATA,
  PROJECT_NUM_COLS
};

void
view_selected(GtkWidget *dialog, const project_t *project)
{
  /* check if the selected project also has a valid osm file */
  gboolean has_osm = project->osm_file_exists() ? TRUE : FALSE;
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT, has_osm);
}

void
changed(GtkTreeSelection *selection, gpointer userdata) {
  select_context_t *context = static_cast<select_context_t *>(userdata);

  GtkTreeModel *model = nullptr;
  GtkTreeIter iter;

  bool sel = gtk_tree_selection_get_selected(selection, &model, &iter) == TRUE;
  if(sel) {
    project_t *project = nullptr;
    gtk_tree_model_get(model, &iter, PROJECT_COL_DATA, &project, -1);
    assert(project != nullptr);

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
project_t *
project_get_selected(GtkWidget *list, GtkTreeModelFilter *filter, GtkTreeIter *iter)
{
  project_t *project = nullptr;
  GtkTreeModel *model;
  GtkTreeIter fiter;

  bool b = list_get_selected(list, &model, &fiter);
  assert(b); (void)b;
  gtk_tree_model_get(model, &fiter, PROJECT_COL_DATA, &project, -1);
  if(iter != nullptr)
    gtk_tree_model_filter_convert_iter_to_child_iter(filter, iter, &fiter);

  assert(project != nullptr);
  return project;
}

/* ------------------------- create a new project ---------------------- */

struct name_callback_context_t {
  name_callback_context_t(GtkWidget *w, fdguard &f) : dialog(w), basefd(f) {}
  name_callback_context_t() O2G_DELETED_FUNCTION;
  name_callback_context_t(const name_callback_context_t &) O2G_DELETED_FUNCTION;
  name_callback_context_t &operator=(const name_callback_context_t &) O2G_DELETED_FUNCTION;
#if __cplusplus >= 201103L
  name_callback_context_t(name_callback_context_t &&) = delete;
  name_callback_context_t &operator=(name_callback_context_t &&) = delete;
#endif
  GtkWidget *dialog;
  fdguard &basefd;
};

void
callback_modified_name(GtkWidget *widget, name_callback_context_t *context)
{
  const gchar *name = gtk_entry_get_text(GTK_ENTRY(widget));

  /* name must not contain some special chars */
  gboolean ok = FALSE;

  /* check if there's a name */
  if(name != nullptr && *name != '\0') {
    /* check if it consists of valid characters */
    if(strpbrk(name, "\\*?()\n\t\r") == nullptr) {
      /* check if such a project already exists */
      if(project_exists(context->basefd, name).empty())
        ok = TRUE;
    }
  }

  gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog),
				    GTK_RESPONSE_ACCEPT, ok);
}

/**
 * @brief query the user for a project name
 * @param parent the parent widget
 * @param oldname the current name of the project
 * @return the new project name
 * @retval std::string() the user cancelled the dialog
 *
 * This will prevent the user entering an invalid project name, which
 * includes a name that is already in use.
 */
std::string
project_name_dialog(GtkWidget *parent, const std::string &oldname)
{
  /* --------------  first choose a name for the project --------------- */
  osm2go_platform::DialogGuard dialog(gtk_dialog_new_with_buttons(_("Project name"),
                                              GTK_WINDOW(parent), GTK_DIALOG_MODAL,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              nullptr));

  GtkWidget *hbox = gtk_hbox_new(FALSE, 8);
  gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("Name:")), TRUE, TRUE, 0);

  name_callback_context_t name_context(dialog.get(), settings_t::instance()->base_path_fd);
  GtkWidget *entry = osm2go_platform::entry_new();
  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
  if(!oldname.empty())
    gtk_entry_set_text(GTK_ENTRY(entry), oldname.c_str());
  g_signal_connect(entry, "changed", G_CALLBACK(callback_modified_name), &name_context);

  gtk_box_pack_start(dialog.vbox(), hbox, TRUE, TRUE, 0);

  /* don't allow user to click ok until a valid name has been specified */
  gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_ACCEPT, FALSE);

  gtk_widget_show_all(dialog.get());
  if(GTK_RESPONSE_ACCEPT != gtk_dialog_run(dialog))
    return std::string();

  return gtk_entry_get_text(GTK_ENTRY(entry));
}

project_t *
project_new(select_context_t *context)
{
  const std::string &name = project_name_dialog(context->dialog, std::string());

  if(name.empty())
    return nullptr;

  std::unique_ptr<project_t> project(new project_t(name, settings_t::instance()->base_path));

  /* no data downloaded yet */
  project->data_dirty = true;

  /* build project osm file name */
  project->osmFile = project->name + ".osm";

  project->bounds.min = pos_t(NAN, NAN);
  project->bounds.max = pos_t(NAN, NAN);

  /* create project file on disk */
  if(!project->save(context->dialog) || !project_edit(context, project.get(), true)) {
    g_debug("creation of project '%s' cancelled, deleting", project->name.c_str());
    project_delete(project);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog), GTK_RESPONSE_ACCEPT, FALSE);
    return nullptr;
  }

  /* enable/disable edit/remove buttons */
  view_selected(context->dialog, project.get());

  return project.release();
}

/**
 * @brief get icon for the given project
 * @param current the currently active project or nullptr
 * @param project the project to check
 * @return the stock identifier
 */
const gchar *
project_get_status_icon_stock_id(project_t::ref current,
                                 const project_t *project) {
  /* is this the currently open project? */
  if(current && current->name == project->name)
    return GTK_STOCK_OPEN;
  else if(!project->osm_file_exists())
    return GTK_STOCK_DIALOG_WARNING;
  else if(project->diff_file_present())
    return GTK_STOCK_PROPERTIES;
  else
    return GTK_STOCK_FILE;

    // TODO: check for outdatedness too. Which icon to use?
}

void
on_project_new(select_context_t *context)
{
  project_t *project = project_new(context);
  if(project != nullptr) {
    context->projects.push_back(project);

    GtkTreeIter iter, fiter;
    const gchar *status_stock_id = project_get_status_icon_stock_id(context->appdata.project,
                                                                    project);
    gtk_list_store_insert_with_values(GTK_LIST_STORE(context->store.get()), &iter,
                                      context->projects.size(),
                                      PROJECT_COL_NAME,        project->name.c_str(),
                                      PROJECT_COL_STATUS,      status_stock_id,
                                      PROJECT_COL_DESCRIPTION, project->desc.c_str(),
                                      PROJECT_COL_DATA,        project,
                                      -1);

    GtkTreeSelection *selection = list_get_selection(context->list);
    if(gtk_tree_model_filter_convert_child_iter_to_iter(context->filter, &fiter, &iter) == TRUE)
      gtk_tree_selection_select_iter(selection, &fiter);
  }
}

void
on_project_delete(select_context_t *context)
{
  GtkTreeIter iter;
  project_t *project = project_get_selected(context->list, context->filter, &iter);

  if(!osm2go_platform::yes_no(_("Delete project?"),
             trstring("Do you really want to delete the project \"%1\"?").arg(project->name),
             0, context->dialog))
    return;

  /* check if we are to delete the currently open project */
  if(context->appdata.project && context->appdata.project->name == project->name) {
    if(!osm2go_platform::yes_no(_("Delete current project?"),
               _("The project you are about to delete is the one you are currently working on!\n\n"
                 "Do you want to delete it anyway?"), 0, context->dialog))
      return;

    project_close(context->appdata);
  }

  /* remove from view */
  gtk_list_store_remove(context->store.get(), &iter);

  /* de-chain entry from project list */
  const std::vector<project_t *>::iterator itEnd = context->projects.end();
  std::vector<project_t *>::iterator it = std::find(context->projects.begin(),
                                                    itEnd, project);
  if(it != itEnd)
    context->projects.erase(it);

  std::unique_ptr<project_t> proj(project);
  project_delete(proj);

  /* disable ok button button */
  gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog), GTK_RESPONSE_ACCEPT, FALSE);
}

void on_project_edit(select_context_t *context)
{
  GtkTreeIter iter;
  project_t *project = project_get_selected(context->list, context->filter, &iter);

  if(project_edit(context, project, false)) {
    /* description etc. may have changed, so update list */
    appdata_t &appdata = context->appdata;
    const gchar *status_stock_id = project_get_status_icon_stock_id(appdata.project, project);
    gtk_list_store_set(GTK_LIST_STORE(context->store.get()), &iter,
                       PROJECT_COL_NAME, project->name.c_str(),
                       PROJECT_COL_STATUS, status_stock_id,
                       PROJECT_COL_DESCRIPTION, project->desc.c_str(),
                       -1);

    /* check if we have actually editing the currently open project */
    if(appdata.project && appdata.project->name == project->name) {
      project_t *cur = appdata.project.get();

      g_debug("edited project was actually the active one!");

      /* update the currently active project also */

      /* update description */
      cur->desc = project->desc;

      // update OSM file, may have changed (gzip or not)
      cur->osmFile = project->osmFile;

      /* update server */
      cur->adjustServer(project->rserver.c_str(), settings_t::instance()->server);

      /* update coordinates */
      if(cur->bounds != project->bounds) {
        // save modified coordinates
        cur->bounds = project->bounds;

        /* if we have valid osm data loaded: save state first */
        if(cur->osm) {
          /* redraw the entire map by destroying all map items  */
          cur->diff_save();
          appdata.map->clear(map_t::MAP_LAYER_ALL);
        }

        /* and load the (hopefully) new file */
        if(cur->parse_osm()) {
          diff_restore(appdata.project, appdata.uicontrol.get());
          appdata.map->paint();
        }

        appdata.main_ui_enable();
      }
    }
  }

  /* enable/disable edit/remove buttons */
  view_selected(context->dialog, project);
}

gboolean
project_update_all_foreach(GtkTreeModel *model, GtkTreePath *, GtkTreeIter *iter, gpointer data)
{
  project_t *prj = nullptr;
  gtk_tree_model_get(model, iter, PROJECT_COL_DATA, &prj, -1);
  /* if the project was already downloaded do it again */
  assert(prj != nullptr);
  if(prj->osm_file_exists()) {
    g_debug("found %s to update", prj->name.c_str());
    if (!osm_download(GTK_WIDGET(data), prj))
      return TRUE;
  }

  return FALSE;
}

void
on_project_update_all(select_context_t *context)
{
  gtk_tree_model_foreach(GTK_TREE_MODEL(context->store.get()), project_update_all_foreach, context->dialog);
}

struct project_list_add {
  GtkListStore * const store;
  project_t::ref cur_proj;
  pos_t pos;
  bool check_pos;
  GtkTreeIter &seliter;
  bool &has_sel;
  project_list_add(GtkListStore *s, appdata_t &a, GtkTreeIter &l, bool &h)
    : store(s), cur_proj(a.project), pos(a.gps_state->get_pos()), check_pos(pos.valid())
    , seliter(l), has_sel(h) {}
  void operator()(const project_t *project);
};

void project_list_add::operator()(const project_t *project)
{
  GtkTreeIter iter;
  const gchar *status_stock_id = project_get_status_icon_stock_id(
                                         cur_proj, project);
  /* Append a row and fill in some data */
  gtk_list_store_insert_with_values(store, &iter, -1,
                                    PROJECT_COL_NAME,        project->name.c_str(),
                                    PROJECT_COL_STATUS,      status_stock_id,
                                    PROJECT_COL_DESCRIPTION, project->desc.c_str(),
                                    PROJECT_COL_DATA,        project,
                                    -1);

  /* decide if to select this project because it matches the current position */
  if(check_pos && project->bounds.contains(pos)) {
    seliter = iter;
    has_sel = true;
    check_pos = false;
  }
}

gboolean
project_matches(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  gchar *pname, *pdescr;

  GtkEntry *entry = static_cast<GtkEntry *>(data);
  const gchar *text = gtk_entry_get_text(entry);
  if(text == nullptr || *text == '\0')
    return TRUE;

  gtk_tree_model_get(model, iter, PROJECT_COL_NAME, &pname,
                                  PROJECT_COL_DESCRIPTION, &pdescr, -1);
  g_string name(pname);
  g_string descr(pdescr);

  if(name && strcasestr(name.get(), text) != nullptr)
    return TRUE;
  if(descr && strcasestr(descr.get(), text) != nullptr)
    return TRUE;

  return FALSE;
}

void
on_filter_changed(gpointer user_data)
{
  select_context_t *context = static_cast<select_context_t *>(user_data);
  gtk_tree_model_filter_refilter(context->filter);

  GtkTreeModel *fmodel = reinterpret_cast<GtkTreeModel *>(context->filter);
  if(gtk_tree_model_iter_n_children(fmodel, nullptr) == 1) {
    GtkTreeIter iter;
    gboolean b = gtk_tree_model_get_iter_first(fmodel, &iter);
    assert(b == TRUE);
    gtk_tree_selection_select_iter(list_get_selection(context->list), &iter);
  }
}

/**
 * @brief create a widget to list the projects
 * @param context the context struct
 * @param has_sel if an item has been selected
 */
GtkWidget *
project_list_widget(select_context_t &context, bool &has_sel)
{
  std::vector<list_view_column> columns;
  columns.push_back(list_view_column(_("Name"), 0));
  columns.push_back(list_view_column(_("State"), LIST_FLAG_STOCK_ICON));
  columns.push_back(list_view_column(_("Description"), LIST_FLAG_ELLIPSIZE));

  std::vector<list_button> buttons;
  buttons.push_back(list_button(_("_New"), G_CALLBACK(on_project_new)));
  buttons.push_back(list_button(_("_Edit"), G_CALLBACK(on_project_edit)));
  buttons.push_back(list_button(_("Remove"), G_CALLBACK(on_project_delete)));
  buttons.push_back(list_button(_("Update all"), G_CALLBACK(on_project_update_all)));

  /* build the store */
  context.store.reset(gtk_list_store_new(PROJECT_NUM_COLS,
                                         G_TYPE_STRING,    // name
                                         G_TYPE_STRING,    // status
                                         G_TYPE_STRING,    // desc
                                         G_TYPE_POINTER));  // data

  context.filter = reinterpret_cast<GtkTreeModelFilter *>(
            gtk_tree_model_filter_new(GTK_TREE_MODEL(context.store.get()), nullptr));

  context.list = list_new(LIST_HILDON_WITHOUT_HEADERS, 0, &context, changed,
                          buttons, columns, GTK_TREE_MODEL(context.filter));

  GtkTreeIter seliter;
  std::for_each(context.projects.begin(), context.projects.end(),
                project_list_add(context.store.get(), context.appdata, seliter, has_sel));

  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(context.store.get()),
                                       PROJECT_COL_NAME, GTK_SORT_ASCENDING);

  if(has_sel) {
    GtkTreeIter fiter;
    if(gtk_tree_model_filter_convert_child_iter_to_iter(context.filter, &fiter, &seliter) == TRUE)
      list_scroll(context.list, &fiter);
  }

  GtkWidget *entry = osm2go_platform::entry_new(osm2go_platform::EntryFlagsNoAutoCap);
  gtk_box_pack_start(GTK_BOX(context.list), entry, FALSE, FALSE, 0);
  gtk_box_reorder_child(GTK_BOX(context.list), entry, 1);
  gtk_tree_model_filter_set_visible_func(context.filter, project_matches, entry, nullptr);
  gtk_widget_grab_focus(entry);

  g_signal_connect_swapped(entry, "changed", G_CALLBACK(on_filter_changed), &context);

  return context.list;
}

/* ---------------------------------------------------- */

void
project_filesize(project_context_t *context)
{
  const char *str = nullptr;
  trstring gstr;
  const project_t * const project = context->project;

  g_debug("Checking size of %s", project->osmFile.c_str());

  bool en = !context->is_new;
  struct stat st;
  bool stret = fstatat(project->dirfd, project->osmFile.c_str(), &st, 0) == 0 &&
               S_ISREG(st.st_mode);
  const GdkColor *color;
  if(!stret && errno == ENOENT) {
    color = osm2go_platform::invalid_text_color();

    str = _("Not downloaded!");
  } else {
    color = nullptr;

    if(!project->data_dirty) {
      if(stret) {
        struct tm loctime;
        localtime_r(&st.st_mtim.tv_sec, &loctime);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%x %X", &loctime);
        gstr = trstring("%1 bytes present\nfrom %2").arg(st.st_size).arg(time_str);

        if(project->osmFile.size() > 3 && project->osmFile.compare(project->osmFile.size() - 3, 3, ".gz") == 0)
          gtk_label_set_text(GTK_LABEL(context->fsizehdr), _("Map data:\n(compressed)"));
        else
          gtk_label_set_text(GTK_LABEL(context->fsizehdr), _("Map data:"));
        str = static_cast<const gchar *>(gstr);
      } else {
        str = _("Error testing data file");
      }
    } else
      str = _("Outdated, please download!");

    en = en || !project->data_dirty;
  }
  gtk_widget_modify_fg(context->fsize, GTK_STATE_NORMAL, color);
  gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog), GTK_RESPONSE_ACCEPT, en ? TRUE : FALSE);

  if(str != nullptr)
    gtk_label_set_text(GTK_LABEL(context->fsize), str);
}

/* a project may currently be open. "unsaved changes" then also */
/* means that the user may have unsaved changes */
bool
active_n_dirty(appdata_t &appdata, const project_t *project)
{
  if(appdata.project && appdata.project->osm && appdata.project->name == project->name) {
    g_debug("editing the currently open project");

    return !appdata.project->osm->is_clean(true);
  }

  return false;
}

void
project_diffstat(project_context_t &context)
{
  const char *str;

  if(context.project->diff_file_present() || active_n_dirty(context.appdata, context.project)) {
    /* this should prevent the user from changing the area */
    str = _("unsaved changes pending");
  } else
    str = _("no pending changes");

  gtk_label_set_text(GTK_LABEL(context.diff_stat), str);
}

void
on_edit_clicked(project_context_t *context)
{
  project_t * const project = context->project;

  if(project->diff_file_present() || active_n_dirty(context->appdata, project))
    message_dlg(_("Pending changes"),
                _("You have pending changes in this project.\n\nChanging "
                  "the area may cause pending changes to be "
                  "lost if they are outside the updated area."), context->dialog);

  if(context->area_edit == nullptr)
    context->area_edit = new area_edit_t(context->appdata.gps_state.get(), project->bounds,
                                         context->dialog);

  context->area_edit->other_bounds.clear();
  std::for_each(context->projects.begin(), context->projects.end(),
                projects_to_bounds(context->area_edit->other_bounds));

  if(context->area_edit->run()) {
    g_debug("coordinates changed!");

    /* the wms layer isn't usable with new coordinates */
    wms_remove_file(*project);

    boundsUpdated(context);

    /* (re-) download area */
    if(project->bounds.valid() && osm_download(GTK_WIDGET(context->dialog), project))
      project->data_dirty = false;
    project_filesize(context);
  }
}

void
on_download_clicked(project_context_t *context)
{
  project_t * const project = context->project;

  if(osm_download(context->dialog, project))
    project->data_dirty = false;

  project_filesize(context);
}

void
on_rename_clicked(project_context_t *context)
{
  project_t * const project = context->project;

  const std::string &name = project_name_dialog(context->dialog, project->name);

  if(name.empty() || name == project->name)
    return;

  project_t::ref openProject = context->appdata.project;
  const bool isOpen = openProject && openProject->name == project->name;

  if(!project->rename(name, openProject, context->dialog))
    return;

  gtk_window_set_title(GTK_WINDOW(context->dialog),
                       static_cast<const gchar *>(trstring("Edit project - %1").arg(project->name)));

  if(!isOpen)
    return;

  context->appdata.set_title();
}

void
on_diff_remove_clicked(project_context_t *context)
{
  const project_t * const project = context->project;

  g_debug("clicked diff remove");

  if(osm2go_platform::yes_no(_("Discard changes?"), _("Do you really want to discard your changes? This will "
                                     "permanently undo all changes you have made so far and which "
                                     "you did not upload yet."), 0, context->dialog)) {
    project->diff_remove_file();

    /* if this is the currently open project, we need to undo */
    /* the map changes as well */
    appdata_t &appdata = context->appdata;

    if(appdata.project && appdata.project->name == project->name) {
      g_debug("undo all on current project: delete map changes as well");

      /* just reload the map */
      appdata.map->clear(map_t::MAP_LAYER_OBJECTS_ONLY);
      if(appdata.project->parse_osm())
        appdata.map->paint();
    }

    /* update button/label state */
    project_diffstat(*context);
    gtk_widget_set_sensitive(context->diff_remove,  FALSE);
  }
}

bool
project_edit(select_context_t *scontext, project_t *project, bool is_new)
{
  GtkWidget *parent = scontext->dialog;

  if(project->check_demo(parent))
    return false;

  /* ------------ project edit dialog ------------- */

  osm2go_platform::DialogGuard dialog;
  /* cancel is enabled for "new" projects only */
  if(is_new) {
    const trstring str = trstring("New project - %1").arg(project->name);

    dialog.reset(gtk_dialog_new_with_buttons(static_cast<const gchar *>(str), GTK_WINDOW(parent),
                                             GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                             GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, nullptr));
  } else {
    const trstring str = trstring("Edit project - %1").arg(project->name);

    dialog.reset(gtk_dialog_new_with_buttons(static_cast<const gchar *>(str), GTK_WINDOW(parent),
                                             GTK_DIALOG_MODAL, GTK_STOCK_CLOSE,
                                             GTK_RESPONSE_ACCEPT, nullptr));
  }
  osm2go_platform::dialog_size_hint(dialog, osm2go_platform::MISC_DIALOG_WIDE);

  project_context_t context(scontext->appdata, project, is_new, scontext->projects, dialog.get());

  gtk_dialog_set_default_response(dialog, GTK_RESPONSE_ACCEPT);

  GtkWidget *table = gtk_table_new(5, 5, FALSE);  // x, y
  gtk_table_set_col_spacing(GTK_TABLE(table), 0, 8);
  gtk_table_set_col_spacing(GTK_TABLE(table), 3, 8);

  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_left_new(_("Description:")), 0, 1, 0, 1);
  gtk_entry_set_activates_default(GTK_ENTRY(context.desc), TRUE);
  if(!project->desc.empty())
    gtk_entry_set_text(GTK_ENTRY(context.desc), project->desc.c_str());
  gtk_table_attach_defaults(GTK_TABLE(table),  context.desc, 1, 4, 0, 1);

  GtkWidget *renameBtn = osm2go_platform::button_new_with_label(_("Rename"));
  gtk_table_attach_defaults(GTK_TABLE(table), renameBtn, 4, 5, 0, 1);
  g_signal_connect_swapped(renameBtn, "clicked", G_CALLBACK(on_rename_clicked), &context);
  gtk_table_set_row_spacing(GTK_TABLE(table), 0, 4);

  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_left_new(_("Latitude:")), 0, 1, 1, 2);
  gtk_table_attach_defaults(GTK_TABLE(table), context.minlat, 1, 2, 1, 2);
  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("to")), 2, 3, 1, 2);
  gtk_table_attach_defaults(GTK_TABLE(table), context.maxlat, 3, 4, 1, 2);

  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_left_new(_("Longitude:")), 0, 1, 2, 3);
  gtk_table_attach_defaults(GTK_TABLE(table), context.minlon, 1, 2, 2, 3);
  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("to")), 2, 3, 2, 3);
  gtk_table_attach_defaults(GTK_TABLE(table), context.maxlon, 3, 4, 2, 3);

  GtkWidget *edit = osm2go_platform::button_new_with_label(_("Edit"));
  g_signal_connect_swapped(edit, "clicked", G_CALLBACK(on_edit_clicked), &context);
  gtk_table_attach(GTK_TABLE(table), edit, 4, 5, 1, 3,
                   static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
                   static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),0,0);

  gtk_table_set_row_spacing(GTK_TABLE(table), 2, 4);

#ifdef SERVER_EDITABLE
  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_left_new(_("Server:")), 0, 1, 3, 4);
  gtk_entry_set_activates_default(GTK_ENTRY(context.server), TRUE);
  gtk_entry_set_text(GTK_ENTRY(context.server),
                     project->server(settings_t::instance()->server).c_str());
  gtk_table_attach_defaults(GTK_TABLE(table),  context.server, 1, 4, 3, 4);

  gtk_table_set_row_spacing(GTK_TABLE(table), 3, 4);
#endif

  gtk_table_attach_defaults(GTK_TABLE(table), context.fsizehdr, 0, 1, 4, 5);
  project_filesize(&context);
  gtk_table_attach_defaults(GTK_TABLE(table), context.fsize, 1, 4, 4, 5);
  g_signal_connect_swapped(context.download, "clicked",
                           G_CALLBACK(on_download_clicked), &context);

  gtk_table_attach_defaults(GTK_TABLE(table), context.download, 4, 5, 4, 5);

  gtk_table_set_row_spacing(GTK_TABLE(table), 4, 4);

  gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_left_new(_("Changes:")), 0, 1, 5, 6);
  project_diffstat(context);
  gtk_table_attach_defaults(GTK_TABLE(table), context.diff_stat, 1, 4, 5, 6);
  if(!project->diff_file_present() && !active_n_dirty(context.appdata, project))
    gtk_widget_set_sensitive(context.diff_remove,  FALSE);
  g_signal_connect_swapped(context.diff_remove, "clicked",
                           G_CALLBACK(on_diff_remove_clicked), &context);
  gtk_table_attach_defaults(GTK_TABLE(table), context.diff_remove, 4, 5, 5, 6);

  /* ---------------------------------------------------------------- */

  gtk_box_pack_start(dialog.vbox(), table, TRUE, TRUE, 0);

  /* disable "ok" if there's no valid file downloaded */
  if(is_new)
    gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_ACCEPT,
                                      project->osm_file_exists() ? TRUE : FALSE);

  boundsUpdated(&context);

  gtk_widget_show_all(dialog.get());

  /* the return value may actually be != ACCEPT, but only if the editor */
  /* is run for a new project which is completely removed afterwards if */
  /* cancel has been selected */
  bool ok = (GTK_RESPONSE_ACCEPT == gtk_dialog_run(dialog));

  /* transfer values from edit dialog into project structure */

  /* fetch values from dialog */
  const gchar *ndesc = gtk_entry_get_text(GTK_ENTRY(context.desc));
  if(ndesc != nullptr && *ndesc != '\0')
    project->desc = ndesc;
  else
    project->desc.clear();

#ifdef SERVER_EDITABLE
  context.project->adjustServer(gtk_entry_get_text(GTK_ENTRY(context.server)),
                                settings_t::instance()->server);
#endif

  project->save(dialog.get());

  return ok;
}

}

project_t *project_select(appdata_t &appdata)
{
  select_context_t context(appdata,
                    gtk_dialog_new_with_buttons(_("Project selection"),
                                    GTK_WINDOW(appdata_t::window), GTK_DIALOG_MODAL,
                                    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                    nullptr));

  osm2go_platform::dialog_size_hint(GTK_WINDOW(context.dialog), osm2go_platform::MISC_DIALOG_MEDIUM);

  /* under fremantle the dialog does not have an "Open" button */
  /* as it's closed when a project is being selected */
  gtk_dialog_set_default_response(GTK_DIALOG(context.dialog),
				  GTK_RESPONSE_ACCEPT);

  bool has_sel = false;
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
                     project_list_widget(context, has_sel), TRUE, TRUE, 0);

  /* don't all user to click ok until something is selected */
  gtk_dialog_set_response_sensitive(GTK_DIALOG(context.dialog),
                                    GTK_RESPONSE_ACCEPT, has_sel ? TRUE : FALSE);

  gtk_widget_show_all(context.dialog);
  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context.dialog))) {
    project_t *pr = project_get_selected(context.list, nullptr, nullptr);

    return new project_t(*pr);
  }

  return nullptr;
}

select_context_t::select_context_t(appdata_t &a, GtkWidget *dial)
  : appdata(a)
  , projects(project_scan(settings_t::instance()->base_path,
                          settings_t::instance()->base_path_fd, settings_t::instance()->server))
  , dialog(dial)
  , filter(nullptr)
  , list(nullptr)
{
}

select_context_t::~select_context_t()
{
  std::for_each(projects.begin(), projects.end(), std::default_delete<project_t>());

  gtk_widget_destroy(dialog);
}
