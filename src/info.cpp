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

#include "info.h"

#include "appdata.h"
#include "josm_presets.h"
#include "list.h"
#include "map.h"
#include "misc.h"
#include "relation_edit.h"

#include <algorithm>
#include <strings.h>

#include <osm2go_cpp.h>

enum {
  TAG_COL_KEY = 0,
  TAG_COL_VALUE,
  TAG_COL_COLLISION,
  TAG_COL_DATA,
  TAG_NUM_COLS
};

struct stag_collision_functor {
  const stag_t *tag;
  stag_collision_functor(const stag_t *t) : tag(t) { }
  bool operator()(const stag_t *t) {
    return (strcasecmp(t->key.c_str(), tag->key.c_str()) == 0);
  }
};

static gboolean info_tag_key_collision(const std::vector<stag_t *> &tags, const stag_t *tag) {
  const std::vector<stag_t *>::const_iterator itEnd = tags.end();
  stag_collision_functor fc(tag);
  const std::vector<stag_t *>::const_iterator it = std::find_if(tags.begin(), itEnd, fc);
  if(it == itEnd)
    return FALSE;
  // check if this is the original tag itself
  if(*it != tag)
    return TRUE;
  // it is, so search in the remaining items again
  return std::find_if(it + 1, itEnd, fc) != itEnd ? TRUE : FALSE;
}

static void changed(G_GNUC_UNUSED GtkTreeSelection *treeselection, gpointer user_data) {
  GtkWidget *list = (GtkWidget*)user_data;

  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean selected = list_get_selected(list, &model, &iter);

  if(selected) {
    stag_t *tag;
    gtk_tree_model_get(model, &iter, TAG_COL_DATA, &tag, -1);

    /* you just cannot delete or edit the "created_by" tag */
    if(!tag || tag->is_creator_tag())
      selected = FALSE;
  }

  list_button_enable(GTK_WIDGET(list), LIST_BUTTON_REMOVE, selected);
  list_button_enable(GTK_WIDGET(list), LIST_BUTTON_EDIT, selected);
}

void tag_context_t::update_collisions()
{
  GtkTreeIter iter;

  /* walk the entire store to get all values */
  if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
    do {
      stag_t *tag = O2G_NULLPTR;
      gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, TAG_COL_DATA, &tag, -1);
      g_assert_nonnull(tag);
      gtk_list_store_set(store, &iter,
         TAG_COL_COLLISION, info_tag_key_collision(tags, tag), -1);

    } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
  }
}

static void on_tag_remove(G_GNUC_UNUSED GtkWidget *button, tag_context_t *context) {
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  GtkTreeSelection *selection = list_get_selection(context->list);
  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    stag_t *tag = O2G_NULLPTR;
    gtk_tree_model_get(model, &iter, TAG_COL_DATA, &tag, -1);

    g_assert_nonnull(tag);

    /* de-chain */
    printf("de-chaining tag %s/%s\n", tag->key.c_str(), tag->value.c_str());
    context->tags.erase(std::find(context->tags.begin(), context->tags.end(), tag));

    /* free tag itself */
    delete tag;

    /* and remove from store */
    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

    context->update_collisions();
  }
}

/**
 * @brief request user input for the given tag
 * @param window the parent window
 * @param tag the tag to change
 * @return if the tag was actually modified
 * @retval false the tag is the same as before
 */
