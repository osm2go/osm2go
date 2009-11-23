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

#include "appdata.h"

/* --------------- relation dialog for an item (node or way) ----------- */

typedef struct {
  object_t *item;
  appdata_t *appdata;
  GtkWidget *dialog, *list;
  GtkListStore *store;
} relitem_context_t;

enum {
  RELITEM_COL_SELECTED = 0,
  RELITEM_COL_TYPE,
  RELITEM_COL_ROLE,
  RELITEM_COL_NAME,
  RELITEM_COL_DATA,
  RELITEM_NUM_COLS
};

typedef struct role_chain_s {
  char *role;
  struct role_chain_s *next;
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
	if(strcasecmp(crole->role, member->role) == 0) already_stored = TRUE;
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

  char *type = osm_tag_get_by_key(OSM_TAG(relation), "type");

  char *info_str = NULL;
  if(type) info_str = g_strdup_printf(_("In relation of type: %s"), type);
  else     info_str = g_strdup_printf(_("In relation #" ITEM_ID_FORMAT), 
				      OSM_ID(relation));
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), 
			      gtk_label_new(info_str));
  g_free(info_str);

  char *name = osm_tag_get_by_key(OSM_TAG(relation), "name");
  if(name) 
    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), 
				gtk_label_new(name));

  GtkWidget *hbox = gtk_hbox_new(FALSE, 8);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), gtk_label_new(_("Role:")));

  GtkWidget *entry = NULL;
  if(chain) {
    entry = gtk_combo_box_entry_new_text();

    /* fill combo box with presets */
    while(chain) {
      role_chain_t *next = chain->next;
      gtk_combo_box_append_text(GTK_COMBO_BOX(entry), chain->role);
      g_free(chain);
      chain = next;
    }
  } else
    entry = gtk_entry_new();

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
  char *ptr = NULL;

  if(GTK_IS_COMBO_BOX(entry))
    ptr = gtk_combo_box_get_active_text(GTK_COMBO_BOX(entry));
  else
    ptr = (char*)gtk_entry_get_text(GTK_ENTRY(entry));

  char *role = NULL;
  if(ptr && strlen(ptr)) role = g_strdup(ptr);

  gtk_widget_destroy(dialog);

  /* search end of member chain */
  member_t **memberP = &relation->member;
  while(*memberP) memberP = &(*memberP)->next;

  g_assert((object->type == NODE)||(object->type == WAY)||
	   (object->type == RELATION));

  /* create new member */
  *memberP = g_new0(member_t, 1);
  (*memberP)->object = *object;
  (*memberP)->role = role;

  OSM_FLAGS(relation) |= OSM_FLAG_DIRTY;
  return TRUE;
}

static void relation_remove_item(relation_t *relation, object_t *object) {

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
  g_assert(0);
}

static void relation_item_list_selected(relitem_context_t *context, 
				   gboolean selected) {

  list_button_enable(context->list, LIST_BUTTON_REMOVE, selected);
  list_button_enable(context->list, LIST_BUTTON_EDIT, selected);
}

/* try to find something descriptive */
static char *relation_get_descriptive_name(relation_t *relation) {
  char *name = osm_tag_get_by_key(OSM_TAG(relation), "ref");
  if (!name)
    name = osm_tag_get_by_key(OSM_TAG(relation), "name");
  if (!name)
    name = osm_tag_get_by_key(OSM_TAG(relation), "note");
  if (!name)
    name = osm_tag_get_by_key(OSM_TAG(relation), "fix" "me");
  return name;
}

static gboolean relation_info_dialog(GtkWidget *parent, appdata_t *appdata, 
				     relation_t *relation) {

  object_t object = { .type = RELATION };
  object.relation = relation;
  return info_dialog(parent, appdata, &object);
} 

