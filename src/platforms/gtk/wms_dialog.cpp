/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <wms.h>
#include <wms_p.h>

#include "list.h"
#include <map.h>
#include <misc.h>
#include <project.h>
#include <settings.h>
#include <uicontrol.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <map>
#include <memory>
#include <strings.h>
#include <vector>
#ifdef FREMANTLE
#include <hildon/hildon-picker-dialog.h>
#endif

#include <osm2go_annotations.h>
#include <osm2go_stl.h>
#include <osm2go_i18n.h>
#include "osm2go_platform.h"
#include "osm2go_platform_gtk.h"

/* ---------------------- use ------------------- */

namespace {

struct find_wms_functor {
  const char *name;
  explicit inline find_wms_functor(const char *n) : name(n) {}
  inline bool operator()(const wms_server_t *srv) const
  {
    return srv->name == name;
  }
};

enum {
  WMS_SERVER_COL_NAME = 0,
  WMS_SERVER_COL_DATA,
  WMS_SERVER_NUM_COLS
};

struct wms_server_context_t {
  inline wms_server_context_t(const std::string &ws, GtkWidget *d)
    : wms_server(ws), dialog(d), list(nullptr) , server_label(nullptr) {}
  wms_server_context_t() O2G_DELETED_FUNCTION;
  wms_server_context_t(const wms_server_context_t &) O2G_DELETED_FUNCTION;
  wms_server_context_t &operator=(const wms_server_context_t &) O2G_DELETED_FUNCTION;
#if __cplusplus >= 201103L
  wms_server_context_t(wms_server_context_t &&) = delete;
  wms_server_context_t &operator=(wms_server_context_t &&) = delete;
  ~wms_server_context_t() = default;
#endif

  const std::string &wms_server;
  GtkWidget * const dialog, *list;
  std::unique_ptr<GtkListStore, g_object_deleter> store;
  GtkWidget *server_label;

  /**
   * @brief select the server referenced in wms in the treeview
   * @returns the matching entry in the settings list
   */
  const wms_server_t * __attribute__ ((warn_unused_result)) select_server() const;
};

wms_server_t *
get_selection(GtkTreeSelection *selection)
{
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  if(gtk_tree_selection_get_selected(selection, &model, &iter) == TRUE) {
    wms_server_t *wms_server;
    gtk_tree_model_get(model, &iter, WMS_SERVER_COL_DATA, &wms_server, -1);
    assert(wms_server != nullptr);
    return wms_server;
  }

  return nullptr;
}

struct server_select_context {
  server_select_context(GtkTreeSelection *sel, const std::string &defsrv)
    : selection(sel), defserver(defsrv), server(nullptr) {}