static bool tag_edit(GtkWindow *window, stag_t &tag) {
  GtkWidget *dialog = misc_dialog_new(MISC_DIALOG_SMALL, _("Edit Tag"),
			  window,
			  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			  GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
			  O2G_NULLPTR);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog),
				  GTK_RESPONSE_ACCEPT);

  GtkWidget *label, *key, *value;
  GtkWidget *table = gtk_table_new(2, 2, FALSE);

  gtk_table_attach(GTK_TABLE(table), label = gtk_label_new(_("Key:")),
                   0, 1, 0, 1,
                   static_cast<GtkAttachOptions>(0),
                   static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),
			    key = entry_new(), 1, 2, 0, 1);
  gtk_entry_set_activates_default(GTK_ENTRY(key), TRUE);
  HILDON_ENTRY_NO_AUTOCAP(key);

  gtk_table_attach(GTK_TABLE(table),  label = gtk_label_new(_("Value:")),
                   0, 1, 1, 2,
                   static_cast<GtkAttachOptions>(0),
                   static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table),
		    value = entry_new(), 1, 2, 1, 2);
  gtk_entry_set_activates_default(GTK_ENTRY(value), TRUE);
  HILDON_ENTRY_NO_AUTOCAP(value);

  gtk_entry_set_text(GTK_ENTRY(key), tag.key.c_str());
  gtk_entry_set_text(GTK_ENTRY(value), tag.value.c_str());

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), table);

  gtk_widget_show_all(dialog);

  bool ret = false;
  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog))) {
    const gchar *nk = gtk_entry_get_text(GTK_ENTRY(key));
    const gchar *nv = gtk_entry_get_text(GTK_ENTRY(value));
    ret = tag.key != nk || tag.value != nv;
    if(ret) {
      tag.key = nk;
      tag.value = nv;
    }
  }

  gtk_widget_destroy(dialog);
  return ret;
}

static void on_tag_edit(G_GNUC_UNUSED GtkWidget *button, tag_context_t *context) {
  GtkTreeModel *model;
  GtkTreeIter iter;

  GtkTreeSelection *sel = list_get_selection(context->list);
  if(!sel) {
    printf("got no selection object\n");
    return;
  }

  if(!gtk_tree_selection_get_selected(sel, &model, &iter)) {
    printf("nothing selected\n");
    return;
  }

  stag_t *tag;
  gtk_tree_model_get(model, &iter, TAG_COL_DATA, &tag, -1);
  printf("got %s/%s\n", tag->key.c_str(), tag->value.c_str());

  if(tag_edit(GTK_WINDOW(context->dialog), *tag)) {
    printf("setting %s/%s\n", tag->key.c_str(), tag->value.c_str());

    gtk_list_store_set(context->store, &iter,
		       TAG_COL_KEY, tag->key.c_str(),
		       TAG_COL_VALUE, tag->value.c_str(),
		       -1);

    /* update collisions for all entries */
    context->update_collisions();
  }
}

static inline void stag_delete(stag_t *t) {
  delete t;
}

static void on_tag_last(G_GNUC_UNUSED GtkWidget *button, tag_context_t *context) {
  if(context->tags.empty() || yes_no_f(context->dialog,
	      context->appdata, MISC_AGAIN_ID_OVERWRITE_TAGS, 0,
	      _("Overwrite tags?"),
	      _("This will overwrite all tags of this %s with the "
		"ones from the %s selected last.\n\n"
		"Do you really want this?"),
                                       context->object.type_string(),
                                       context->object.type_string())) {

    std::for_each(context->tags.begin(), context->tags.end(), stag_delete);

    if(context->object.type == NODE)
      context->tags = osm_tags_list_copy(context->appdata->map->last_node_tags);
    else
      context->tags = osm_tags_list_copy(context->appdata->map->last_way_tags);

    context->info_tags_replace();

    // Adding those tags above will usually make the first of the newly
    // added tags selected. Enable edit/remove buttons now.
    GtkTreeSelection *sel = list_get_selection(context->list);
    changed(sel, context->list);
  }
}

static GtkTreeIter store_append(GtkListStore *store, stag_t *tag, gboolean collision) {
  GtkTreeIter iter;
  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter,
                     TAG_COL_KEY, tag->key.c_str(),
                     TAG_COL_VALUE, tag->value.c_str(),
                     TAG_COL_COLLISION, collision,
                     TAG_COL_DATA, tag,
                     -1);
  return iter;
}

struct stag_identity_functor {
  const stag_t &tag;
  stag_identity_functor(const stag_t &t) : tag(t) { }
  bool operator()(const stag_t *t) {
    return tag == t;
  }
};

