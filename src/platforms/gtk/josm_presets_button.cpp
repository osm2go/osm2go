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

#include "josm_presets.h"
#include "josm_presets_p.h"

#include "icon.h"
#include "info.h"
#include "osm.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>
#include <numeric>

#ifdef FREMANTLE
#include <hildon/hildon-pannable-area.h>
#include <hildon/hildon-picker-button.h>
#endif
#include <strings.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_platform.h>
#include <osm2go_platform_gtk.h>

/* --------------------- the items dialog -------------------- */

struct preset_attach_context {
  inline preset_attach_context(GtkTable *t, guint &r)
    : table(t), y(r) {}
  GtkTable *table;
  guint &y;
};

static void attach_both(preset_attach_context &attctx, GtkWidget *widget) {
  gtk_table_attach(attctx.table, widget, 0, 2, attctx.y, attctx.y + 1,
                   static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
                   static_cast<GtkAttachOptions>(0), 0, 0);
  attctx.y++;
}

static void attach_right(preset_attach_context &attctx, const char *text, GtkWidget *widget) {
  if(text != nullptr) {
    gtk_table_attach(attctx.table, gtk_label_new(text), 0, 1, attctx.y, attctx.y + 1,
                     static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
                     static_cast<GtkAttachOptions>(0), 0, 0);
  }
  gtk_table_attach(attctx.table, widget, 1, 2, attctx.y, attctx.y + 1,
                   static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
                   static_cast<GtkAttachOptions>(0), 0, 0);
  attctx.y++;
}

/**
 * @brief update the given tag with the newly entered value
 * @param widget the entry widget description
 * @param tags all tags of the object
 * @param value the new value
 */
static bool store_value(const presets_element_t *widget, osm_t::TagMap &tags,
                        std::string value)
{
  bool changed;
  osm_t::TagMap::iterator ctag = tags.find(widget->key);
  if(!value.empty()) {
    if(ctag != tags.end()) {
      /* only update if the value actually changed */
      changed = ctag->second != value;
      if(changed)
        ctag->second.swap(value);
    } else {
      tags.insert(osm_t::TagMap::value_type(widget->key, std::move(value)));
      changed = true;
    }
  } else if (ctag != tags.end()) {
    g_debug("removed key = %s, value = %s", widget->key.c_str(), ctag->second.c_str());
    tags.erase(ctag);
    changed = true;
  } else {
    g_debug("ignore empty key = %s", widget->key.c_str());
    changed = false;
  }

  return changed;
}

#ifdef FREMANTLE
static gint table_expose_event(GtkWidget *widget, GdkEventExpose *,
			 gboolean *first) {

  if(*first) {
    guint border_width =
      gtk_container_get_border_width(GTK_CONTAINER(widget->parent));
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(widget->parent), GTK_SHADOW_NONE);

    gtk_widget_set_size_request(GTK_WIDGET(widget->parent), -1,
				widget->allocation.height +  2*border_width);
    *first = FALSE;
  }
  return FALSE;
}
#endif

struct presets_context_t {
  explicit presets_context_t(presets_items *pr, tag_context_t *t);
  ~presets_context_t();

  icon_t &icons;
  presets_items * const presets;
#ifndef PICKER_MENU
  osm2go_platform::WidgetGuard menu;
#else
  struct submenu {
    explicit submenu(presets_item_group *i = nullptr, GtkWidget *w = nullptr)
      : item(i), widget(w) {}
    presets_item_group *item;
#if __cplusplus >= 201103L
    osm2go_platform::WidgetGuard widget;
#else
    // this could use osm2go_platform::WidgetGuard, but the important part is the move constructor
    // to make sure only one instance holds the reference
    GtkWidget *widget;
#endif

    inline bool operator==(const presets_item_group *other) const
    { return item == other; }
  };
  typedef std::vector<submenu> submenu_vector;
  submenu_vector submenus;
  GtkWidget *subwidget; ///< dynamic submenu (if shown)

  GtkWidget *presets_picker(const std::vector<presets_item_t *> &items, bool top_level);
  GtkWidget *preset_picker_recent();
  GtkWidget *preset_picker_lru();

  presets_item *selected_item; ///< the item finally selected
#endif
  tag_context_t * const tag_context;
  unsigned int presets_mask;
  static presets_context_t *instance;
};

presets_context_t *presets_context_t::instance;

presets_context_t::presets_context_t(presets_items* pr, tag_context_t* t)
  : icons(icon_t::instance())
  , presets(pr)
#ifdef PICKER_MENU
  , subwidget(nullptr)
  , selected_item(nullptr)
