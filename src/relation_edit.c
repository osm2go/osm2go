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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "relation_edit.h"

#include "appdata.h"
#include "info.h"
#include "list.h"
#include "map.h"
#include "misc.h"

#include <strings.h>

/* --------------- relation dialog for an item (node or way) ----------- */

typedef struct {
  object_t *item;
  appdata_t *appdata;
  GtkWidget *dialog, *view;
  GtkListStore *store;
} relitem_context_t;

enum {
  RELITEM_COL_TYPE = 0,
  RELITEM_COL_ROLE,
  RELITEM_COL_NAME,
  RELITEM_COL_DATA,
  RELITEM_NUM_COLS
};

typedef struct role_chain_t {
  char *role;
  struct role_chain_t *next;
} role_chain_t;

static gboolean relation_add_item(GtkWidget *parent,
			      relation_t *relation, object_t *object) {
  role_chain_t *chain = NULL, **chainP = &chain;

  printf("add object of type %d to relation #" ITEM_ID_FORMAT "\n",
	 object->type, OSM_ID(relation));

  /* ask the user for the role of the new object in this relation */

  /* collect roles first */
  member_t *member = relation->member;
  while(member) {
    if(member->role) {
      /* check if this role has already been saved */
      gboolean already_stored = FALSE;
      role_chain_t *crole = chain;
      while(crole) {
	if(strcasecmp(crole->role, member->role) == 0) {
	  already_stored = TRUE;
	  break;
	}
	crole = crole->next;
      }

      /* not stored yet: attach it */
      if(!already_stored) {
	*chainP = g_new0(role_chain_t, 1);
	(*chainP)->role = g_strdup(member->role);
	chainP = &(*chainP)->next;
      }
    }
    member = member->next;
  }

  /* ------------------ role dialog ---------------- */
  GtkWidget *dialog =
    misc_dialog_new(MISC_DIALOG_NOSIZE,_("Select role"),
		    GTK_WINDOW(parent),
		    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		    NULL);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

  const char *type = osm_tag_get_by_key(OSM_TAG(relation), "type");

  char *info_str = NULL;
  if(type) info_str = g_strdup_printf(_("In relation of type: %s"), type);
  else     info_str = g_strdup_printf(_("In relation #" ITEM_ID_FORMAT),
				      OSM_ID(relation));
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox),
			      gtk_label_new(info_str));
  g_free(info_str);

  const char *name = osm_tag_get_by_key(OSM_TAG(relation), "name");
  if(name)
    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox),
				gtk_label_new(name));

  GtkWidget *hbox = gtk_hbox_new(FALSE, 8);

#ifdef FREMANTLE
  if(!chain)
#endif
    gtk_box_pack_start_defaults(GTK_BOX(hbox), gtk_label_new(_("Role:")));

  GtkWidget *entry = NULL;
  if(chain) {
    entry = combo_box_entry_new(_("Role"));

    /* fill combo box with presets */
    while(chain) {
      role_chain_t *next = chain->next;
      combo_box_append_text(entry, chain->role);
      g_free(chain);
      chain = next;
    }
  } else
    entry = entry_new();

  gtk_box_pack_start_defaults(GTK_BOX(hbox), entry);
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox);

  gtk_widget_show_all(dialog);
  if(GTK_RESPONSE_ACCEPT != gtk_dialog_run(GTK_DIALOG(dialog))) {
    printf("user clicked cancel\n");
    gtk_widget_destroy(dialog);
    return FALSE;
  }

  printf("user clicked ok\n");

  /* get role from dialog */
  const char *ptr = NULL;

  if(GTK_WIDGET_TYPE(entry) == combo_box_entry_type())
    ptr = combo_box_get_active_text(entry);
  else
    ptr = gtk_entry_get_text(GTK_ENTRY(entry));

  char *role = NULL;
  if(ptr && strlen(ptr)) role = g_strdup(ptr);

  gtk_widget_destroy(dialog);

  /* search end of member chain */
  member_t **memberP = &relation->member;
  while(*memberP) memberP = &(*memberP)->next;

  g_assert(osm_object_is_real(object));

  /* create new member */
  *memberP = g_new0(member_t, 1);
  (*memberP)->object = *object;
  (*memberP)->role = role;

  OSM_FLAGS(relation) |= OSM_FLAG_DIRTY;
  return TRUE;
}