static void on_tag_add(G_GNUC_UNUSED GtkWidget *button, tag_context_t *context) {
  stag_t tag = stag_t(std::string(), std::string());

  if(!tag_edit(GTK_WINDOW(context->dialog), tag)) {
    printf("cancelled\n");
    return;
  }

  std::vector<stag_t *> &tags = context->tags;
  const std::vector<stag_t *>::const_iterator itEnd = tags.end();
  std::vector<stag_t *>::const_iterator it = std::find_if(cbegin(tags), itEnd,
                                                          stag_identity_functor(tag));
  if(G_UNLIKELY(it != itEnd)) {
    // the very same tag is already in the list, just select the old one
    GtkTreeIter iter;
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(context->store), &iter,
                                  O2G_NULLPTR, it - tags.begin());
    gtk_tree_selection_select_iter(list_get_selection(context->list), &iter);
    return;
  }

  stag_t * const ntag = new stag_t(tag);
  // check if the new key introduced a collision
  gboolean collision = info_tag_key_collision(tags, ntag);
  tags.push_back(ntag);
  /* append a row for the new data */
  GtkTreeIter iter = store_append(context->store, ntag, collision);

  gtk_tree_selection_select_iter(list_get_selection(context->list), &iter);

  if(collision)
    context->update_collisions();
}

struct tag_replace_functor {
  GtkListStore * const store;
  const std::vector<stag_t *> &tags;
  tag_replace_functor(GtkListStore *s, const std::vector<stag_t *> &t) : store(s), tags(t) {}
  void operator()(stag_t *tag) {
    store_append(store, tag, info_tag_key_collision(tags, tag));
  }
};

void tag_context_t::info_tags_replace() {
  gtk_list_store_clear(store);

  std::for_each(tags.begin(), tags.end(), tag_replace_functor(store, tags));
}

static void on_relations(G_GNUC_UNUSED GtkWidget *button, tag_context_t *context) {
  relation_membership_dialog(context->dialog, context->appdata,
                             context->object);
}

static GtkWidget *tag_widget(tag_context_t *context) {
  context->list = list_new(LIST_HILDON_WITHOUT_HEADERS);

  list_set_static_buttons(context->list, 0, G_CALLBACK(on_tag_add),
	  G_CALLBACK(on_tag_edit), G_CALLBACK(on_tag_remove), context);

  list_override_changed_event(context->list, changed, context->list);

  list_set_user_buttons(context->list,
			LIST_BUTTON_USER0, _("Last"),      on_tag_last,
			LIST_BUTTON_USER2, _("Relations"), on_relations,
			0);

  /* "relations of a relation" is something we can't handle correctly */
  /* at the moment */
  if(context->object.type == RELATION)
    list_button_enable(context->list, LIST_BUTTON_USER2, FALSE);

  /* setup both columns */
  list_set_columns(context->list,
      _("Key"),   TAG_COL_KEY,
	   LIST_FLAG_ELLIPSIZE|LIST_FLAG_CAN_HIGHLIGHT, TAG_COL_COLLISION,
      _("Value"), TAG_COL_VALUE,
	   LIST_FLAG_ELLIPSIZE,
      O2G_NULLPTR);

  GtkWidget *presets = josm_build_presets_button(context->appdata, context);
  if(presets) {
    list_set_custom_user_button(context->list, LIST_BUTTON_USER1, presets);
    if(!context->appdata->presets)
      list_button_enable(context->list, LIST_BUTTON_USER1, FALSE);
  }

  /* disable if no appropriate "last" tags have been stored or if the */
  /* selected item isn't a node or way */
  if(((context->object.type == NODE) &&
      (context->appdata->map->last_node_tags.empty())) ||
     ((context->object.type == WAY) &&
      (context->appdata->map->last_way_tags.empty())) ||
     ((context->object.type != NODE) && (context->object.type != WAY)))
	list_button_enable(context->list, LIST_BUTTON_USER0, FALSE);

  /* --------- build and fill the store ------------ */
  context->store = gtk_list_store_new(TAG_NUM_COLS,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER);

  list_set_store(context->list, context->store);

  context->info_tags_replace();

  g_object_unref(context->store);

  return context->list;
}