#endif
  , tag_context(t)
  , presets_mask(presets_type_mask(t->object))
{
  assert_null(instance);
  instance = this;
}

presets_context_t::~presets_context_t()
{
  assert(instance == this);
  instance = nullptr;
}

typedef std::map<const presets_element_t *, presets_element_t::attach_key *> WidgetMap;

struct add_widget_functor {
  preset_attach_context attctx;
  WidgetMap &gtk_widgets;
  add_widget_functor(WidgetMap &g, GtkWidget *t, guint &r)
    : attctx(GTK_TABLE(t), r), gtk_widgets(g) {}
  void operator()(const WidgetMap::key_type w);
};

void add_widget_functor::operator()(const WidgetMap::key_type w)
{
  if(w->type == WIDGET_TYPE_REFERENCE) {
    const presets_element_reference * const r = static_cast<const presets_element_reference *>(w);
    std::for_each(r->item->widgets.begin(), r->item->widgets.end(), *this);
    return;
  }

  const osm_t::TagMap &tags = presets_context_t::instance->tag_context->tags;
  /* check if there's a value with this key already */
  const osm_t::TagMap::const_iterator otagIt = !w->key.empty() ?
                                                 tags.find(w->key) :
                                                 tags.end();
  const std::string &preset = otagIt != tags.end() ? otagIt->second : std::string();

  presets_element_t::attach_key *widget = w->attach(attctx, preset);

  if(widget != nullptr)
    gtk_widgets[w] = widget;
}

struct get_widget_functor {
  bool &changed;
  osm_t::TagMap &tags;
  const WidgetMap &gtk_widgets;
  const WidgetMap::const_iterator hintEnd;
  get_widget_functor(bool &c, osm_t::TagMap &t, const WidgetMap &g)
    : changed(c), tags(t), gtk_widgets(g), hintEnd(g.end()) {}
  void operator()(const presets_element_t *w);
};

void get_widget_functor::operator()(const presets_element_t* w)
{
  const WidgetMap::const_iterator hint = gtk_widgets.find(w);
  WidgetMap::mapped_type akey = hint != hintEnd ? hint->second : nullptr;

  switch(w->type) {
  case WIDGET_TYPE_KEY:
  case WIDGET_TYPE_CHECK:
  case WIDGET_TYPE_COMBO:
  case WIDGET_TYPE_MULTISELECT:
  case WIDGET_TYPE_TEXT:
    changed |= store_value(w, tags, w->getValue(akey));
    return;

  case WIDGET_TYPE_REFERENCE: {
    const presets_element_reference * const r = static_cast<const presets_element_reference *>(w);
    std::for_each(r->item->widgets.begin(), r->item->widgets.end(), *this);
    return;
  }

  default:
    return;
  }
}

static void presets_item_dialog(const presets_item *item) {
  osm2go_platform::DialogGuard dialog;
  bool ok;

  g_debug("dialog for item %s", item->name.c_str());

  /* build dialog from items widget list */

  /* check for widgets that have an interactive gui element. We won't show a
   * dialog if there's no interactive gui element at all */
  const std::vector<presets_element_t *>::const_iterator itEnd = item->widgets.end();
  std::vector<presets_element_t *>::const_iterator it = std::find_if(item->widgets.begin(), itEnd,
                                                                    presets_element_t::isInteractive);
  WidgetMap gtk_widgets;
  tag_context_t * const tag_context = presets_context_t::instance->tag_context;

  if(it != itEnd)  {
    dialog.reset(gtk_dialog_new_with_buttons(item->name.c_str(), tag_context->dialog,
                                             GTK_DIALOG_MODAL,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                             GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                             nullptr));

    /* if a web link has been provided for this item install */
    /* a button for this */
    if(!item->link.empty()) {
      GtkWidget *button = gtk_dialog_add_button(dialog, _("Info"), GTK_RESPONSE_HELP);
      g_signal_connect_swapped(button, "clicked", G_CALLBACK(osm2go_platform::open_url),
                               const_cast<char *>(item->link.c_str()));
    }

    /* special handling for the first label/separators */
    if(item->addEditName) {
      g_string title(g_strdup_printf(_("Edit %s"), item->name.c_str()));
      gtk_window_set_title(dialog, title.get());
    } else {
      // use the first label as title
      const presets_element_t * const w = item->widgets.front();
      if(w->type == WIDGET_TYPE_LABEL)
        gtk_window_set_title(dialog, w->text.c_str());
    }

    /* skip all following non-interactive widgets: use the first one that
     * was found to be interactive above. */
    assert((*it)->is_interactive());

    /* create table of required size */
    GtkWidget *table = gtk_table_new(std::accumulate(it, item->widgets.end(), 0, widget_rows), 2, FALSE);

    guint row = 0;
    add_widget_functor fc(gtk_widgets, table, row);
    std::for_each(it, itEnd, fc);

    GtkWidget *mwidget;
    gint dlgwidth, dlgheight;
#ifndef FREMANTLE
    dlgwidth = 300;
    dlgheight = 50;
    /* add widget directly to dialog */
    mwidget = table;
#else
    dlgwidth = -1;
    dlgheight = 500;
    /* put view into a pannable area */
    mwidget = hildon_pannable_area_new();
    hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(mwidget), table);

    gboolean first = TRUE;
    g_signal_connect(table, "expose_event", G_CALLBACK(table_expose_event), &first);
#endif
    gtk_box_pack_start(dialog.vbox(), mwidget, TRUE, TRUE, 0);
    gtk_window_set_default_size(dialog, dlgwidth, dlgheight);

    gtk_widget_show_all(dialog.get());

    /* run gtk_dialog_run, but continue if e.g. the help button was pressed */
    gint result;
    do {
      result = gtk_dialog_run(dialog);
    } while(result != GTK_RESPONSE_DELETE_EVENT &&
            result != GTK_RESPONSE_ACCEPT &&
            result != GTK_RESPONSE_REJECT);

    ok = (result == GTK_RESPONSE_ACCEPT);
  } else
    ok = true;

  if(ok) {
    /* handle all children of the table */
    bool changed = false;

    osm_t::TagMap ntags = tag_context->tags;
    get_widget_functor fc(changed, ntags, gtk_widgets);
    std::for_each(item->widgets.begin(), itEnd, fc);

    if(changed)
      tag_context->info_tags_replace(ntags);

    static_cast<presets_items_internal *>(presets_context_t::instance->presets)->lru_update(item);
  }
}

