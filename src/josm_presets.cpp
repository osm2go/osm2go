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

#include "josm_presets.h"
#include "josm_presets_p.h"

#include "appdata.h"
#include "icon.h"
#include "info.h"
#include "misc.h"
#include "osm.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <numeric>

#ifdef FREMANTLE
#include <hildon/hildon-picker-button.h>
#endif
#include <strings.h>

#include <osm2go_cpp.h>

#ifdef ENABLE_BROWSER_INTERFACE
static void on_info(GtkWidget *widget, gpointer context) {
  const char *link = (char*)g_object_get_data(G_OBJECT(widget), "link");
  open_url((appdata_t*)context, link);
}
#endif

/* --------------------- the items dialog -------------------- */

static void attach_both(GtkTable *table, GtkWidget *widget, guint &y) {
  gtk_table_attach(table, widget, 0, 2, y, y + 1,
                   static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
                   static_cast<GtkAttachOptions>(0), 0, 0);
  y++;
}

static void attach_right(GtkTable *table, const char *text, GtkWidget *widget, guint &y) {
  if(text) {
    gtk_table_attach(table, gtk_label_new(text), 0, 1, y, y + 1,
                     static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
                     static_cast<GtkAttachOptions>(0), 0, 0);
  }
  gtk_table_attach(table, widget, 1, 2, y, y + 1,
                   static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL),
                   static_cast<GtkAttachOptions>(0), 0, 0);
  y++;
}

/**
 * @brief update the given tag with the newly entered value
 * @param widget the entry widget description
 * @param tags all tags of the object
 * @param ctag iterator of the tag to edit, tags.end() in case it does not yet exist
 * @param value the new value
 */
static bool store_value(const presets_widget_t *widget, std::vector<stag_t *> &tags,
                        std::vector<stag_t *>::iterator ctag, const char *value) {
  bool changed = false;
  if(value && strlen(value)) {
    const char *chstr;
    if(ctag != tags.end()) {
      /* update the previous tag structure */
      g_assert(strcasecmp((*ctag)->key.c_str(), widget->key.c_str()) == 0);
      /* only update if the value actually changed */
      if((*ctag)->value != value) {
        changed = true; /* mark as updated, actual change below */
        chstr = "updated";
      } else {
        chstr = "kept";
      }
    } else {
      /* no old entry, create a new one */
      stag_t *tag = new stag_t(widget->key, std::string());
      /* value will be updated below */
      tags.push_back(tag);
      ctag = tags.end() - 1;
      changed = true;
      chstr = "new";
    }

    if(changed)
      (*ctag)->value = value;

    printf("%s key = %s, value = %s\n", chstr,
           widget->key.c_str(), (*ctag)->value.c_str());
  } else if (ctag != tags.end()) {
    printf("removed key = %s, value = %s\n", widget->key.c_str(), (*ctag)->value.c_str());
    stag_t *tag = *ctag;
    tags.erase(ctag);
    delete tag;
    changed = true;
  } else
    printf("ignore empty key = %s\n", widget->key.c_str());

  return changed;
}

#ifdef USE_HILDON
static gint table_expose_event(GtkWidget *widget, G_GNUC_UNUSED GdkEventExpose *event,
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
  explicit presets_context_t(appdata_t *a, tag_context_t *t)
    : appdata(a)
#ifndef FREMANTLE
    , menu(O2G_NULLPTR)
#endif
    , tag_context(t)
  {
  }

  appdata_t * const appdata;
#ifndef FREMANTLE
  GtkWidget *menu;
#endif
#ifdef PICKER_MENU
  std::vector<presets_item_group *> submenus;
#endif
  tag_context_t * const tag_context;
};

struct find_tag_functor {
  const std::string &key;
  find_tag_functor(const std::string &k) : key(k) {}
  bool operator()(const stag_t *tag) {
    return strcasecmp(tag->key.c_str(), key.c_str()) == 0;
  }
};

typedef std::pair<GtkWidget *, stag_t *> HintPair;

struct add_widget_functor {
  guint &row;
  presets_context_t * const context;
  std::map<const presets_widget_t *, HintPair> &gtk_widgets;
  GtkWidget * const table;
  add_widget_functor(std::map<const presets_widget_t *, HintPair> &g, presets_context_t *c, GtkWidget *t, guint &r)
    : row(r), context(c), gtk_widgets(g), table(t) {}
  void operator()(const presets_widget_t *w);
};

