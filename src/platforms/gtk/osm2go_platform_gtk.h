/*
 * Copyright (C) 2017 Rolf Eike Beer <eike@sf-mail.de>.
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

#ifndef OSM2GO_PLATFORM_GTK_H
#define OSM2GO_PLATFORM_GTK_H

#include <gtk/gtk.h>
#include <memory>
#include <string>
#include <vector>

typedef struct _GdkColor GdkColor;
class statusbar_t;

namespace osm2go_platform {
  bool init();

  void cleanup();

  GtkWidget *notebook_new(void);
  void notebook_append_page(GtkWidget *notebook, GtkWidget *page, const char *label);
  GtkNotebook *notebook_get_gtk_notebook(GtkWidget *notebook);

  GtkTreeView *tree_view_new();
  GtkWidget *scrollable_container(GtkWidget *view, bool shadowed = true);

  /* unified widgets */
  enum EntryFlags {
    EntryFlagsDefault,
    EntryFlagsNoAutoCap
  };
  GtkWidget *entry_new(EntryFlags flags = EntryFlagsDefault);
  bool isEntryWidget(GtkWidget *widget);

  GtkWidget *button_new_with_label(const char *label);

  GtkWidget *check_button_new_with_label(const char *label);
  void check_button_set_active(GtkWidget *button, bool active);
  bool check_button_get_active(GtkWidget *button);
  bool isCheckButtonWidget(GtkWidget *widget);

  /**
   * @brief create a new combo box
   * @param title the title string (only used on Fremantle)
   * @param items the texts to fill
   * @param active the item to preselect or -1 for none
   */
  GtkWidget *combo_box_new(const char *title, const std::vector<const char *> &items = std::vector<const char *>(), int active = -1);
  GtkWidget *combo_box_entry_new(const char *title);

  void combo_box_append_text(GtkWidget *cbox, const char *text);
  void combo_box_set_active(GtkWidget *cbox, int index);
  int combo_box_get_active(GtkWidget *cbox);
  std::string combo_box_get_active_text(GtkWidget *cbox);
  void combo_box_set_active_text(GtkWidget *cbox, const char *text);
  bool isComboBoxWidget(GtkWidget *widget);
  bool isComboBoxEntryWidget(GtkWidget *widget);

  enum SelectionFlags {
    NoSelectionFlags    = 0,
    AllowEditing        = (1<<1),  ///< if the user may enter custom text
    AllowMultiSelection = (1<<2)
  };

  /**
   * @brief create a widget that let's the user do a selection
   * @param model the model containing the values
   * @param flags flags which type of widget is created
   * @param delimiter the delimiter to use when joining multiple values
   *
   * @model should have 2 columns, the first containing the text that is
   * displayed, the second with the values that are returned. Both must be
   * of type G_TYPE_STRING.
   *
   * The widget takes a reference on the model. Only the first character of delimiter
   * is used, so it may be a pointer to a single char.
   */
  GtkWidget *select_widget(GtkTreeModel *model, unsigned int flags = NoSelectionFlags, const char *delimiter = ";") __attribute__((nonnull(1)));

  /**
   * @brief create a widget that let's the user do a selection
   * @param title the dialog title on Fremantle
   * @param model the model containing the values
   * @param flags flags which type of widget is created
   * @param delimiter the delimiter to use when joining multiple values
   *
   * @model should have 2 columns, the first containing the text that is
   * displayed, the second with the values that are returned. Both must be
   * of type G_TYPE_STRING.
   *
   * The widget takes a reference on the model. Only the first character of delimiter
   * is used, so it may be a pointer to a single char.
   *
   * This puts the select_widget() behind a picker button on Fremantle. It just
   * returns the select_widget() on desktop systems.
   */
  GtkWidget *select_widget_wrapped(const char *title, GtkTreeModel *model, unsigned int flags = NoSelectionFlags, const char *delimiter = ";") __attribute__((nonnull(1, 2)));

  /**
   * @brief return the value selected with the select widget
   * @param widget the widget returned by select_widget_wrapped()
   */
  std::string select_widget_value(GtkWidget *widget);

  /**
   * @brief check if the given widget has a selection
   * @param widget the widget returned by select_widget()
   *
   * This only makes sense for widgets created with AllowMultiSelection.
   */
  bool select_widget_has_selection(GtkWidget *widget);

  /**
   * @brief select one or more items
   */
  void select_widget_select(GtkWidget *widget, const std::vector<unsigned int> &indexes);

  void setEntryText(GtkEntry *entry, const char *text, const char *placeholder);

  /* dialog size are specified rather fuzzy */
  enum DialogSizeHint {
    MISC_DIALOG_SMALL  =  0,
    MISC_DIALOG_MEDIUM =  1,
    MISC_DIALOG_LARGE  =  2,
    MISC_DIALOG_WIDE   =  3,
    MISC_DIALOG_HIGH   =  4
  };

  void dialog_size_hint(GtkWindow *window, DialogSizeHint hint);

  /**
   * @brief returns the color to highlight invalid values (i.e. red)
   */
  const GdkColor *invalid_text_color() __attribute__((pure));

  /**
   * @brief returns the widget that contains the statusbar
   *
   * This widget will be added to the main window.
   */
  GtkWidget *statusBarWidget(statusbar_t *statusbar);

  class Timer {
    guint id;
  public:
    explicit inline Timer() noexcept
      : id(0) {}
    inline ~Timer()
    { stop(); }

    void restart(unsigned int seconds, GSourceFunc callback, void *data);
    void stop();

    inline bool isActive() const noexcept
    { return id != 0; }
  };
};

// simplified form of unique_ptr
struct g_deleter {
  inline void operator()(gpointer mem) {
    g_free(mem);
  }
};

typedef std::unique_ptr<gchar, g_deleter> g_string;

struct g_object_deleter {
  inline void operator()(gpointer obj) {
    g_object_unref(obj);
  }
};

#endif // OSM2GO_PLATFORM_GTK_H
