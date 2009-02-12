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
  relation_item_t *item;
  appdata_t *appdata;
  GtkWidget *dialog, *view;
  GtkListStore *store;
  GtkWidget *but_add, *but_edit, *but_remove;
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
			      relation_t *relation, relation_item_t *item) {
  role_chain_t *chain = NULL, **chainP = &chain;

  printf("add item of type %d to relation #%ld\n", 
	 item->type, relation->id);

  /* ask the user for the role of the new item in this relation */
  
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
  GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Select role"),
	  GTK_WINDOW(parent), GTK_DIALOG_MODAL,
	  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
	  NULL);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

  char *type = osm_tag_get_by_key(relation->tag, "type");

  char *info_str = NULL;
  if(type) info_str = g_strdup_printf(_("In relation of type: %s"), type);
  else     info_str = g_strdup_printf(_("In relation #%ld"), relation->id);
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), 
			      gtk_label_new(info_str));
  g_free(info_str);

  char *name = osm_tag_get_by_key(relation->tag, "name");
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

  /* create new member */
  *memberP = g_new0(member_t, 1);
  (*memberP)->type = item->type;
  (*memberP)->role = role;
  switch(item->type) {
  case NODE:
    (*memberP)->node = item->node;
    break;
  case WAY:
    (*memberP)->way = item->way;
    break;
  case RELATION:
    (*memberP)->relation = item->relation;
    break;
  default:
    g_assert((item->type == NODE)||(item->type == WAY)||
	     (item->type == RELATION));
    break;
  }

  relation->flags |= OSM_FLAG_DIRTY;
  return TRUE;
}

static void relation_remove_item(relation_t *relation, relation_item_t *item) {

  printf("remove item of type %d from relation #%ld\n", 
	 item->type, relation->id);

  member_t **member = &relation->member;
  while(*member) {
    if(((*member)->type == item->type) &&
       (((item->type == NODE) && (item->node == (*member)->node)) ||
	((item->type == WAY) && (item->way == (*member)->way)) ||
      ((item->type == RELATION) && (item->relation == (*member)->relation)))) {
      
      member_t *next = (*member)->next;
      osm_member_free(*member);
      *member = next;

      relation->flags |= OSM_FLAG_DIRTY;

      return;
    } else
      member = &(*member)->next;
  }
  g_assert(0);
}

static void relation_list_selected(relitem_context_t *context, 
				   gboolean selected) {

  if(context->but_remove)
    gtk_widget_set_sensitive(context->but_remove, FALSE);
  if(context->but_edit)
    gtk_widget_set_sensitive(context->but_edit, selected);
}

static gboolean
relation_list_selection_func(GtkTreeSelection *selection, GtkTreeModel *model,
		     GtkTreePath *path, gboolean path_currently_selected,
		     gpointer userdata) {
  relitem_context_t *context = (relitem_context_t*)userdata;
  GtkTreeIter iter;
    
  if(gtk_tree_model_get_iter(model, &iter, path)) {
    g_assert(gtk_tree_path_get_depth(path) == 1);
    relation_list_selected(context, TRUE);
  }
  
  return TRUE; /* allow selection state to change */
}

static void on_relation_add(GtkWidget *but, relitem_context_t *context) {
  /* create a new relation */

  relation_t *relation = osm_relation_new();
  if(!info_dialog(context->dialog, context->appdata, relation)) {
    printf("tag edit cancelled\n");
    osm_relation_free(relation);
  } else {
    osm_relation_attach(context->appdata->osm, relation);

    /* add to list */

    /* append a row for the new data */
    /* try to find something descriptive */
    char *name = osm_tag_get_by_key(relation->tag, "name");
    if(!name) name = osm_tag_get_by_key(relation->tag, "ref");

    GtkTreeIter iter;
    gtk_list_store_append(context->store, &iter);
    gtk_list_store_set(context->store, &iter,
		       RELITEM_COL_SELECTED, FALSE,
		       RELITEM_COL_TYPE, 
		       osm_tag_get_by_key(relation->tag, "type"),
		       RELITEM_COL_NAME, name,
		       RELITEM_COL_DATA, relation,
		       -1);

    gtk_tree_selection_select_iter(gtk_tree_view_get_selection(
	       GTK_TREE_VIEW(context->view)), &iter);

    /* scroll to end */
    //    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment();
    /* xyz */
  }
}

static relation_t *get_selection(relitem_context_t *context) {
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(context->view));
  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    relation_t *relation;
    gtk_tree_model_get(model, &iter, RELITEM_COL_DATA, &relation, -1);
    return(relation);
  }
  return NULL;
}

static void on_relation_edit(GtkWidget *but, relitem_context_t *context) {
  relation_t *sel = get_selection(context);
  if(!sel) return;

  printf("edit relation #%ld\n", sel->id);

  info_dialog(context->dialog, context->appdata, sel);
}

static void on_relation_remove(GtkWidget *but, relitem_context_t *context) {
  relation_t *sel = get_selection(context);
  if(!sel) return;

  printf("remove relation #%ld\n", sel->id);
}