/* ------------------- the item list (popup menu) -------------- */

#ifndef PICKER_MENU
static GtkWidget *create_menuitem(icon_t &icons, const presets_item_named *item)
{
  GtkWidget *menu_item;

  if(item->icon.empty())
    menu_item = gtk_menu_item_new_with_label(item->name.c_str());
  else {
    menu_item = gtk_image_menu_item_new_with_label(item->name.c_str());

    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
                                  icons.widget_load(item->icon, 16));
  }

  return menu_item;
}

struct build_menu_functor {
  GtkWidget * const menu;
  GtkWidget ** const matches;
  bool was_separator;
  bool was_item;
  build_menu_functor(GtkWidget *m, GtkWidget **a)
    : menu(m), matches(a), was_separator(false), was_item(false) {}
  void operator()(const presets_item_t *item);
};

template<typename T>
static GtkWidget *build_menu(const T &items, GtkWidget **matches)
{
  build_menu_functor fc(gtk_menu_new(), matches);

  std::for_each(items.begin(), items.end(), fc);

  return fc.menu;
}

void build_menu_functor::operator()(const presets_item_t *item)
{
  /* check if this presets entry is appropriate for the current item */
  if(item->type & presets_context_t::instance->presets_mask) {
    GtkWidget *menu_item;

    /* Show a separator if one was requested, but not if there was no item
     * before to prevent to show one as the first entry. */
    if(was_item && was_separator)
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    was_item = true;
    was_separator = false;

    menu_item = create_menuitem(presets_context_t::instance->icons,
                                static_cast<const presets_item_named *>(item));

    if(item->type & presets_item_t::TY_GROUP) {
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
                                build_menu(static_cast<const presets_item_group *>(item)->items,
                                           matches));
    } else {
      g_signal_connect_swapped(menu_item, "activate",
                               G_CALLBACK(presets_item_dialog), const_cast<presets_item_t *>(item));

      if(matches && item->matches(presets_context_t::instance->tag_context->tags)) {
        if(!*matches)
          *matches = gtk_menu_new();

        GtkWidget *used_item = create_menuitem(presets_context_t::instance->icons,
                                               static_cast<const presets_item_named *>(item));
        g_signal_connect_swapped(used_item, "activate",
                                 G_CALLBACK(presets_item_dialog), const_cast<presets_item_t *>(item));
        gtk_menu_shell_append(GTK_MENU_SHELL(*matches), used_item);
      }
    }
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  } else if(item->type == presets_item_t::TY_SEPARATOR)
    /* Record that there was a separator. Do not immediately add it here to
     * prevent to show one as last entry. */
    was_separator = true;
}

#else // PICKER_MENU

