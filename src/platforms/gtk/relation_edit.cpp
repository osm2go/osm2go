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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "relation_edit.h"

#include <info.h>
#include <josm_presets.h>
#include <list.h>
#include <map.h>
#include <misc.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <set>
#include <string>
#include <strings.h>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include "osm2go_i18n.h"
#include "osm2go_platform_gtk.h"

/* --------------- relation dialog for an item (node or way) ----------- */

struct relitem_context_t {
  relitem_context_t(object_t &o, const presets_items *pr, osm_t *os);

  object_t &item;
  const presets_items * const presets;
  osm_t * const osm;
  g_widget dialog;
  std::unique_ptr<GtkListStore, g_object_deleter> store;
};

enum {
  RELITEM_COL_TYPE = 0,
  RELITEM_COL_ROLE,
  RELITEM_COL_NAME,
  RELITEM_COL_DATA,
  RELITEM_NUM_COLS
};

relitem_context_t::relitem_context_t(object_t &o, const presets_items *pr, osm_t *os)
  : item(o)
  , presets(pr)
  , osm(os)
  , store(O2G_NULLPTR)
{
}

struct entry_insert_text {
  GtkWidget * const entry;
  explicit entry_insert_text(GtkWidget *en) : entry(en) {}
  inline void operator()(const std::string &role) {
    combo_box_append_text(entry, role.c_str());
  }
};

struct relation_context_t {
  inline relation_context_t(map_t *m, osm_t *o, presets_items *p, GtkWidget *d)
    : map(m), osm(o), presets(p), dialog(d) {}

  map_t * const map;
  osm_t * const osm;
  presets_items * const presets;
  g_widget dialog;
  GtkWidget *list, *show_btn;
  std::unique_ptr<GtkListStore, g_object_deleter> store;
};

static bool relation_add_item(GtkWidget *parent, relation_t *relation,
                              const object_t &object, const presets_items *presets) {
  g_debug("add object of type %d to relation #" ITEM_ID_FORMAT "", object.type, relation->id);

  const std::set<std::string> &roles = preset_roles(relation, object, presets);

  /* ask the user for the role of the new object in this relation */
  /* ------------------ role dialog ---------------- */
  g_widget dialog(gtk_dialog_new_with_buttons(_("Select role"), GTK_WINDOW(parent),
                                              GTK_DIALOG_MODAL,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              O2G_NULLPTR));

  gtk_dialog_set_default_response(GTK_DIALOG(dialog.get()), GTK_RESPONSE_ACCEPT);

  const char *type = relation->tags.get_value("type");

  g_string info_str(type ?
                    g_strdup_printf(_("In relation of type: %s"), type) :
                    g_strdup_printf(_("In relation #" ITEM_ID_FORMAT), relation->id));
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog.get())->vbox),
                              gtk_label_new(info_str.get()));
  info_str.reset();

  const char *name = relation->tags.get_value("name");
  if(name)
    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog.get())->vbox),
				gtk_label_new(name));

  GtkWidget *hbox = gtk_hbox_new(FALSE, 8);

#ifdef FREMANTLE
  if(roles.empty())
#endif
    gtk_box_pack_start_defaults(GTK_BOX(hbox), gtk_label_new(_("Role:")));

  GtkWidget *entry = O2G_NULLPTR;
  if(!roles.empty()) {
    entry = combo_box_entry_new(_("Role"));

    /* fill combo box with presets */
    std::for_each(roles.begin(), roles.end(), entry_insert_text(entry));
  } else
    entry = entry_new();

  gtk_box_pack_start_defaults(GTK_BOX(hbox), entry);
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog.get())->vbox), hbox);

  gtk_widget_show_all(dialog.get());
  if(GTK_RESPONSE_ACCEPT != gtk_dialog_run(GTK_DIALOG(dialog.get()))) {
    g_debug("user clicked cancel");
    return false;
  }

  g_debug("user clicked ok");

  /* get role from dialog */
  const char *role = O2G_NULLPTR;
  std::string rstr;

  if(isComboBoxEntryWidget(entry)) {
    rstr = combo_box_get_active_text(entry);
    if(!rstr.empty())
      role = rstr.c_str();
  } else {
    const gchar *ptr = gtk_entry_get_text(GTK_ENTRY(entry));
    if(ptr && strlen(ptr))
      role = ptr;
  }

  // create new member
  // must be done before the widget is destroyed as it may reference the
  // internal string from the text entry
  relation->members.push_back(member_t(object, role));

  assert(object.is_real());

  relation->flags |= OSM_FLAG_DIRTY;
  return true;
}