static char *relitem_get_role_in_relation(relation_item_t *item, relation_t *relation) {
  member_t *member = relation->member;
  while(member) {
    switch(member->type) {

    case NODE:
      if((item->type == NODE) && (item->node == member->node))
	return member->role;
      break;

    case WAY:
      if((item->type == WAY) && (item->way == member->way))
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

static gboolean relitem_is_in_relation(relation_item_t *item, relation_t *relation) {
  member_t *member = relation->member;
  while(member) {
    switch(member->type) {

    case NODE:
      if((item->type == NODE) && (item->node == member->node))
	return TRUE;
      break;

    case WAY:
      if((item->type == WAY) && (item->way == member->way))
	return TRUE;
      break;

    default:
      break;
    }
    member = member->next;
  }
  return FALSE;
}

static GtkWidget *relation_list(relitem_context_t *context) {
  GtkWidget *vbox = gtk_vbox_new(FALSE,3);
  context->view = gtk_tree_view_new();

  gtk_tree_selection_set_select_function(
	 gtk_tree_view_get_selection(GTK_TREE_VIEW(context->view)), 
	 relation_list_selection_func, 
	 context, NULL);


  /* --- "selected" column --- */
  GtkCellRenderer *renderer = gtk_cell_renderer_toggle_new();
  g_signal_connect(renderer, "toggled", G_CALLBACK(relitem_toggled), context);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(context->view),
	-1, _(""), renderer, 
        "active", RELITEM_COL_SELECTED, 
	NULL);

  /* --- "Type" column --- */
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(context->view),
	-1, _("Type"), renderer, "text", RELITEM_COL_TYPE, NULL);

  /* --- "Role" column --- */
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(context->view),
	-1, _("Role"), renderer, "text", RELITEM_COL_ROLE, NULL);

  /* --- "Name" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  GtkTreeViewColumn *column = 
    gtk_tree_view_column_new_with_attributes(_("Name"), renderer, 
		 "text", RELITEM_COL_NAME, NULL);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(context->view), column, -1);


  /* build and fill the store */
  context->store = gtk_list_store_new(RELITEM_NUM_COLS, 
		G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, 
	        G_TYPE_STRING, G_TYPE_POINTER);

  gtk_tree_view_set_model(GTK_TREE_VIEW(context->view), 
			  GTK_TREE_MODEL(context->store));

  GtkTreeIter iter;
  relation_t *relation = context->appdata->osm->relation;
  while(relation) {
    /* try to find something descriptive */
    char *name = osm_tag_get_by_key(relation->tag, "name");
    if(!name) name = osm_tag_get_by_key(relation->tag, "ref");

    /* Append a row and fill in some data */
    gtk_list_store_append(context->store, &iter);
    gtk_list_store_set(context->store, &iter,
       RELITEM_COL_SELECTED, relitem_is_in_relation(context->item, relation),
       RELITEM_COL_TYPE, osm_tag_get_by_key(relation->tag, "type"),
       RELITEM_COL_ROLE, relitem_get_role_in_relation(context->item, relation),
       RELITEM_COL_NAME, name,
       RELITEM_COL_DATA, relation,
       -1);
    
    relation = relation->next;
  }
  
  g_object_unref(context->store);

  /* put it into a scrolled window */
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), 
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), 
				      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scrolled_window), context->view);

  gtk_box_pack_start_defaults(GTK_BOX(vbox), scrolled_window);

  /* ------- button box ------------ */

  GtkWidget *hbox = gtk_hbox_new(TRUE,3);

  context->but_add = gtk_button_new_with_label(_("Add..."));
  //  gtk_widget_set_sensitive(context->but_add, FALSE);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), context->but_add);
  gtk_signal_connect(GTK_OBJECT(context->but_add), "clicked",
    		     GTK_SIGNAL_FUNC(on_relation_add), context);

  context->but_edit = gtk_button_new_with_label(_("Edit..."));
  gtk_box_pack_start_defaults(GTK_BOX(hbox), context->but_edit);
  gtk_signal_connect(GTK_OBJECT(context->but_edit), "clicked",
    		     GTK_SIGNAL_FUNC(on_relation_edit), context);

  context->but_remove = gtk_button_new_with_label(_("Remove"));
  gtk_box_pack_start_defaults(GTK_BOX(hbox), context->but_remove);
  gtk_signal_connect(GTK_OBJECT(context->but_remove), "clicked",
  		     GTK_SIGNAL_FUNC(on_relation_remove), context);

  relation_list_selected(context, FALSE);

  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  return vbox;
}

void relation_add_dialog(appdata_t *appdata, relation_item_t *relitem) {
  relitem_context_t *context = g_new0(relitem_context_t, 1);
  map_t *map = appdata->map;
  g_assert(map);

  context->appdata = appdata;
  context->item = relitem;

  char *str = NULL;
  switch(relitem->type) {
  case NODE:
    str = g_strdup_printf(_("Relations for node #%ld"), relitem->node->id);
    break;
  case WAY:
    str = g_strdup_printf(_("Relations for way #%ld"), relitem->way->id);
    break;
  default:
    g_assert((relitem->type == NODE) || (relitem->type == WAY));
    break;
  }
  
  context->dialog = gtk_dialog_new_with_buttons(str,
	GTK_WINDOW(appdata->window), GTK_DIALOG_MODAL,
	GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, 
	NULL);
  g_free(str);
  
  gtk_dialog_set_default_response(GTK_DIALOG(context->dialog), 
				  GTK_RESPONSE_ACCEPT);

  /* making the dialog a little wider makes it less "crowded" */
#ifdef USE_HILDON
  gtk_window_set_default_size(GTK_WINDOW(context->dialog), 500, 300);
#else
  gtk_window_set_default_size(GTK_WINDOW(context->dialog), 400, 200);
#endif
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context->dialog)->vbox),
  		     relation_list(context), TRUE, TRUE, 0);

  /* ----------------------------------- */

  gtk_widget_show_all(context->dialog);
  gtk_dialog_run(GTK_DIALOG(context->dialog));
  gtk_widget_destroy(context->dialog);

  g_free(context);
}
