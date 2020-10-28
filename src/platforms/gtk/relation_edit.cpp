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

struct member_context_t {
  inline member_context_t(const relation_t *r, osm_t::ref o, GtkWidget *parent)
    : relation(r)
    , dialog(gtk_dialog_new_with_buttons(static_cast<const gchar *>(trstring("Members of relation \"%1\"").arg(relation->descriptive_name())),
                                         GTK_WINDOW(parent), GTK_DIALOG_MODAL,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         nullptr))
    , osm(o)
  {
  }

  member_context_t() O2G_DELETED_FUNCTION;
  member_context_t(const member_context_t &) O2G_DELETED_FUNCTION;
  member_context_t &operator=(const member_context_t &) O2G_DELETED_FUNCTION;
#if __cplusplus >= 201103L
  member_context_t(member_context_t &&) = delete;
  member_context_t &operator=(member_context_t &&) = delete;
#endif
  const relation_t * const relation;
  GtkWidget * const dialog;
  osm_t::ref osm;
};

enum {
  MEMBER_COL_TYPE = 0,
  MEMBER_COL_ID,
  MEMBER_COL_NAME,
  MEMBER_COL_ROLE,
  MEMBER_COL_REF_ONLY,
  MEMBER_COL_TYPE_CHANGED,
  MEMBER_COL_ID_CHANGED,
  MEMBER_COL_ROLE_CHANGED,
  MEMBER_COL_DATA,
  MEMBER_NUM_COLS
};

gboolean
member_list_selection_func(GtkTreeSelection *, GtkTreeModel *model, GtkTreePath *path, gboolean, gpointer)
{
  GtkTreeIter iter;

  if(gtk_tree_model_get_iter(model, &iter, path) == TRUE) {
    assert_cmpnum(gtk_tree_path_get_depth(path), 1);

    const member_t *member = nullptr;
    gtk_tree_model_get(model, &iter, MEMBER_COL_DATA, &member, -1);
    if(member != nullptr && member->object.is_real())
      return TRUE;
  }

  return FALSE;
}