static void relation_remove_item(relation_t *relation, const object_t *object) {

  printf("remove object of type %d from relation #" ITEM_ID_FORMAT "\n",
	 object->type, OSM_ID(relation));

  member_t **member = &relation->member;
  while(*member) {
    if(((*member)->object.type == object->type) &&
       (((object->type == NODE) &&
	 (object->node == (*member)->object.node)) ||
	((object->type == WAY) &&
	 (object->way == (*member)->object.way)) ||
	((object->type == RELATION) &&
	 (object->relation == (*member)->object.relation)))) {

      member_t *next = (*member)->next;
      osm_member_free(*member);
      *member = next;

      OSM_FLAGS(relation) |= OSM_FLAG_DIRTY;

      return;
    } else
      member = &(*member)->next;
  }
  g_assert_not_reached();
}

static gboolean relation_info_dialog(GtkWidget *parent, appdata_t *appdata,
				     relation_t *relation) {

  object_t object = { .type = RELATION };
  object.relation = relation;
  return info_dialog(parent, appdata, &object);
}

static const char *relitem_get_role_in_relation(const object_t *item, const relation_t *relation) {
  const member_t *member = relation->member;
  while(member) {
    switch(member->object.type) {

    case NODE:
      if((item->type == NODE) && (item->node == member->object.node))
	return member->role;
      break;

    case WAY:
      if((item->type == WAY) && (item->way == member->object.way))
	return member->role;
      break;

    default:
      break;
    }
    member = member->next;
  }
  return NULL;
}

static gboolean relitem_is_in_relation(const object_t *item, const relation_t *relation) {
  const member_t *member = relation->member;
  while(member) {
    switch(member->object.type) {

    case NODE:
      if((item->type == NODE) && (item->node == member->object.node))
	return TRUE;
      break;

    case WAY:
      if((item->type == WAY) && (item->way == member->object.way))
	return TRUE;
      break;

    default:
      break;
    }
    member = member->next;
  }
  return FALSE;
}

static void changed(GtkTreeSelection *sel, gpointer user_data) {
  relitem_context_t *context = (relitem_context_t*)user_data;

  printf("relation-edit changed event\n");

  /* we need to know what changed in order to let the user acknowlege it! */

  /* walk the entire store */

  GtkTreeIter iter;
  gboolean done = FALSE, ok =
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(context->store), &iter);
  while(ok && !done) {
    relation_t *relation = NULL;
    gtk_tree_model_get(GTK_TREE_MODEL(context->store), &iter,
		       RELITEM_COL_DATA, &relation, -1);
    g_assert(relation);

    if(!relitem_is_in_relation(context->item, relation) &&
       gtk_tree_selection_iter_is_selected(sel, &iter)) {

      printf("selected: " ITEM_ID_FORMAT "\n", OSM_ID(relation));

      /* either accept this or unselect again */
      if(relation_add_item(context->dialog, relation, context->item))
	gtk_list_store_set(context->store, &iter,
	   RELITEM_COL_ROLE,
	   relitem_get_role_in_relation(context->item, relation),
			   -1);
      else
	gtk_tree_selection_unselect_iter(sel, &iter);

      done = TRUE;
    } else if(relitem_is_in_relation(context->item, relation) &&
	      !gtk_tree_selection_iter_is_selected(sel, &iter)) {

      printf("deselected: " ITEM_ID_FORMAT "\n", OSM_ID(relation));

      relation_remove_item(relation, context->item);
      gtk_list_store_set(context->store, &iter,
		       RELITEM_COL_ROLE, NULL,
		       -1);

      done = TRUE;
    }

    if(!done)
      ok = gtk_tree_model_iter_next(GTK_TREE_MODEL(context->store), &iter);
  }
}

