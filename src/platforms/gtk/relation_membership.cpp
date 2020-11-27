/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file relation_membership
 *
 * This file contains the dialog that shows all relation and if the given object
 * is member of any of these, and if it is the role of the first occurrence.
 */

#include "relation_p.h"

#include <josm_presets.h>
#include "list.h"
#include <map.h>
#include <object_dialogs.h>

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

/* --------------- relation dialog for an item (node, way, or other relation) ----------- */

namespace {

struct relitem_context_t {
  relitem_context_t(object_t &o, const presets_items *pr, osm_t::ref os);
  relitem_context_t() O2G_DELETED_FUNCTION;
  relitem_context_t(const relitem_context_t &) O2G_DELETED_FUNCTION;
  relitem_context_t &operator=(const relitem_context_t &) O2G_DELETED_FUNCTION;
#if __cplusplus >= 201103L
  relitem_context_t(relitem_context_t &&) = delete;
  relitem_context_t &operator=(relitem_context_t &&) = delete;
  ~relitem_context_t() = default;
#endif

  object_t &item;
  const presets_items * const presets;
  osm_t::ref osm;
  osm2go_platform::DialogGuard dialog;
  std::unique_ptr<GtkListStore, g_object_deleter> store;
  GtkTreeSelection * selection;
};

enum {
  RELITEM_COL_TYPE = 0,
  RELITEM_COL_MEMBER_MODIFIED, ///< if the membership state of this entry in the given relation has changed
  RELITEM_COL_ROLE, ///< the current role in the relation
  RELITEM_COL_ROLE_MODIFIED, ///< if the membership has not changed, but the role
  RELITEM_COL_NAME,
  RELITEM_COL_DATA,
  RELITEM_NUM_COLS
};

relitem_context_t::relitem_context_t(object_t &o, const presets_items *pr, osm_t::ref os)
  : item(o)
  , presets(pr)
  , osm(os)
  , store(nullptr)
  , selection(nullptr)
{
}

struct entry_insert_text {
  GtkWidget * const entry;
  explicit inline entry_insert_text(GtkWidget *en) : entry(en) {}
  inline void operator()(const std::string &role) const
  {
    osm2go_platform::combo_box_append_text(entry, role.c_str());
  }
};

} // namespace

#if __cplusplus < 201103L
namespace std {

// this extra specialization is needed as member_t can't be default-constructed
template<>
optional<member_t>::optional() : match(false), result(member_t(object_t())) {}

} // namespace std
#endif

std::optional<member_t>
selectObjectRole(GtkWidget *parent, const relation_t *relation, const object_t &object, const presets_items *presets, const char *role)
{
  const std::set<std::string> &roles = presets->roles(relation, object);

  /* ask the user for the role of the new object in this relation */
  /* ------------------ role dialog ---------------- */
  osm2go_platform::DialogGuard dialog(gtk_dialog_new_with_buttons(static_cast<const gchar *>(_("Select role")), GTK_WINDOW(parent),
                                              GTK_DIALOG_MODAL,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              nullptr));

  gtk_dialog_set_default_response(dialog, GTK_RESPONSE_ACCEPT);

  const char *type = relation->tags.get_value("type");

  { // scope to free info_str earlier
    const trstring info_str = type != nullptr ?
                      trstring("In relation of type: %1").arg(type) :
                      trstring("In relation #%1").arg(relation->id);
    gtk_box_pack_start(dialog.vbox(), gtk_label_new(static_cast<const gchar *>(info_str)),
                       TRUE, TRUE, 0);
  }

  const char *name = relation->tags.get_value("name");
  if(name != nullptr)
    gtk_box_pack_start(dialog.vbox(), gtk_label_new(name), TRUE, TRUE, 0);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 8);

#ifdef FREMANTLE
  if(roles.empty())