static bool relation_info_dialog(relation_context_t *context, relation_t *relation) {
  object_t object(relation);
  return info_dialog(context->dialog.get(), context->map, context->osm, context->presets, object);
}

static void changed(GtkTreeSelection *sel, relitem_context_t *context) {
  g_debug("relation-edit changed event");

  /* we need to know what changed in order to let the user acknowlege it! */

  /* walk the entire store */

  GtkTreeIter iter;
  gboolean ok = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(context->store.get()), &iter);
  while(ok == TRUE) {
    relation_t *relation = O2G_NULLPTR;
    gtk_tree_model_get(GTK_TREE_MODEL(context->store.get()), &iter,
		       RELITEM_COL_DATA, &relation, -1);
    assert(relation != O2G_NULLPTR);

    const std::vector<member_t>::const_iterator itEnd = relation->members.end();
    const std::vector<member_t>::iterator it = relation->find_member_object(context->item);

    if(it == itEnd && gtk_tree_selection_iter_is_selected(sel, &iter)) {
      g_debug("selected: " ITEM_ID_FORMAT, relation->id);

      /* either accept this or unselect again */
      if(relation_add_item(context->dialog.get(), relation, context->item, context->presets)) {
        // the item is now the last one in the chain
        const member_t &member = relation->members.back();
        gtk_list_store_set(context->store.get(), &iter, RELITEM_COL_ROLE, member.role, -1);
      } else
        gtk_tree_selection_unselect_iter(sel, &iter);

      break;
    } else if(it != itEnd && !gtk_tree_selection_iter_is_selected(sel, &iter)) {
      g_debug("deselected: " ITEM_ID_FORMAT, relation->id);

      relation->remove_member(it);
      gtk_list_store_set(context->store.get(), &iter, RELITEM_COL_ROLE, O2G_NULLPTR, -1);

      break;
    }

    ok = gtk_tree_model_iter_next(GTK_TREE_MODEL(context->store.get()), &iter);
  }
}

#ifndef FREMANTLE
/* we handle these events on our own in order to implement */
/* a very direct selection mechanism (multiple selections usually */
/* require the control key to be pressed). This interferes with */
/* fremantle finger scrolling, but fortunately the fremantle */
/* default behaviour already is what we want. */
static gboolean on_view_clicked(GtkWidget *widget, GdkEventButton *event, gpointer) {
  if(event->window == gtk_tree_view_get_bin_window(GTK_TREE_VIEW(widget))) {
    GtkTreePath *path;

    if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
		     event->x, event->y, &path, O2G_NULLPTR, O2G_NULLPTR, O2G_NULLPTR)) {
      GtkTreeSelection *sel =
	gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

      if(!gtk_tree_selection_path_is_selected(sel, path))
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
  GtkTreeSelection * const selection;
  GtkTreeIter sel_iter;
  std::string selname; /* name of sel_iter */
  relation_list_insert_functor(relitem_context_t &c, GtkTreeSelection *s) : context(c), selection(s) {}
  void operator()(std::pair<item_id_t, relation_t *> pair);
};

void relation_list_insert_functor::operator()(std::pair<item_id_t, relation_t *> pair)
{
  const relation_t * const relation = pair.second;

  if(relation->flags & OSM_FLAG_DELETED)
    return;

  GtkTreeIter iter;
  /* try to find something descriptive */
  std::string name = relation->descriptive_name();

  const std::vector<member_t>::const_iterator it = relation->find_member_object(context.item);
  const bool isMember = it != relation->members.end();

  /* Append a row and fill in some data */
  gtk_list_store_append(context.store.get(), &iter);
  gtk_list_store_set(context.store.get(), &iter,
     RELITEM_COL_TYPE, relation->tags.get_value("type"),
     RELITEM_COL_ROLE, isMember ? it->role : O2G_NULLPTR,
     RELITEM_COL_NAME, name.c_str(),
     RELITEM_COL_DATA, relation,
     -1);

  /* select all relations the current object is part of */

  if(isMember) {
    gtk_tree_selection_select_iter(selection, &iter);
    /* check if this element is earlier by name in the list */
    if(selname.empty() || strcmp(name.c_str(), selname.c_str()) < 0) {
      selname.swap(name);
      sel_iter = iter;
    }
  }
}