static void on_relation_item_add(GtkWidget *but, relitem_context_t *context) {
  /* create a new relation */

  relation_t *relation = osm_relation_new();
  if(!relation_info_dialog(context->dialog, context->appdata, relation)) {
    printf("tag edit cancelled\n");
    osm_relation_free(relation);
  } else {
    osm_relation_attach(context->appdata->osm, relation);

    /* add to list */

    /* append a row for the new data */
    char *name = relation_get_descriptive_name(relation);

    GtkTreeIter iter;
    gtk_list_store_append(context->store, &iter);
    gtk_list_store_set(context->store, &iter,
		       RELITEM_COL_SELECTED, FALSE,
		       RELITEM_COL_TYPE, 
		       osm_tag_get_by_key(OSM_TAG(relation), "type"),
		       RELITEM_COL_NAME, name,
		       RELITEM_COL_DATA, relation,
		       -1);

    gtk_tree_selection_select_iter(list_get_selection(context->list), &iter);
  }
}

static relation_t *get_selection(relitem_context_t *context) {
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  selection = list_get_selection(context->list);
  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    relation_t *relation;
    gtk_tree_model_get(model, &iter, RELITEM_COL_DATA, &relation, -1);
    return(relation);
  }
  return NULL;
}

static void on_relation_item_edit(GtkWidget *but, relitem_context_t *context) {
  relation_t *sel = get_selection(context);
  if(!sel) return;

  printf("edit relation item #" ITEM_ID_FORMAT "\n", OSM_ID(sel));

  if (!relation_info_dialog(context->dialog, context->appdata, sel))
    return;

  // Locate the changed item
  GtkTreeIter iter;
  gboolean valid = gtk_tree_model_get_iter_first(
    GTK_TREE_MODEL(context->store), &iter);
  while (valid) {
    relation_t *row_rel;
    gtk_tree_model_get(GTK_TREE_MODEL(context->store), &iter,
                       RELITEM_COL_DATA, &row_rel,
                       -1);
    if (row_rel == sel)
      break;
    valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(context->store), &iter);
  }
  if (!valid)
    return;

  // Found it. Update all visible fields that belong to the relation iself.
  gtk_list_store_set(context->store, &iter,
     RELITEM_COL_TYPE,    osm_tag_get_by_key(OSM_TAG(sel), "type"),
     RELITEM_COL_NAME,    relation_get_descriptive_name(sel),
    -1);

  // Order will probably have changed, so refocus
  list_focus_on(context->list, &iter, TRUE);
}

/* remove the selected relation */
static void on_relation_item_remove(GtkWidget *but, relitem_context_t *context) {
  relation_t *sel = get_selection(context);
  if(!sel) return;

  printf("remove relation #" ITEM_ID_FORMAT "\n", OSM_ID(sel));

  gint members = osm_relation_members_num(sel);

  if(members) 
    if(!yes_no_f(context->dialog, NULL, 0, 0,
		 _("Delete non-empty relation?"), 
		 _("This relation still has %d members. "
		   "Delete it anyway?"), members))
      return;
  
  /* first remove selected row from list */
  GtkTreeIter       iter;
  GtkTreeSelection *selection = list_get_selection(context->list);
  if(gtk_tree_selection_get_selected(selection, NULL, &iter)) 
    gtk_list_store_remove(context->store, &iter);

  /* then really delete it */
  osm_relation_delete(context->appdata->osm, sel, FALSE); 

  relation_item_list_selected(context, FALSE);
}

