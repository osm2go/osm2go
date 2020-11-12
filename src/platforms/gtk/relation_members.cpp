/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file relation_members
 *
 * This file contains the dialog that shows all members of a given relation.
 */

#include "relation_edit.h"
#include "relation_p.h"

#include <info.h>
#include <josm_presets.h>
#include "list.h"
#include <map.h>

#include <algorithm>
#include <array>
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

struct member_context_t {
  static std::vector<member_t> emptyMembers;  ///< dummy list to show all new relation members as modified

  inline member_context_t(relation_t *r, osm_t::ref o, GtkWidget *parent, const presets_items *p)
    : relation(r)
    , dialog(gtk_dialog_new_with_buttons(static_cast<const gchar *>(trstring("Members of relation \"%1\"").arg(relation->descriptive_name())),
                                         GTK_WINDOW(parent), GTK_DIALOG_MODAL,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                         nullptr))
    , osm(o)
    , presets(p)
#ifdef FREMANTLE
    , buttonUp(osm2go_platform::button_new_with_label(_("Up")))
    , buttonDown(osm2go_platform::button_new_with_label(_("Down")))
#else
    , buttonUp(gtk_button_new_with_mnemonic(static_cast<const gchar *>(_("_Up"))))
    , buttonDown(gtk_button_new_with_mnemonic(static_cast<const gchar *>(_("_Down"))))
#endif
    , roleColumn(nullptr)
    , origRelation(osm->originalObject(relation))
    , origMembers(origRelation != nullptr ? origRelation->members : relation->isNew() ? emptyMembers : relation->members)
    , currentMembers(relation->members)
  {
  }

  member_context_t() O2G_DELETED_FUNCTION;
  member_context_t(const member_context_t &) O2G_DELETED_FUNCTION;
  member_context_t &operator=(const member_context_t &) O2G_DELETED_FUNCTION;
#if __cplusplus >= 201103L
  member_context_t(member_context_t &&) = delete;
  member_context_t &operator=(member_context_t &&) = delete;
  ~member_context_t() = default;
#endif
  relation_t * const relation;
  const osm2go_platform::DialogGuard dialog;
  GtkTreeView *view;
  osm_t::ref osm;
  const presets_items * const presets;
  GtkWidget * const buttonUp;
  GtkWidget * const buttonDown;
  GtkTreeViewColumn *roleColumn;
  const relation_t * const origRelation;
  const std::vector<member_t> &origMembers;
  std::vector<member_t> currentMembers;

  /**
   * @brief returns the new values for the *_CHANGED columns
   * @returns the values for the MEMBER_COL_TYPE_CHANGED, MEMBER_COL_ID_CHANGED, and MEMBER_COL_ROLE_CHANGED columns
   */
  std::array<gboolean, 3> valuesChanged(size_t index) const;
};

std::array<gboolean, 3> member_context_t::valuesChanged(size_t index) const
{
  std::array<gboolean, 3> ret;

  if (index >= origMembers.size()) {
    ret[0] = TRUE;
    ret[1] = TRUE;
    ret[2] = TRUE;
  } else {
    const member_t &origMember = origMembers.at(index);
    const member_t &member = currentMembers.at(index);
    ret[0] =  origMember.object.type != member.object.type ? TRUE : FALSE;
    ret[1] = origMember.object.get_id() != member.object.get_id() ? TRUE : FALSE;
    ret[2] = origMember.role != member.role ? TRUE : FALSE;
  }

  return ret;
}

std::vector<member_t> member_context_t::emptyMembers;

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

struct g_tree_path_deleter {
  inline void operator()(gpointer mem) const
  {
    gtk_tree_path_free(static_cast<GtkTreePath *>(mem));
  }
};

gboolean
member_list_selection_func(GtkTreeSelection *, GtkTreeModel *model, GtkTreePath *path, gboolean, gpointer ctx)
{
  GtkTreeIter iter;

  if(gtk_tree_model_get_iter(model, &iter, path) == TRUE) {
    assert_cmpnum(gtk_tree_path_get_depth(path), 1);

    member_context_t *context = static_cast<member_context_t *>(ctx);

    gint *ind = gtk_tree_path_get_indices(path);
    assert(ind != nullptr);

    gtk_widget_set_sensitive(context->buttonUp, *ind > 0 ? TRUE : FALSE);
    gtk_widget_set_sensitive(context->buttonDown, gtk_tree_model_iter_next(model, &iter));
  }

  return TRUE;
}

void
member_selection_changed(GtkTreeSelection *sel, member_context_t *context)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  if(unlikely(gtk_tree_selection_get_selected(sel, &model, &iter) == FALSE)) {
    gtk_widget_set_sensitive(context->buttonUp, FALSE);
    gtk_widget_set_sensitive(context->buttonDown, FALSE);
  }
}