#ifndef FREMANTLE
/* we handle these events on our own in order to implement */
/* a very direct selection mechanism (multiple selections usually */
/* require the control key to be pressed). This interferes with */
/* fremantle finger scrolling, but fortunately the fremantle */
/* default behaviour already is what we want. */
static gboolean on_view_clicked(GtkWidget *widget, GdkEventButton *event,
				G_GNUC_UNUSED gpointer user_data) {
  if(event->window == gtk_tree_view_get_bin_window(GTK_TREE_VIEW(widget))) {
    GtkTreePath *path;

    if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
		     event->x, event->y, &path, NULL, NULL, NULL)) {
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

static GtkWidget *relation_item_list_widget(relitem_context_t *context) {
#ifndef FREMANTLE
  context->view = gtk_tree_view_new();
#else
  context->view = hildon_gtk_tree_view_new(HILDON_UI_MODE_EDIT);
#endif

#ifdef USE_HILDON
  /* hildon hides these by default */
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(context->view), TRUE);
#endif

  /* change list mode to "multiple" */
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection(GTK_TREE_VIEW(context->view));
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

#ifndef FREMANTLE
  /* catch views button-press event for our custom handling */
  g_signal_connect(context->view, "button-press-event",
		   G_CALLBACK(on_view_clicked), context);
#endif

  /* --- "Name" column --- */
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
       "Name", renderer, "text", RELITEM_COL_NAME, NULL);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(context->view), column, -1);

  /* --- "Type" column --- */
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(context->view),
      -1, "Type", renderer, "text", RELITEM_COL_TYPE, NULL);

  /* --- "Role" column --- */
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(context->view),
      -1, "Role", renderer, "text", RELITEM_COL_ROLE, NULL);

  /* build and fill the store */
  context->store = gtk_list_store_new(RELITEM_NUM_COLS,
		G_TYPE_STRING, G_TYPE_STRING,
	        G_TYPE_STRING, G_TYPE_POINTER);

  gtk_tree_view_set_model(GTK_TREE_VIEW(context->view),
			  GTK_TREE_MODEL(context->store));
  g_object_unref(context->store);

  // Debatable whether to sort by the "selected" or the "Name" column by
  // default. Both are be useful, in different ways.
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(context->store),
                                       RELITEM_COL_NAME, GTK_SORT_ASCENDING);

  /* build a list of iters of all items that should be selected */

  GtkTreeIter iter, sel_iter;
  gchar *selname = NULL; /* name of sel_iter */
  relation_t *relation = context->appdata->osm->relation;
  while(relation) {
    /* try to find something descriptive */
    gchar *name = relation_get_descriptive_name(relation);

    /* Append a row and fill in some data */
    gtk_list_store_append(context->store, &iter);
    gtk_list_store_set(context->store, &iter,
       RELITEM_COL_TYPE, osm_tag_get_by_key(OSM_TAG(relation), "type"),
       RELITEM_COL_ROLE, relitem_get_role_in_relation(context->item, relation),
       RELITEM_COL_NAME, name,
       RELITEM_COL_DATA, relation,
       -1);

    /* select all relations the current object is part of */
    if(relitem_is_in_relation(context->item, relation)) {
      gtk_tree_selection_select_iter(selection, &iter);
      /* check if this element is earlier by name in the list */
      if(selname == NULL || strcmp(name, selname) < 0) {
        selname = name;
        sel_iter = iter;
      }
    }

    relation = relation->next;
  }

  if(selname != NULL)
    list_view_scroll(GTK_TREE_VIEW(context->view), selection, &sel_iter);

  g_signal_connect(G_OBJECT(selection), "changed",
		   G_CALLBACK(changed), context);

#ifndef FREMANTLE_PANNABLE_AREA
  /* put view into a scrolled window */
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window),
				      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scrolled_window), context->view);
  return scrolled_window;
#else
  /* put view into a pannable area */
  GtkWidget *pannable_area = hildon_pannable_area_new();
  gtk_container_add(GTK_CONTAINER(pannable_area), context->view);
  return pannable_area;
#endif
}

static void
on_dialog_destroy(G_GNUC_UNUSED GtkWidget *widget, gpointer user_data ) {
  relitem_context_t *context = (relitem_context_t*)user_data;
  g_free(context);
}

