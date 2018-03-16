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

#include "info.h"

#include "josm_presets.h"
#include "list.h"
#include "map.h"
#include "misc.h"
#include "pos.h"
#include "relation_edit.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <strings.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include "osm2go_platform_gtk.h"

using namespace osm2go_platform;

enum {
  TAG_COL_KEY = 0,
  TAG_COL_VALUE,
  TAG_COL_COLLISION,
  TAG_NUM_COLS
};

class info_tag_context_t : public tag_context_t {
public:
  explicit info_tag_context_t(map_t *m, osm_t *os, presets_items *p, const object_t &o);

  map_t * const map;
  osm_t * const osm;
  presets_items * const presets;
  GtkWidget *list;
  std::unique_ptr<GtkListStore, g_object_deleter> store;

  void update_collisions(const std::string &k);
};

static void changed(GtkTreeSelection *, gpointer user_data) {
  GtkWidget *list = static_cast<info_tag_context_t *>(user_data)->list;

  GtkTreeModel *model;
  GtkTreeIter iter;
  bool selected = list_get_selected(list, &model, &iter);

  if(selected) {
    const gchar *key = O2G_NULLPTR;
    gtk_tree_model_get(model, &iter, TAG_COL_KEY, &key, -1);

    // WARNING: for whatever reason, key CAN be NULL on N900

    /* you just cannot delete or edit the "created_by" tag */
    if(key && tag_t::is_creator_tag(key))
      selected = false;
  }

  list_button_enable(GTK_WIDGET(list), LIST_BUTTON_REMOVE, selected);
  list_button_enable(GTK_WIDGET(list), LIST_BUTTON_EDIT, selected);
}

void info_tag_context_t::update_collisions(const std::string &k)
{
  GtkTreeIter iter;
  const bool checkAll = k.empty();

  /* walk the entire store to get all values */
  if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store.get()), &iter)) {
    do {
      const gchar *key = O2G_NULLPTR;
      gtk_tree_model_get(GTK_TREE_MODEL(store.get()), &iter, TAG_COL_KEY, &key, -1);
      assert(key != O2G_NULLPTR);
      if(checkAll || k == key)
        gtk_list_store_set(store.get(), &iter,
           TAG_COL_COLLISION, (tags.count(key) > 1) ? TRUE : FALSE, -1);

    } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(store.get()), &iter));
  }
}

struct value_match_functor {
  const std::string &value;
  explicit value_match_functor(const std::string &v) : value(v) {}
  bool operator()(const osm_t::TagMap::value_type &pair) {
    return pair.second == value;
  }
};

static void on_tag_remove(info_tag_context_t *context) {
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  GtkTreeSelection *selection = list_get_selection(context->list);
  if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
    const gchar *kc = O2G_NULLPTR, *vc = O2G_NULLPTR;
    gtk_tree_model_get(model, &iter, TAG_COL_KEY, &kc, TAG_COL_VALUE, &vc, -1);

    assert(kc != O2G_NULLPTR);
    assert(vc != O2G_NULLPTR);

    /* de-chain */
    g_debug("de-chaining tag %s/%s", kc, vc);
    const std::string k = kc;
    osm_t::TagMap::iterator it = context->tags.findTag(k, vc);
    assert(it != context->tags.end());

    context->tags.erase(it);

    GtkTreeIter n = iter;
    // select the next entry if it exists
    if(gtk_tree_model_iter_next(model, &n))
      gtk_tree_selection_select_iter(selection, &n);
    /* and remove from store */
    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

    // no collision was there if this was the only instance of the key
    if(unlikely(context->tags.count(k) > 0))
      context->update_collisions(k);
  }
}

/**
 * @brief request user input for the given tag
 * @param window the parent window
 * @param k the key
 * @param v the value
 * @return if the tag was actually modified
 * @retval false the tag is the same as before
 */