struct group_member_used {
  const osm_t::TagMap &tags;
  explicit group_member_used(const osm_t::TagMap &t) : tags(t) {}
  bool operator()(const presets_item_t *item);
};

static bool preset_group_is_used(const presets_item_group *item,
                                 const osm_t::TagMap &tags)
{
  assert(item->type & presets_item_t::TY_GROUP);
  return std::find_if(item->items.begin(), item->items.end(),
                      group_member_used(tags)) != item->items.end();
}

bool group_member_used::operator()(const presets_item_t *item)
{
  if(item->type & presets_item_t::TY_GROUP)
    return preset_group_is_used(static_cast<const presets_item_group *>(item), tags);
  else
    return item->matches(tags);
}

enum {
  PRESETS_PICKER_COL_ICON = 0,
  PRESETS_PICKER_COL_NAME,
  PRESETS_PICKER_COL_ITEM_PTR,
  PRESETS_PICKER_COL_SUBMENU_ICON,
  PRESETS_PICKER_COL_SUBMENU_PTR,
  PRESETS_PICKER_NUM_COLS
};

static void remove_sub(presets_context_t::submenu_vector::value_type &sub_item) {
#if __cplusplus < 201103L
  // the unique_ptr will remove them automatically on destruction
  if(sub_item.widget) {
    gtk_widget_destroy(sub_item.widget);
    sub_item.widget = nullptr;
  }
#else
  (void) sub_item;
#endif
}

static void
on_presets_picker_selected(GtkTreeSelection *selection, presets_context_t *context) {
#ifdef FREMANTLE
  /* try to prevent inital selection */
  if(!g_object_get_data(G_OBJECT(selection), "setup_done")) {
    gtk_tree_selection_unselect_all (selection);
    g_object_set_data(G_OBJECT(selection), "setup_done", (gpointer)TRUE);
    return;
  }
#endif

  GtkTreeIter   iter;
  GtkTreeModel *model;

  if(gtk_tree_selection_get_selected(selection, &model, &iter) != TRUE)
    return;

  presets_item_named *item = nullptr;
  presets_item_group *sub_item = nullptr;
  gtk_tree_model_get(model, &iter,
                     PRESETS_PICKER_COL_SUBMENU_PTR, &sub_item,
                     PRESETS_PICKER_COL_ITEM_PTR, &item,
                     -1);

  GtkWidget * const view = GTK_WIDGET(gtk_tree_selection_get_tree_view(selection));

  if(item != nullptr && !(item->type & presets_item_t::TY_GROUP)) {
    /* save item pointer in dialog */
    context->selected_item = static_cast<presets_item *>(item);
    /* and request closing of menu */
    gtk_dialog_response(GTK_DIALOG(gtk_widget_get_toplevel(view)),
                        GTK_RESPONSE_ACCEPT);
  } else {
    /* check if this already had a submenu */
    // check if dynamic submenu is shown
    if(context->subwidget != nullptr) {
      gtk_widget_destroy(context->subwidget);
      context->subwidget = nullptr;
    }

    GtkWidget *sub;
    presets_context_t::submenu_vector::iterator subitBegin = context->submenus.begin();
    presets_context_t::submenu_vector::iterator subitEnd = context->submenus.end();
    if(sub_item != nullptr) {
      /* normal submenu */

      // this item is not currently visible
      // this is connected to the "changed" signal, so clicking an already selected
      // submenu does not cause an event
      if(sub_item->parent != nullptr) {
        // the parent item has to be visible, otherwise this could not have been clicked
        presets_context_t::submenu_vector::iterator it = std::find(subitBegin, subitEnd,
                                                                   sub_item->parent);
        assert(it != subitEnd);
        it++; // keep the parent
        std::for_each(it, subitEnd, remove_sub);
        context->submenus.erase(it, subitEnd);
      } else {
        // this is a top level menu, so everything currently shown can be removed
        std::for_each(subitBegin, subitEnd, remove_sub);
        context->submenus.clear();
      }

      sub = context->presets_picker(sub_item->items, false);
      context->submenus.push_back(presets_context_t::submenu(sub_item, sub));
    } else {
      // dynamic submenu
      // this is always on top level, so all old submenu entries can be removed
      std::for_each(subitBegin, subitEnd, remove_sub);
      context->submenus.clear();
      char *text = nullptr;
      gtk_tree_model_get(model, &iter, PRESETS_PICKER_COL_NAME, &text, -1);
      g_string textGuard(text);
      assert(textGuard);
      if (strcmp(text, _("Used presets")) == 0)
        sub = context->preset_picker_recent();
      else
        sub = context->preset_picker_lru();

      context->subwidget = sub;
    }

    /* views parent is a scrolled window whichs parent in turn is the hbox */
    assert(view->parent != nullptr);
    GtkWidget *hbox = view->parent->parent;

    gtk_box_pack_start(GTK_BOX(hbox), sub, TRUE, TRUE, 0);
    gtk_widget_show_all(sub);
  }
}

