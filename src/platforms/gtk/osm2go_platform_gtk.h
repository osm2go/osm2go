/*
 * SPDX-FileCopyrightText: 2017-2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <memory>
#include <string>
#include <vector>

#include "osm2go_i18n.h"

typedef struct _GdkColor GdkColor;
class icon_item;
class presets_items;
class statusbar_t;
class tag_context_t;

namespace osm2go_platform {
  bool init(bool &startGps) __attribute__((warn_unused_result));

  void cleanup();

  GtkWidget *notebook_new(void) __attribute__((warn_unused_result));
  void notebook_append_page(GtkWidget *notebook, GtkWidget *page, trstring::native_type_arg label);
  GtkNotebook *notebook_get_gtk_notebook(GtkWidget *notebook) __attribute__((warn_unused_result));

  GtkTreeView *tree_view_new() __attribute__((warn_unused_result));
  GtkWidget *scrollable_container(GtkWidget *view, bool shadowed = true) __attribute__((warn_unused_result));

  /* unified widgets */
  enum EntryFlags {
    EntryFlagsDefault,
    EntryFlagsNoAutoCap
  };
  GtkWidget *entry_new(EntryFlags flags = EntryFlagsDefault) __attribute__((warn_unused_result));
  bool isEntryWidget(GtkWidget *widget) __attribute__((warn_unused_result));

  GtkWidget *button_new_with_label(trstring::arg_type label) __attribute__((warn_unused_result));

  GtkWidget *check_button_new_with_label(const char *label) __attribute__((warn_unused_result));
  void check_button_set_active(GtkWidget *button, bool active);
  bool check_button_get_active(GtkWidget *button) __attribute__((warn_unused_result));
  bool isCheckButtonWidget(GtkWidget *widget) __attribute__((warn_unused_result));

  /**
   * @brief create a new combo box
   * @param title the title string (only used on Fremantle)
   * @param items the texts to fill
   * @param active the item to preselect or -1 for none
   */
  GtkWidget *combo_box_new(trstring::native_type_arg title, const std::vector<trstring::native_type> &items = std::vector<trstring::native_type>(), int active = -1) __attribute__((warn_unused_result));
  GtkWidget *combo_box_entry_new(const char *title) __attribute__((warn_unused_result));

  void combo_box_append_text(GtkWidget *cbox, const char *text);
  void combo_box_set_active(GtkWidget *cbox, int index);
  int combo_box_get_active(GtkWidget *cbox) __attribute__((warn_unused_result));
  std::string combo_box_get_active_text(GtkWidget *cbox) __attribute__((warn_unused_result));
  void combo_box_set_active_text(GtkWidget *cbox, const char *text);
  bool isComboBoxWidget(GtkWidget *widget) __attribute__((warn_unused_result));
  bool isComboBoxEntryWidget(GtkWidget *widget) __attribute__((warn_unused_result));

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
  GtkWidget *select_widget(GtkTreeModel *model, unsigned int flags = NoSelectionFlags, char delimiter = ';') __attribute__((nonnull(1))) __attribute__((warn_unused_result));

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
   * The widget takes a reference on the model.
   *
   * This puts the select_widget() behind a picker button on Fremantle. It just
   * returns the select_widget() on desktop systems.
   */
  GtkWidget *select_widget_wrapped(const char *title, GtkTreeModel *model, unsigned int flags = NoSelectionFlags, char delimiter = ';') __attribute__((nonnull(1, 2))) __attribute__((warn_unused_result));

  /**
   * @brief return the value selected with the select widget
   * @param widget the widget returned by select_widget_wrapped()
   */
  std::string select_widget_value(GtkWidget *widget) __attribute__((warn_unused_result));

  /**
   * @brief check if the given widget has a selection
   * @param widget the widget returned by select_widget()
   *
   * This only makes sense for widgets created with AllowMultiSelection.
   */
  bool select_widget_has_selection(GtkWidget *widget) __attribute__((warn_unused_result));

  /**
   * @brief select one or more items
   */
  void select_widget_select(GtkWidget *widget, const std::vector<unsigned int> &indexes);

  void setEntryText(GtkEntry *entry, const char *text, trstring::native_type_arg placeholder);

  /* dialog size are specified rather fuzzy */
  enum DialogSizeHint {
    MISC_DIALOG_SMALL  =  0,
    MISC_DIALOG_MEDIUM =  1,
    MISC_DIALOG_LARGE  =  2,
    MISC_DIALOG_WIDE   =  3,
    MISC_DIALOG_HIGH   =  4,
    _MISC_DIALOG_SIZEHINT_COUNT
  };

  void dialog_size_hint(GtkWindow *window, DialogSizeHint hint);

  /**
   * @brief returns the color to highlight invalid values (i.e. red)
   */
  const GdkColor *invalid_text_color() __attribute__((pure)) __attribute__((warn_unused_result));

  /**
   * @brief returns the widget that contains the statusbar
   *
   * This widget will be added to the main window.
   */
  GtkWidget *statusBarWidget(statusbar_t *statusbar) __attribute__((warn_unused_result));

  class Timer {
    guint id;
  public:
    explicit inline Timer() noexcept
      : id(0) {}
    inline ~Timer()
    { stop(); }

    void restart(unsigned int seconds, GSourceFunc callback, void *data);
    void stop();

    inline bool __attribute__((warn_unused_result)) isActive() const noexcept
    { return id != 0; }
  };

  GdkPixbuf *icon_pixmap(const icon_item *icon) __attribute__((warn_unused_result));

  void josm_build_presets_button(GtkWidget *button, presets_items *presets, tag_context_t *tag_context);

#ifdef FINGER_UI
  void iconbar_register_buttons(appdata_t &appdata, GtkWidget *ok, GtkWidget *cancel);
#endif
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

static inline GtkWidget * __attribute__((warn_unused_result)) gtk_label_new(trstring::native_type_arg str)
{ return gtk_label_new(static_cast<const gchar *>(str)); }

static inline void gtk_label_set_text(GtkLabel *label, trstring::native_type_arg str)
{ return gtk_label_set_text(label, static_cast<const gchar *>(str)); }

// all versions need to be provided here, otherwise the missing "explict" keyword on
// operators will make the calls ambiguous with gcc 4.2
static inline void gtk_window_set_title(GtkWindow *window, trstring::native_type title)
{ return gtk_window_set_title(window, static_cast<const gchar *>(title)); }
static inline void gtk_window_set_title(GtkWindow *window, const trstring &title)
{ return gtk_window_set_title(window, static_cast<const gchar *>(title)); }
static inline void gtk_window_set_title(GtkWindow *window, trstring::arg_type title)
{ return gtk_window_set_title(window, static_cast<trstring::native_type>(title)); }

static inline GtkWidget * __attribute__((warn_unused_result)) gtk_dialog_add_button(GtkDialog *dialog, trstring::native_type_arg button_text, gint response_id)
{ return gtk_dialog_add_button(dialog, static_cast<const gchar *>(button_text), response_id); }