static char *relitem_get_role_in_relation(object_t *item, relation_t *relation) {
  member_t *member = relation->member;
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

static void
relitem_toggled(GtkCellRendererToggle *cell, const gchar *path_str,
		relitem_context_t *context) {
  GtkTreePath *path;
  GtkTreeIter iter;

  path = gtk_tree_path_new_from_string(path_str);
  gtk_tree_model_get_iter(GTK_TREE_MODEL(context->store), &iter, path);
  gtk_tree_path_free(path);

  /* get current enabled flag */
  gboolean enabled;
  relation_t *relation = NULL;
  gtk_tree_model_get(GTK_TREE_MODEL(context->store), &iter, 
		     RELITEM_COL_SELECTED, &enabled, 
		     RELITEM_COL_DATA, &relation, 
		     -1);

  list_pre_inplace_edit_tweak(GTK_TREE_MODEL(context->store));

  if(!enabled) {
    printf("will now become be part of this relation\n");
    if(relation_add_item(context->dialog, relation, context->item))
      gtk_list_store_set(context->store, &iter, 
		 RELITEM_COL_SELECTED, TRUE, 
		 RELITEM_COL_ROLE, 
		 relitem_get_role_in_relation(context->item, relation),
		 -1);
  } else {
    printf("item will not be part of this relation anymore\n");
    relation_remove_item(relation, context->item);
    gtk_list_store_set(context->store, &iter, 
		       RELITEM_COL_SELECTED, FALSE, 
		       RELITEM_COL_ROLE, NULL, 
		       -1);
  }

}

static gboolean relitem_is_in_relation(object_t *item, relation_t *relation) {
  member_t *member = relation->member;
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

static GtkWidget *relation_item_list_widget(relitem_context_t *context) {
  context->list = list_new(LIST_HILDON_WITH_HEADERS);

  list_set_columns(context->list, 
		   _(""), RELITEM_COL_SELECTED, LIST_FLAG_TOGGLE, 
		            G_CALLBACK(relitem_toggled), context,
		   _("Type"), RELITEM_COL_TYPE, 0,
		   _("Role"), RELITEM_COL_ROLE, 0,
		   _("Name"), RELITEM_COL_NAME, LIST_FLAG_ELLIPSIZE,
		   NULL);

  /* build and fill the store */
  context->store = gtk_list_store_new(RELITEM_NUM_COLS, 
		G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, 
	        G_TYPE_STRING, G_TYPE_POINTER);

  list_set_store(context->list, context->store);

  // Debatable whether to sort by the "selected" or the "Name" column by
  // default. Both are be useful, in different ways.
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(context->store),
                                       RELITEM_COL_NAME, GTK_SORT_ASCENDING);

  GtkTreeIter iter;
  relation_t *relation = context->appdata->osm->relation;
  while(relation) {
    /* try to find something descriptive */
    char *name = relation_get_descriptive_name(relation);

    /* Append a row and fill in some data */
    gtk_list_store_append(context->store, &iter);
    gtk_list_store_set(context->store, &iter,
       RELITEM_COL_SELECTED, relitem_is_in_relation(context->item, relation),
       RELITEM_COL_TYPE, osm_tag_get_by_key(OSM_TAG(relation), "type"),
       RELITEM_COL_ROLE, relitem_get_role_in_relation(context->item, relation),
       RELITEM_COL_NAME, name,
       RELITEM_COL_DATA, relation,
       -1);
    
    relation = relation->next;
  }
  
  g_object_unref(context->store);

  list_set_static_buttons(context->list, LIST_BTN_BIG, 
    G_CALLBACK(on_relation_item_add), G_CALLBACK(on_relation_item_edit),
    G_CALLBACK(on_relation_item_remove), context);

  relation_item_list_selected(context, FALSE);

  return context->list;
}

void relation_add_dialog(GtkWidget *parent, 
			 appdata_t *appdata, object_t *object) {
  relitem_context_t *context = g_new0(relitem_context_t, 1);
  map_t *map = appdata->map;
  g_assert(map);

  context->appdata = appdata;
  context->item = object;

  char *str = NULL;
  switch(object->type) {
  case NODE:
    str = g_strdup_printf(_("Relations for node #" ITEM_ID_FORMAT), 
			  OBJECT_ID(*object));
    break;
  case WAY:
    str = g_strdup_printf(_("Relations for way #" ITEM_ID_FORMAT), 
			  OBJECT_ID(*object));
    break;
  default:
    g_assert((object->type == NODE) || (object->type == WAY));
    break;
  }
  
  context->dialog = 
    misc_dialog_new(MISC_DIALOG_LARGE, str,
		    GTK_WINDOW(parent),
		    GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, 
		    NULL);
  g_free(str);
  
  gtk_dialog_set_default_response(GTK_DIALOG(context->dialog), 
				  GTK_RESPONSE_CLOSE);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context->dialog)->vbox),
  		     relation_item_list_widget(context), TRUE, TRUE, 0);

  /* ----------------------------------- */

  gtk_widget_show_all(context->dialog);
  gtk_dialog_run(GTK_DIALOG(context->dialog));
  gtk_widget_destroy(context->dialog);

  g_free(context);
}