static GtkListStore *presets_picker_store(GtkTreeView **view) {
  GtkCellRenderer *renderer;

  *view = osm2go_platform::tree_view_new();

  gtk_tree_view_set_headers_visible(*view, FALSE);

  /* --- "Icon" column --- */
  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_insert_column_with_attributes(*view,
      -1, "Icon", renderer, "pixbuf", PRESETS_PICKER_COL_ICON, nullptr);

  /* --- "Name" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, nullptr);
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
		 "Name", renderer, "text", PRESETS_PICKER_COL_NAME, nullptr);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(*view, column, -1);

  /* --- "submenu icon" column --- */
  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_insert_column_with_attributes(*view,
      -1, "Submenu Icon", renderer, "pixbuf",
      PRESETS_PICKER_COL_SUBMENU_ICON, nullptr);

  return gtk_list_store_new(PRESETS_PICKER_NUM_COLS,
			     GDK_TYPE_PIXBUF,
			     G_TYPE_STRING,
			     G_TYPE_POINTER,
			     GDK_TYPE_PIXBUF,
			     G_TYPE_POINTER
			     );
}

static GtkWidget *presets_picker_embed(GtkTreeView *view, GtkListStore *store,
                                       presets_context_t *context) {
  gtk_tree_view_set_model(view, GTK_TREE_MODEL(store));
  g_object_unref(store);

  /* Setup the selection handler */
  GtkTreeSelection *select = gtk_tree_view_get_selection(view);
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
  g_signal_connect(select, "changed", G_CALLBACK (on_presets_picker_selected), context);

  gtk_tree_selection_unselect_all(select);

  return osm2go_platform::scrollable_container(GTK_WIDGET(view), false);
}

static GtkTreeIter preset_insert_item(const presets_item_named *item, icon_t &icons,
                                      GtkListStore *store) {
  /* icon load can cope with empty string as name (returns nullptr then) */
  icon_item *icon = icons.load(item->icon, 16);

  /* Append a row and fill in some data */
  GtkTreeIter iter;

  GdkPixbuf *pixmap = icon == nullptr ? nullptr : osm2go_platform::icon_pixmap(icon);

  gtk_list_store_insert_with_values(store, &iter, -1,
                                    PRESETS_PICKER_COL_ICON, pixmap,
                                    PRESETS_PICKER_COL_NAME, item->name.c_str(),
                                    PRESETS_PICKER_COL_ITEM_PTR, item,
                                    -1);

  return iter;
}

template<bool b>
struct insert_recent_items {
  const presets_context_t * const context;
  GtkListStore * const store;
  insert_recent_items(const presets_context_t *c, GtkListStore *s) : context(c), store(s) {}
  void operator()(const presets_item_t *preset);
};

/**
 * @brief match those members that are used in the preset or a group containing such an item
 */
template<> void insert_recent_items<false>::operator()(const presets_item_t *preset)
{
  if(preset->type & presets_item_t::TY_GROUP) {
    const presets_item_group *gr = static_cast<const presets_item_group *>(preset);
    std::for_each(gr->items.begin(), gr->items.end(),
                  insert_recent_items(context, store));
  } else if(preset->matches(context->tag_context->tags))
    preset_insert_item(static_cast<const presets_item_named *>(preset),
                       context->icons, store);
}

/**
 * @brief match any member in the list that has a matching type
 */
template<> void insert_recent_items<true>::operator()(const presets_item_t *preset)
{
  if(preset->type & context->presets_mask)
    preset_insert_item(static_cast<const presets_item_named *>(preset),
                       context->icons, store);
}

GtkWidget *presets_context_t::preset_picker_recent() {
  GtkTreeView *view;
  insert_recent_items<false> fc(this, presets_picker_store(&view));

  const std::vector<presets_item_t *> &pitems = static_cast<const presets_items_internal *>(presets)->items;
  std::for_each(pitems.begin(), pitems.end(), fc);

  return presets_picker_embed(view, fc.store, this);
}

GtkWidget *presets_context_t::preset_picker_lru() {
  GtkTreeView *view;
  insert_recent_items<true> fc(this, presets_picker_store(&view));

  const std::vector<const presets_item_t *> &pitems = static_cast<const presets_items_internal *>(presets)->lru;
  std::for_each(pitems.begin(), pitems.end(), fc);

  return presets_picker_embed(view, fc.store, this);
}