void relation_membership_dialog(GtkWidget *parent,
			 appdata_t *appdata, object_t *object) {
  relitem_context_t *context = g_new0(relitem_context_t, 1);
  map_t *map = appdata->map;
  g_assert(map);

  context->appdata = appdata;
  context->item = object;

  char *str = NULL;
  switch(object->type) {
  case NODE:
    str = g_strdup_printf(_("Relation memberships of node #" ITEM_ID_FORMAT),
			  OBJECT_ID(*object));
    break;
  case WAY:
    str = g_strdup_printf(_("Relation memberships of way #" ITEM_ID_FORMAT),
			  OBJECT_ID(*object));
    break;
  default:
    g_assert_not_reached();
    break;
  }

  context->dialog =
    misc_dialog_new(MISC_DIALOG_LARGE, str,
		    GTK_WINDOW(parent),
		    GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		    NULL);
  g_free(str);

  gtk_signal_connect(GTK_OBJECT(context->dialog), "destroy",
	     (GtkSignalFunc)on_dialog_destroy, context);

  gtk_dialog_set_default_response(GTK_DIALOG(context->dialog),
				  GTK_RESPONSE_CLOSE);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context->dialog)->vbox),
  		     relation_item_list_widget(context), TRUE, TRUE, 0);

  /* ----------------------------------- */

  gtk_widget_show_all(context->dialog);
  gtk_dialog_run(GTK_DIALOG(context->dialog));
  gtk_widget_destroy(context->dialog);
}

/* -------------------- global relation list ----------------- */

typedef struct {
  appdata_t *appdata;
  GtkWidget *dialog, *list, *show_btn;
  GtkListStore *store;
  object_t *object;     /* object this list relates to, NULL if global */
} relation_context_t;

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
  return NULL;
}

static void relation_list_selected(relation_context_t *context,
				   relation_t *selected) {

  list_button_enable(context->list, LIST_BUTTON_USER0,
		     (selected != NULL) && (selected->member != NULL));
  list_button_enable(context->list, LIST_BUTTON_USER1,
		     (selected != NULL) && (selected->member != NULL));

  list_button_enable(context->list, LIST_BUTTON_REMOVE, selected != NULL);
  list_button_enable(context->list, LIST_BUTTON_EDIT, selected != NULL);
}

static void
relation_list_changed(GtkTreeSelection *selection, gpointer userdata) {
  relation_context_t *context = (relation_context_t*)userdata;
  GtkTreeModel *model = NULL;
  GtkTreeIter iter;

  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    relation_t *relation = NULL;
    gtk_tree_model_get(model, &iter, RELATION_COL_DATA, &relation, -1);
    relation_list_selected(context, relation);
  }
}

typedef struct {
  relation_t *relation;
  GtkWidget *dialog, *view;
  GtkListStore *store;
} member_context_t;

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
member_list_selection_func(G_GNUC_UNUSED GtkTreeSelection *selection, GtkTreeModel *model,
		     GtkTreePath *path, G_GNUC_UNUSED gboolean path_currently_selected,
		     G_GNUC_UNUSED gpointer userdata) {
  GtkTreeIter iter;

  if(gtk_tree_model_get_iter(model, &iter, path)) {
    g_assert_cmpint(gtk_tree_path_get_depth(path), ==, 1);

    const member_t *member = NULL;
    gtk_tree_model_get(model, &iter, MEMBER_COL_DATA, &member, -1);
    if(member && member->object.type < NODE_ID)
      return TRUE;
  }

  return FALSE;
}


