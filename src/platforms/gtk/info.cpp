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

namespace {

enum {
  TAG_COL_KEY = 0,
  TAG_COL_VALUE,
  TAG_COL_COLLISION,
  TAG_NUM_COLS
};

class info_tag_context_t : public tag_context_t {
public:
  explicit info_tag_context_t(map_t *m, osm_t::ref os, presets_items *p, const object_t &o, GtkWidget *dlg);
  info_tag_context_t() O2G_DELETED_FUNCTION;
  info_tag_context_t(const info_tag_context_t &) O2G_DELETED_FUNCTION;
  info_tag_context_t &operator=(const info_tag_context_t &) O2G_DELETED_FUNCTION;
#if __cplusplus >= 201103L
  info_tag_context_t(info_tag_context_t &&) = delete;
  info_tag_context_t &operator=(info_tag_context_t &&) = delete;
  ~info_tag_context_t() = default;
#endif

  map_t * const map;
  osm_t::ref osm;
  presets_items * const presets;
  GtkWidget *list;
  std::unique_ptr<GtkListStore, g_object_deleter> store;
  osm_t::TagMap m_tags;

  void update_collisions(const std::string &k);
};

void
changed(GtkTreeSelection *, gpointer user_data)
{
  GtkWidget *list = static_cast<info_tag_context_t *>(user_data)->list;

  GtkTreeModel *model;
  GtkTreeIter iter;
  bool selected = list_get_selected(list, &model, &iter);

  if(selected) {
    const gchar *key = nullptr;
    gtk_tree_model_get(model, &iter, TAG_COL_KEY, &key, -1);

    // WARNING: for whatever reason, key CAN be NULL on N900

    /* you just cannot delete or edit the discardable tags */
    if(unlikely(key != nullptr && tag_t::is_discardable(key)))
      selected = false;
  }

  list_button_enable(GTK_WIDGET(list), LIST_BUTTON_REMOVE, selected);
  list_button_enable(GTK_WIDGET(list), LIST_BUTTON_EDIT, selected);
}

struct update_collisions_context {
  inline update_collisions_context(const std::string &k, const osm_t::TagMap &t)
    : key(k), tags(t) {}
  const std::string &key;
  const osm_t::TagMap &tags;
};

gboolean
update_collisions_foreach(GtkTreeModel *model, GtkTreePath *, GtkTreeIter *iter, gpointer data)
{
  const update_collisions_context * const ctx = static_cast<update_collisions_context *>(data);

  gchar *key = nullptr;
  gtk_tree_model_get(model, iter, TAG_COL_KEY, &key, -1);
  g_string skey(key);
  assert(skey);
  if(ctx->key == key)
    gtk_list_store_set(GTK_LIST_STORE(model), iter,
                       TAG_COL_COLLISION, (ctx->tags.count(key) > 1) ? TRUE : FALSE,
                       -1);

  return FALSE;
}

struct value_match_functor {
  const std::string &value;
  explicit inline value_match_functor(const std::string &v) : value(v) {}
  bool operator()(const osm_t::TagMap::value_type &pair) {
    return pair.second == value;
  }
};

void
on_tag_remove(info_tag_context_t *context)
{
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  GtkTreeSelection *selection = list_get_selection(context->list);
  if(gtk_tree_selection_get_selected(selection, &model, &iter) == TRUE) {
    const gchar *kc = nullptr, *vc = nullptr;
    gtk_tree_model_get(model, &iter, TAG_COL_KEY, &kc, TAG_COL_VALUE, &vc, -1);

    assert(kc != nullptr);
    assert(vc != nullptr);

    /* de-chain */
    g_debug("de-chaining tag %s/%s", kc, vc);
    const std::string k = kc;
    osm_t::TagMap::iterator it = context->m_tags.findTag(k, vc);
    assert(it != context->tags.end());

    context->m_tags.erase(it);

    GtkTreeIter n = iter;
    // select the next entry if it exists
    if(gtk_tree_model_iter_next(model, &n) == TRUE)
      gtk_tree_selection_select_iter(selection, &n);
    /* and remove from store */
    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

    // collision flag has only changed if there is exactly one instance of this tag left
    if(unlikely(context->tags.count(k) == 1))
      context->update_collisions(k);
  }
}

struct key_context {
  key_context(const osm_t::TagMap &k, const std::string &s) : m_tags(k), initial(s) {}
  const osm_t::TagMap &m_tags;
  const std::string &initial;
};

void
callback_modified_key(GtkWidget *key, const key_context *context)
{
  const char *txt = gtk_entry_get_text(GTK_ENTRY(key));
  GtkDialog *dialog = GTK_DIALOG(gtk_widget_get_toplevel(key));
  gboolean en;

  if(txt == nullptr || *txt == '\0') {
    // new keys that are empty are not allowed
    en = FALSE;
  } else if(context->initial == txt) {
    // collision or not, if the key is the original one this is fine
    en = TRUE;
  } else {
    // only allow a new key if it is not already in the map (i.e. no collision)
    en = context->m_tags.find(txt) == context->m_tags.end() ? TRUE : FALSE;
  }

  gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_ACCEPT, en);
}