/* -------------------- global relation list ----------------- */

typedef struct {
  appdata_t *appdata;
  GtkWidget *dialog, *list, *show_btn;
  GtkListStore *store;
  object_t *object;     /* object this list relates to, NULL if global */
} relation_context_t;

enum {
  RELATION_COL_ID = 0,
  RELATION_COL_TYPE,
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

static gboolean
relation_list_selection_func(GtkTreeSelection *selection, GtkTreeModel *model,
		     GtkTreePath *path, gboolean path_currently_selected,
		     gpointer userdata) {
  relation_context_t *context = (relation_context_t*)userdata;
  GtkTreeIter iter;
    
  if(gtk_tree_model_get_iter(model, &iter, path)) {
    g_assert(gtk_tree_path_get_depth(path) == 1);
    
    relation_t *relation = NULL;
    gtk_tree_model_get(model, &iter, RELATION_COL_DATA, &relation, -1);
    relation_list_selected(context, relation);
  }
  
  return TRUE; /* allow selection state to change */
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
member_list_selection_func(GtkTreeSelection *selection, GtkTreeModel *model,
		     GtkTreePath *path, gboolean path_currently_selected,
		     gpointer userdata) {
  GtkTreeIter iter;
    
  if(gtk_tree_model_get_iter(model, &iter, path)) {
    g_assert(gtk_tree_path_get_depth(path) == 1);

    member_t *member = NULL;
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
    tag_t *tags = osm_object_get_tags(&member->object);
    char *id = osm_object_id_string(&member->object);

    /* try to find something descriptive */
    char *name = NULL;
    if(tags)
      name = osm_tag_get_by_key(tags, "name");

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

  char *str = osm_tag_get_by_key(OSM_TAG(mcontext->relation), "name");
  if(!str) str = osm_tag_get_by_key(OSM_TAG(mcontext->relation), "ref");
  if(!str)
    str = g_strdup_printf(_("Members of relation #" ITEM_ID_FORMAT),
			  OSM_ID(mcontext->relation));
  else
    str = g_strdup_printf(_("Members of relation \"%s\""), str);

  mcontext->dialog = 
    misc_dialog_new(MISC_DIALOG_MEDIUM, str,
		    GTK_WINDOW(parent),
		    GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, 
		    NULL);
  g_free(str);
  
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
static void on_relation_members(GtkWidget *but, relation_context_t *context) {
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


static void on_relation_add(GtkWidget *but, relation_context_t *context) {
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
		       RELATION_COL_ID, OSM_ID(relation),
		       RELATION_COL_TYPE,
		       osm_tag_get_by_key(OSM_TAG(relation), "type"),
		       RELATION_COL_NAME, name,
		       RELATION_COL_MEMBERS, num,
		       RELATION_COL_DATA, relation,
		       -1);

    gtk_tree_selection_select_iter(list_get_selection(context->list), &iter);

    /* scroll to end */
    //    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment();
    /* xyz */
  }
}

/* user clicked "edit..." button in relation list */
static void on_relation_edit(GtkWidget *but, relation_context_t *context) {
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
    RELATION_COL_ID,      OSM_ID(sel),
    RELATION_COL_TYPE,    osm_tag_get_by_key(OSM_TAG(sel), "type"),
    RELATION_COL_NAME,    relation_get_descriptive_name(sel),
    RELATION_COL_MEMBERS, osm_relation_members_num(sel),
    -1);

  // Order will probably have changed, so refocus
  list_focus_on(context->list, &iter, TRUE);
}


/* remove the selected relation */
static void on_relation_remove(GtkWidget *but, relation_context_t *context) {
  relation_t *sel = get_selected_relation(context);
  if(!sel) return;

  printf("remove relation #" ITEM_ID_FORMAT "\n", OSM_ID(sel));

  gint members = osm_relation_members_num(sel);

  if(members) 
    if(!yes_no_f(context->dialog, NULL, 0, 0,
		 _("Delete non-empty relation?"), 
		 _("This relation still has %d members. "
		   "Delete it anyway?"), members))
      return;
  
  /* first remove selected row from list */
  GtkTreeIter       iter;
  GtkTreeSelection *selection = list_get_selection(context->list);
  if(gtk_tree_selection_get_selected(selection, NULL, &iter)) 
    gtk_list_store_remove(context->store, &iter);

  /* then really delete it */
  osm_relation_delete(context->appdata->osm, sel, FALSE); 

  relation_list_selected(context, NULL);
}

static GtkWidget *relation_list_widget(relation_context_t *context) {
  context->list = list_new(LIST_HILDON_WITH_HEADERS);

  list_set_selection_function(context->list, relation_list_selection_func, 
			      context);

  list_set_columns(context->list,
		   _("Id"),      RELATION_COL_ID, 0,
		   _("Type"),    RELATION_COL_TYPE, 0,
		   _("Name"),    RELATION_COL_NAME, LIST_FLAG_ELLIPSIZE,
		   _("Members"), RELATION_COL_MEMBERS, 0,
		   NULL);

  /* build and fill the store */
  context->store = gtk_list_store_new(RELATION_NUM_COLS, 
		G_TYPE_ITEM_ID_T, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT, 
	        G_TYPE_POINTER);

  list_set_store(context->list, context->store);

  // Sorting by ref/name by default is useful for places with lots of numbered
  // bus routes. Especially for small screens.
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(context->store),
                                       RELATION_COL_NAME, GTK_SORT_ASCENDING);
  
  GtkTreeIter iter;
  relation_t *relation = NULL;
  relation_chain_t *rchain = NULL;

  if(context->object) 
    rchain = osm_object_to_relation(context->appdata->osm, context->object);
  else
    relation = context->appdata->osm->relation;

  while(relation || rchain) {
    relation_t *rel = relation?relation:rchain->relation;

    char *name = relation_get_descriptive_name(rel);
    guint num = osm_relation_members_num(rel);

    /* Append a row and fill in some data */
    gtk_list_store_append(context->store, &iter);
    gtk_list_store_set(context->store, &iter,
		       RELATION_COL_ID, OSM_ID(rel),
		       RELATION_COL_TYPE,
		       osm_tag_get_by_key(OSM_TAG(rel), "type"),
		       RELATION_COL_NAME, name,
		       RELATION_COL_MEMBERS, num,
		       RELATION_COL_DATA, rel,
		       -1);

    if(relation) relation = relation->next;
    if(rchain)   rchain = rchain->next;
  }

  if(rchain)
    osm_relation_chain_free(rchain);
  
  g_object_unref(context->store);

  list_set_static_buttons(context->list, LIST_BTN_NEW, 
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
  relation_context_t *context = g_new0(relation_context_t, 1);
  context->appdata = appdata;

  char *str = NULL;
  if(!object) 
    str = g_strdup(_("All relations"));
  else {
    str = g_strdup_printf(_("Relations of %s"), osm_object_string(object));
    context->object = object;
  }

  context->dialog = 
    misc_dialog_new(MISC_DIALOG_LARGE, str,
		    GTK_WINDOW(parent),
		    GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, 
		    NULL);

  g_free(str);
  
  gtk_dialog_set_default_response(GTK_DIALOG(context->dialog), 
				  GTK_RESPONSE_CLOSE);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context->dialog)->vbox),
  		     relation_list_widget(context), TRUE, TRUE, 0);

  /* ----------------------------------- */


  gtk_widget_show_all(context->dialog);
  gtk_dialog_run(GTK_DIALOG(context->dialog));

  gtk_widget_destroy(context->dialog);
  g_free(context);
}

// vim:et:ts=8:sw=2:sts=2:ai
