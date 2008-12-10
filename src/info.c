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

enum {
  TAG_COL_KEY = 0,
  TAG_COL_VALUE,
  TAG_COL_COLLISION,
  TAG_COL_DATA,
  TAG_NUM_COLS
};

gboolean info_tag_key_collision(tag_t *tags, tag_t *tag) {
  while(tags) {
    if((tags != tag) && (strcasecmp(tags->key, tag->key) == 0))
      return TRUE;

    tags = tags->next;
  }
  return FALSE;
}

static gboolean
view_selection_func(GtkTreeSelection *selection, GtkTreeModel *model,
		     GtkTreePath *path, gboolean path_currently_selected,
		     gpointer userdata) {
  tag_context_t *context = (tag_context_t*)userdata;
  GtkTreeIter iter;
    
  if(gtk_tree_model_get_iter(model, &iter, path)) {
    g_assert(gtk_tree_path_get_depth(path) == 1);

    tag_t *tag;
    gtk_tree_model_get(model, &iter, TAG_COL_DATA, &tag, -1);

    if(context->but_remove && context->but_edit) {
      
      /* you just cannot delete or edit the "created_by" tag */
      if(context->but_remove && context->but_edit) {
	if(strcasecmp(tag->key, "created_by") == 0) {
	  gtk_widget_set_sensitive(context->but_remove, FALSE);
	  gtk_widget_set_sensitive(context->but_edit, FALSE);
	} else {
	  gtk_widget_set_sensitive(context->but_remove, TRUE);
	  gtk_widget_set_sensitive(context->but_edit, TRUE);
	}
      }
    }
  }
  
  return TRUE; /* allow selection state to change */
}

static void update_collisions(GtkListStore *store, tag_t *tags) {
  GtkTreeIter iter;
  tag_t *tag = NULL;
      
  /* walk the entire store to get all values */
  if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, TAG_COL_DATA, &tag, -1);
    g_assert(tag);      
    gtk_list_store_set(store, &iter,
       TAG_COL_COLLISION, info_tag_key_collision(tags, tag), -1);

    while(gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter)) {
      gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 
			   TAG_COL_DATA, &tag, -1);
      g_assert(tag);      
      gtk_list_store_set(store, &iter,
	 TAG_COL_COLLISION, info_tag_key_collision(tags, tag), -1);
    }
  }
}

static void on_tag_remove(GtkWidget *but, tag_context_t *context) {
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(context->view));
  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    tag_t *tag;
    gtk_tree_model_get(model, &iter, TAG_COL_DATA, &tag, -1);

    g_assert(tag);

    /* de-chain */
    printf("de-chaining tag %s/%s\n", tag->key, tag->value);
    tag_t **prev = context->tag;
    while(*prev != tag) prev = &((*prev)->next);
    *prev = tag->next;

    /* free tag itself */
    osm_tag_free(tag);

    /* and remove from store */
    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

    update_collisions(context->store, *context->tag);
  }
  
  /* disable remove and edit buttons */
  gtk_widget_set_sensitive(context->but_remove,  FALSE);
  gtk_widget_set_sensitive(context->but_edit,  FALSE);
}

static gboolean tag_edit(tag_context_t *context) {

  GtkTreeModel *model;
  GtkTreeIter iter;
  tag_t *tag;

  GtkTreeSelection *sel = gtk_tree_view_get_selection(
		      GTK_TREE_VIEW(context->view));
  if(!sel) {
    printf("got no selection object\n");
    return FALSE;
  }

  if(!gtk_tree_selection_get_selected(sel, &model, &iter)) {
    printf("nothing selected\n");
    return FALSE;
  }
  
  gtk_tree_model_get(model, &iter, TAG_COL_DATA, &tag, -1);
  printf("got %s/%s\n", tag->key, tag->value);

  GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Edit Tag"),
	  GTK_WINDOW(context->dialog), GTK_DIALOG_MODAL,
	  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
          NULL);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog), 
				  GTK_RESPONSE_ACCEPT);

  GtkWidget *label, *key, *value;
  GtkWidget *table = gtk_table_new(2, 2, FALSE);

  gtk_table_attach_defaults(GTK_TABLE(table),
		    label = gtk_label_new(_("Key:")), 0, 1, 0, 1);
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), 
		    key = gtk_entry_new(), 1, 2, 0, 1);
  gtk_entry_set_activates_default(GTK_ENTRY(key), TRUE);
  HILDON_ENTRY_NO_AUTOCAP(key);

  gtk_table_attach_defaults(GTK_TABLE(table),
		    label = gtk_label_new(_("Value:")), 0, 1, 1, 2);
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), 
		    value = gtk_entry_new(), 1, 2, 1, 2);
  gtk_entry_set_activates_default(GTK_ENTRY(value), TRUE);
  HILDON_ENTRY_NO_AUTOCAP(value);

  gtk_entry_set_text(GTK_ENTRY(key), tag->key);
  gtk_entry_set_text(GTK_ENTRY(value), tag->value);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), table);

  gtk_widget_show_all(dialog);

  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog))) {
    free(tag->key); free(tag->value);
    tag->key = strdup((char*)gtk_entry_get_text(GTK_ENTRY(key)));
    tag->value = strdup((char*)gtk_entry_get_text(GTK_ENTRY(value)));
    printf("setting %s/%s\n", tag->key, tag->value);

    gtk_list_store_set(context->store, &iter,
		       TAG_COL_KEY, tag->key,
		       TAG_COL_VALUE, tag->value,
		       -1);

    gtk_widget_destroy(dialog);

    /* update collisions for all entries */
    update_collisions(context->store, *context->tag);
    return TRUE;
  }

  gtk_widget_destroy(dialog);
  return FALSE;
}