static bool tag_edit(GtkWindow *window, std::string &k, std::string &v) {
  osm2go_platform::WidgetGuard dialog(gtk_dialog_new_with_buttons(_("Edit Tag"), window, GTK_DIALOG_MODAL,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              O2G_NULLPTR));

  dialog_size_hint(GTK_WINDOW(dialog.get()), MISC_DIALOG_SMALL);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog.get()),
				  GTK_RESPONSE_ACCEPT);

  GtkWidget *label = gtk_label_new(_("Key:"));
  GtkWidget *key = entry_new(EntryFlagsNoAutoCap);
  GtkWidget *table = gtk_table_new(2, 2, FALSE);

  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                   static_cast<GtkAttachOptions>(0),
                   static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), key, 1, 2, 0, 1);
  gtk_entry_set_activates_default(GTK_ENTRY(key), TRUE);

  label = gtk_label_new(_("Value:"));
  GtkWidget *value = entry_new(EntryFlagsNoAutoCap);
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
                   static_cast<GtkAttachOptions>(0),
                   static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), value, 1, 2, 1, 2);
  gtk_entry_set_activates_default(GTK_ENTRY(value), TRUE);

  gtk_entry_set_text(GTK_ENTRY(key), k.c_str());
  gtk_entry_set_text(GTK_ENTRY(value), v.c_str());

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog.get())->vbox), table);

  gtk_widget_show_all(dialog.get());

  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog.get()))) {
    const gchar *nk = gtk_entry_get_text(GTK_ENTRY(key));
    const gchar *nv = gtk_entry_get_text(GTK_ENTRY(value));
    if(k != nk || v != nv) {
      k = nk;
      v = nv;
      return true;
    }
  }

  return false;
}

static void select_item(const std::string &k, const std::string &v, info_tag_context_t *context) {
  GtkTreeIter iter;
  gtk_tree_model_get_iter_first(GTK_TREE_MODEL(context->store.get()), &iter);
  // just a linear search as there is not match between the tagmap order and the
  // store order
  do {
    const gchar *key = O2G_NULLPTR, *value = O2G_NULLPTR;
    gtk_tree_model_get(GTK_TREE_MODEL(context->store.get()), &iter,
                       TAG_COL_KEY, &key,
                       TAG_COL_VALUE, &value, -1);
    assert(key != O2G_NULLPTR);
    assert(value != O2G_NULLPTR);
    if(k == key && v == value) {
      gtk_tree_selection_select_iter(list_get_selection(context->list), &iter);
      return;
    }
  } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(context->store.get()), &iter));
}

static void on_tag_edit(info_tag_context_t *context) {
  GtkTreeModel *model;
  GtkTreeIter iter;

  GtkTreeSelection *sel = list_get_selection(context->list);
  if(!sel) {
    g_debug("got no selection object");
    return;
  }

  if(!gtk_tree_selection_get_selected(sel, &model, &iter)) {
    g_debug("nothing selected");
    return;
  }

  char *kc, *vc;
  gtk_tree_model_get(model, &iter, TAG_COL_KEY, &kc, TAG_COL_VALUE, &vc, -1);
  g_debug("got %s/%s", kc, vc);

  // keep it in string for easier string compare
  const std::string oldv = vc;
  const std::string oldk = kc;

  std::string k = oldk, v = oldv;

  if(tag_edit(GTK_WINDOW(context->dialog.get()), k, v)) {
    if(k == kc && v == oldv)
      return;

    g_debug("setting %s/%s", k.c_str(), v.c_str());

    const std::pair<osm_t::TagMap::iterator, osm_t::TagMap::iterator> matches = context->tags.equal_range(oldk);
    assert(matches.first != matches.second);
    osm_t::TagMap::iterator it = std::find_if(matches.first, matches.second, value_match_functor(oldv));
    assert(it != matches.second);

    if(it->first == k) {
      // only value was changed
      // collision flags only need to be updated if there is more than one entry with that key
      osm_t::TagMap::iterator i = matches.first;
      if(unlikely(++i != matches.second)) {
        // check if the entry is now equal to another entry
        i = std::find_if(matches.first, matches.second, value_match_functor(v));

        if(i != matches.second) {
          // the item is now a duplicate, so it can be removed
          gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
          context->tags.erase(it);

          select_item(k, v, context);
          context->update_collisions(k);
          return;
        }
        // if the collisions persist no update has to be done as there already was a collision before
      }
      it->second = v;
    } else {
      context->tags.erase(it);
      it = context->tags.findTag(k, v);
      if(unlikely(it != context->tags.end())) {
        // this tag is now duplicate, drop it and select the other one
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

        select_item(k, v, context);
        // update collision marker for the old entry
        context->update_collisions(oldk);
        return;
      } else {
        context->tags.insert(osm_t::TagMap::value_type(k, v));

        /* update collisions for all entries */
        context->update_collisions(std::string());
      }
    }

    gtk_list_store_set(context->store.get(), &iter,
                       TAG_COL_KEY, k.c_str(),
                       TAG_COL_VALUE, v.c_str(),
                       -1);
  }
}

