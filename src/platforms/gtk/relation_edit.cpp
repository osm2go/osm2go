/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "relation_edit.h"

#include <info.h>
#include <josm_presets.h>
#include "list.h"
#include <map.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <set>
#include <string>
#include <strings.h>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include "osm2go_i18n.h"
#include "osm2go_platform.h"
#include "osm2go_platform_gtk.h"

namespace {

struct relation_context_t {
  inline relation_context_t(map_t *m, osm_t::ref o, presets_items *p, GtkWidget *d)
    : map(m), osm(o), presets(p), dialog(d), list(nullptr) {}
  relation_context_t() O2G_DELETED_FUNCTION;
  relation_context_t(const relation_context_t &) O2G_DELETED_FUNCTION;
  relation_context_t &operator=(const relation_context_t &) O2G_DELETED_FUNCTION;
#if __cplusplus >= 201103L
  relation_context_t(relation_context_t &&) = delete;
  relation_context_t &operator=(relation_context_t &&) = delete;
  ~relation_context_t() = default;
#endif

  map_t * const map;
  osm_t::ref osm;
  presets_items * const presets;
  osm2go_platform::DialogGuard dialog;
  GtkWidget *list;
  std::unique_ptr<GtkListStore, g_object_deleter> store;
};

/* -------------------- global relation list ----------------- */

enum {
  RELATION_COL_TYPE = 0,
  RELATION_COL_NAME,
  RELATION_COL_MEMBERS,
  RELATION_COL_TAGS_MODIFIED,
  RELATION_COL_MEMBERS_MODIFIED,
  RELATION_COL_DATA,
  RELATION_NUM_COLS
};

relation_t *
get_selected_relation(relation_context_t *context)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  selection = list_get_selection(context->list);
  if(gtk_tree_selection_get_selected(selection, &model, &iter) == TRUE) {
    relation_t *relation;
    gtk_tree_model_get(model, &iter, RELATION_COL_DATA, &relation, -1);
    return(relation);
  }
  return nullptr;
}

void
relation_list_selected(GtkWidget *list, relation_t *selected)
{
  list_button_enable(list, LIST_BUTTON_USER0,
		     (selected != nullptr) && (!selected->members.empty()));
  list_button_enable(list, LIST_BUTTON_USER1,
		     (selected != nullptr) && (!selected->members.empty()));

  list_button_enable(list, LIST_BUTTON_REMOVE, selected != nullptr);
  list_button_enable(list, LIST_BUTTON_EDIT, selected != nullptr);
}

void
relation_list_changed(GtkTreeSelection *selection, gpointer userdata)
{
  GtkWidget *list = static_cast<relation_context_t *>(userdata)->list;
  GtkTreeModel *model = nullptr;
  GtkTreeIter iter;

  if(gtk_tree_selection_get_selected(selection, &model, &iter) == TRUE) {
    relation_t *relation = nullptr;
    gtk_tree_model_get(model, &iter, RELATION_COL_DATA, &relation, -1);
    relation_list_selected(list, relation);
  }
}

/* user clicked "members" button in relation list */
void
on_relation_members(relation_context_t *context)
{
  relation_t *sel = get_selected_relation(context);

  if(sel != nullptr)
    relation_show_members(context->dialog.get(), sel, context->osm);
}

/* user clicked "select" button in relation list */
void
on_relation_select(relation_context_t *context, GtkWidget *but)
{
  relation_t *sel = get_selected_relation(context);
  context->map->item_deselect();

  if(sel != nullptr) {
    context->map->select_relation(sel);

    /* tell dialog to close as we want to see the selected relation */

    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(but));
    assert(GTK_IS_DIALOG(toplevel) == TRUE);

    /* emit a "response" signal so we might close the dialog */
    gtk_dialog_response(GTK_DIALOG(toplevel), GTK_RESPONSE_CLOSE);
  }
}

bool
relation_info_dialog(relation_context_t *context, relation_t *relation)
{
  object_t object(relation);
  return info_dialog(context->dialog.get(), context->map, context->osm, context->presets, object);
}

void
on_relation_add(relation_context_t *context)
{
  /* create a new relation */

  std::unique_ptr<relation_t> relation(std::make_unique<relation_t>());
  if(relation_info_dialog(context, relation.get())) {
    relation_t *r = context->osm->attach(relation.release());

    // append a row for the new data
    GtkTreeIter iter;
    gtk_list_store_insert_with_values(context->store.get(), &iter, -1,
                                      RELATION_COL_TYPE,    r->tags.get_value("type"),
                                      RELATION_COL_NAME,    r->descriptive_name().c_str(),
                                      RELATION_COL_MEMBERS, r->members.size(),
                                      RELATION_COL_DATA,    r,
                                      -1);

    gtk_tree_selection_select_iter(list_get_selection(context->list), &iter);
  }
}

struct relation_edit_context {
  inline relation_edit_context(const relation_t *s, GtkWidget *l) : sel(s), list(l) {}
  const relation_t *sel;
  GtkWidget *list;
};

gboolean
relation_edit_foreach(GtkTreeModel *model, GtkTreePath *, GtkTreeIter *iter, gpointer data)
{
  const relation_edit_context * const context = static_cast<relation_edit_context *>(data);

  relation_t *row_rel;
  gtk_tree_model_get(model, iter, RELATION_COL_DATA, &row_rel, -1);
  if (row_rel != context->sel)
    return FALSE;

  const std::string &name = context->sel->descriptive_name();
  // Found it. Update all visible fields.
  gtk_list_store_set(GTK_LIST_STORE(model), iter,
                      RELATION_COL_TYPE,    context->sel->tags.get_value("type"),
                      RELATION_COL_NAME,    name.c_str(),
                      RELATION_COL_MEMBERS, context->sel->members.size(),
                      -1);

  // Order will probably have changed, so refocus
  list_focus_on(context->list, iter);
  return TRUE;
}