struct picker_add_functor {
  presets_context_t * const context;
  GtkListStore * const store;
  GdkPixbuf * const subicon;
  bool &show_recent;
  bool scan_for_recent;
  picker_add_functor(presets_context_t *c, GtkListStore *s, GdkPixbuf *i, bool r, bool &w)
    : context(c), store(s), subicon(i), show_recent(w)
    , scan_for_recent(r) {}
  void operator()(const presets_item_t *item);
};

void picker_add_functor::operator()(const presets_item_t *item)
{
  /* check if this presets entry is appropriate for the current item */
  if(!(item->type & context->presets_mask))
    return;

  const presets_item_named * const itemv = static_cast<const presets_item_named *>(item);

  if(itemv->name.empty())
    return;

  GtkTreeIter iter = preset_insert_item(itemv, context->icons, store);

  /* mark submenues as such */
  if(item->type & presets_item_t::TY_GROUP) {
    gtk_list_store_set(store, &iter,
                       PRESETS_PICKER_COL_SUBMENU_PTR,  item,
                       PRESETS_PICKER_COL_SUBMENU_ICON, subicon, -1);
    if(scan_for_recent) {
      show_recent = preset_group_is_used(static_cast<const presets_item_group *>(itemv),
                                         context->tag_context->tags);
      scan_for_recent = !show_recent;
    }
  } else if(scan_for_recent) {
    show_recent = itemv->matches(context->tag_context->tags);
    scan_for_recent = !show_recent;
  }
}

struct matching_type_functor {
  const unsigned int type; ///< the type to match
  explicit matching_type_functor(unsigned int t) : type(t) {}
  bool operator()(const presets_item_t *item) {
    return item->type & type;
  }
};

/**
 * @brief create a picker list for preset items
 * @param items the list of presets to show
 * @param top_level if a "Used Presets" subentry should be created
 *
 * This will create a view with the given items. This is just one column in the
 * presets view, every submenu will get it's own view created by a call to this
 * function.
 */
GtkWidget *
presets_context_t::presets_picker(const std::vector<presets_item_t *> &items,
                                  bool top_level) {
  GtkTreeView *view;
  GtkListStore *store = presets_picker_store(&view);

  bool show_recent = false;
  icon_item *subicon = icons.load("submenu_arrow");
  GdkPixbuf *subpix = osm2go_platform::icon_pixmap(subicon);
  picker_add_functor fc(this, store, subpix, top_level, show_recent);

  std::for_each(items.begin(), items.end(), fc);
  const std::vector<const presets_item_t *> &lru = static_cast<const presets_items_internal *>(presets)->lru;

  if(top_level &&
     std::find_if(lru.begin(), lru.end(), matching_type_functor(presets_mask)) != lru.end())
    gtk_list_store_insert_with_values(store, nullptr, 0,
                       PRESETS_PICKER_COL_NAME, _("Last used presets"),
                       PRESETS_PICKER_COL_SUBMENU_ICON, subpix,
                       -1);

  if(show_recent)
    gtk_list_store_insert_with_values(store, nullptr, 0,
                       PRESETS_PICKER_COL_NAME, _("Used presets"),
                       PRESETS_PICKER_COL_SUBMENU_ICON, subpix,
                       -1);

  icons.icon_free(subicon);

  return presets_picker_embed(view, store, this);
}
#endif

static gint button_press(GtkWidget *widget, GdkEventButton *event) {
  if(event->type != GDK_BUTTON_PRESS)
    return FALSE;

  g_debug("button press %d", event->button);

  presets_items_internal *pinternal = static_cast<presets_items_internal *>(presets_context_t::instance->presets);

#ifndef PICKER_MENU
  (void)widget;

  if (!presets_context_t::instance->menu) {
    GtkWidget *matches = nullptr;
    presets_context_t::instance->menu.reset(build_menu(pinternal->items, &matches));
    if(!pinternal->lru.empty()) {
      // This will not update the menu while the dialog is open. Not worth the effort.
      GtkWidget *menu_item = gtk_menu_item_new_with_label(_("Last used presets"));
      GtkWidget *lrumenu = build_menu(pinternal->lru, nullptr);

      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), lrumenu);
      gtk_menu_shell_prepend(GTK_MENU_SHELL(presets_context_t::instance->menu.get()), gtk_separator_menu_item_new());
      gtk_menu_shell_prepend(GTK_MENU_SHELL(presets_context_t::instance->menu.get()), menu_item);
    }
    if(matches) {
      GtkWidget *menu_item = gtk_menu_item_new_with_label(_("Used presets"));

      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), matches);
      gtk_menu_shell_prepend(GTK_MENU_SHELL(presets_context_t::instance->menu.get()), gtk_separator_menu_item_new());
      gtk_menu_shell_prepend(GTK_MENU_SHELL(presets_context_t::instance->menu.get()), menu_item);
    }
  }
  gtk_widget_show_all(presets_context_t::instance->menu.get());

  gtk_menu_popup(GTK_MENU(presets_context_t::instance->menu.get()), nullptr, nullptr, nullptr, nullptr,
                 event->button, event->time);