void add_widget_functor::operator()(const presets_widget_t *w)
{
  if(w->type == WIDGET_TYPE_REFERENCE) {
    const presets_widget_reference * const r = static_cast<const presets_widget_reference *>(w);
    std::for_each(r->item->widgets.begin(), r->item->widgets.end(), *this);
    return;
  }

  tag_context_t * const tag_context = context->tag_context;
  /* check if there's a value with this key already */
  std::vector<stag_t *>::const_iterator otagIt = !w->key.empty() ?
                                                 std::find_if(tag_context->tags.begin(),
                                                              tag_context->tags.end(),
                                                              find_tag_functor(w->key)) :
                                                 tag_context->tags.end();
  stag_t *otag = otagIt != tag_context->tags.end() ? *otagIt : O2G_NULLPTR;
  const char *preset = otag ? otag->value.c_str() : O2G_NULLPTR;

  GtkWidget *widget = w->attach(GTK_TABLE(table), row, preset, context);

  if(widget)
    gtk_widgets[w] = HintPair(widget, otag);
}

struct get_widget_functor {
  bool &changed;
  std::vector<stag_t *> &tags;
  const std::map<const presets_widget_t *, HintPair> &gtk_widgets;
  const std::map<const presets_widget_t *, HintPair>::const_iterator hintEnd;
  get_widget_functor(bool &c, std::vector<stag_t *> &t,
                     const std::map<const presets_widget_t *, HintPair> &g)
    : changed(c), tags(t), gtk_widgets(g), hintEnd(g.end()) {}
  void operator()(const presets_widget_t *w);
};

void get_widget_functor::operator()(const presets_widget_t* w)
{
  const std::map<const presets_widget_t *, HintPair>::const_iterator hint = gtk_widgets.find(w);
  const HintPair &pair = hint != hintEnd ? hint->second : HintPair(O2G_NULLPTR, O2G_NULLPTR);
  stag_t *otag = pair.second;
  const std::vector<stag_t *>::iterator citEnd = tags.end();
  std::vector<stag_t *>::iterator ctag = otag ?
                                         std::find(tags.begin(), citEnd, otag) :
                                         citEnd; // the place to do the change
  const char *text;
  g_assert(!otag == (ctag == citEnd));

  switch(w->type) {
  case WIDGET_TYPE_KEY:
    g_assert(ctag == citEnd);
    ctag = std::find_if(tags.begin(), citEnd, find_tag_functor(w->key));
    // fallthrough
  case WIDGET_TYPE_CHECK:
  case WIDGET_TYPE_COMBO:
  case WIDGET_TYPE_TEXT:
    text = w->getValue(pair.first);
    break;

  case WIDGET_TYPE_REFERENCE: {
    const presets_widget_reference * const r = static_cast<const presets_widget_reference *>(w);
    std::for_each(r->item->widgets.begin(), r->item->widgets.end(), *this);
    return;
  }

  default:
    return;
  }

  changed |= store_value(w, tags, ctag, text);
}

static void presets_item_dialog(presets_context_t *context,
                                const presets_item *item) {
  appdata_t *appdata = context->appdata;
  GtkWindow *parent = GTK_WINDOW(context->tag_context->dialog);

  GtkWidget *dialog = O2G_NULLPTR;
  bool ok = false;

  printf("dialog for item %s\n", item->name.c_str());

  /* build dialog from items widget list */

  /* check for widgets that have an interactive gui element. We won't show a
   * dialog if there's no interactive gui element at all */
  const std::vector<presets_widget_t *>::const_iterator itEnd = item->widgets.end();
  std::vector<presets_widget_t *>::const_iterator it = std::find_if(item->widgets.begin(), itEnd,
                                                                    is_widget_interactive);
  bool has_interactive_widget = (it != itEnd);

  std::map<const presets_widget_t *, HintPair> gtk_widgets;

  if(has_interactive_widget)  {
    dialog =
      misc_dialog_new(MISC_DIALOG_NOSIZE,
                      item->name.c_str(), parent,
		      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		      O2G_NULLPTR);

#ifdef ENABLE_BROWSER_INTERFACE
    /* if a web link has been provided for this item install */
    /* a button for this */
    if(!item->link.empty()) {
      GtkWidget *button = gtk_dialog_add_button(GTK_DIALOG(dialog), _
			("Info"), GTK_RESPONSE_HELP);
      g_object_set_data(G_OBJECT(button), "link", const_cast<char *>(item->link.c_str()));
      g_signal_connect(GTK_OBJECT(button), "clicked",
                       G_CALLBACK(on_info), appdata);
    }
#endif
    /* special handling for the first label/separators */
    if(item->addEditName) {
      gchar *title = g_strdup_printf(_("Edit %s"), item->name.c_str());
      gtk_window_set_title(GTK_WINDOW(dialog), title);
      g_free(title);
    } else {
      // use the first label as title
      const presets_widget_t * const w = item->widgets.front();
      if(w->type == WIDGET_TYPE_LABEL)
        gtk_window_set_title(GTK_WINDOW(dialog), w->text.c_str());
    }

    /* skip all following non-interactive widgets: use the first one that
     * was found to be interactive above. */
    g_assert((*it)->is_interactive());

    /* create table of required size */
    GtkWidget *table = gtk_table_new(std::accumulate(it, item->widgets.end(), 0, widget_rows), 2, FALSE);

    guint row = 0;
    add_widget_functor fc(gtk_widgets, context, table, row);
    std::for_each(it, itEnd, fc);

#ifndef USE_HILDON
    /* add widget to dialog */
    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), table);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 50);