static GtkWidget *member_list_widget(member_context_t *context) {
  GtkWidget *vbox = gtk_vbox_new(FALSE,3);

#ifndef FREMANTLE_PANNABLE_AREA
  context->view = gtk_tree_view_new();
#else
  context->view = hildon_gtk_tree_view_new(HILDON_UI_MODE_EDIT);
#endif

  gtk_tree_selection_set_select_function(
	 gtk_tree_view_get_selection(GTK_TREE_VIEW(context->view)),
	 member_list_selection_func,
	 context, NULL);

  /* --- "type" column --- */
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "foreground", "grey", NULL);
  GtkTreeViewColumn *column =
    gtk_tree_view_column_new_with_attributes(_("Type"), renderer,
		     "text", MEMBER_COL_TYPE,
		     "foreground-set", MEMBER_COL_REF_ONLY,  NULL);
  gtk_tree_view_column_set_sort_column_id(column, MEMBER_COL_TYPE);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(context->view), column, -1);

  /* --- "id" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "foreground", "grey", NULL);
  column = gtk_tree_view_column_new_with_attributes(_("Id"), renderer,
		     "text", MEMBER_COL_ID,
		     "foreground-set", MEMBER_COL_REF_ONLY,  NULL);
  gtk_tree_view_column_set_sort_column_id(column, MEMBER_COL_ID);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(context->view), column, -1);


  /* --- "Name" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "foreground", "grey", NULL);
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  column = gtk_tree_view_column_new_with_attributes(_("Name"), renderer,
		     "text", MEMBER_COL_NAME,
		     "foreground-set", MEMBER_COL_REF_ONLY,  NULL);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_column_set_sort_column_id(column, MEMBER_COL_NAME);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(context->view), column, -1);

  /* --- "role" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "foreground", "grey", NULL);
  column = gtk_tree_view_column_new_with_attributes(_("Role"), renderer,
		     "text", MEMBER_COL_ROLE,
		     "foreground-set", MEMBER_COL_REF_ONLY,  NULL);
  gtk_tree_view_column_set_sort_column_id(column, MEMBER_COL_ROLE);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(context->view), column, -1);


  /* build and fill the store */
  context->store = gtk_list_store_new(MEMBER_NUM_COLS,
 	      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
	      G_TYPE_BOOLEAN, G_TYPE_POINTER);

  gtk_tree_view_set_model(GTK_TREE_VIEW(context->view),
			  GTK_TREE_MODEL(context->store));

  GtkTreeIter iter;
  member_t *member = context->relation->member;
  while(member) {
    const tag_t *tags = osm_object_get_tags(&member->object);
    gchar *id = osm_object_id_string(&member->object);

    /* try to find something descriptive */
    const char *name = osm_tag_get_by_key(tags, "name");

    /* Append a row and fill in some data */
    gtk_list_store_append(context->store, &iter);
    gtk_list_store_set(context->store, &iter,
       MEMBER_COL_TYPE, osm_object_type_string(&member->object),
       MEMBER_COL_ID,   id,
       MEMBER_COL_NAME, name,
       MEMBER_COL_ROLE, member->role,
       MEMBER_COL_REF_ONLY, member->object.type >= NODE_ID,
       MEMBER_COL_DATA, member,
       -1);

    g_free(id);
    member = member->next;
  }

  g_object_unref(context->store);

#ifndef FREMANTLE_PANNABLE_AREA
  /* put it into a scrolled window */
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window),
				      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scrolled_window), context->view);

  gtk_box_pack_start_defaults(GTK_BOX(vbox), scrolled_window);
#else
  /* put view into a pannable area */
  GtkWidget *pannable_area = hildon_pannable_area_new();
  gtk_container_add(GTK_CONTAINER(pannable_area), context->view);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), pannable_area);
#endif

  return vbox;
}

void relation_show_members(GtkWidget *parent, relation_t *relation) {
  member_context_t *mcontext = g_new0(member_context_t, 1);
  mcontext->relation = relation;
  gchar *nstr = NULL;

  const char *str = osm_tag_get_by_key(OSM_TAG(mcontext->relation), "name");
  if(!str) str = osm_tag_get_by_key(OSM_TAG(mcontext->relation), "ref");
  if(!str)
    str = nstr = g_strdup_printf(_("Members of relation #" ITEM_ID_FORMAT),
                                 OSM_ID(mcontext->relation));
  else
    str = nstr = g_strdup_printf(_("Members of relation \"%s\""), str);

  mcontext->dialog =
    misc_dialog_new(MISC_DIALOG_MEDIUM, str,
		    GTK_WINDOW(parent),
		    GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		    NULL);
  g_free(nstr);

  gtk_dialog_set_default_response(GTK_DIALOG(mcontext->dialog),
				  GTK_RESPONSE_CLOSE);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mcontext->dialog)->vbox),
      		     member_list_widget(mcontext), TRUE, TRUE, 0);

  /* ----------------------------------- */

  gtk_widget_show_all(mcontext->dialog);
  gtk_dialog_run(GTK_DIALOG(mcontext->dialog));
  gtk_widget_destroy(mcontext->dialog);

  g_free(mcontext);
}