  GtkTreeSelection *selection;
  const std::string &defserver;
  wms_server_t *server;
};

gboolean
server_select_foreach(GtkTreeModel *model, GtkTreePath *, GtkTreeIter *iter, gpointer data)
{
  server_select_context * const ctx = static_cast<server_select_context *>(data);
  wms_server_t *server = nullptr;
  gtk_tree_model_get(model, iter, WMS_SERVER_COL_DATA, &server, -1);
  assert(server != nullptr);

  if(ctx->defserver == server->server) {
    gtk_tree_selection_select_iter(ctx->selection, iter);
    ctx->server = server;
    return TRUE;
  }

  return FALSE;
}

const wms_server_t *wms_server_context_t::select_server() const
{
  if(wms_server.empty())
    return nullptr;

  /* if the projects settings match a list entry, then select this */
  server_select_context ctx(list_get_selection(list), wms_server);

  gtk_tree_model_foreach(GTK_TREE_MODEL(store.get()), server_select_foreach, &ctx);

  return ctx.server;
}

void
wms_server_selected(wms_server_context_t *context, const wms_server_t *selected)
{
  list_button_enable(context->list, LIST_BUTTON_REMOVE, selected != nullptr);
  list_button_enable(context->list, LIST_BUTTON_EDIT, selected != nullptr);

  /* user can click ok if a entry is selected or if both fields are */
  /* otherwise valid */
  gboolean en;
  const std::string *s;
  if(selected != nullptr) {
    en = TRUE;
    s = &selected->server;
  } else {
    en = context->wms_server.empty() ? FALSE : TRUE;
    s = &context->wms_server;
  }

  gtk_dialog_set_response_sensitive(GTK_DIALOG(context->dialog), GTK_RESPONSE_ACCEPT, en);

  gtk_label_set_text(GTK_LABEL(context->server_label), s->c_str());
}

void
wms_server_changed(GtkTreeSelection *selection, gpointer userdata) {
  wms_server_context_t *context = static_cast<wms_server_context_t *>(userdata);

  wms_server_t *wms_server = get_selection(selection);
  if(wms_server != nullptr)
    wms_server_selected(context, wms_server);
}

void
on_server_remove(wms_server_context_t *context)
{
  GtkTreeSelection *selection = list_get_selection(context->list);
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  if(gtk_tree_selection_get_selected(selection, &model, &iter) == TRUE) {
    wms_server_t *server = nullptr;
    gtk_tree_model_get(model, &iter, WMS_SERVER_COL_DATA, &server, -1);

    assert(server != nullptr);

    /* de-chain */
    g_debug("de-chaining server %s", server->name.c_str());
    settings_t::ref settings = settings_t::instance();
    std::vector<wms_server_t *> &servers = settings->wms_server;
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

void
callback_modified_name(GtkWidget *widget)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
  if(!GTK_IS_DIALOG(toplevel))
    return;

  const gchar *name = gtk_entry_get_text(GTK_ENTRY(widget));

  /* search all entries except the last (which is the one we are editing) */
  settings_t::ref settings = settings_t::instance();
  std::vector<wms_server_t *> &servers = settings->wms_server;
  const std::vector<wms_server_t *>::iterator itEnd = std::prev(servers.end());
  bool uniqueName = std::none_of(servers.begin(), itEnd, find_wms_functor(name));

  gtk_dialog_set_response_sensitive(GTK_DIALOG(toplevel), GTK_RESPONSE_ACCEPT, uniqueName ? TRUE : FALSE);
}

/* edit url and path of a given wms server entry */
bool wms_server_edit(wms_server_context_t *context, bool edit_name, wms_server_t *wms_server) {
  osm2go_platform::DialogGuard dialog(gtk_dialog_new_with_buttons(static_cast<const gchar *>(_("Edit WMS Server")),
                                      GTK_WINDOW(context->dialog), GTK_DIALOG_MODAL,
                                      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, nullptr));

  osm2go_platform::dialog_size_hint(dialog, osm2go_platform::MISC_DIALOG_WIDE);
  gtk_dialog_set_default_response(dialog, GTK_RESPONSE_ACCEPT);

  GtkWidget *label = gtk_label_new(_("Name:"));
  GtkWidget *name = osm2go_platform::entry_new(osm2go_platform::EntryFlagsNoAutoCap);
  GtkWidget *table = gtk_table_new(2, 3, FALSE);

  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
		   GTK_FILL, static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), name, 1, 2, 0, 1);
  gtk_entry_set_activates_default(GTK_ENTRY(name), TRUE);
  gtk_widget_set_sensitive(GTK_WIDGET(name), edit_name ? TRUE : FALSE);
  g_signal_connect(name, "changed", G_CALLBACK(callback_modified_name), nullptr);

  label = gtk_label_new(_("Server:"));
  GtkWidget *server = osm2go_platform::entry_new(osm2go_platform::EntryFlagsNoAutoCap);
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
		   GTK_FILL, static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), server, 1, 2, 1, 2);
  gtk_entry_set_activates_default(GTK_ENTRY(server), TRUE);

  osm2go_platform::setEntryText(GTK_ENTRY(name), wms_server->name.c_str(), _("<service name>"));
  osm2go_platform::setEntryText(GTK_ENTRY(server), wms_server->server.c_str(), _("<server url>"));

  gtk_box_pack_start(dialog.vbox(), table, TRUE, TRUE, 0);

  gtk_widget_show_all(dialog.get());

  const bool ret = (GTK_RESPONSE_ACCEPT == gtk_dialog_run(dialog));
  if(ret) {
    if(edit_name)
      wms_server->name = gtk_entry_get_text(GTK_ENTRY(name));

    wms_server->server = gtk_entry_get_text(GTK_ENTRY(server));
    g_debug("setting URL for WMS server %s to %s", wms_server->name.c_str(), wms_server->server.c_str());

    /* set texts below */
    gtk_label_set_text(GTK_LABEL(context->server_label), wms_server->server.c_str());
  }

  return ret;
}