#else
#ifndef FREMANTLE_PANNABLE_AREA
    /* put it into a scrolled window */
    GtkWidget *scroll_win = gtk_scrolled_window_new(O2G_NULLPTR, O2G_NULLPTR);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_win),
				   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll_win),
    					  table);
#else
    gtk_window_set_default_size(GTK_WINDOW(dialog), -1, 500);
    /* put view into a pannable area */
    GtkWidget *scroll_win = hildon_pannable_area_new();
    hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(scroll_win), table);
#endif

    gboolean first = TRUE;
    g_signal_connect(GTK_OBJECT(table), "expose_event",
                     G_CALLBACK(table_expose_event), &first);

    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), scroll_win);
#endif

    gtk_widget_show_all(dialog);

    /* run gtk_dialog_run, but continue if e.g. the help button was pressed */
    int result = -1;
    do
      result = gtk_dialog_run(GTK_DIALOG(dialog));
    while((result != GTK_RESPONSE_DELETE_EVENT) &&
	  (result != GTK_RESPONSE_ACCEPT) &&
	  (result != GTK_RESPONSE_REJECT));

    if(result == GTK_RESPONSE_ACCEPT)
      ok = true;

  } else
    ok = true;

  if(ok) {
    /* handle all children of the table */
    bool changed = false;

    get_widget_functor fc(changed, context->tag_context->tags, gtk_widgets);
    std::for_each(item->widgets.begin(), itEnd, fc);

    if(changed)
      context->tag_context->info_tags_replace();

    std::vector<presets_item_t *> &lru = appdata->presets->lru;
    std::vector<presets_item_t *>::iterator it = std::find(lru.begin(),
                                                           lru.end(), item);
    // if it is already the first item in the list nothing is to do
    if(it == lru.end()) {
      // drop the oldest ones if too many
      if(lru.size() >= LRU_MAX)
        lru.resize(LRU_MAX - 1);
      lru.insert(lru.begin(), const_cast<presets_item *>(item));
    } else if(it != lru.begin()) {
      // move to front
      std::rotate(lru.begin(), it, it + 1);
    }
  }

  if(has_interactive_widget)
    gtk_widget_destroy(dialog);
}

/* ------------------- the item list (popup menu) -------------- */

/**
 * @brief find the first widget that gives a negative match
 */
struct used_preset_functor {
  const std::vector<stag_t *> &tags;
  bool &is_interactive;
  bool &hasPositive;   ///< set if a positive match is found at all
  used_preset_functor(const std::vector<stag_t *> &t, bool &i, bool &m)
    : tags(t), is_interactive(i), hasPositive(m) {}
  bool operator()(const presets_widget_t *w);
};

bool used_preset_functor::operator()(const presets_widget_t* w)
{
  is_interactive |= w->is_interactive();

  int ret = w->matches(tags);
  hasPositive |= (ret > 0);

  return (ret < 0);
}

/**
 * @brief check if the currently active object uses this preset and the preset is interactive
 */
bool presets_item_t::matches(const std::vector<stag_t *> &tags) const
{
  bool is_interactive = false;
  bool hasPositive = false;
  used_preset_functor fc(tags, is_interactive, hasPositive);
  if(isItem()) {
    const presets_item * const item = static_cast<const presets_item *>(this);
    if(std::find_if(item->widgets.begin(), item->widgets.end(), fc) != item->widgets.end())
      return false;
  }

  return hasPositive && is_interactive;
}

#ifndef PICKER_MENU
static void
cb_menu_item(GtkWidget *menu_item, gpointer data) {
  presets_context_t *context = (presets_context_t*)data;

  presets_item *item = static_cast<presets_item *>(g_object_get_data(G_OBJECT(menu_item), "item"));
  g_assert(item);

  presets_item_dialog(context, item);
}