#else
  assert(presets_context_t::instance->submenus.empty());
  /* popup our special picker like menu */
  osm2go_platform::DialogGuard dialog(gtk_dialog_new_with_buttons(_("Presets"),
                                              GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(widget))),
                                              GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              nullptr));

  gtk_window_set_default_size(dialog, 400, 480);

  /* create root picker */
  GtkWidget *hbox = gtk_hbox_new(TRUE, 0);

  GtkWidget *root = presets_context_t::instance->presets_picker(pinternal->items, true);
  gtk_box_pack_start(GTK_BOX(hbox), root, TRUE, TRUE, 0);
  gtk_box_pack_start(dialog.vbox(), hbox, TRUE, TRUE, 0);

  assert_null(presets_context_t::instance->selected_item);
  gtk_widget_show_all(dialog.get());
  gtk_dialog_run(dialog);

  // remove all references to the widgets, they will be destroyed together with the dialog
  presets_context_t::instance->submenus.clear();
  presets_context_t::instance->subwidget = nullptr;

  // then delete the dialog, it would delete the submenus first otherwise
  dialog.reset();

  if(presets_context_t::instance->selected_item != nullptr) {
    presets_item_dialog(presets_context_t::instance->selected_item);
    presets_context_t::instance->selected_item = nullptr;
  }
#endif

  /* Tell calling code that we have handled this event; the buck
   * stops here. */
  return TRUE;
}

static gint on_button_destroy(presets_context_t *context) {
  delete context;

  return FALSE;
}