/* user clicked "edit..." button in the wms server list */
void
on_server_edit(wms_server_context_t *context)
{
  wms_server_t *server = get_selection(list_get_selection(context->list));
  assert(server != nullptr);

  wms_server_edit(context, false, server);
}

struct store_fill_functor {
  GtkListStore * const store;
  explicit inline store_fill_functor(GtkListStore *s) : store(s) {}
  inline GtkTreeIter operator()(const wms_server_t *srv) const;
};

GtkTreeIter store_fill_functor::operator()(const wms_server_t *srv) const
{
  GtkTreeIter iter;

  gtk_list_store_insert_with_values(store, &iter, -1,
                                    WMS_SERVER_COL_NAME, srv->name.c_str(),
                                    WMS_SERVER_COL_DATA, srv,
                                    -1);

  return iter;
}

/* user clicked "add..." button in the wms server list */
void
on_server_add(wms_server_context_t *context)
{
  std::unique_ptr<wms_server_t> newserver(std::make_unique<wms_server_t>());
  // in case the project has a server set, but the global list is empty,
  // fill the data of the project server
  settings_t::ref settings(settings_t::instance());
  std::vector<wms_server_t *> &servers = settings->wms_server;
  if(servers.empty() && !context->wms_server.empty())
    newserver->server = context->wms_server;

  if(wms_server_edit(context, true, newserver.get())) {
    /* attach a new server item to the chain */
    servers.push_back(newserver.release());

    GtkTreeIter iter = store_fill_functor(context->store.get())(servers.back());

    GtkTreeSelection *selection = list_get_selection(context->list);
    gtk_tree_selection_select_iter(selection, &iter);

    wms_server_selected(context, servers.back());
  }
}

/* widget to select a wms server from a list */
GtkWidget *
wms_server_widget(wms_server_context_t *context)
{
  std::vector<list_button> buttons;
  buttons.push_back(list_button::addButton(G_CALLBACK(on_server_add)));
  buttons.push_back(list_button::editButton(G_CALLBACK(on_server_edit)));
  buttons.push_back(list_button::removeButton(G_CALLBACK(on_server_remove)));

  context->store.reset(gtk_list_store_new(WMS_SERVER_NUM_COLS,
                                          G_TYPE_STRING, G_TYPE_POINTER));

  context->list = list_new(LIST_HILDON_WITHOUT_HEADERS, context,
                           wms_server_changed, buttons,
                           std::vector<list_view_column>(1, list_view_column(_("Name"), LIST_FLAG_ELLIPSIZE)),
                           GTK_TREE_MODEL(context->store.get()));

  settings_t::ref settings = settings_t::instance();
  const std::vector<wms_server_t *> &servers = settings->wms_server;
  std::for_each(servers.begin(), servers.end(), store_fill_functor(context->store.get()));

  return context->list;
}

} // namespace

std::string
wms_server_dialog(osm2go_platform::Widget *parent, const std::string &wms_server)
{
  wms_server_context_t context(wms_server,
                               gtk_dialog_new_with_buttons(static_cast<const gchar *>(_("WMS Server Selection")),
                                                           GTK_WINDOW(parent),
                                                           GTK_DIALOG_MODAL,
                                                           GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                                           GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                                           nullptr));

  /* server selection box */
  osm2go_platform::dialog_size_hint(GTK_WINDOW(context.dialog), osm2go_platform::MISC_DIALOG_MEDIUM);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
                     wms_server_widget(&context), TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
		     gtk_hseparator_new(), FALSE, FALSE, 0);

  GtkWidget *label;
  GtkWidget *table = gtk_table_new(2, 2, FALSE);  // x, y
  gtk_table_set_col_spacing(GTK_TABLE(table), 0, 10);

  label = gtk_label_new(_("Server:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL,
                   static_cast<GtkAttachOptions>(0), 0, 0);
  context.server_label = gtk_label_new(nullptr);
  gtk_label_set_ellipsize(GTK_LABEL(context.server_label),
			  PANGO_ELLIPSIZE_MIDDLE);
  gtk_misc_set_alignment(GTK_MISC(context.server_label), 0.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), context.server_label,
			    1, 2, 0, 1);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
		     table, FALSE, FALSE, 0);

  wms_server_selected(&context, context.select_server());

  gtk_widget_show_all(context.dialog);

  std::string ret;
  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(context.dialog))) {
    const wms_server_t *server = get_selection(list_get_selection(context.list));
    if(server != nullptr) {
      /* fetch parameters from selected entry */
      g_debug("WMS: using %s", server->name.c_str());
      ret = server->server;
    } else {
      ret = wms_server;
    }
  }

  gtk_widget_destroy(context.dialog);

  return ret;
}