static GtkWidget *create_menuitem(icon_t **icons, const presets_item_named *item)
{
  GtkWidget *menu_item;

  if(item->icon.empty())
    menu_item = gtk_menu_item_new_with_label(item->name.c_str());
  else {
    menu_item = gtk_image_menu_item_new_with_label(item->name.c_str());

    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
                                  icon_widget_load(icons, item->icon, 16));
  }

  return menu_item;
}

struct build_menu_functor {
  presets_context_t * const context;
  GtkWidget * const menu;
  GtkWidget ** const matches;
  bool was_separator;
  bool was_item;
  build_menu_functor(presets_context_t *c, GtkWidget *m, GtkWidget **a)
    : context(c), menu(m), matches(a), was_separator(false), was_item(false) {}
  void operator()(presets_item_t *item);
};

static GtkWidget *build_menu(presets_context_t *context,
			     std::vector<presets_item_t *> &items, GtkWidget **matches) {
  build_menu_functor fc(context, gtk_menu_new(), matches);

  std::for_each(items.begin(), items.end(), fc);

  return fc.menu;
}

void build_menu_functor::operator()(presets_item_t *item)
{
  /* check if this presets entry is appropriate for the current item */
  if(item->type & context->tag_context->presets_type) {
    GtkWidget *menu_item;

    /* Show a separator if one was requested, but not if there was no item
     * before to prevent to show one as the first entry. */
    if(was_item && was_separator)
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    was_item = true;
    was_separator = false;

    menu_item = create_menuitem(&context->appdata->icon,
                                static_cast<presets_item_named *>(item));

    if(item->type & presets_item_t::TY_GROUP) {
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
                                build_menu(context,
                                           static_cast<presets_item_group *>(item)->items,
                                           matches));
    } else {
      g_object_set_data(G_OBJECT(menu_item), "item", item);
      g_signal_connect(menu_item, "activate",
                       GTK_SIGNAL_FUNC(cb_menu_item), context);

      if(matches && item->matches(context->tag_context->tags)) {
        if(!*matches)
          *matches = gtk_menu_new();

        GtkWidget *used_item = create_menuitem(&context->appdata->icon,
                                               static_cast<presets_item_named *>(item));
        g_object_set_data(G_OBJECT(used_item), "item", item);
        g_signal_connect(used_item, "activate",
                         GTK_SIGNAL_FUNC(cb_menu_item), context);
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
  const std::vector<stag_t *> &tags;
  group_member_used(  const std::vector<stag_t *> &t) : tags(t) {}
  bool operator()(const presets_item_t *item);
};

static bool preset_group_is_used(const presets_item_group *item,
                                 const std::vector<stag_t *> &tags)
{
  g_assert(item->type & presets_item_t::TY_GROUP);
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

static GtkWidget *presets_picker(presets_context_t *context,
                                 const std::vector<presets_item_t *> &items,
                                 bool scan_for_recent);
static GtkWidget *preset_picker_recent(presets_context_t *context);
static GtkWidget *preset_picker_lru(presets_context_t *context);

static void remove_sub(presets_item_group *sub_item) {
  if(sub_item->widget) {
    gtk_widget_destroy(sub_item->widget);
    sub_item->widget = O2G_NULLPTR;
  }
}

/**
 * @brief remove all child pickers
 */
static void remove_subs(std::vector<presets_item_group *> &oldsubs,
                        presets_item_group *sub_item) {
  std::vector<presets_item_group *>::iterator it =
             std::find(oldsubs.begin(), oldsubs.end(), sub_item);
  g_assert(it != oldsubs.end());
  std::for_each(++it, oldsubs.end(), remove_sub);
  oldsubs.erase(it, oldsubs.end());
}

static void
on_presets_picker_selected(GtkTreeSelection *selection, gpointer data) {
  presets_context_t *context = (presets_context_t*)data;

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

  if(!gtk_tree_selection_get_selected(selection, &model, &iter))
    return;

  presets_item_named *item = O2G_NULLPTR;
  presets_item_group *sub_item = O2G_NULLPTR;
  const char *text = O2G_NULLPTR;
  gtk_tree_model_get(model, &iter,
                     PRESETS_PICKER_COL_SUBMENU_PTR, &sub_item,
                     PRESETS_PICKER_COL_ITEM_PTR, &item,
                     PRESETS_PICKER_COL_NAME, &text,
                     -1);

  printf("clicked on %s, submenu = %p (%s)\n", text,
         sub_item, sub_item ? sub_item->name.c_str() : "");

  GtkWidget * const view = GTK_WIDGET(gtk_tree_selection_get_tree_view(selection));

  if(item && !(item->type & presets_item_t::TY_GROUP)) {
    /* save item pointer in dialog */
    g_object_set_data(G_OBJECT(gtk_widget_get_toplevel(view)),
                      "item", (gpointer)item);

    /* and request closing of menu */
    gtk_dialog_response(GTK_DIALOG(gtk_widget_get_toplevel(view)),
                        GTK_RESPONSE_ACCEPT);
  } else {
    /* check if this already had a submenu */
    GtkWidget *sub = O2G_NULLPTR;
    if(context->submenus.empty()) {
      // check if dynamic submenu is shown
      sub = GTK_WIDGET(g_object_get_data(G_OBJECT(view), "sub"));
      if(sub) {
        g_object_set_data(G_OBJECT(view), "sub", O2G_NULLPTR);
        gtk_widget_destroy(sub);
      }
    }

    if(sub_item) {
      /* normal submenu */

      // the current list of submenus may or may not have common anchestors with this one
      if(sub_item->widget) {
        // this item is already visible, so it must be in the list, just drop all childs
        remove_subs(context->submenus, sub_item);
      } else {
        // this item is not currently visible
        if(sub_item->parent) {
          // the parent item has to be visible, otherwise this could not have been clicked
          remove_subs(context->submenus, sub_item->parent);
        } else {
          // this is a top level menu, so everything currently shown can be removed
          std::for_each(context->submenus.begin(), context->submenus.end(), remove_sub);
          context->submenus.clear();
        }

        sub = presets_picker(context, sub_item->items, false);
        sub_item->widget = sub;
      }
      context->submenus.push_back(sub_item);
    } else {
      // dynamic submenu
      // this is always on top level, so all old submenu entries can be removed
      std::for_each(context->submenus.begin(), context->submenus.end(), remove_sub);
      context->submenus.clear();
      if (strcmp(text, _("Used presets")) == 0)
        sub = preset_picker_recent(context);
      else
        sub = preset_picker_lru(context);

      g_object_set_data(G_OBJECT(view), "sub", sub);
    }

    /* views parent is a scrolled window whichs parent in turn is the hbox */
    g_assert(sub);
    g_assert(view->parent);
    GtkWidget *hbox = view->parent->parent;

    gtk_box_pack_start_defaults(GTK_BOX(hbox), sub);
    gtk_widget_show_all(sub);
  }
}

static GtkListStore *presets_picker_store(GtkTreeView **view) {
  GtkCellRenderer *renderer;

#ifndef FREMANTLE
  *view = GTK_TREE_VIEW(gtk_tree_view_new());
#else
  *view = GTK_TREE_VIEW(hildon_gtk_tree_view_new(HILDON_UI_MODE_EDIT));
#endif

  gtk_tree_view_set_headers_visible(*view, FALSE);

  /* --- "Icon" column --- */
  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_insert_column_with_attributes(*view,
      -1, "Icon", renderer, "pixbuf", PRESETS_PICKER_COL_ICON, O2G_NULLPTR);

  /* --- "Name" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, O2G_NULLPTR);
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
		 "Name", renderer, "text", PRESETS_PICKER_COL_NAME, O2G_NULLPTR);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(*view, column, -1);

  /* --- "submenu icon" column --- */
  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_insert_column_with_attributes(*view,
      -1, "Submenu Icon", renderer, "pixbuf",
      PRESETS_PICKER_COL_SUBMENU_ICON, O2G_NULLPTR);

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
  g_signal_connect (G_OBJECT (select), "changed",
		    G_CALLBACK (on_presets_picker_selected), context);

  gtk_tree_selection_unselect_all(select);

  /* put this inside a scrolled view */
  GtkWidget *c;
#ifndef FREMANTLE_PANNABLE_AREA
  c = gtk_scrolled_window_new (O2G_NULLPTR, O2G_NULLPTR);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(c),
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
#else
  c = hildon_pannable_area_new();
#endif
  gtk_container_add(GTK_CONTAINER(c), GTK_WIDGET(view));
  return c;
}

static GtkTreeIter preset_insert_item(const presets_item_named *item, icon_t **icons,
                                      GtkListStore *store) {
  /* icon load can cope with empty string as name (returns O2G_NULLPTR then) */
  GdkPixbuf *icon = icon_load(icons, item->icon, 16);

  /* Append a row and fill in some data */
  GtkTreeIter iter;
  gtk_list_store_append(store, &iter);

  gtk_list_store_set(store, &iter,
		     PRESETS_PICKER_COL_ICON, icon,
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
                       &context->appdata->icon, store);
}

/**
 * @brief match any member in the list that has a matching type
 */
template<> void insert_recent_items<true>::operator()(const presets_item_t *preset)
{
  if(preset->type & context->tag_context->presets_type)
    preset_insert_item(static_cast<const presets_item_named *>(preset),
                       &context->appdata->icon, store);
}

static GtkWidget *preset_picker_recent(presets_context_t *context) {
  GtkTreeView *view;
  insert_recent_items<false> fc(context, presets_picker_store(&view));

  std::for_each(context->appdata->presets->items.begin(), context->appdata->presets->items.end(), fc);

  return presets_picker_embed(view, fc.store, context);
}

static GtkWidget *preset_picker_lru(presets_context_t *context) {
  GtkTreeView *view;
  insert_recent_items<true> fc(context, presets_picker_store(&view));

  std::for_each(context->appdata->presets->lru.begin(), context->appdata->presets->lru.end(), fc);

  return presets_picker_embed(view, fc.store, context);
}

struct picker_add_functor {
  presets_context_t * const context;
  GtkListStore * const store;
  GdkPixbuf * const subicon;
  bool &show_recent;
  bool scan_for_recent;
  picker_add_functor(presets_context_t *c, GtkListStore *s, GdkPixbuf *i, bool r, bool &w)
    : context(c), store(s), subicon(i), show_recent(w), scan_for_recent(r) {}
  void operator()(const presets_item_t *item);
};

void picker_add_functor::operator()(const presets_item_t *item)
{
  /* check if this presets entry is appropriate for the current item */
  if(!(item->type & context->tag_context->presets_type))
    return;

  const presets_item_named * const itemv = static_cast<typeof(itemv)>(item);

  if(itemv->name.empty())
    return;

  GtkTreeIter iter = preset_insert_item(itemv, &context->appdata->icon, store);

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
  matching_type_functor(unsigned int t) : type(t) {}
  bool operator()(const presets_item_t *item) {
    return item->type & type;
  }
};

/**
 * @brief create a picker list for preset items
 * @param context the tag context
 * @param items the list of presets to show
 * @param scan_for_recent if a "Used Presets" subentry should be created
 *
 * This will create a view with the given items. This is just one column in the
 * presets view, every submenu will get it's own view created by a call to this
 * function.
 */
static GtkWidget *
presets_picker(presets_context_t *context, const std::vector<presets_item_t *> &items,
                bool top_level) {
  GtkTreeView *view;
  GtkListStore *store = presets_picker_store(&view);

  bool show_recent = false;
  GdkPixbuf *subicon = icon_load(&context->appdata->icon,
                                 "submenu_arrow");
  picker_add_functor fc(context, store, subicon, top_level, show_recent);

  std::for_each(items.begin(), items.end(), fc);
  const std::vector<presets_item_t *> &lru = context->appdata->presets->lru;

  if(top_level &&
     std::find_if(lru.begin(), lru.end(),
                  matching_type_functor(context->tag_context->presets_type)) != lru.end()) {
    GtkTreeIter     iter;

    /* Append a row and fill in some data */
    gtk_list_store_prepend(store, &iter);
    gtk_list_store_set(store, &iter,
                       PRESETS_PICKER_COL_NAME, _("Last used presets"),
                       PRESETS_PICKER_COL_SUBMENU_ICON, subicon,
		       -1);
  }
  if(show_recent) {
    GtkTreeIter     iter;

    /* Append a row and fill in some data */
    gtk_list_store_prepend(store, &iter);
    gtk_list_store_set(store, &iter,
		       PRESETS_PICKER_COL_NAME, _("Used presets"),
		       PRESETS_PICKER_COL_SUBMENU_ICON, subicon,
		       -1);
  }

  icon_free(&context->appdata->icon, subicon);

  return presets_picker_embed(view, store, context);
}

static inline void remove_sub_reference(presets_item_group *sub_item) {
  sub_item->widget = O2G_NULLPTR;
}
#endif

static gint button_press(GtkWidget *widget, GdkEventButton *event,
			 gpointer data) {
  presets_context_t *context = (presets_context_t*)data;

  if(event->type != GDK_BUTTON_PRESS)
    return FALSE;

  printf("button press %d %d\n", event->button, event->time);

#ifndef PICKER_MENU
  (void)widget;

  if (!context->menu) {
    GtkWidget *matches = O2G_NULLPTR;
    context->menu = build_menu(context, context->appdata->presets->items, &matches);
    if(!context->appdata->presets->lru.empty()) {
      // This will not update the menu while the dialog is open. Not worth the effort.
      GtkWidget *menu_item = gtk_menu_item_new_with_label(_("Last used presets"));
      GtkWidget *lrumenu = build_menu(context, context->appdata->presets->lru, NULL);

      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), lrumenu);
      gtk_menu_shell_prepend(GTK_MENU_SHELL(context->menu), gtk_separator_menu_item_new());
      gtk_menu_shell_prepend(GTK_MENU_SHELL(context->menu), menu_item);
    }
    if(matches) {
      GtkWidget *menu_item = gtk_menu_item_new_with_label(_("Used presets"));

      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), matches);
      gtk_menu_shell_prepend(GTK_MENU_SHELL(context->menu), gtk_separator_menu_item_new());
      gtk_menu_shell_prepend(GTK_MENU_SHELL(context->menu), menu_item);
    }
  }
  gtk_widget_show_all( GTK_WIDGET(context->menu) );

  gtk_menu_popup(GTK_MENU(context->menu), O2G_NULLPTR, O2G_NULLPTR, O2G_NULLPTR, O2G_NULLPTR,
                 event->button, event->time);