static bool replace_with_last(const info_tag_context_t *context, const osm_t::TagMap &ntags) {
  // if the new object has no tags replacing is always permitted
  if(context->tags.empty())
    return true;

  // if all tags of the object are part of the new tag list no information will be lost
  if(osm_t::tagSubset(context->tags, ntags))
    return true;

  const char *ts = context->object.type_string();
  return yes_no_f(context->dialog.get(), MISC_AGAIN_ID_OVERWRITE_TAGS, _("Overwrite tags?"),
                  _("This will overwrite all tags of this %s with the ones from "
                    "the %s selected last.\n\nDo you really want this?"), ts, ts);
}

static void on_tag_last(info_tag_context_t *context) {
  const osm_t::TagMap &ntags = context->object.type == object_t::NODE ?
                               context->map->last_node_tags :
                               context->map->last_way_tags;

  if(!replace_with_last(context, ntags))
    return;

  context->tags = ntags;

  context->info_tags_replace();

  // Adding those tags above will usually make the first of the newly
  // added tags selected. Enable edit/remove buttons now.
  GtkTreeSelection *sel = list_get_selection(context->list);
  changed(sel, context);
}

static GtkTreeIter store_append(GtkListStore *store, const std::string &key,
                                const std::string &value, bool collision) {
  GtkTreeIter iter;
  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter,
                     TAG_COL_KEY, key.c_str(),
                     TAG_COL_VALUE, value.c_str(),
                     TAG_COL_COLLISION, collision ? TRUE : FALSE,
                     -1);
  return iter;
}

static void on_tag_add(info_tag_context_t *context) {
  std::string k, v;

  if(!tag_edit(GTK_WINDOW(context->dialog.get()), k, v)) {
    g_debug("cancelled");
    return;
  }

  osm_t::TagMap::iterator it = context->tags.findTag(k, v);
  if(unlikely(it != context->tags.end())) {
    select_item(k, v, context);
    return;
  }

  // check if the new key introduced a collision
  bool collision = context->tags.count(k) > 0;
  context->tags.insert(osm_t::TagMap::value_type(k, v));
  /* append a row for the new data */
  GtkTreeIter iter = store_append(context->store.get(), k, v, collision);

  gtk_tree_selection_select_iter(list_get_selection(context->list), &iter);

  if(collision)
    context->update_collisions(k);
}

struct tag_replace_functor {
  GtkListStore * const store;
  const osm_t::TagMap &tags;
  tag_replace_functor(GtkListStore *s, const osm_t::TagMap &t) : store(s), tags(t) {}
  void operator()(const osm_t::TagMap::value_type &pair) {
    store_append(store, pair.first, pair.second, tags.count(pair.first) > 1);
  }
};

void tag_context_t::info_tags_replace() {
  GtkListStore *store = static_cast<info_tag_context_t *>(this)->store.get();
  gtk_list_store_clear(store);

  std::for_each(tags.begin(), tags.end(), tag_replace_functor(store, tags));
}

static void on_relations(info_tag_context_t *context) {
  relation_membership_dialog(context->dialog.get(), context->presets,
                             context->osm, context->object);
}