/* user clicked "members" button in relation list */
static void on_relation_members(G_GNUC_UNUSED GtkWidget *button, relation_context_t *context) {
  relation_t *sel = get_selected_relation(context);

  if(sel) relation_show_members(context->dialog, sel);
}

/* user clicked "select" button in relation list */
static void on_relation_select(GtkWidget *but, relation_context_t *context) {
  relation_t *sel = get_selected_relation(context);
  map_item_deselect(context->appdata);

  if(sel) {
    map_relation_select(context->appdata, sel);

    /* tell dialog to close as we want to see the selected relation */

    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(but));
    g_assert(GTK_IS_DIALOG(toplevel));

    /* emit a "response" signal so we might close the dialog */
    gtk_dialog_response(GTK_DIALOG(toplevel), GTK_RESPONSE_CLOSE);
  }
}


static void on_relation_add(G_GNUC_UNUSED GtkWidget *button, relation_context_t *context) {
  /* create a new relation */

  relation_t *relation = osm_relation_new();
  if(!relation_info_dialog(context->dialog, context->appdata, relation)) {
    printf("tag edit cancelled\n");
    osm_relation_free(relation);
  } else {
    osm_relation_attach(context->appdata->osm, relation);

    /* append a row for the new data */

    char *name = relation_get_descriptive_name(relation);

    guint num = osm_relation_members_num(relation);

    /* Append a row and fill in some data */
    GtkTreeIter iter;
    gtk_list_store_append(context->store, &iter);
    gtk_list_store_set(context->store, &iter,
		       RELATION_COL_TYPE,
		       osm_tag_get_by_key(OSM_TAG(relation), "type"),
		       RELATION_COL_NAME, name,
		       RELATION_COL_MEMBERS, num,
		       RELATION_COL_DATA, relation,
		       -1);

    gtk_tree_selection_select_iter(list_get_selection(context->list), &iter);
  }
}

/* user clicked "edit..." button in relation list */
static void on_relation_edit(G_GNUC_UNUSED GtkWidget *button, relation_context_t *context) {
  relation_t *sel = get_selected_relation(context);
  if(!sel) return;

  printf("edit relation #" ITEM_ID_FORMAT "\n", OSM_ID(sel));

  if (!relation_info_dialog(context->dialog, context->appdata, sel))
    return;

  // Locate the changed item
  GtkTreeIter iter;
  gboolean valid = gtk_tree_model_get_iter_first(
    GTK_TREE_MODEL(context->store), &iter);
  while (valid) {
    relation_t *row_rel;
    gtk_tree_model_get(GTK_TREE_MODEL(context->store), &iter,
                       RELATION_COL_DATA, &row_rel,
                       -1);
    if (row_rel == sel)
      break;
    valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(context->store), &iter);
  }
  if (!valid)
    return;

  // Found it. Update all visible fields.
  gtk_list_store_set(context->store, &iter,
    RELATION_COL_TYPE,    osm_tag_get_by_key(OSM_TAG(sel), "type"),
    RELATION_COL_NAME,    relation_get_descriptive_name(sel),
    RELATION_COL_MEMBERS, osm_relation_members_num(sel),
    -1);

  // Order will probably have changed, so refocus
  list_focus_on(context->list, &iter, TRUE);
}