gboolean
on_view_clicked(GtkWidget *widget, GdkEventButton *event, member_context_t *context)
{
  GtkTreeView *view = GTK_TREE_VIEW(widget);
  if(event->window != gtk_tree_view_get_bin_window(view))
    return FALSE;

  GtkTreePath *path;
  GtkTreeViewColumn *column;

  if(gtk_tree_view_get_path_at_pos(view, event->x, event->y, &path,
                                    &column, nullptr, nullptr) != TRUE)
    return FALSE;

  if (column != context->roleColumn)
    return FALSE;

  gint *ind = gtk_tree_path_get_indices(path);
  assert(ind != nullptr);

  relation_t *rel = context->relation;
  const member_t &oldmember = context->currentMembers.at(*ind);

  GtkTreeSelection *sel = gtk_tree_view_get_selection(view);
  gboolean wasSelected = gtk_tree_selection_path_is_selected(sel, path);
  GtkTreeModel *model = gtk_tree_view_get_model(view);
  GtkTreeIter iter;
  gboolean b = gtk_tree_model_get_iter(model, &iter, path);

  // Fremantle would do a multi-selection by default
#ifdef FREMANTLE
  if (wasSelected == FALSE) {
    // update the selection here, so the correct selection is visible in the background while
    // the combobox dialog is in the foreground
    gtk_tree_selection_unselect_all(sel);
    gtk_tree_selection_select_iter(sel, &iter);
    wasSelected = TRUE;
  }
#endif

  const std::optional<member_t> change = selectObjectRole(context->dialog.get(), rel, oldmember.object, context->presets, oldmember.role);

  if (change) {
    context->currentMembers.at(*ind) = *change;
    const char *newRole = (*change).role;

    gboolean rChanged;
    if (rel->isNew()) {
      rChanged = TRUE;
    } else {
      if (static_cast<unsigned int>(*ind) >= context->origMembers.size())
        rChanged = TRUE;
      else
        rChanged = context->origMembers.at(*ind).role != newRole ? TRUE : FALSE;
    }

    assert(b == TRUE);(void)b;
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                      MEMBER_COL_ROLE, newRole,
                      MEMBER_COL_ROLE_CHANGED, rChanged,
                      -1);
  }

  // Stop other handlers, i.e. selection, if the row is already selected,
  // to prevent it becoming deselected.
  // Otherwise allow this role to also be selected by the dialog code.
  return wasSelected;
}

GtkWidget *
member_list_widget(member_context_t &context)
{
  GtkWidget *vbox = gtk_vbox_new(FALSE,3);
  GtkTreeView *view = context.view = osm2go_platform::tree_view_new();

  /* catch views button-press event for our custom handling */
  g_signal_connect(view, "button-press-event", G_CALLBACK(on_view_clicked), &context);

  GtkTreeSelection *sel = gtk_tree_view_get_selection(view);
  gtk_tree_selection_set_select_function(sel, member_list_selection_func, &context, nullptr);
  gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);
  g_signal_connect(sel, "changed", G_CALLBACK(member_selection_changed), &context);

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
  context.roleColumn = column;
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

  for (size_t i = 0; i < context.currentMembers.size(); i++) {
    const member_t &member = context.currentMembers.at(i);

    const std::array<gboolean, 3> changedVals = context.valuesChanged(i);

    /* Append a row and fill in some data */
    bool realObj = member.object.is_real();
    GtkTreeIter iter;
    gtk_list_store_insert_with_values(store, &iter, -1,
                                      MEMBER_COL_TYPE,         static_cast<const gchar *>(member.object.type_string()),
                                      MEMBER_COL_TYPE_CHANGED, changedVals[0],
                                      MEMBER_COL_ID,           member.object.id_string().c_str(),
                                      MEMBER_COL_ID_CHANGED,   changedVals[1],
                                      MEMBER_COL_NAME,         realObj ? static_cast<const gchar *>(member.object.get_name(*context.osm)) : nullptr,
                                      MEMBER_COL_ROLE,         member.role,
                                      MEMBER_COL_ROLE_CHANGED, changedVals[2],
                                      MEMBER_COL_REF_ONLY,     realObj ? FALSE : TRUE,
                                      MEMBER_COL_DATA,         &member,
                                      -1);
  }

  gtk_box_pack_start(GTK_BOX(vbox), osm2go_platform::scrollable_container(GTK_WIDGET(view)), TRUE, TRUE, 0);

  return vbox;
}

GtkTreeModel *get_selected_row(member_context_t *context, GtkTreeIter *iter)
{
  GtkTreeSelection *selection = gtk_tree_view_get_selection(context->view);
  GtkTreeModel *model;

  if(gtk_tree_selection_get_selected(selection, &model, iter) == FALSE)
    return nullptr;

  return model;
}

gint indexFromIter(GtkTreeModel *model, GtkTreeIter *iter)
{
  std::unique_ptr<GtkTreePath, g_tree_path_deleter> path(gtk_tree_model_get_path(model, iter));
  assert(path);
  gint *ind = gtk_tree_path_get_indices(path.get());
  assert(ind != nullptr);
  assert_cmpnum_op(*ind, >=, 0);
  return *ind;
}

void __attribute__ ((nonnull(1,2)))
memberListUpdate(GtkTreeModel *model, GtkTreeIter *iter, member_context_t &context, int index)
{
  const std::array<gboolean, 3> changedVals = context.valuesChanged(index);

  gtk_list_store_set(GTK_LIST_STORE(model), iter,
                     MEMBER_COL_TYPE_CHANGED, changedVals[0],
                     MEMBER_COL_ID_CHANGED,   changedVals[1],
                     MEMBER_COL_ROLE_CHANGED, changedVals[2],
                     -1);
}