static GtkWidget *tag_widget(info_tag_context_t &context) {
  /* setup both columns */
  std::vector<list_view_column> columns;
  columns.push_back(list_view_column(_("Key"),   LIST_FLAG_ELLIPSIZE|LIST_FLAG_CAN_HIGHLIGHT, TAG_COL_COLLISION));
  columns.push_back(list_view_column(_("Value"), LIST_FLAG_ELLIPSIZE));

  std::vector<list_button> buttons;
  buttons.push_back(list_button(_("_Add"), G_CALLBACK(on_tag_add)));
  buttons.push_back(list_button(_("_Edit"), G_CALLBACK(on_tag_edit)));
  buttons.push_back(list_button(_("Remove"), G_CALLBACK(on_tag_remove)));
  buttons.push_back(list_button(_("Last"), G_CALLBACK(on_tag_last)));
  buttons.push_back(list_button(O2G_NULLPTR, O2G_NULLPTR));
  buttons.push_back(list_button(_("Relations"), G_CALLBACK(on_relations)));

  context.store.reset(gtk_list_store_new(TAG_NUM_COLS,
                                          G_TYPE_STRING, G_TYPE_STRING,
                                          G_TYPE_BOOLEAN, G_TYPE_POINTER));

  context.list = list_new(LIST_HILDON_WITHOUT_HEADERS, LIST_BTN_2ROW, &context, changed,
                          buttons, columns, context.store.get());

  list_set_custom_user_button(context.list, LIST_BUTTON_USER1,
                              josm_build_presets_button(context.presets, &context));
  if(unlikely(context.presets == O2G_NULLPTR))
    list_button_enable(context.list, LIST_BUTTON_USER1, FALSE);

  /* disable if no appropriate "last" tags have been stored or if the */
  /* selected item isn't a node or way */
  if((context.object.type == object_t::NODE && context.map->last_node_tags.empty()) ||
     (context.object.type == object_t::WAY && context.map->last_way_tags.empty()) ||
     (context.object.type != object_t::NODE && context.object.type != object_t::WAY))
    list_button_enable(context.list, LIST_BUTTON_USER0, FALSE);

  /* --------- build and fill the store ------------ */
  context.info_tags_replace();

  return context.list;
}

static void on_relation_members(GtkWidget *, const info_tag_context_t *context) {
  assert_cmpnum(context->object.type, object_t::RELATION);
  relation_show_members(context->dialog.get(), context->object.relation);
}

static void table_attach(GtkWidget *table, GtkWidget *child, int x, int y) {
  gtk_table_attach_defaults(GTK_TABLE(table), child, x, x+1, y, y+1);
}

static GtkWidget *details_widget(const info_tag_context_t &context, bool big) {
  GtkWidget *table = gtk_table_new(big?4:2, 2, FALSE);  // y, x

  const std::map<int, std::string>::const_iterator userIt = context.osm->users.find(context.object.obj->user);
  GtkWidget *label;

  /* ------------ user ----------------- */
  if(userIt != context.osm->users.end()) {
    if(big) table_attach(table, gtk_label_new(_("User:")), 0, 0);

    label = gtk_label_new(userIt->second.c_str());
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
    g_string dv_str(g_strdup_printf(_("%s (# %u)"), time_str,
                                    static_cast<unsigned int>(context.object.obj->version)));
    label = gtk_label_new(dv_str.get());
  } else {
    label = gtk_label_new(_("Not yet uploaded"));
  }
  table_attach(table, label, 1, big?1:0);

  /* ------------ coordinate (only for nodes) ----------------- */
  switch(context.object.type) {
  case object_t::NODE: {
    char pos_str[32];
    pos_lat_str(pos_str, sizeof(pos_str), context.object.node->pos.lat);
    label = gtk_label_new(pos_str);
    if(big) table_attach(table, gtk_label_new(_("Latitude:")), 0, 2);
    table_attach(table, label, big?1:0, big?2:1);
    pos_lon_str(pos_str, sizeof(pos_str), context.object.node->pos.lon);
    label = gtk_label_new(pos_str);
    if(big) table_attach(table, gtk_label_new(_("Longitude:")), 0, 3);
    table_attach(table, label, 1, big?3:1);
  } break;

  case object_t::WAY: {
    size_t ncount = context.object.way->node_chain.size();
    g_string nodes_str(g_strdup_printf(big ?
                                             ngettext("%zu node", "%zu nodes", ncount) :
                                             ngettext("Length: %zu node", "Length: %zu nodes", ncount),
                                       ncount));
    label = gtk_label_new(nodes_str.get());

    if(big) table_attach(table, gtk_label_new(_("Length:")), 0, 2);
    table_attach(table, label, big?1:0, big?2:1);

    std::string type_str = context.object.way->is_closed() ? "closed way" : "open way";
    type_str += " (";
    type_str += (context.object.way->draw.flags & OSM_DRAW_FLAG_AREA)? "area" : "line";
    type_str += ")";

    label = gtk_label_new(type_str.c_str());
    if(big) table_attach(table, gtk_label_new(_("Type:")), 0, 3);
    table_attach(table, label, 1, big?3:1);
  } break;

  case object_t::RELATION: {
    /* relations tell something about their members */
    guint nodes = 0, ways = 0, relations = 0;
    context.object.relation->members_by_type(nodes, ways, relations);

    g_string str(g_strdup_printf(_("Members: %u nodes, %u ways, %u relations"),
                                 nodes, ways, relations));

    GtkWidget *member_btn = button_new_with_label(str.get());
    g_signal_connect(member_btn, "clicked", G_CALLBACK(on_relation_members),
                     const_cast<info_tag_context_t *>(&context));

    gtk_table_attach_defaults(GTK_TABLE(table), member_btn, 0, 2,
			      big?2:1, big?4:2);
    break;
  }

  default:
    g_error("ERROR: No node, way or relation (real type: %i)", context.object.type);
  }

  return table;
}