static void on_tag_edit(GtkWidget *button, tag_context_t *context) {
  tag_edit(context);
}

static void on_tag_last(GtkWidget *button, tag_context_t *context) {
  static const char *type_name[] = { "illegal", "node", "way", "relation" };

  if(yes_no_f(context->dialog, 
	      context->appdata, MISC_AGAIN_ID_OVERWRITE_TAGS, 0,
	      _("Overwrite tags?"),
	      _("This will overwrite all tags of this %s with the "
		"ones from the %s selected last.\n\n"
		"Do you really want this?"),
	      type_name[context->type], type_name[context->type])) {

    osm_tags_free(*context->tag);

    if(context->type == NODE)
      *context->tag = osm_tags_copy(context->appdata->map->last_node_tags, TRUE);
    else
      *context->tag = osm_tags_copy(context->appdata->map->last_way_tags, TRUE);

    info_tags_replace(context);
  }
}

static void on_tag_add(GtkWidget *button, tag_context_t *context) {
  /* search end of tag chain */
  tag_t **tag = context->tag;
  while(*tag) 
    tag = &(*tag)->next;

  /* create and append a new tag */
  *tag = g_new0(tag_t, 1);
  if(!*tag) {
    errorf(GTK_WIDGET(context->appdata->window), _("Out of memory"));
    return;
  }

  /* fill with some empty strings */
  (*tag)->key = strdup("");
  (*tag)->value = strdup("");

  /* append a row for the new data */
  GtkTreeIter iter;
  gtk_list_store_append(context->store, &iter);
  gtk_list_store_set(context->store, &iter,
		     TAG_COL_KEY, (*tag)->key,
		     TAG_COL_VALUE, (*tag)->value,
		     TAG_COL_COLLISION, FALSE,
		     TAG_COL_DATA, *tag,
		     -1);

  gtk_tree_selection_select_iter(gtk_tree_view_get_selection(
		     GTK_TREE_VIEW(context->view)), &iter);

  if(!tag_edit(context)) {
    printf("cancelled\n");
    on_tag_remove(NULL, context);
  }
}

void info_tags_replace(tag_context_t *context) {
  gtk_list_store_clear(context->store);

  GtkTreeIter iter;
  tag_t *tag = *context->tag;
  while(tag) {
    gtk_list_store_append(context->store, &iter);
    gtk_list_store_set(context->store, &iter,
	       TAG_COL_KEY, tag->key,
	       TAG_COL_VALUE, tag->value,
	       TAG_COL_COLLISION, info_tag_key_collision(*context->tag, tag),
	       TAG_COL_DATA, tag,
	       -1);
    tag = tag->next;
  }
}