/**
 * @brief reorder 2 relation members
 * @param from the selected row in the model
 * @param to the row the member should be moved to
 */
void reorderMembers(member_context_t *context, GtkTreeModel *model, GtkTreeIter *from, GtkTreeIter *to)
{
  gint idxFrom = indexFromIter(model, from);
  gint idxTo = indexFromIter(model, to);

  gtk_list_store_swap(GTK_LIST_STORE(model), from, to);

  assert_cmpnum_op(static_cast<unsigned int>(idxFrom), <, context->currentMembers.size());
  assert_cmpnum_op(static_cast<unsigned int>(idxTo), <, context->currentMembers.size());

  member_t tmp = context->currentMembers[idxFrom];
  context->currentMembers[idxFrom] = context->currentMembers[idxTo];
  context->currentMembers[idxTo] = tmp;

  // the values have already be exchanged, now update the possible changes to the original members
  // the idx values and the iterators are swapped, as gtk_list_store_swap modifies the GtkTreeIter values
  memberListUpdate(model, from, *context, idxTo);
  memberListUpdate(model, to, *context, idxFrom);

  // idxTo is the new position of the selected index
  gtk_widget_set_sensitive(context->buttonUp, idxTo > 0 ? TRUE : FALSE);
  gtk_widget_set_sensitive(context->buttonDown, static_cast<unsigned int>(idxTo) < context->currentMembers.size() - 1 ? TRUE : FALSE);
}

void
on_up_clicked(member_context_t *context)
{
  GtkTreeIter iter;
  GtkTreeModel *model = get_selected_row(context, &iter);

  /* likely nothing selected anymore, fix the button enable */
  if (unlikely(model == nullptr)) {
    gtk_widget_set_sensitive(context->buttonUp, FALSE);
    return;
  }

  std::unique_ptr<GtkTreePath, g_tree_path_deleter> path(gtk_tree_model_get_path(model, &iter));
  assert(path);
  if (gtk_tree_path_prev(path.get()) == FALSE) {
    g_warning("up clicked on first member");
    return;
  }

  GtkTreeIter prev;
  gboolean b = gtk_tree_model_get_iter(model, &prev, path.get());
  assert(b == TRUE); (void)b;

  reorderMembers(context, model, &iter, &prev);
}

void
on_down_clicked(member_context_t *context)
{
  GtkTreeIter iter;
  GtkTreeModel *model = get_selected_row(context, &iter);

  /* likely nothing selected anymore, fix the button enable */
  if (unlikely(model == nullptr)) {
    gtk_widget_set_sensitive(context->buttonDown, FALSE);
    return;
  }

  GtkTreeIter next = iter;
  if (gtk_tree_model_iter_next(model, &next) == FALSE) {
    g_warning("down clicked on last member");
    return;
  }

  reorderMembers(context, model, &iter, &next);
}

} // namespace

void relation_show_members(GtkWidget *parent, relation_t *relation, osm_t::ref osm, const presets_items *p)
{
  member_context_t mcontext(relation, osm, parent, p);

  osm2go_platform::dialog_size_hint(static_cast<GtkWindow *>(mcontext.dialog), osm2go_platform::MISC_DIALOG_MEDIUM);
  gtk_dialog_set_default_response(static_cast<GtkDialog *>(mcontext.dialog), GTK_RESPONSE_CLOSE);

  GtkBox *box = GTK_BOX(static_cast<GtkDialog *>(mcontext.dialog)->vbox);
  gtk_box_pack_start(box, member_list_widget(mcontext), TRUE, TRUE, 0);

  GtkWidget *table = gtk_table_new(1, 2, TRUE);
  gtk_box_pack_start(box, table, FALSE, FALSE, 0);

  if (mcontext.currentMembers.size() > 1) {
    gtk_table_attach_defaults(GTK_TABLE(table), mcontext.buttonUp, 0, 1, 0, 1);
    g_signal_connect_swapped(mcontext.buttonUp, "clicked", G_CALLBACK(on_up_clicked), &mcontext);

#ifndef FREMANTLE
    GtkWidget *iconw = gtk_image_new_from_icon_name("go-up", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(mcontext.buttonUp), iconw);
#endif

    gtk_table_attach_defaults(GTK_TABLE(table), mcontext.buttonDown, 1, 2, 0, 1);
    g_signal_connect_swapped(mcontext.buttonDown, "clicked", G_CALLBACK(on_down_clicked), &mcontext);

#ifndef FREMANTLE
    iconw = gtk_image_new_from_icon_name("go-down", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(mcontext.buttonDown), iconw);
#endif
  }

  gtk_widget_show_all(mcontext.dialog.get());
  if (gtk_dialog_run(static_cast<GtkDialog *>(mcontext.dialog)) == GTK_RESPONSE_ACCEPT)
    relation->updateMembers(mcontext.currentMembers, osm);
}