static GtkWidget *relation_item_list_widget(relitem_context_t &context) {
  GtkTreeView *view = osm2go_platform::tree_view_new();

#ifdef FREMANTLE
  /* hildon hides these by default */
  gtk_tree_view_set_headers_visible(view, TRUE);
#endif

  /* change list mode to "multiple" */
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection(view);
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

#ifndef FREMANTLE
  /* catch views button-press event for our custom handling */
  g_signal_connect(view, "button-press-event",
		   G_CALLBACK(on_view_clicked), &context);
#endif

  /* --- "Name" column --- */
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, O2G_NULLPTR );
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
       "Name", renderer, "text", RELITEM_COL_NAME, O2G_NULLPTR);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(view, column, -1);

  /* --- "Type" column --- */
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(view,
      -1, "Type", renderer, "text", RELITEM_COL_TYPE, O2G_NULLPTR);

  /* --- "Role" column --- */
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(view,
      -1, "Role", renderer, "text", RELITEM_COL_ROLE, O2G_NULLPTR);

  /* build and fill the store */
  context.store.reset(gtk_list_store_new(RELITEM_NUM_COLS, G_TYPE_STRING, G_TYPE_STRING,
                                         G_TYPE_STRING, G_TYPE_POINTER));

  gtk_tree_view_set_model(view, GTK_TREE_MODEL(context.store.get()));

  // Debatable whether to sort by the "selected" or the "Name" column by
  // default. Both are be useful, in different ways.
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(context.store.get()),
                                       RELITEM_COL_NAME, GTK_SORT_ASCENDING);

  /* build a list of iters of all items that should be selected */
  relation_list_insert_functor inserter(context, selection);

  std::for_each(context.osm->relations.begin(),
                context.osm->relations.end(), inserter);

  if(!inserter.selname.empty())
    list_view_scroll(view, selection, &inserter.sel_iter);

  g_signal_connect(selection, "changed", G_CALLBACK(changed), &context);

  return osm2go_platform::scrollable_container(GTK_WIDGET(view));
}

void relation_membership_dialog(GtkWidget *parent, const presets_items *presets,
                                osm_t *osm, object_t &object) {
  relitem_context_t context(object, presets, osm);

  g_string str(g_strdup_printf(_("Relation memberships of %s #" ITEM_ID_FORMAT),
                               object.type_string(), object.get_id()));

  context.dialog.reset(gtk_dialog_new_with_buttons(str.get(), GTK_WINDOW(parent),
                                               GTK_DIALOG_MODAL,
                                               GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                               O2G_NULLPTR));
  str.reset();

  dialog_size_hint(GTK_WINDOW(context.dialog.get()), MISC_DIALOG_LARGE);
  gtk_dialog_set_default_response(GTK_DIALOG(context.dialog.get()), GTK_RESPONSE_CLOSE);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog.get())->vbox),
                     relation_item_list_widget(context), TRUE, TRUE, 0);

  /* ----------------------------------- */

  gtk_widget_show_all(context.dialog.get());
  gtk_dialog_run(GTK_DIALOG(context.dialog.get()));
}

/* -------------------- global relation list ----------------- */

enum {
  RELATION_COL_TYPE = 0,
  RELATION_COL_NAME,
  RELATION_COL_MEMBERS,
  RELATION_COL_DATA,
  RELATION_NUM_COLS
};

static relation_t *get_selected_relation(relation_context_t *context) {
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  selection = list_get_selection(context->list);
  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    relation_t *relation;
    gtk_tree_model_get(model, &iter, RELATION_COL_DATA, &relation, -1);
    return(relation);
  }
  return O2G_NULLPTR;
}

static void relation_list_selected(GtkWidget *list,
				   relation_t *selected) {

  list_button_enable(list, LIST_BUTTON_USER0,
		     (selected != O2G_NULLPTR) && (!selected->members.empty()));
  list_button_enable(list, LIST_BUTTON_USER1,
		     (selected != O2G_NULLPTR) && (!selected->members.empty()));

  list_button_enable(list, LIST_BUTTON_REMOVE, selected != O2G_NULLPTR);
  list_button_enable(list, LIST_BUTTON_EDIT, selected != O2G_NULLPTR);
}

static void
relation_list_changed(GtkTreeSelection *selection, gpointer userdata) {
  GtkWidget *list = static_cast<relation_context_t *>(userdata)->list;
  GtkTreeModel *model = O2G_NULLPTR;
  GtkTreeIter iter;

  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    relation_t *relation = O2G_NULLPTR;
    gtk_tree_model_get(model, &iter, RELATION_COL_DATA, &relation, -1);
    relation_list_selected(list, relation);
  }
}