static GtkWidget *tag_widget(tag_context_t *context) {
  GtkWidget *vbox = gtk_vbox_new(FALSE,3);
  context->view = gtk_tree_view_new();

  gtk_tree_selection_set_select_function(
	 gtk_tree_view_get_selection(GTK_TREE_VIEW(context->view)), 
	 view_selection_func, 
	 context, NULL);

  /* --- "Key" column --- */
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, 
	       "ellipsize", PANGO_ELLIPSIZE_END, 
	       "background", "red", 
	       NULL );
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
		 _("Key"), renderer, 
		 "text", TAG_COL_KEY, 
		 "background-set", TAG_COL_COLLISION,
		 NULL);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(context->view), column, -1);

  /* --- "Value" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, 
	       "ellipsize", PANGO_ELLIPSIZE_END, 
	       NULL );
  column = gtk_tree_view_column_new_with_attributes(
		 _("Value"), renderer, 
		 "text", TAG_COL_VALUE, 
		 NULL);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(context->view), column, -1);

  /* build and fill the store */
  context->store = gtk_list_store_new(TAG_NUM_COLS, 
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER);

  gtk_tree_view_set_model(GTK_TREE_VIEW(context->view), 
			  GTK_TREE_MODEL(context->store));

  GtkTreeIter iter;
  tag_t *tag = *context->tag;
  while(tag) {
    /* Append a row and fill in some data */
    gtk_list_store_append(context->store, &iter);
    gtk_list_store_set(context->store, &iter,
	       TAG_COL_KEY, tag->key,
	       TAG_COL_VALUE, tag->value,
	       TAG_COL_COLLISION, info_tag_key_collision(*context->tag, tag),
	       TAG_COL_DATA, tag,
	       -1);
    tag = tag->next;
  }
  
  g_object_unref(context->store);

  /* put it into a scrolled window */
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), 
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), 
				      GTK_SHADOW_ETCHED_IN);
  //  gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 3);
  gtk_container_add(GTK_CONTAINER(scrolled_window), context->view);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), scrolled_window);

  /* ------- button box ------------ */

  GtkWidget *hbox = gtk_hbox_new(TRUE,3);

  GtkWidget *but_last = gtk_button_new_with_label(_("Last..."));
  gtk_box_pack_start_defaults(GTK_BOX(hbox), but_last);

  /* disable if no appropriate "last" tags have been stored or if the */
  /* selected item isn't a node or way */
  if(((context->type == NODE) && 
      (!context->appdata->map->last_node_tags)) ||
     ((context->type == WAY) && 
      (!context->appdata->map->last_way_tags)) ||
     ((context->type != NODE) && (context->type != WAY)))
    gtk_widget_set_sensitive(but_last, FALSE);

  gtk_signal_connect(GTK_OBJECT(but_last), "clicked",
  		     GTK_SIGNAL_FUNC(on_tag_last), context);

  GtkWidget *presets = josm_presets_select(context->appdata, context);
  if(presets) gtk_box_pack_start_defaults(GTK_BOX(hbox), presets);

  context->but_add = gtk_button_new_with_label(_("Add..."));
  gtk_box_pack_start_defaults(GTK_BOX(hbox), context->but_add);
  gtk_signal_connect(GTK_OBJECT(context->but_add), "clicked",
  		     GTK_SIGNAL_FUNC(on_tag_add), context);

  context->but_edit = gtk_button_new_with_label(_("Edit..."));
  gtk_widget_set_sensitive(context->but_edit, FALSE);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), context->but_edit);
  gtk_signal_connect(GTK_OBJECT(context->but_edit), "clicked",
  		     GTK_SIGNAL_FUNC(on_tag_edit), context);

  context->but_remove = gtk_button_new_with_label(_("Remove"));
  gtk_widget_set_sensitive(context->but_remove, FALSE);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), context->but_remove);
  gtk_signal_connect(GTK_OBJECT(context->but_remove), "clicked",
		     GTK_SIGNAL_FUNC(on_tag_remove), context);


  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  return vbox;
}