#endif
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("Role:")), TRUE, TRUE, 0);

  GtkWidget *entry = nullptr;
  if(!roles.empty()) {
    entry = osm2go_platform::combo_box_entry_new(static_cast<const char *>(_("Role")));

    /* fill combo box with presets */
    const std::set<std::string>::const_iterator itBegin = roles.begin();
    const std::set<std::string>::const_iterator itEnd = roles.end();
    std::for_each(itBegin, itEnd, entry_insert_text(entry));
    if (role != nullptr) {
      const std::set<std::string>::const_iterator it = std::find(itBegin, itEnd, role);
      if (it != itEnd)
        osm2go_platform::combo_box_set_active(entry, std::distance(itBegin, it));
      else
        osm2go_platform::combo_box_set_active_text(entry, role);
    }
  } else {
    entry = osm2go_platform::entry_new();
    if (role != nullptr)
      osm2go_platform::setEntryText(GTK_ENTRY(entry), role, _("Role"));
  }

  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
  gtk_box_pack_start(dialog.vbox(), hbox, TRUE, TRUE, 0);

  gtk_widget_show_all(dialog.get());
  if(GTK_RESPONSE_ACCEPT != gtk_dialog_run(dialog)) {
    g_debug("user clicked cancel");
    return member_t(object_t());
  }

  g_debug("user clicked ok");

  /* get role from dialog */
  const char *newRole = nullptr;
  std::string rstr;

  if(osm2go_platform::isComboBoxEntryWidget(entry)) {
    rstr = osm2go_platform::combo_box_get_active_text(entry);
    if(!rstr.empty())
      newRole = rstr.c_str();
  } else {
    const gchar *ptr = gtk_entry_get_text(GTK_ENTRY(entry));
    if(ptr != nullptr && *ptr != '\0')
      newRole = ptr;
  }

  if (newRole == role || (newRole && role && strcmp(newRole, role) == 0))
    return std::optional<member_t>();

  return member_t(object, newRole);
}

namespace {

bool
relation_add_item(GtkWidget *parent, relation_t *relation, const object_t &object, const presets_items *presets, osm_t::ref osm)
{
  g_debug("add object of type %d to relation #" ITEM_ID_FORMAT, object.type, relation->id);
  assert(object.is_real());

  const std::optional<member_t> change = selectObjectRole(parent, relation, object, presets, nullptr);

  if (!change)
    return false;

  osm->mark_dirty(relation);
  // create new member
  // must be done before the widget is destroyed as it may reference the
  // internal string from the text entry
  relation->members.push_back(*change);

  return true;
}

unsigned int
isOriginalRelation(osm_t::ref osm, const relation_t *rel, object_t &obj)
{
  const relation_t *orig = osm->originalObject(rel);

  return rel->objectMembershipState(obj, orig);
}

gboolean
changed_foreach(GtkTreeModel *model, GtkTreePath *, GtkTreeIter *iter, gpointer data)
{
  relitem_context_t *context = static_cast<relitem_context_t *>(data);
  relation_t *relation = nullptr;
  gtk_tree_model_get(model, iter, RELITEM_COL_DATA, &relation, -1);
  assert(relation != nullptr);

  std::vector<member_t>::const_iterator itEnd = relation->members.end();
  std::vector<member_t>::const_iterator it = relation->find_member_object(context->item);

  gboolean isSelected = gtk_tree_selection_iter_is_selected(context->selection, iter);

  if(it == itEnd && isSelected == TRUE) {
    g_debug("selected: " ITEM_ID_FORMAT, relation->id);

    /* either accept this or unselect again */
    if(relation_add_item(context->dialog.get(), relation, context->item, context->presets, context->osm)) {
      // vector was modified, update the iterator
      itEnd = relation->members.end();
      // the item is now the last one in the chain
      it = std::prev(itEnd);
    } else {
      gtk_tree_selection_unselect_iter(context->selection, iter);
      return TRUE;
    }
  } else if(it != itEnd && isSelected == FALSE) {
    g_debug("deselected: " ITEM_ID_FORMAT, relation->id);

    context->osm->mark_dirty(relation);
    it = relation->eraseMember(it);
    // vector was modified, update the iterator
    itEnd = relation->members.end();

    // there could have been multiple instances, so check if there is more
    it = relation->find_member_object(context->item, it);
  } else {
    return FALSE;
  }

  const unsigned int mflags = isOriginalRelation(context->osm, relation, context->item);

  gtk_list_store_set(GTK_LIST_STORE(model), iter,
                     RELITEM_COL_ROLE, (it != itEnd) ? it->role : nullptr,
                     RELITEM_COL_ROLE_MODIFIED, (mflags & relation_t::RoleChanged) ? TRUE : FALSE,
                     RELITEM_COL_MEMBER_MODIFIED, (mflags & relation_t::MembershipChanged) ? TRUE : FALSE,
                     -1);

  return TRUE;
}

void
changed(relitem_context_t *context)
{
  g_debug("relation-edit changed event");

  gtk_tree_model_foreach(GTK_TREE_MODEL(context->store.get()), changed_foreach, context);
}

#ifndef FREMANTLE
/* we handle these events on our own in order to implement */
/* a very direct selection mechanism (multiple selections usually */
/* require the control key to be pressed). This interferes with */
/* fremantle finger scrolling, but fortunately the fremantle */
/* default behaviour already is what we want. */
gboolean
on_view_clicked(GtkWidget *widget, GdkEventButton *event, gpointer)
{
  if(event->window == gtk_tree_view_get_bin_window(GTK_TREE_VIEW(widget))) {
    GtkTreePath *path;

    if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event->x, event->y, &path,
                                     nullptr, nullptr, nullptr) == TRUE) {
      GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

      if(gtk_tree_selection_path_is_selected(sel, path) != TRUE)
        gtk_tree_selection_select_path(sel, path);
      else
        gtk_tree_selection_unselect_path(sel, path);
    }
    return TRUE;
  }
  return FALSE;
}
#endif