struct member_context_t {
  member_context_t(const relation_t *r, GtkWidget *d)
    : relation(r), dialog(d) {}
  const relation_t * const relation;
  GtkWidget * const dialog;
};

enum {
  MEMBER_COL_TYPE = 0,
  MEMBER_COL_ID,
  MEMBER_COL_NAME,
  MEMBER_COL_ROLE,
  MEMBER_COL_REF_ONLY,
  MEMBER_COL_DATA,
  MEMBER_NUM_COLS
};

static gboolean
member_list_selection_func(GtkTreeSelection *, GtkTreeModel *model,
                           GtkTreePath *path, gboolean, gpointer) {
  GtkTreeIter iter;

  if(gtk_tree_model_get_iter(model, &iter, path)) {
    assert_cmpnum(gtk_tree_path_get_depth(path), 1);

    const member_t *member = O2G_NULLPTR;
    gtk_tree_model_get(model, &iter, MEMBER_COL_DATA, &member, -1);
    if(member && member->object.type < NODE_ID)
      return TRUE;
  }

  return FALSE;
}

struct members_list_functor {
  GtkListStore * const store;
  explicit members_list_functor(GtkListStore *s) : store(s) {}
  void operator()(const member_t &member);
};

void members_list_functor::operator()(const member_t &member)
{
  GtkTreeIter iter;

  const std::string &id = member.object.id_string();

  /* try to find something descriptive */
  const std::string &name = member.object.is_real() ? member.object.get_name() : std::string();

  /* Append a row and fill in some data */
  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter,
     MEMBER_COL_TYPE, member.object.type_string(),
     MEMBER_COL_ID,   id.c_str(),
     MEMBER_COL_NAME, name.c_str(),
     MEMBER_COL_ROLE, member.role,
     MEMBER_COL_REF_ONLY, member.object.type >= NODE_ID,
     MEMBER_COL_DATA, &member,
     -1);
}

static GtkWidget *member_list_widget(member_context_t &context) {
  GtkWidget *vbox = gtk_vbox_new(FALSE,3);
  GtkTreeView * const view = osm2go_platform::tree_view_new();

  gtk_tree_selection_set_select_function(gtk_tree_view_get_selection(view),
                                         member_list_selection_func, &context, O2G_NULLPTR);

  /* --- "type" column --- */
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "foreground", "grey", O2G_NULLPTR);
  GtkTreeViewColumn *column =
    gtk_tree_view_column_new_with_attributes(_("Type"), renderer,
		     "text", MEMBER_COL_TYPE,
		     "foreground-set", MEMBER_COL_REF_ONLY,  O2G_NULLPTR);
  gtk_tree_view_column_set_sort_column_id(column, MEMBER_COL_TYPE);
  gtk_tree_view_insert_column(view, column, -1);

  /* --- "id" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "foreground", "grey", O2G_NULLPTR);
  column = gtk_tree_view_column_new_with_attributes(_("Id"), renderer,
		     "text", MEMBER_COL_ID,
		     "foreground-set", MEMBER_COL_REF_ONLY,  O2G_NULLPTR);
  gtk_tree_view_column_set_sort_column_id(column, MEMBER_COL_ID);
  gtk_tree_view_insert_column(view, column, -1);

  /* --- "Name" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "foreground", "grey", O2G_NULLPTR);
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, O2G_NULLPTR);
  column = gtk_tree_view_column_new_with_attributes(_("Name"), renderer,
		     "text", MEMBER_COL_NAME,
		     "foreground-set", MEMBER_COL_REF_ONLY,  O2G_NULLPTR);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_column_set_sort_column_id(column, MEMBER_COL_NAME);
  gtk_tree_view_insert_column(view, column, -1);

  /* --- "role" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "foreground", "grey", O2G_NULLPTR);
  column = gtk_tree_view_column_new_with_attributes(_("Role"), renderer,
		     "text", MEMBER_COL_ROLE,
		     "foreground-set", MEMBER_COL_REF_ONLY,  O2G_NULLPTR);
  gtk_tree_view_column_set_sort_column_id(column, MEMBER_COL_ROLE);
  gtk_tree_view_insert_column(view, column, -1);


  /* build and fill the store */
  GtkListStore * const store = gtk_list_store_new(MEMBER_NUM_COLS,
                                                  G_TYPE_STRING, G_TYPE_STRING,
                                                  G_TYPE_STRING, G_TYPE_STRING,
                                                  G_TYPE_BOOLEAN, G_TYPE_POINTER);

  gtk_tree_view_set_model(view, GTK_TREE_MODEL(store));

  std::for_each(context.relation->members.begin(), context.relation->members.end(),
                members_list_functor(store));

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
                              osm2go_platform::scrollable_container(GTK_WIDGET(view)));

  return vbox;
}