GtkWidget *josm_build_presets_button(presets_items *presets, tag_context_t *tag_context) {
  presets_context_t *context = new presets_context_t(presets, tag_context);

  GtkWidget *but = osm2go_platform::button_new_with_label(_("Presets"));
  gtk_widget_set_events(but, GDK_EXPOSURE_MASK);
  gtk_widget_add_events(but, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(but, "button-press-event", G_CALLBACK(button_press), nullptr);
  g_signal_connect_swapped(but, "destroy", G_CALLBACK(on_button_destroy), context);

  return but;
}

presets_element_t::attach_key *presets_element_text::attach(preset_attach_context &attctx,
                                                            const std::string &preset) const
{
  GtkWidget *ret = osm2go_platform::entry_new();
  if(!preset.empty())
    gtk_entry_set_text(GTK_ENTRY(ret), preset.c_str());
  else if(!def.empty())
    gtk_entry_set_text(GTK_ENTRY(ret), def.c_str());

  attach_right(attctx, text.c_str(), ret);

  return reinterpret_cast<presets_element_t::attach_key *>(ret);
}

std::string presets_element_text::getValue(presets_element_t::attach_key *akey) const
{
  GtkWidget * const widget = reinterpret_cast<GtkWidget *>(akey);
  assert(osm2go_platform::isEntryWidget(widget));

  return gtk_entry_get_text(GTK_ENTRY(widget));
}

presets_element_t::attach_key *presets_element_separator::attach(preset_attach_context &attctx,
                                                                 const std::string &) const
{
  attach_both(attctx, gtk_hseparator_new());
  return nullptr;
}

presets_element_t::attach_key *presets_element_label::attach(preset_attach_context &attctx,
                                                             const std::string &) const
{
  attach_both(attctx, gtk_label_new(text.c_str()));
  return nullptr;
}

static GtkListStore *
selectorModel(const std::vector<std::string> &values, const std::vector<std::string> &display_values)
{
  GtkListStore *store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

  const std::vector<std::string> &d = display_values.empty() ? values : display_values;

  for(std::vector<std::string>::size_type count = 0; count < d.size(); count++) {
    const std::string &value = d[count].empty() ? values[count] : d[count];

    gtk_list_store_insert_with_values(store, nullptr, -1, 0, value.c_str(), 1, values[count].c_str(), -1);
  }

  return store;
}

presets_element_t::attach_key *presets_element_combo::attach(preset_attach_context &attctx,
                                                             const std::string &preset) const
{
  const std::string &pr = preset.empty() ? def : preset;
  int active = editable ? 0 : 1; // account for the extra "unset" entry for non-editable ones
  bool matched = false;

  std::unique_ptr<GtkListStore, g_object_deleter> store(selectorModel(values, display_values));
  unsigned int flags;

  if(editable) {
    flags = osm2go_platform::AllowEditing;
  } else {
    gtk_list_store_insert_with_values(store.get(), nullptr, 0, 0, _("unset"), 1, "", -1);
    flags = osm2go_platform::NoSelectionFlags;
  }
  GtkWidget * const ret = osm2go_platform::select_widget_wrapped(text.c_str(), GTK_TREE_MODEL(store.get()), flags);

  if(!pr.empty()) {
    const std::vector<std::string>::const_iterator itEnd = values.end();
    const std::vector<std::string>::const_iterator itBegin = values.begin();
    const std::vector<std::string>::const_iterator it = std::find(itBegin, itEnd, pr);
    if(it != itEnd) {
      matched = true;
      active += std::distance(itBegin, it);
    }
  }

  if(editable && !matched) {
    osm2go_platform::combo_box_set_active(ret, -1);
    osm2go_platform::combo_box_set_active_text(ret, preset.c_str());
  } else
    osm2go_platform::combo_box_set_active(ret, active);
#ifndef FREMANTLE
  attach_right(attctx, text.c_str(), ret);
#else
  attach_both(attctx, ret);
#endif

  return reinterpret_cast<presets_element_t::attach_key *>(ret);
}

std::string presets_element_combo::getValue(presets_element_t::attach_key *akey) const
{
  return osm2go_platform::select_widget_value(reinterpret_cast<GtkWidget *>(akey));
}

presets_element_t::attach_key *presets_element_multiselect::attach(preset_attach_context &attctx,
                                                                   const std::string &preset) const
{
  const std::string &pr = preset.empty() ? def : preset;

  std::unique_ptr<GtkListStore, g_object_deleter> store(selectorModel(values, display_values));
  GtkWidget *ret = osm2go_platform::select_widget_wrapped(text.c_str(), GTK_TREE_MODEL(store.get()),
                                                          osm2go_platform::AllowMultiSelection,
                                                          &delimiter);

  const std::vector<unsigned int> &indexes = matchedIndexes(pr);

  osm2go_platform::select_widget_select(ret, indexes);

#ifndef FREMANTLE
  // arbitrary number for height scaling
  gtk_widget_set_size_request(ret, -1, rows_height * 24);

  attach_right(attctx, text.c_str(), ret);
#else
  attach_both(attctx, ret);
#endif

  return reinterpret_cast<presets_element_t::attach_key *>(ret);
}

std::string presets_element_multiselect::getValue(presets_element_t::attach_key *akey) const
{
  return osm2go_platform::select_widget_value(reinterpret_cast<GtkWidget *>(akey));
}

presets_element_t::attach_key *presets_element_checkbox::attach(preset_attach_context &attctx,
                                                                const std::string &preset) const
{
  bool deflt;
  if(!preset.empty())
    deflt = matchValue(preset);
  else
    deflt = def;

  GtkWidget *ret = osm2go_platform::check_button_new_with_label(text.c_str());
  osm2go_platform::check_button_set_active(ret, deflt);
#ifndef FREMANTLE
  attach_right(attctx, nullptr, ret);
#else
  attach_both(attctx, ret);
#endif

  return reinterpret_cast<presets_element_t::attach_key *>(ret);
}

std::string presets_element_checkbox::getValue(presets_element_t::attach_key *akey) const
{
  GtkWidget * const widget = reinterpret_cast<GtkWidget *>(akey);
  assert(osm2go_platform::isCheckButtonWidget(widget));

  return osm2go_platform::check_button_get_active(widget) ?
         (value_on.empty() ? "yes" : value_on) : std::string();
}

static void item_link_clicked(presets_item *item) {
  presets_item_dialog(item);
}

presets_element_t::attach_key *presets_element_link::attach(preset_attach_context &attctx,
                                                            const std::string &) const
{
  g_string label(g_strdup_printf(_("[Preset] %s"), item->name.c_str()));
  GtkWidget *button = osm2go_platform::button_new_with_label(label.get());
  GtkWidget *img = icon_t::instance().widget_load(item->icon, 16);
  if(img != nullptr) {
    gtk_button_set_image(GTK_BUTTON(button), img);
    // make sure the image is always shown, Hildon seems to hide it by default
    gtk_widget_show(img);
  }
  g_signal_connect_swapped(button, "clicked", G_CALLBACK(item_link_clicked), item);
  attach_both(attctx, button);
  return nullptr;
}