static void on_relation_members(G_GNUC_UNUSED GtkWidget *button, const tag_context_t *context) {
  g_assert(context->object.type == RELATION);
  relation_show_members(context->dialog, context->object.relation);
}

static void table_attach(GtkWidget *table, GtkWidget *child, int x, int y) {
  gtk_table_attach_defaults(GTK_TABLE(table), child, x, x+1, y, y+1);
}

static GtkWidget *details_widget(const tag_context_t &context, bool big) {
  GtkWidget *table = gtk_table_new(big?4:2, 2, FALSE);  // y, x

  const char *user = context.object.obj->user;
  GtkWidget *label;

  /* ------------ user ----------------- */
  if(user) {
    if(big) table_attach(table, gtk_label_new(_("User:")), 0, 0);

    label = gtk_label_new(user);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    table_attach(table, label, big?1:0, 0);
  }

  /* ------------ time ----------------- */

  if(big) table_attach(table, gtk_label_new(_("Date/Time:")), 0, 1);
  if(context.object.obj->time > 0) {
    struct tm loctime;
    localtime_r(&context.object.obj->time, &loctime);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%x %X", &loctime);
    label = gtk_label_new(time_str);
  } else {
    label = gtk_label_new(_("Not yet uploaded"));
  }
  table_attach(table, label, 1, big?1:0);

  /* ------------ coordinate (only for nodes) ----------------- */
  switch(context.object.type) {
  case NODE: {
    char pos_str[32];
    pos_lat_str(pos_str, sizeof(pos_str), context.object.node->pos.lat);
    label = gtk_label_new(pos_str);
    if(big) table_attach(table, gtk_label_new(_("Latitude:")), 0, 2);
    table_attach(table, label, big?1:0, big?2:1);
    pos_lat_str(pos_str, sizeof(pos_str), context.object.node->pos.lon);
    label = gtk_label_new(pos_str);
    if(big) table_attach(table, gtk_label_new(_("Longitude:")), 0, 3);
    table_attach(table, label, 1, big?3:1);
  } break;

  case WAY: {
    char *nodes_str = g_strdup_printf(_("%s%zu nodes"),
             big?"":_("Length: "), context.object.way->node_chain.size());
    label = gtk_label_new(nodes_str);
    if(big) table_attach(table, gtk_label_new(_("Length:")), 0, 2);
    table_attach(table, label, big?1:0, big?2:1);
    g_free(nodes_str);

    char *type_str = g_strconcat(context.object.way->is_closed() ?
			     "closed way":"open way",
			     " (",
	     (context.object.way->draw.flags & OSM_DRAW_FLAG_AREA)?
			       "area":"line",
			     ")", O2G_NULLPTR);

    label = gtk_label_new(type_str);
    if(big) table_attach(table, gtk_label_new(_("Type:")), 0, 3);
    table_attach(table, label, 1, big?3:1);
    g_free(type_str);
  } break;

  case RELATION: {
    /* relations tell something about their members */
    guint nodes = 0, ways = 0, relations = 0;
    context.object.relation->members_by_type(&nodes, &ways, &relations);

    char *str =
      g_strdup_printf(_("Members: %u nodes, %u ways, %u relations"),
		      nodes, ways, relations);

    GtkWidget *member_btn = button_new_with_label(str);
    g_signal_connect(GTK_OBJECT(member_btn), "clicked",
                     G_CALLBACK(on_relation_members),
                     const_cast<tag_context_t *>(&context));

    gtk_table_attach_defaults(GTK_TABLE(table), member_btn, 0, 2,
			      big?2:1, big?4:2);

    g_free(str);
    break;
  }

  default:
    printf("ERROR: No node, way or relation\n");
    g_assert_not_reached();
    break;
  }

  return table;
}