void relation_show_members(GtkWidget *parent, const relation_t *relation) {
  const char *str = relation->tags.get_value("name");
  if(!str)
    str = relation->tags.get_value("ref");

  g_string nstr(str == O2G_NULLPTR ?
                g_strdup_printf(_("Members of relation #" ITEM_ID_FORMAT), relation->id) :
                g_strdup_printf(_("Members of relation \"%s\""), str));

  member_context_t mcontext(relation,
                            gtk_dialog_new_with_buttons(nstr.get(), GTK_WINDOW(parent),
                                                        GTK_DIALOG_MODAL,
                                                        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                                        O2G_NULLPTR));

  dialog_size_hint(GTK_WINDOW(mcontext.dialog), MISC_DIALOG_MEDIUM);
  gtk_dialog_set_default_response(GTK_DIALOG(mcontext.dialog),
				  GTK_RESPONSE_CLOSE);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mcontext.dialog)->vbox),
      		     member_list_widget(mcontext), TRUE, TRUE, 0);

  /* ----------------------------------- */

  gtk_widget_show_all(mcontext.dialog);
  gtk_dialog_run(GTK_DIALOG(mcontext.dialog));
  gtk_widget_destroy(mcontext.dialog);
}

/* user clicked "members" button in relation list */
static void on_relation_members(relation_context_t *context) {
  relation_t *sel = get_selected_relation(context);

  if(sel != O2G_NULLPTR)
    relation_show_members(context->dialog.get(), sel);
}

/* user clicked "select" button in relation list */
static void on_relation_select(relation_context_t *context, GtkWidget *but) {
  relation_t *sel = get_selected_relation(context);
  context->map->item_deselect();

  if(sel) {
    context->map->select_relation(sel);

    /* tell dialog to close as we want to see the selected relation */

    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(but));
    assert(GTK_IS_DIALOG(toplevel) == TRUE);

    /* emit a "response" signal so we might close the dialog */
    gtk_dialog_response(GTK_DIALOG(toplevel), GTK_RESPONSE_CLOSE);
  }
}


static void on_relation_add(relation_context_t *context) {
  /* create a new relation */

  relation_t *relation = new relation_t(0);
  if(!relation_info_dialog(context, relation)) {
    g_debug("tag edit cancelled");
    relation->cleanup();
    delete relation;
  } else {
    context->osm->relation_attach(relation);

    /* append a row for the new data */

    const std::string &name = relation->descriptive_name();

    /* Append a row and fill in some data */
    GtkTreeIter iter;
    gtk_list_store_append(context->store.get(), &iter);
    gtk_list_store_set(context->store.get(), &iter,
		       RELATION_COL_TYPE,
		       relation->tags.get_value("type"),
		       RELATION_COL_NAME, name.c_str(),
		       RELATION_COL_MEMBERS, relation->members.size(),
		       RELATION_COL_DATA, relation,
		       -1);

    gtk_tree_selection_select_iter(list_get_selection(context->list), &iter);
  }
}

/* user clicked "edit..." button in relation list */
static void on_relation_edit(relation_context_t *context) {
  relation_t *sel = get_selected_relation(context);
  if(!sel) return;

  g_debug("edit relation #" ITEM_ID_FORMAT, sel->id);

  if (!relation_info_dialog(context, sel))
    return;

  // Locate the changed item
  GtkTreeIter iter;
  gboolean valid = gtk_tree_model_get_iter_first(
    GTK_TREE_MODEL(context->store.get()), &iter);
  while (valid) {
    relation_t *row_rel;
    gtk_tree_model_get(GTK_TREE_MODEL(context->store.get()), &iter,
                       RELATION_COL_DATA, &row_rel,
                       -1);
    if (row_rel == sel)
      break;
    valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(context->store.get()), &iter);
  }
  if (!valid)
    return;

  const std::string &name = sel->descriptive_name();
  // Found it. Update all visible fields.
  gtk_list_store_set(context->store.get(), &iter,
    RELATION_COL_TYPE,    sel->tags.get_value("type"),
    RELATION_COL_NAME,    name.c_str(),
    RELATION_COL_MEMBERS, sel->members.size(),
    -1);

  // Order will probably have changed, so refocus
  list_focus_on(context->list, &iter);
}