struct relation_list_insert_functor {
  relitem_context_t &context;
  GtkTreeIter &sel_iter;
  trstring &selname; /* name of sel_iter */
  inline relation_list_insert_functor(relitem_context_t &c, trstring &sn, GtkTreeIter &it)
    : context(c), sel_iter(it), selname(sn) {}
  void operator()(std::pair<item_id_t, relation_t *> pair);
};

void relation_list_insert_functor::operator()(std::pair<item_id_t, relation_t *> pair)
{
  const relation_t * const relation = pair.second;

  if(relation->isDeleted())
    return;

  GtkTreeIter iter;
  /* try to find something descriptive */
  trstring name = relation->descriptiveNameOrId();

  const std::vector<member_t>::const_iterator it = relation->find_member_object(context.item);
  const bool isMember = it != relation->members.end();

  const unsigned int mflags = isOriginalRelation(context.osm, relation, context.item);

  /* Append a row and fill in some data */
  gtk_list_store_insert_with_values(context.store.get(), &iter, -1,
                                    RELITEM_COL_TYPE, relation->tags.get_value("type"),
                                    RELITEM_COL_ROLE, isMember ? it->role : nullptr,
                                    RELITEM_COL_NAME, static_cast<const gchar *>(name),
                                    RELITEM_COL_ROLE_MODIFIED, (mflags & relation_t::RoleChanged) ? TRUE : FALSE,
                                    RELITEM_COL_MEMBER_MODIFIED, (mflags & relation_t::MembershipChanged) ? TRUE : FALSE,
                                    RELITEM_COL_DATA, relation,
                                    -1);

  /* select all relations the current object is part of */

  if(isMember) {
    gtk_tree_selection_select_iter(context.selection, &iter);
    /* check if this element is earlier by name in the list */
    if(selname.isEmpty() || name.toStdString().compare(selname.toStdString()) < 0) {
      selname.swap(name);
      sel_iter = iter;
    }
  }
}