gboolean
cb_value_focus_in(GtkEntry *value, GdkEventFocus *, GtkEntry *key)
{
  const gchar *nk = gtk_entry_get_text(GTK_ENTRY(key));
  GtkEntry *ventry = GTK_ENTRY(value);
  const gchar *nv = gtk_entry_get_text(ventry);

  if((g_strcmp0(nk, "survey:date") == 0 || g_strcmp0(nk, "source:date") == 0) && (nv == nullptr || strcmp(nv, "") == 0)) {
    struct tm loctime;
    const time_t t = time(nullptr);
    localtime_r(&t, &loctime);
    char time_str[16];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d", &loctime);

    gtk_entry_set_text(ventry, time_str);
  }

  return FALSE;
}

/**
 * @brief request user input for the given tag
 * @param window the parent window
 * @param k the key
 * @param v the value
 * @return if the tag was actually modified
 * @retval false the tag is the same as before
 */
bool
tag_edit(GtkWindow *window, std::string &k, std::string &v, const osm_t::TagMap &otherkeys)
{
  osm2go_platform::DialogGuard dialog(gtk_dialog_new_with_buttons(_("Edit Tag"), window, GTK_DIALOG_MODAL,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              nullptr));

  osm2go_platform::dialog_size_hint(dialog, osm2go_platform::MISC_DIALOG_SMALL);
  gtk_dialog_set_default_response(dialog, GTK_RESPONSE_ACCEPT);

  GtkWidget *label = gtk_label_new(_("Key:"));
  GtkWidget *key = entry_new(osm2go_platform::EntryFlagsNoAutoCap);
  GtkWidget *table = gtk_table_new(2, 2, FALSE);

  key_context kctx(otherkeys, k);
  g_signal_connect(key, "changed", G_CALLBACK(callback_modified_key), &kctx);

  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                   static_cast<GtkAttachOptions>(0),
                   static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), key, 1, 2, 0, 1);
  gtk_entry_set_activates_default(GTK_ENTRY(key), TRUE);

  label = gtk_label_new(_("Value:"));
  GtkWidget *value = osm2go_platform::entry_new(osm2go_platform::EntryFlagsNoAutoCap);
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
                   static_cast<GtkAttachOptions>(0),
                   static_cast<GtkAttachOptions>(0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
  gtk_table_attach_defaults(GTK_TABLE(table), value, 1, 2, 1, 2);
  gtk_entry_set_activates_default(GTK_ENTRY(value), TRUE);
  g_signal_connect(value, "focus-in-event", G_CALLBACK(cb_value_focus_in), key);

  gtk_entry_set_text(GTK_ENTRY(key), k.c_str());
  gtk_entry_set_text(GTK_ENTRY(value), v.c_str());

  gtk_box_pack_start(dialog.vbox(), table, TRUE, TRUE, 0);

  gtk_widget_show_all(dialog.get());

  callback_modified_key(key, &kctx);

  if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(dialog)) {
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

struct select_item_context {
  inline select_item_context(const std::string &key, const std::string &value, GtkWidget *l)
    : k(key), v(value), list(l) {}
  const std::string &k;
  const std::string &v;
  GtkWidget * const list;
};

gboolean
select_item_foreach(GtkTreeModel *model, GtkTreePath *, GtkTreeIter *iter, gpointer data)
{
  const select_item_context * const context = static_cast<select_item_context *>(data);
  const gchar *key = nullptr, *value = nullptr;
  gtk_tree_model_get(model, iter,
                      TAG_COL_KEY, &key,
                      TAG_COL_VALUE, &value,
                     -1);
  assert(key != nullptr);
  assert(value != nullptr);
  if(context->k == key && context->v == value) {
    gtk_tree_selection_select_iter(list_get_selection(context->list), iter);
    return TRUE;
  }

  return FALSE;
}

void
select_item(const std::string &k, const std::string &v, info_tag_context_t *context)
{
  select_item_context ctx(k, v, context->list);

  gtk_tree_model_foreach(GTK_TREE_MODEL(context->store.get()), select_item_foreach, &ctx);
}

void
on_tag_edit(info_tag_context_t *context)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  GtkTreeSelection *sel = list_get_selection(context->list);
  if(sel == nullptr) {
    g_debug("got no selection object");
    return;
  }

  if(gtk_tree_selection_get_selected(sel, &model, &iter) != TRUE) {
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

  if(tag_edit(context->dialog, k, v, context->m_tags)) {
    g_debug("setting %s/%s", k.c_str(), v.c_str());

    std::pair<osm_t::TagMap::iterator, osm_t::TagMap::iterator> matches = context->m_tags.equal_range(oldk);
    assert(matches.first != matches.second);
    osm_t::TagMap::iterator it = std::find_if(matches.first, matches.second, value_match_functor(oldv));
    assert(it != matches.second);
    const unsigned int match_cnt = std::distance(matches.first, matches.second);

    if(it->first == k) {
      // only value was changed
      // collision flags only need to be updated if there is more than one entry with that key
      if(unlikely(match_cnt > 1)) {
        // check if the entry is now equal to another entry
        if(std::find_if(matches.first, matches.second, value_match_functor(v)) != matches.second) {
          // the item is now a duplicate, so it can be removed
          gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
          context->m_tags.erase(it);

          select_item(k, v, context);
          context->update_collisions(k);
          return;
        }
        // if the collisions persist no update has to be done as there already was a collision before
      }
      it->second = v;
    } else {
      context->m_tags.erase(it);
      // update collisions for the old entry if there was one and it is now gone
      if(unlikely(match_cnt == 2))
        context->update_collisions(oldk);

      // There can't be collisions for the new entry as the Ok button is not enabled then
      context->m_tags.insert(osm_t::TagMap::value_type(k, v));
    }

    gtk_list_store_set(context->store.get(), &iter,
                       TAG_COL_KEY, k.c_str(),
                       TAG_COL_VALUE, v.c_str(),
                       -1);
  }
}

bool
replace_with_last(const info_tag_context_t *context, const osm_t::TagMap &ntags)
{
  // if all tags of the object are part of the new tag list no information will be lost
  if(osm_t::tagSubset(context->tags, ntags))
    return true;

  const char *ts = context->object.type_string();
  return osm2go_platform::yes_no(_("Overwrite tags?"),
                trstring("This will overwrite all tags of this %1 with the ones from "
                         "the %1 selected last.\n\nDo you really want this?").arg(ts),
                MISC_AGAIN_ID_OVERWRITE_TAGS, context->dialog.get());
}

void
on_tag_last(info_tag_context_t *context)
{
  const osm_t::TagMap &ntags = context->object.type == object_t::NODE ?
                               context->map->last_node_tags :
                               context->map->last_way_tags;

  if(!replace_with_last(context, ntags))
    return;

  context->info_tags_replace(ntags);

  // Adding those tags above will usually make the first of the newly
  // added tags selected. Enable edit/remove buttons now.
  GtkTreeSelection *sel = list_get_selection(context->list);
  changed(sel, context);
}

GtkTreeIter
store_append(GtkListStore *store, const std::string &key, const std::string &value, bool collision)
{
  GtkTreeIter iter;
  gtk_list_store_insert_with_values(store, &iter, -1,
                                    TAG_COL_KEY,       key.c_str(),
                                    TAG_COL_VALUE,     value.c_str(),
                                    TAG_COL_COLLISION, collision ? TRUE : FALSE,
                                    -1);
  return iter;
}

void
on_tag_add(info_tag_context_t *context)
{
  std::string k, v;

  if(!tag_edit(context->dialog, k, v, context->m_tags))
    return;

  // there can't be a new collision as the ok button in tag_edit() is not enabled then
  context->m_tags.insert(osm_t::TagMap::value_type(k, v));
  /* append a row for the new data */
  GtkTreeIter iter = store_append(context->store.get(), k, v, FALSE);

  gtk_tree_selection_select_iter(list_get_selection(context->list), &iter);
}

// bad name, but avoids name collisions
struct tag_replace_functor {
  GtkListStore * const store;
  const osm_t::TagMap &tags;
  inline tag_replace_functor(GtkListStore *s, const osm_t::TagMap &t) : store(s), tags(t) {}
  void operator()(const osm_t::TagMap::value_type &pair) {
    store_append(store, pair.first, pair.second, tags.count(pair.first) > 1);
  }
};

void
store_fill(GtkListStore *store, const osm_t::TagMap &tags)
{
  std::for_each(tags.begin(), tags.end(), tag_replace_functor(store, tags));
}

void
on_relations(info_tag_context_t *context)
{
  relation_membership_dialog(context->dialog.get(), context->presets,
                             context->osm, context->object);
}

GtkWidget *
tag_widget(info_tag_context_t &context)
{
  /* setup both columns */
  std::vector<list_view_column> columns;
  columns.push_back(list_view_column(_("Key"),   LIST_FLAG_ELLIPSIZE|LIST_FLAG_CAN_HIGHLIGHT, TAG_COL_COLLISION));
  columns.push_back(list_view_column(_("Value"), LIST_FLAG_ELLIPSIZE));

  std::vector<list_button> buttons;
  buttons.push_back(list_button(_("_Add"), G_CALLBACK(on_tag_add)));
  buttons.push_back(list_button(_("_Edit"), G_CALLBACK(on_tag_edit)));
  buttons.push_back(list_button(_("Remove"), G_CALLBACK(on_tag_remove)));
  buttons.push_back(list_button(_("Last"), G_CALLBACK(on_tag_last)));
  buttons.push_back(list_button(nullptr, nullptr));
  buttons.push_back(list_button(_("Relations"), G_CALLBACK(on_relations)));

  context.store.reset(gtk_list_store_new(TAG_NUM_COLS,
                                          G_TYPE_STRING, G_TYPE_STRING,
                                          G_TYPE_BOOLEAN, G_TYPE_POINTER));

  context.list = list_new(LIST_HILDON_WITHOUT_HEADERS, LIST_BTN_2ROW, &context, changed,
                          buttons, columns, GTK_TREE_MODEL(context.store.get()));

  list_set_custom_user_button(context.list, LIST_BUTTON_USER1,
                              josm_build_presets_button(context.presets, &context));
  if(unlikely(context.presets == nullptr))
    list_button_enable(context.list, LIST_BUTTON_USER1, FALSE);

  /* disable if no appropriate "last" tags have been stored or if the */
  /* selected item isn't a node or way */
  if((context.object.type == object_t::NODE && context.map->last_node_tags.empty()) ||
     (context.object.type == object_t::WAY && context.map->last_way_tags.empty()) ||
     (context.object.type != object_t::NODE && context.object.type != object_t::WAY))
    list_button_enable(context.list, LIST_BUTTON_USER0, FALSE);

  /* --------- build and fill the store ------------ */
  store_fill(context.store.get(), context.tags);

  return context.list;
}

void
on_relation_members(GtkWidget *, const info_tag_context_t *context)
{
  assert_cmpnum(context->object.type, object_t::RELATION);
  relation_show_members(context->dialog.get(), context->object.relation, context->osm);
}

void
table_attach(GtkWidget *table, GtkWidget *child, int x, int y)
{
  gtk_table_attach_defaults(GTK_TABLE(table), child, x, x+1, y, y+1);
}

GtkWidget *
details_widget(const info_tag_context_t &context, bool big)
{
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
    const trstring dv_str = trstring("%1 (# %2)").arg(time_str).arg(context.object.obj->version);
    label = gtk_label_new(static_cast<const gchar *>(dv_str));
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

    const char *type_str;
    if(context.object.way->is_area())
      type_str = _("area");
    else if(context.object.way->is_closed())
      type_str = _("closed way");
    else
      type_str = _("open way");

    label = gtk_label_new(type_str);
    if(big) table_attach(table, gtk_label_new(_("Type:")), 0, 3);
    table_attach(table, label, 1, big?3:1);
  } break;

  case object_t::RELATION: {
    /* relations tell something about their members */
    guint nodes = 0, ways = 0, relations = 0;
    context.object.relation->members_by_type(nodes, ways, relations);

    const trstring str = trstring("Members: %1 nodes, %2 ways, %3 relations").arg(nodes).arg(ways).arg(relations);

    GtkWidget *member_btn = osm2go_platform::button_new_with_label(static_cast<const gchar *>(str));
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
void
info_more(const info_tag_context_t &context)
{
  osm2go_platform::DialogGuard dialog(gtk_dialog_new_with_buttons(_("Object details"),
                                      context.dialog, GTK_DIALOG_MODAL,
                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, nullptr));

  osm2go_platform::dialog_size_hint(dialog, osm2go_platform::MISC_DIALOG_SMALL);
  gtk_dialog_set_default_response(dialog, GTK_RESPONSE_CANCEL);

  gtk_box_pack_start(dialog.vbox(), details_widget(context, true), FALSE, FALSE, 0);
  gtk_widget_show_all(dialog.get());
  gtk_dialog_run(dialog);
}
#endif

trstring
objid(const object_t &object)
{
  /* use implicit selection if not explicitely given */
  trstring msgtpl;

  switch(object.type) {
  case object_t::NODE:
    if (object.obj->isNew())
      msgtpl = trstring("Node #%1 (new)");
    else if (object.obj->isDirty())
      msgtpl = trstring("Node #%1 (modified)");
    else
      msgtpl = trstring("Node #%1");
    break;

  case object_t::WAY:
    if (object.obj->isNew())
      msgtpl = trstring("Way #%1 (new)");
    else if (object.obj->isDirty())
      msgtpl = trstring("Way #%1 (modified)");
    else
      msgtpl = trstring("Way #%1");
    break;

  case object_t::RELATION:
    if (object.obj->isNew())
      msgtpl = trstring("Relation #%1 (new)");
    else if (object.obj->isDirty())
      msgtpl = trstring("Relation #%1 (modified)");
    else
      msgtpl = trstring("Relation #%1");
    break;

  default:
    assert_unreachable();
  }

  return msgtpl.arg(object.obj->id);
}

} // namespace

/* edit tags of currently selected node or way or of the relation */
/* given */
bool info_dialog(GtkWidget *parent, map_t *map, osm_t::ref osm, presets_items *presets, object_t &object) {

  assert(object.is_real());

  info_tag_context_t context(map, osm, presets, object,
                             gtk_dialog_new_with_buttons(static_cast<const gchar *>(objid(object)),
                                                         GTK_WINDOW(parent), GTK_DIALOG_MODAL,
#ifdef FREMANTLE
                                               _("More"), GTK_RESPONSE_HELP,
#endif
                                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                               GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                               nullptr));

  osm2go_platform::dialog_size_hint(context.dialog, osm2go_platform::MISC_DIALOG_LARGE);
  gtk_dialog_set_default_response(context.dialog, GTK_RESPONSE_ACCEPT);

#ifndef FREMANTLE
  /* -------- details box --------- */
  gtk_box_pack_start(context.dialog.vbox(), details_widget(context, false), FALSE, FALSE, 0);
#endif

  /* ------------ tags ----------------- */

  gtk_box_pack_start(context.dialog.vbox(), tag_widget(context), TRUE, TRUE, 0);

  /* ----------------------------------- */

  gtk_widget_show_all(context.dialog.get());
  bool ok = false, quit = false;

  do {
    switch(gtk_dialog_run(context.dialog)) {
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

tag_context_t::tag_context_t(const object_t &o, const osm_t::TagMap &t, osm2go_platform::Widget *dlg)
  : dialog(dlg)
  , object(o)
  , tags(t)
{
}

void tag_context_t::info_tags_replace(const osm_t::TagMap &ntags)
{
  info_tag_context_t *ictx = static_cast<info_tag_context_t *>(this);
  GtkListStore *store = ictx->store.get();
  gtk_list_store_clear(store);

  ictx->m_tags = ntags;

  store_fill(store, tags);
}

info_tag_context_t::info_tag_context_t(map_t *m, osm_t::ref os, presets_items *p, const object_t &o, GtkWidget *dlg)
  : tag_context_t(o, m_tags, dlg)
  , map(m)
  , osm(os)
  , presets(p)
  , list(nullptr)
  , m_tags(object.obj->tags.asMap())
{
}

void info_tag_context_t::update_collisions(const std::string &k)
{
  update_collisions_context ctx(k, tags);

  gtk_tree_model_foreach(GTK_TREE_MODEL(store.get()), update_collisions_foreach, &ctx);
}