/* remove the selected relation */
static void on_relation_remove(G_GNUC_UNUSED GtkWidget *button, relation_context_t *context) {
  relation_t *sel = get_selected_relation(context);
  if(!sel) return;

  printf("remove relation #" ITEM_ID_FORMAT "\n", OSM_ID(sel));

  guint members = osm_relation_members_num(sel);

  if(members)
    if(!yes_no_f(context->dialog, NULL, 0, 0,
		 _("Delete non-empty relation?"),
		 _("This relation still has %u members. "
		   "Delete it anyway?"), members))
      return;

  /* first remove selected row from list */
  GtkTreeIter       iter;
  GtkTreeModel      *model;
  if(list_get_selected(context->list, &model, &iter))
    gtk_list_store_remove(context->store, &iter);

  /* then really delete it */
  osm_relation_delete(context->appdata->osm, sel, FALSE);

  relation_list_selected(context, NULL);
}

static void relation_list_widget_functor(const relation_t *rel,
                                         relation_context_t *context)
{
  char *name = relation_get_descriptive_name(rel);
  guint num = osm_relation_members_num(rel);
  GtkTreeIter iter;

  /* Append a row and fill in some data */
  gtk_list_store_append(context->store, &iter);
  gtk_list_store_set(context->store, &iter,
                     RELATION_COL_TYPE,
                     osm_tag_get_by_key(OSM_TAG(rel), "type"),
                     RELATION_COL_NAME, name,
                     RELATION_COL_MEMBERS, num,
                     RELATION_COL_DATA, rel,
                     -1);
}

static GtkWidget *relation_list_widget(relation_context_t *context) {
  context->list = list_new(LIST_HILDON_WITH_HEADERS);

  list_override_changed_event(context->list, relation_list_changed, context);

  list_set_columns(context->list,
		   _("Name"),    RELATION_COL_NAME, LIST_FLAG_ELLIPSIZE,
		   _("Type"),    RELATION_COL_TYPE, 0,
		   _("Members"), RELATION_COL_MEMBERS, 0,
		   NULL);

  /* build and fill the store */
  context->store = gtk_list_store_new(RELATION_NUM_COLS,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT,
	        G_TYPE_POINTER);

  list_set_store(context->list, context->store);

  // Sorting by ref/name by default is useful for places with lots of numbered
  // bus routes. Especially for small screens.
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(context->store),
                                       RELATION_COL_NAME, GTK_SORT_ASCENDING);

  if(context->object) {
    GSList *rchain = osm_object_to_relation(context->appdata->osm, context->object);
    g_slist_foreach(rchain, (GFunc)relation_list_widget_functor, context);
    g_slist_free(rchain);
  } else {
    const relation_t *relation = context->appdata->osm->relation;
    for(; relation; relation = relation->next)
      relation_list_widget_functor(relation, context);
  }

  g_object_unref(context->store);

  list_set_static_buttons(context->list, LIST_BTN_NEW | LIST_BTN_WIDE,
	  G_CALLBACK(on_relation_add), G_CALLBACK(on_relation_edit),
	  G_CALLBACK(on_relation_remove), context);

  list_set_user_buttons(context->list,
	LIST_BUTTON_USER0, _("Members"), G_CALLBACK(on_relation_members),
	LIST_BUTTON_USER1, _("Select"),  G_CALLBACK(on_relation_select),
	0);

  relation_list_selected(context, NULL);

  return context->list;
}

/* a global view on all relations */
void relation_list(GtkWidget *parent, appdata_t *appdata, object_t *object) {
  relation_context_t context = { 0 };
  context.appdata = appdata;

  char *str;
  gchar *dstr = NULL;
  if(!object)
    str = _("All relations");
  else {
    dstr = g_strdup_printf(_("Relations of %s"), osm_object_string(object));
    str = dstr;
    context.object = object;
  }

  context.dialog =
    misc_dialog_new(MISC_DIALOG_LARGE, str,
		    GTK_WINDOW(parent),
		    GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		    NULL);

  g_free(dstr);

  gtk_dialog_set_default_response(GTK_DIALOG(context.dialog),
				  GTK_RESPONSE_CLOSE);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
                     relation_list_widget(&context), TRUE, TRUE, 0);

  /* ----------------------------------- */


  gtk_widget_show_all(context.dialog);
  gtk_dialog_run(GTK_DIALOG(context.dialog));

  gtk_widget_destroy(context.dialog);
}

// vim:et:ts=8:sw=2:sts=2:ai