#ifdef FREMANTLE
/* put additional infos into a seperate dialog for fremantle as */
/* screen space is sparse there */
static void info_more(const tag_context_t &context) {
  GtkWidget *dialog =
    misc_dialog_new(MISC_DIALOG_SMALL, _("Object details"),
		    GTK_WINDOW(context.dialog),
		    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		    O2G_NULLPTR);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog),
				  GTK_RESPONSE_CANCEL);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
		     details_widget(context, true),
		     FALSE, FALSE, 0);
  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}
#endif

/* edit tags of currently selected node or way or of the relation */
/* given */
void info_dialog(GtkWidget *parent, appdata_t *appdata) {
  bool ret = info_dialog(parent, appdata, appdata->map->selected.object);

  /* since nodes being parts of ways but with no tags are invisible, */
  /* the result of editing them may have changed their visibility */
  if(ret && appdata->map->selected.object.type != RELATION)
    map_item_redraw(appdata->map, &appdata->map->selected);
}

/* edit tags of currently selected node or way or of the relation */
/* given */
bool info_dialog(GtkWidget *parent, appdata_t *appdata, object_t &object) {

  g_assert_true(object.is_real());

  /* use implicit selection if not explicitely given */
  tag_context_t context(appdata, object);
  char *str = O2G_NULLPTR;

  switch(context.object.type) {
  case NODE:
    str = g_strdup_printf(_("Node #" ITEM_ID_FORMAT),
			  context.object.obj->id);
    context.presets_type = PRESETS_TYPE_NODE;
    break;

  case WAY:
    str = g_strdup_printf(_("Way #" ITEM_ID_FORMAT),
			  context.object.obj->id);
    context.presets_type = PRESETS_TYPE_WAY;

    if(context.object.way->is_closed())
      context.presets_type |= PRESETS_TYPE_CLOSEDWAY;

    break;

  case RELATION:
    str = g_strdup_printf(_("Relation #" ITEM_ID_FORMAT),
			  context.object.obj->id);
    context.presets_type = PRESETS_TYPE_RELATION;

    if(context.object.relation->is_multipolygon())
      context.presets_type |= PRESETS_TYPE_MULTIPOLYGON;
    break;

  default:
    g_assert_not_reached();
    break;
  }

  context.dialog = misc_dialog_new(MISC_DIALOG_LARGE, str,
	  GTK_WINDOW(parent),
#ifdef FREMANTLE
	  _("More"), GTK_RESPONSE_HELP,
#endif
	  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	  GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
	  O2G_NULLPTR);
  g_free(str);

  gtk_dialog_set_default_response(GTK_DIALOG(context.dialog),
				  GTK_RESPONSE_ACCEPT);

#ifndef FREMANTLE
  /* -------- details box --------- */
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
		     details_widget(context, false),
		     FALSE, FALSE, 0);
#endif

  /* ------------ tags ----------------- */

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
		     tag_widget(&context), TRUE, TRUE, 0);

  /* ----------------------------------- */

  gtk_widget_show_all(context.dialog);
  bool ok = false, quit = false;

  do {
    switch(gtk_dialog_run(GTK_DIALOG(context.dialog))) {
    case GTK_RESPONSE_ACCEPT:
      quit = true;
      ok = true;
      break;
#ifdef FREMANTLE
    case GTK_RESPONSE_HELP:
      info_more(context);
      break;
#endif

    default:
      quit = true;
      break;
    }

  } while(!quit);

  gtk_widget_destroy(context.dialog);

  if(ok) {
    context.object.obj->tags.replace(context.tags);

    context.object.set_flags(OSM_FLAG_DIRTY);
  } else {
    std::for_each(context.tags.begin(), context.tags.end(), stag_delete);
    context.tags.clear();
  }

  return ok;
}

tag_context_t::tag_context_t(appdata_t *a, const object_t &o)
  : appdata(a)
  , dialog(O2G_NULLPTR)
  , list(O2G_NULLPTR)
  , store(O2G_NULLPTR)
  , object(o)
  , presets_type(0)
  , tags(object.obj->tags.asPointerVector())
{
}

tag_context_t::~tag_context_t()
{
  std::for_each(tags.begin(), tags.end(), stag_delete);
}