/* user clicked "edit..." button in relation list */
void
on_relation_edit(relation_context_t *context)
{
  relation_t *sel = get_selected_relation(context);
  if(sel == nullptr)
    return;

  g_debug("edit relation #" ITEM_ID_FORMAT, sel->id);

  if (!relation_info_dialog(context, sel))
    return;

  relation_edit_context ctx(sel, context->list);
  gtk_tree_model_foreach(GTK_TREE_MODEL(context->store.get()), relation_edit_foreach, &ctx);
}

/* remove the selected relation */
void
on_relation_remove(relation_context_t *context)
{
  relation_t *sel = get_selected_relation(context);
  if(sel == nullptr)
    return;

  g_debug("remove relation #" ITEM_ID_FORMAT, sel->id);

  if(!sel->members.empty()) {
    trstring msg(ngettext("This relation still has %zu member. Delete it anyway?",
                          "This relation still has %zu members. Delete it anyway?",
                          sel->members.size()), nullptr, sel->members.size());
    if(!osm2go_platform::yes_no(_("Delete non-empty relation?"), msg, 0, context->dialog.get()))
      return;
  }

  /* first remove selected row from list */
  GtkTreeIter       iter;
  GtkTreeModel      *model;
  if(list_get_selected(context->list, &model, &iter))
    gtk_list_store_remove(context->store.get(), &iter);

  /* then really delete it */
  context->osm->relation_delete(sel);

  relation_list_selected(context->list, nullptr);
}

struct relation_list_widget_functor {
  GtkListStore * const store;
  osm_t::ref osm;
  explicit inline relation_list_widget_functor(GtkListStore *s, osm_t::ref o) : store(s), osm(o) {}
  void operator()(const relation_t *rel);
  inline void operator()(std::pair<item_id_t, relation_t *> pair) {
    operator()(pair.second);
  }
};

void relation_list_widget_functor::operator()(const relation_t *rel)
{
  if(rel->isDeleted())
    return;

  const std::string &name = rel->descriptive_name();
  const relation_t * const orig = osm->originalObject(rel);

  /* Append a row and fill in some data */
  gtk_list_store_insert_with_values(store, nullptr, -1,
                                    RELATION_COL_TYPE, rel->tags.get_value("type"),
                                    RELATION_COL_NAME, name.c_str(),
                                    RELATION_COL_TAGS_MODIFIED, rel->isNew() || (orig && orig->tags != rel->tags) ? TRUE : FALSE,
                                    RELATION_COL_MEMBERS, rel->members.size(),
                                    RELATION_COL_MEMBERS_MODIFIED, rel->isNew() || (orig && orig->members != rel->members) ? TRUE : FALSE,
                                    RELATION_COL_DATA, rel,
                                    -1);
}

GtkWidget *
relation_list_widget(relation_context_t &context)
{
  std::vector<list_view_column> columns;
  columns.push_back(list_view_column(_("Type"),    0));
  columns.push_back(list_view_column(_("Name"),    LIST_FLAG_ELLIPSIZE | LIST_FLAG_MARK_MODIFIED, RELATION_COL_TAGS_MODIFIED));
  columns.push_back(list_view_column(_("Members"), LIST_FLAG_MARK_MODIFIED, RELATION_COL_MEMBERS_MODIFIED));

  std::vector<list_button> buttons;
  buttons.push_back(list_button::addButton(G_CALLBACK(on_relation_add)));
  buttons.push_back(list_button::editButton(G_CALLBACK(on_relation_edit)));
  buttons.push_back(list_button::removeButton(G_CALLBACK(on_relation_remove)));
  buttons.push_back(list_button(_("Members"), G_CALLBACK(on_relation_members)));
  buttons.push_back(list_button(_("Select"),  G_CALLBACK(on_relation_select)));

  /* build and fill the store */
  context.store.reset(gtk_list_store_new(RELATION_NUM_COLS,
                                         G_TYPE_STRING, G_TYPE_STRING,
                                         G_TYPE_UINT, G_TYPE_BOOLEAN,
                                         G_TYPE_BOOLEAN, G_TYPE_POINTER));

  context.list = list_new(LIST_HILDON_WITH_HEADERS, &context,
                          relation_list_changed, buttons, columns,
                          GTK_TREE_MODEL(context.store.get()));

  // Sorting by ref/name by default is useful for places with lots of numbered
  // bus routes. Especially for small screens.
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(context.store.get()),
                                       RELATION_COL_NAME, GTK_SORT_ASCENDING);

  relation_list_widget_functor fc(context.store.get(), context.osm);

  const std::map<item_id_t, relation_t *> &rchain = context.osm->relations;
  std::for_each(rchain.begin(), rchain.end(), fc);

  relation_list_selected(context.list, nullptr);

  return context.list;
}

} // namespace

/* a global view on all relations */
void relation_list(GtkWidget *parent, map_t *map, osm_t::ref osm, presets_items *presets) {
  relation_context_t context(map, osm, presets,
                             gtk_dialog_new_with_buttons(static_cast<const gchar *>(_("All relations")),
                                                         GTK_WINDOW(parent),
                                                         GTK_DIALOG_MODAL,
                                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                                         nullptr));

  osm2go_platform::dialog_size_hint(context.dialog, osm2go_platform::MISC_DIALOG_LARGE);
  gtk_dialog_set_default_response(context.dialog, GTK_RESPONSE_CLOSE);

  gtk_box_pack_start(context.dialog.vbox(), relation_list_widget(context), TRUE, TRUE, 0);

  /* ----------------------------------- */


  gtk_widget_show_all(context.dialog.get());
  gtk_dialog_run(context.dialog);
}