#else
  g_assert(context->submenus.empty());
  /* popup our special picker like menu */
  GtkWidget *dialog =
      gtk_dialog_new_with_buttons(_("Presets"),
		  GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(widget))),
				  GTK_DIALOG_MODAL,
          GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
	  O2G_NULLPTR);

  gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 480);

  /* create root picker */
  GtkWidget *hbox = gtk_hbox_new(TRUE, 0);

  GtkWidget *root = presets_picker(context, context->appdata->presets->items, true);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), root);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox);

  gtk_widget_show_all(dialog);
  presets_item *item = O2G_NULLPTR;
  if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    item = static_cast<presets_item *>(g_object_get_data(G_OBJECT(dialog), "item"));

  gtk_widget_destroy(dialog);

  // remove all references to the already destroyed widgets
  std::for_each(context->submenus.begin(), context->submenus.end(), remove_sub_reference);
  context->submenus.clear();

  if(item)
    presets_item_dialog(context, item);
#endif

  /* Tell calling code that we have handled this event; the buck
   * stops here. */
  return TRUE;
}

static gint on_button_destroy(gpointer data) {
  presets_context_t *context = (presets_context_t*)data;

#ifndef FREMANTLE
  printf("freeing preset button context\n");
  if (context->menu)
    gtk_widget_destroy(context->menu);
#endif

  delete context;

  return FALSE;
}