/* edit tags of currently selected node or way or of the relation */
/* given */
void info_dialog(GtkWidget *parent, appdata_t *appdata, relation_t *relation) {
  if(!relation)
    g_assert(appdata->map->selected.type != MAP_TYPE_ILLEGAL);

  tag_context_t *context = g_new0(tag_context_t, 1);
  user_t *user = NULL;
  char *str = NULL;
  time_t stime = 0;
  tag_t *work_copy = NULL;

  context->appdata = appdata;
  context->tag = &work_copy;

  if(!relation) {
    switch(appdata->map->selected.type) {
    case MAP_TYPE_NODE:
      str = g_strdup_printf(_("Node #%ld"), appdata->map->selected.node->id);
      user = appdata->map->selected.node->user;
      work_copy = osm_tags_copy(appdata->map->selected.node->tag, FALSE);
      stime = appdata->map->selected.node->time;
      context->type = NODE;
      break;
    case MAP_TYPE_WAY:
      str = g_strdup_printf(_("Way #%ld"), appdata->map->selected.way->id);
      user = appdata->map->selected.way->user;
      work_copy = osm_tags_copy(appdata->map->selected.way->tag, FALSE);
      stime = appdata->map->selected.way->time;
      context->type = WAY;
      break;
    default:
      g_assert((appdata->map->selected.type == MAP_TYPE_NODE) ||
	       (appdata->map->selected.type == MAP_TYPE_WAY));
      break;
    }
  } else {
    str = g_strdup_printf(_("Relation #%ld"), relation->id);
    user = relation->user;
    work_copy = osm_tags_copy(relation->tag, FALSE);
    stime = relation->time;
    context->type = RELATION;
  }

  context->dialog = gtk_dialog_new_with_buttons(str,
	  GTK_WINDOW(parent), GTK_DIALOG_MODAL,
	  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
	  GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, 
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

  GtkWidget *label;
  GtkWidget *table = gtk_table_new(2, 2, FALSE);  // x, y

  /* ------------ user ----------------- */
  char *u_str = NULL;
  if(user) u_str = g_strdup_printf(_("User: %s"), user->name);
  else     u_str = g_strdup_printf(_("User: ---"));
  label = gtk_label_new(u_str);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 0, 1);
  g_free(u_str);

  /* ------------ time ----------------- */

  struct tm *loctime = localtime(&stime);
  char time_str[32];
  strftime(time_str, sizeof(time_str), "%x %X", loctime);
  char *t_str = g_strdup_printf(_("Time: %s"), time_str);
  label = gtk_label_new(t_str);
  g_free(t_str);
  gtk_table_attach_defaults(GTK_TABLE(table),  label, 1, 2, 0, 1);

  /* ------------ coordinate (only for nodes) ----------------- */
  if(!relation) {
    if(appdata->map->selected.type == MAP_TYPE_NODE) {
      char pos_str[32];
      pos_lat_str(pos_str, sizeof(pos_str),appdata->map->selected.node->pos.lat);
      label = gtk_label_new(pos_str);
      gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 1, 2);
      pos_lat_str(pos_str, sizeof(pos_str),appdata->map->selected.node->pos.lon);
      label = gtk_label_new(pos_str);
      gtk_table_attach_defaults(GTK_TABLE(table),  label, 1, 2, 1, 2);
    } else {
      char *nodes_str = g_strdup_printf(_("Length: %u nodes"), 
		osm_way_number_of_nodes(appdata->map->selected.way));
      label = gtk_label_new(nodes_str);
      gtk_table_attach_defaults(GTK_TABLE(table),  label, 0, 1, 1, 2);
      g_free(nodes_str);

      char *type_str = g_strdup_printf("%s (%s)",
	 (osm_way_get_last_node(appdata->map->selected.way) == 
	  osm_way_get_first_node(appdata->map->selected.way))?
				       "closed way":"open way",
	 (appdata->map->selected.way->draw.flags & OSM_DRAW_FLAG_AREA)?
				       "area":"line");
 
      label = gtk_label_new(type_str);      
      gtk_table_attach_defaults(GTK_TABLE(table),  label, 1, 2, 1, 2);
      g_free(type_str);
    }
  } else {
    /* relations tell something about their members */
    gint nodes = 0, ways = 0, relations = 0;
    member_t *member = relation->member;
    while(member) {
      switch(member->type) {
      case NODE:
      case NODE_ID:
	nodes++;
	break;
      case WAY:
      case WAY_ID:
	ways++;
	break;
      case RELATION:
      case RELATION_ID:
	relations++;
	break;

      default:
	break;
      }

      member = member->next;
    }

    char *str = g_strdup_printf(_("Members: %d nodes, %d ways, %d relations"),
				nodes, ways, relations);

    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(str), 0, 2, 1, 2);
    g_free(str);
  }

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context->dialog)->vbox), table, 
		     FALSE, FALSE, 0);


  /* ------------ tags ----------------- */

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context->dialog)->vbox),
		     tag_widget(context), TRUE, TRUE, 0);

  /* ----------------------------------- */

  gtk_widget_show_all(context->dialog);
  if(gtk_dialog_run(GTK_DIALOG(context->dialog)) == GTK_RESPONSE_ACCEPT) {
    gtk_widget_destroy(context->dialog);

    if(!relation) {
      /* replace original tags with work copy */
      switch(appdata->map->selected.type) {

      case MAP_TYPE_NODE:
	osm_tags_free(appdata->map->selected.node->tag);
	appdata->map->selected.node->tag = osm_tags_copy(work_copy, TRUE);
	break;
	
      case MAP_TYPE_WAY:
	osm_tags_free(appdata->map->selected.way->tag);
	appdata->map->selected.way->tag = osm_tags_copy(work_copy, TRUE);
	break;
	
      default:
	break;
      }

      /* since nodes being parts of ways but with no tags are invisible, */
      /* the result of editing them may have changed their visibility */
      map_item_redraw(appdata, &appdata->map->selected);
      map_item_set_flags(&context->appdata->map->selected, OSM_FLAG_DIRTY, 0);
    } else {
      osm_tags_free(relation->tag);
      relation->tag = osm_tags_copy(work_copy, TRUE);
      relation->flags |= OSM_FLAG_DIRTY;
    }
  } else {
    gtk_widget_destroy(context->dialog);
    osm_tags_free(work_copy);
  }

  g_free(context);
}