namespace {

#ifdef FREMANTLE
#define DIALOG_RESULT_OK GTK_RESPONSE_OK
#else
#define DIALOG_RESULT_OK GTK_RESPONSE_ACCEPT
#endif

void
layer_changed(GtkWidget *widget)
{
  gboolean okEn = osm2go_platform::select_widget_has_selection(widget) ? TRUE : FALSE;

  GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
                                    DIALOG_RESULT_OK, okEn);
}

enum {
  LAYER_COL_TITLE = 0,
  LAYER_COL_NAME,
  LAYER_NUM_COLS
};

struct fitting_layers_functor {
  GtkListStore * const store;
  const pos_area &bounds;
  inline fitting_layers_functor(GtkListStore *s, const pos_area &b)
    : store(s), bounds(b) {}
  void operator()(const wms_layer_t &layer);
};

void fitting_layers_functor::operator()(const wms_layer_t &layer)
{
  if(!layer.llbbox.valid || !wms_llbbox_fits(bounds, layer.llbbox))
    return;

  /* Append a row and fill in some data */
  gtk_list_store_insert_with_values(store, nullptr, -1,
                                    LAYER_COL_TITLE, layer.title.c_str(),
                                    LAYER_COL_NAME, layer.name.c_str(),
                                    -1);
}

GtkWidget *
wms_layer_widget(const pos_area &bounds, const wms_layer_t::list &layers)
{
  /* build the store */
  std::unique_ptr<GtkListStore, g_object_deleter> store(gtk_list_store_new(LAYER_NUM_COLS,
      G_TYPE_STRING, G_TYPE_STRING));
  std::for_each(layers.begin(), layers.end(), fitting_layers_functor(store.get(), bounds));

  GtkWidget *widget = osm2go_platform::select_widget(GTK_TREE_MODEL(store.get()),
                                                     osm2go_platform::AllowMultiSelection, ',');

#ifdef FREMANTLE
  g_signal_connect_swapped(widget,
#else
  g_signal_connect_swapped(gtk_tree_view_get_selection(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(widget)))),
#endif
                           "changed", G_CALLBACK(layer_changed), widget);

  return widget;
}

} // namespace

std::string
wms_layer_dialog(osm2go_platform::Widget *parent, const pos_area &bounds, const wms_layer_t::list &layers)
{
  GtkWidget *sel_widget = wms_layer_widget(bounds, layers);
  osm2go_platform::DialogGuard dialog(
#ifdef FREMANTLE
                                      hildon_picker_dialog_new(GTK_WINDOW(parent)));
  hildon_picker_dialog_set_selector(HILDON_PICKER_DIALOG(dialog.get()),
                                    HILDON_TOUCH_SELECTOR(sel_widget));
#else
                                      gtk_dialog_new_with_buttons(static_cast<const gchar *>(_("WMS layer selection")),
                                              GTK_WINDOW(parent),
                                              GTK_DIALOG_MODAL,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              nullptr));

  osm2go_platform::dialog_size_hint(dialog, osm2go_platform::MISC_DIALOG_LARGE);

  /* layer list */
  gtk_box_pack_start(dialog.vbox(), sel_widget, TRUE, TRUE, 0);
#endif
  gtk_dialog_set_response_sensitive(dialog, DIALOG_RESULT_OK, FALSE);

  gtk_widget_show_all(dialog.get());

  if(DIALOG_RESULT_OK != gtk_dialog_run(dialog))
    return std::string();

  return osm2go_platform::select_widget_value(sel_widget);
}