#ifdef FREMANTLE
/* put additional infos into a seperate dialog for fremantle as */
/* screen space is sparse there */
static void info_more(const info_tag_context_t &context) {
  osm2go_platform::WidgetGuard dialog(gtk_dialog_new_with_buttons(_("Object details"),
                                              GTK_WINDOW(context.dialog.get()), GTK_DIALOG_MODAL,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                              O2G_NULLPTR));

  dialog_size_hint(GTK_WINDOW(dialog.get()), MISC_DIALOG_SMALL);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog.get()), GTK_RESPONSE_CANCEL);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.get())->vbox),
                     details_widget(context, true), FALSE, FALSE, 0);
  gtk_widget_show_all(dialog.get());
  gtk_dialog_run(GTK_DIALOG(dialog.get()));
}
#endif

/* edit tags of currently selected node or way or of the relation */
/* given */
void info_dialog(GtkWidget *parent, map_t *map, osm_t *osm, presets_items *presets) {
  bool ret = info_dialog(parent, map, osm, presets, map->selected.object);

  /* since nodes being parts of ways but with no tags are invisible, */
  /* the result of editing them may have changed their visibility */
  if(ret && map->selected.object.type != object_t::RELATION)
    map->redraw_item(map->selected.object);
}

/* edit tags of currently selected node or way or of the relation */
/* given */
bool info_dialog(GtkWidget *parent, map_t *map, osm_t *osm, presets_items *presets, object_t &object) {

  assert(object.is_real());

  /* use implicit selection if not explicitely given */
  info_tag_context_t context(map, osm, presets, object);
  const char *msgtpl;

  switch(context.object.type) {
  case object_t::NODE:
    msgtpl = _("Node #" ITEM_ID_FORMAT);
    break;

  case object_t::WAY:
    msgtpl = _("Way #" ITEM_ID_FORMAT);
    break;

  case object_t::RELATION:
    msgtpl = _("Relation #" ITEM_ID_FORMAT);
    break;

  default:
    assert_unreachable();
  }

  g_string str(g_strdup_printf(msgtpl, context.object.obj->id));

  context.dialog.reset(gtk_dialog_new_with_buttons(str.get(), GTK_WINDOW(parent),
                                               GTK_DIALOG_MODAL,
#ifdef FREMANTLE
                                               _("More"), GTK_RESPONSE_HELP,
#endif
                                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                               GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                               O2G_NULLPTR));
  str.reset();

  dialog_size_hint(GTK_WINDOW(context.dialog.get()), MISC_DIALOG_LARGE);
  gtk_dialog_set_default_response(GTK_DIALOG(context.dialog.get()), GTK_RESPONSE_ACCEPT);

#ifndef FREMANTLE
  /* -------- details box --------- */
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog.get())->vbox),
		     details_widget(context, false),
		     FALSE, FALSE, 0);
#endif

  /* ------------ tags ----------------- */

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog.get())->vbox),
                     tag_widget(context), TRUE, TRUE, 0);

  /* ----------------------------------- */

  gtk_widget_show_all(context.dialog.get());
  bool ok = false, quit = false;

  do {
    switch(gtk_dialog_run(GTK_DIALOG(context.dialog.get()))) {
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

  if(ok)
    context.object.obj->updateTags(context.tags);

  return ok;
}

tag_context_t::tag_context_t(const object_t &o)
  : dialog(O2G_NULLPTR)
  , object(o)
  , tags(object.obj->tags.asMap())
{
}

info_tag_context_t::info_tag_context_t(map_t *m, osm_t *os, presets_items *p, const object_t &o)
  : tag_context_t(o)
  , map(m)
  , osm(os)
  , presets(p)
  , list(O2G_NULLPTR)
{
}