/* remove the selected relation */
static void on_relation_remove(relation_context_t *context) {
  relation_t *sel = get_selected_relation(context);
  if(!sel) return;

  g_debug("remove relation #" ITEM_ID_FORMAT, sel->id);

  if(!sel->members.empty())
    if(!yes_no_f(context->dialog.get(), 0, _("Delete non-empty relation?"),
                 _("This relation still has %zu members. Delete it anyway?"),
                 sel->members.size()))
      return;

  /* first remove selected row from list */
  GtkTreeIter       iter;
  GtkTreeModel      *model;
  if(list_get_selected(context->list, &model, &iter))
    gtk_list_store_remove(context->store.get(), &iter);

  /* then really delete it */
  context->osm->relation_delete(sel);

  relation_list_selected(context->list, O2G_NULLPTR);
}

struct relation_list_widget_functor {
  GtkListStore * const store;
  explicit relation_list_widget_functor(GtkListStore *s) : store(s) {}
  void operator()(const relation_t *rel);
  inline void operator()(std::pair<item_id_t, relation_t *> pair) {
    operator()(pair.second);
  }
};

void relation_list_widget_functor::operator()(const relation_t *rel)
{
  if(rel->flags & OSM_FLAG_DELETED)
    return;

  const std::string &name = rel->descriptive_name();
  GtkTreeIter iter;

  /* Append a row and fill in some data */
  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter,
                     RELATION_COL_TYPE, rel->tags.get_value("type"),
                     RELATION_COL_NAME, name.c_str(),
                     RELATION_COL_MEMBERS, rel->members.size(),
                     RELATION_COL_DATA, rel,
                     -1);
}

static GtkWidget *relation_list_widget(relation_context_t &context) {
  std::vector<list_view_column> columns;
  columns.push_back(list_view_column(_("Type"),    0));
  columns.push_back(list_view_column(_("Name"),    LIST_FLAG_ELLIPSIZE));
  columns.push_back(list_view_column(_("Members"), 0));

  std::vector<list_button> buttons;
  buttons.push_back(list_button(_("_New"), G_CALLBACK(on_relation_add)));
  buttons.push_back(list_button(_("_Edit"), G_CALLBACK(on_relation_edit)));
  buttons.push_back(list_button(_("Remove"), G_CALLBACK(on_relation_remove)));
  buttons.push_back(list_button(_("Members"), GCallback(on_relation_members)));
  buttons.push_back(list_button(_("Select"),  GCallback(on_relation_select)));

  /* build and fill the store */
  context.store.reset(gtk_list_store_new(RELATION_NUM_COLS,
                                         G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT,
                                         G_TYPE_POINTER));

  context.list = list_new(LIST_HILDON_WITH_HEADERS, 0, &context,
                          relation_list_changed, buttons, columns,
                          context.store.get());

  // Sorting by ref/name by default is useful for places with lots of numbered
  // bus routes. Especially for small screens.
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(context.store.get()),
                                       RELATION_COL_NAME, GTK_SORT_ASCENDING);

  relation_list_widget_functor fc(context.store.get());

  const std::map<item_id_t, relation_t *> &rchain = context.osm->relations;
  std::for_each(rchain.begin(), rchain.end(), fc);

  relation_list_selected(context.list, O2G_NULLPTR);

  return context.list;
}

/* a global view on all relations */
void relation_list(GtkWidget *parent, map_t *map, osm_t *osm, presets_items *presets) {
  relation_context_t context(map, osm, presets,
                             gtk_dialog_new_with_buttons(_("All relations"),
                                                         GTK_WINDOW(parent),
                                                         GTK_DIALOG_MODAL,
                                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                                         O2G_NULLPTR));

  dialog_size_hint(GTK_WINDOW(context.dialog.get()), MISC_DIALOG_LARGE);
  gtk_dialog_set_default_response(GTK_DIALOG(context.dialog.get()),
				  GTK_RESPONSE_CLOSE);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog.get())->vbox),
                     relation_list_widget(context), TRUE, TRUE, 0);

  /* ----------------------------------- */


  gtk_widget_show_all(context.dialog.get());
  gtk_dialog_run(GTK_DIALOG(context.dialog.get()));
}

// vim:et:ts=8:sw=2:sts=2:ai