GtkWidget *
member_list_widget(member_context_t &context)
{
  GtkWidget *vbox = gtk_vbox_new(FALSE,3);
  GtkTreeView * const view = osm2go_platform::tree_view_new();

  gtk_tree_selection_set_select_function(gtk_tree_view_get_selection(view),
                                         member_list_selection_func, &context, nullptr);

  /* --- "type" column --- */
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "foreground", "grey", nullptr);
  g_object_set(renderer, "underline", PANGO_UNDERLINE_SINGLE, nullptr);
  GtkTreeViewColumn *column =
    gtk_tree_view_column_new_with_attributes(static_cast<const gchar *>(_("Type")), renderer,
                                             "text", MEMBER_COL_TYPE,
                                             "underline-set", MEMBER_COL_TYPE_CHANGED,
                                             "foreground-set", MEMBER_COL_REF_ONLY,  nullptr);
  gtk_tree_view_column_set_sort_column_id(column, MEMBER_COL_TYPE);
  gtk_tree_view_insert_column(view, column, -1);

  /* --- "id" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "foreground", "grey", nullptr);
  g_object_set(renderer, "underline", PANGO_UNDERLINE_SINGLE, nullptr);
  column = gtk_tree_view_column_new_with_attributes(static_cast<const gchar *>(_("Id")), renderer,
                                                    "text", MEMBER_COL_ID,
                                                    "underline-set", MEMBER_COL_ID_CHANGED,
                                                    "foreground-set", MEMBER_COL_REF_ONLY,  nullptr);
  gtk_tree_view_column_set_sort_column_id(column, MEMBER_COL_ID);
  gtk_tree_view_insert_column(view, column, -1);

  /* --- "Name" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "foreground", "grey", nullptr);
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, nullptr);
  column = gtk_tree_view_column_new_with_attributes(static_cast<const gchar *>(_("Name")), renderer,
		     "text", MEMBER_COL_NAME,
		     "foreground-set", MEMBER_COL_REF_ONLY,  nullptr);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_column_set_sort_column_id(column, MEMBER_COL_NAME);
  gtk_tree_view_insert_column(view, column, -1);

  /* --- "role" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "foreground", "grey", nullptr);
  g_object_set(renderer, "underline", PANGO_UNDERLINE_SINGLE, nullptr);
  column = gtk_tree_view_column_new_with_attributes(static_cast<const gchar *>(_("Role")), renderer,
                                                    "text", MEMBER_COL_ROLE,
                                                    "underline-set", MEMBER_COL_ROLE_CHANGED,
                                                    "foreground-set", MEMBER_COL_REF_ONLY,  nullptr);
  gtk_tree_view_column_set_sort_column_id(column, MEMBER_COL_ROLE);
  gtk_tree_view_insert_column(view, column, -1);

  /* build and fill the store */
  GtkListStore * const store = gtk_list_store_new(MEMBER_NUM_COLS,
                                                  G_TYPE_STRING, G_TYPE_STRING,
                                                  G_TYPE_STRING, G_TYPE_STRING,
                                                  G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
                                                  G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
                                                  G_TYPE_POINTER);

  gtk_tree_view_set_model(view, GTK_TREE_MODEL(store));

  const relation_t *origRel = static_cast<const relation_t *>(context.osm->originalObject(object_t(object_t::RELATION_ID, context.relation->id)));

  for (size_t i = 0; i < context.relation->members.size(); i++) {
    const member_t &member = context.relation->members.at(i);

    const member_t origMember = (origRel == nullptr || i >= origRel->members.size()) ? member_t(object_t()) :
                                origRel->members.at(i);

    /* Append a row and fill in some data */
    bool realObj = member.object.is_real();
    GtkTreeIter iter;
    gtk_list_store_insert_with_values(store, &iter, -1,
                                      MEMBER_COL_TYPE,         static_cast<const gchar *>(member.object.type_string()),
                                      MEMBER_COL_TYPE_CHANGED, origMember.object.type != member.object.type ? TRUE : FALSE,
                                      MEMBER_COL_ID,           member.object.id_string().c_str(),
                                      MEMBER_COL_ID_CHANGED,   origMember.object.get_id() != member.object.get_id() ? TRUE : FALSE,
                                      MEMBER_COL_NAME,         realObj ? static_cast<const gchar *>(member.object.get_name(*context.osm)) : nullptr,
                                      MEMBER_COL_ROLE,         member.role,
                                      MEMBER_COL_ROLE_CHANGED, origMember.role != member.role ? TRUE : FALSE,
                                      MEMBER_COL_REF_ONLY,     realObj ? FALSE : TRUE,
                                      MEMBER_COL_DATA,         &member,
                                      -1);
  }

  gtk_box_pack_start(GTK_BOX(vbox), osm2go_platform::scrollable_container(GTK_WIDGET(view)), TRUE, TRUE, 0);

  return vbox;
}

} // namespace

void relation_show_members(GtkWidget *parent, const relation_t *relation, osm_t::ref osm) {
  member_context_t mcontext(relation, osm, parent);

  osm2go_platform::dialog_size_hint(GTK_WINDOW(mcontext.dialog), osm2go_platform::MISC_DIALOG_MEDIUM);
  gtk_dialog_set_default_response(GTK_DIALOG(mcontext.dialog),
				  GTK_RESPONSE_CLOSE);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mcontext.dialog)->vbox),
      		     member_list_widget(mcontext), TRUE, TRUE, 0);

  /* ----------------------------------- */

  gtk_widget_show_all(mcontext.dialog);
  gtk_dialog_run(GTK_DIALOG(mcontext.dialog));
  gtk_widget_destroy(mcontext.dialog);
}

namespace {

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

  relation_edit_context ctx;
  ctx.sel = sel;
  ctx.list = context->list;
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
  const relation_t * const orig = static_cast<const relation_t *>(osm->originalObject(object_t(object_t::RELATION_ID, rel->id)));

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

}

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