GtkWidget *
relation_item_list_widget(relitem_context_t &context)
{
  GtkTreeView *view = osm2go_platform::tree_view_new();

#ifdef FREMANTLE
  /* hildon hides these by default */
  gtk_tree_view_set_headers_visible(view, TRUE);
#endif

  /* change list mode to "multiple" */
  context.selection = gtk_tree_view_get_selection(view);
  gtk_tree_selection_set_mode(context.selection, GTK_SELECTION_MULTIPLE);

#ifndef FREMANTLE
  /* catch views button-press event for our custom handling */
  g_signal_connect(view, "button-press-event",
		   G_CALLBACK(on_view_clicked), &context);
#endif

  /* --- "Name" column --- */
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, nullptr );
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
       static_cast<const gchar *>(_("Name")), renderer, "text", RELITEM_COL_NAME, nullptr);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(view, column, -1);

  /* --- "Type" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "underline", PANGO_UNDERLINE_SINGLE, nullptr);
  gtk_tree_view_insert_column_with_attributes(view, -1, static_cast<const gchar *>(_("Type")), renderer,
                                              "text", RELITEM_COL_TYPE,
                                              "underline-set", RELITEM_COL_MEMBER_MODIFIED,
                                              nullptr);

  /* --- "Role" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "underline", PANGO_UNDERLINE_SINGLE, nullptr);
  gtk_tree_view_insert_column_with_attributes(view, -1, static_cast<const gchar *>(_("Role")), renderer,
                                              "text", RELITEM_COL_ROLE,
                                              "underline-set", RELITEM_COL_ROLE_MODIFIED,
                                              nullptr);

  /* build and fill the store */
  context.store.reset(gtk_list_store_new(RELITEM_NUM_COLS,
                                         G_TYPE_STRING,    // RELITEM_COL_TYPE
                                         G_TYPE_BOOLEAN,   // RELITEM_COL_MEMBER_MODIFIED
                                         G_TYPE_STRING,    // RELITEM_COL_ROLE
                                         G_TYPE_BOOLEAN,   // RELITEM_COL_ROLE_MODIFIED
                                         G_TYPE_STRING,    // RELITEM_COL_NAME
                                         G_TYPE_POINTER)); // RELITEM_COL_DATA

  gtk_tree_view_set_model(view, GTK_TREE_MODEL(context.store.get()));

  // Debatable whether to sort by the "selected" or the "Name" column by
  // default. Both are be useful, in different ways.
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(context.store.get()),
                                       RELITEM_COL_NAME, GTK_SORT_ASCENDING);

  /* build a list of iters of all items that should be selected */
  trstring selname;
  GtkTreeIter sel_iter;
  relation_list_insert_functor inserter(context, selname, sel_iter);

  std::for_each(context.osm->relations.begin(),
                context.osm->relations.end(), inserter);

  if(!selname.isEmpty())
    list_view_scroll(view, context.selection, &sel_iter);

  g_signal_connect_swapped(context.selection, "changed", G_CALLBACK(changed), &context);

  return osm2go_platform::scrollable_container(GTK_WIDGET(view));
}

} // namespace

void relation_membership_dialog(GtkWidget *parent, const presets_items *presets,
                                osm_t::ref osm, object_t &object) {
  relitem_context_t context(object, presets, osm);

  { // scope to free str earlier
    const trstring str = trstring("Relation memberships of %1 #%2").arg(object.type_string()).arg(object.get_id());

    context.dialog.reset(gtk_dialog_new_with_buttons(static_cast<const gchar *>(str),
                                                     GTK_WINDOW(parent), GTK_DIALOG_MODAL,
                                                     GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                                     nullptr));
  }

  osm2go_platform::dialog_size_hint(context.dialog, osm2go_platform::MISC_DIALOG_LARGE);
  gtk_dialog_set_default_response(context.dialog, GTK_RESPONSE_CLOSE);

  gtk_box_pack_start(context.dialog.vbox(), relation_item_list_widget(context), TRUE, TRUE, 0);

  /* ----------------------------------- */

  gtk_widget_show_all(context.dialog.get());
  gtk_dialog_run(context.dialog);
}