GtkWidget *josm_build_presets_button(appdata_t *appdata,
			       tag_context_t *tag_context) {
  presets_context_t *context = new presets_context_t(appdata, tag_context);

  GtkWidget *but = button_new_with_label(_("Presets"));
  gtk_widget_set_events(but, GDK_EXPOSURE_MASK);
  gtk_widget_add_events(but, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(GTK_OBJECT(but), "button-press-event",
                   G_CALLBACK(button_press), context);

  g_signal_connect_swapped(GTK_OBJECT(but), "destroy",
                           G_CALLBACK(on_button_destroy), context);

  return but;
}

GtkWidget *presets_widget_t::attach(GtkTable *, guint &, const char *,
                                    presets_context_t *) const
{
  return O2G_NULLPTR;
}

const char *presets_widget_t::getValue(GtkWidget *) const
{
  g_assert_not_reached();
  return O2G_NULLPTR;
}

int presets_widget_t::matches(const std::vector<stag_t *> &tags) const
{
  if(match == MatchIgnore)
    return 0;

  const std::vector<stag_t *>::const_iterator itEnd = tags.end();
  const std::vector<stag_t *>::const_iterator it = std::find_if(tags.begin(),
                                                                itEnd,
                                                                find_tag_functor(key));

  if(it == itEnd) {
    switch(match) {
    case MatchKey:
    case MatchKeyValue:
      return 0;
    default:
      return -1;
    }
  }

  if(match == MatchKey || match == MatchKey_Force)
    return 1;

  if(matchValue((*it)->value.c_str()))
    return 1;

  return match == MatchKeyValue_Force ? -1 : 0;
}

GtkWidget *presets_widget_text::attach(GtkTable *table, guint &row, const char *preset,
                                       presets_context_t *) const
{
  if(!preset)
    preset = def.c_str();
  GtkWidget *ret = entry_new();
  if(preset)
    gtk_entry_set_text(GTK_ENTRY(ret), preset);

  attach_right(table, text.c_str(), ret, row);

  return ret;
}

const char *presets_widget_text::getValue(GtkWidget *widget) const
{
  g_assert(GTK_WIDGET_TYPE(widget) == entry_type());

  return gtk_entry_get_text(GTK_ENTRY(widget));
}

GtkWidget *presets_widget_separator::attach(GtkTable *table, guint &row, const char *,
                                            presets_context_t *) const
{
  attach_both(table, gtk_hseparator_new(), row);
  return O2G_NULLPTR;
}

GtkWidget *presets_widget_label::attach(GtkTable *table, guint &row, const char *,
                                        presets_context_t *) const
{
  attach_both(table, gtk_label_new(text.c_str()), row);
  return O2G_NULLPTR;
}

bool presets_widget_combo::matchValue(const char *val) const
{
  return std::find(values.begin(), values.end(), val) != values.end();
}

GtkWidget *presets_widget_combo::attach(GtkTable *table, guint &row, const char *preset,
                                        presets_context_t *) const
{
  if(!preset)
    preset = def.c_str();
  GtkWidget *ret = combo_box_new(text.c_str());
  combo_box_append_text(ret, _("<unset>"));
  int active = 0;

  const std::vector<std::string> &d = display_values.empty() ? values : display_values;

  for(std::vector<std::string>::size_type count = 0; count < d.size(); count++) {
    const std::string &value = d[count].empty() ? values[count] : d[count];

    combo_box_append_text(ret, value.c_str());

    if(preset && values[count] == preset) {
      active = count + 1;
      preset = O2G_NULLPTR;
    }
  }

  combo_box_set_active(ret, active);
#ifndef FREMANTLE
  attach_right(table, text.c_str(), ret, row);
#else
  attach_both(table, ret, row);
#endif

  return ret;
}

const char *presets_widget_combo::getValue(GtkWidget* widget) const
{
  g_assert(GTK_WIDGET_TYPE(widget) == combo_box_type());

  const char *text = combo_box_get_active_text(widget);

  if(!strcmp(text, _("<unset>")))
    return O2G_NULLPTR;

  if(display_values.empty())
    return text;

  // map back from display string to value string
  const std::vector<std::string>::const_iterator it = std::find(display_values.begin(),
                                                                display_values.end(),
                                                                text);
  g_assert(it != display_values.end());

  // get the value corresponding to the displayed string
  return values[it - display_values.begin()].c_str();
}

bool presets_widget_key::matchValue(const char *val) const
{
  return (value == val);
}

const char *presets_widget_key::getValue(GtkWidget *widget) const
{
  g_assert(!widget);
  return value.c_str();
}

bool presets_widget_checkbox::matchValue(const char *val) const
{
  if(!value_on.empty())
    return (value_on == val);

  return ((strcasecmp(val, "true") == 0) ||
          (strcasecmp(val, "yes") == 0));
}

GtkWidget *presets_widget_checkbox::attach(GtkTable *table, guint &row, const char *preset,
                                           presets_context_t *) const
{
  gboolean deflt = FALSE;
  if(preset)
    deflt = matchValue(preset) ? TRUE : FALSE;
  else
    deflt = def;

  GtkWidget *ret = check_button_new_with_label(text.c_str());
  check_button_set_active(ret, deflt);
#ifndef FREMANTLE
  attach_right(table, O2G_NULLPTR, ret, row);
#else
  attach_both(table, ret, row);
#endif

  return ret;
}

const char *presets_widget_checkbox::getValue(GtkWidget *widget) const
{
  g_assert(GTK_WIDGET_TYPE(widget) == check_button_type());

  return check_button_get_active(widget) ?
         (value_on.empty() ? "yes" : value_on.c_str()) : O2G_NULLPTR;
}

static void item_link_clicked(GtkButton *button, gpointer data) {
  presets_item * const item = static_cast<presets_item *>(data);
  presets_context_t * const context = static_cast<presets_context_t *>(
                                          g_object_get_data(G_OBJECT(button),
                                                            "presets_context"));

  presets_item_dialog(context, item);
}

GtkWidget *presets_widget_link::attach(GtkTable *table, guint &row, const char *,
                                       presets_context_t *context) const
{
  gchar *label = g_strdup_printf(_("[Preset] %s"), item->name.c_str());
  GtkWidget *button = button_new_with_label(label);
  g_free(label);
  g_object_set_data(G_OBJECT(button), "presets_context", context);
  GtkWidget *img = icon_widget_load(&context->appdata->icon, item->icon, 16);
  if(img) {
    gtk_button_set_image(GTK_BUTTON(button), img);
    // make sure the image is always shown, Hildon seems to hide it by default
    gtk_widget_show(img);
  }
  g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(item_link_clicked), item);
  attach_both(table, button, row);
  return O2G_NULLPTR;
}

// vim:et:ts=8:sw=2:sts=2:ai
