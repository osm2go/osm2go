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

#ifndef MISC_H
#define MISC_H

#include "fdguard.h"

#include <glib.h>
#include <gtk/gtk.h>

enum {
  MISC_AGAIN_ID_DELETE           = (1<<0),
  MISC_AGAIN_ID_JOIN_NODES       = (1<<1),
  MISC_AGAIN_ID_JOIN_WAYS        = (1<<2),
  MISC_AGAIN_ID_OVERWRITE_TAGS   = (1<<3),
  MISC_AGAIN_ID_EXTEND_WAY       = (1<<4),
  MISC_AGAIN_ID_EXTEND_WAY_END   = (1<<5),
  MISC_AGAIN_ID_EXPORT_OVERWRITE = (1<<6),
  MISC_AGAIN_ID_AREA_TOO_BIG     = (1<<7),

  /* these flags prevent you from leaving the dialog with no (or yes) */
  /* if the "dont show me this dialog again" checkbox is selected. This */
  /* makes sure, that you can't permanently switch certain things in, but */
  /* only on. e.g. it doesn't make sense to answer a "do you really want to */
  /* delete this" dialog with "no and don't ask me again". You'd never be */
  /* able to delete anything again */
  MISC_AGAIN_FLAG_DONT_SAVE_NO   = (1<<30),
  MISC_AGAIN_FLAG_DONT_SAVE_YES  = (1<<31)
};

#include <osm2go_cpp.h>

#include <memory>
#include <string>
#include <vector>

#if __cplusplus < 201103L
#include <osm2go_stl.h>
#endif

struct datapath {
#if __cplusplus >= 201103L
  explicit inline datapath(fdguard &&f)  : fd(std::move(f)) {}
#else
  explicit inline datapath(fdguard &f)  : fd(f) {}
#endif
  fdguard fd;
  std::string pathname;
};

const std::vector<datapath> &base_paths();

std::string find_file(const std::string &n);

/* some compat code */

GtkWidget *button_new_with_label(const char *label);

void errorf(GtkWidget *parent, const char *fmt, ...) __attribute__((format (printf, 2, 3)));
void warningf(GtkWidget *parent, const char *fmt, ...) __attribute__((format (printf, 2, 3)));
void messagef(GtkWidget *parent, const char *title, const char *fmt, ...) __attribute__((format (printf, 3, 4)));

/* dialog size are specified rather fuzzy */
enum DialogSizeHint {
  MISC_DIALOG_SMALL  =  0,
  MISC_DIALOG_MEDIUM =  1,
  MISC_DIALOG_LARGE  =  2,
  MISC_DIALOG_WIDE   =  3,
  MISC_DIALOG_HIGH   =  4
};

void dialog_size_hint(GtkWindow *window, DialogSizeHint hint);

/* unified widgets */
enum EntryFlags {
  EntryFlagsDefault,
  EntryFlagsNoAutoCap
};
GtkWidget *entry_new(EntryFlags flags = EntryFlagsDefault);
bool isEntryWidget(GtkWidget *widget);

bool yes_no_f(GtkWidget *parent, unsigned int again_flags,
              const char *title, const char *fmt, ...) __attribute__((format (printf, 4, 5)));
GtkWidget *check_button_new_with_label(const char *label);
void check_button_set_active(GtkWidget *button, bool active);
bool check_button_get_active(GtkWidget *button);
bool isCheckButtonWidget(GtkWidget *widget);

GtkWidget *notebook_new(void);
void notebook_append_page(GtkWidget *notebook, GtkWidget *page, const char *label);
GtkNotebook *notebook_get_gtk_notebook(GtkWidget *notebook);

GtkWidget *combo_box_new(const char *title);
GtkWidget *combo_box_entry_new(const char *title);
void combo_box_append_text(GtkWidget *cbox, const char *text);
void combo_box_set_active(GtkWidget *cbox, int index);
int combo_box_get_active(GtkWidget *cbox);
std::string combo_box_get_active_text(GtkWidget *cbox);
bool isComboBoxWidget(GtkWidget *widget);
bool isComboBoxEntryWidget(GtkWidget *widget);

GtkWidget *string_select_widget(const char *title, const std::vector<std::string> &entries, int match);

// simplified form of unique_ptr
struct g_deleter {
  inline void operator()(gpointer mem) {
    g_free(mem);
  }
};

typedef std::unique_ptr<gchar, g_deleter> g_string;

struct gtk_widget_deleter {
  inline void operator()(GtkWidget *mem) {
    gtk_widget_destroy(mem);
  }
};

typedef std::unique_ptr<GtkWidget, gtk_widget_deleter> g_widget;

struct g_mapped_file_deleter {
  inline void operator()(GMappedFile *map) {
#if GLIB_CHECK_VERSION(2,22,0)
    g_mapped_file_unref(map);
#else
    g_mapped_file_free(map);
#endif
  }
};

typedef std::unique_ptr<GMappedFile, g_mapped_file_deleter> g_mapped_file;

struct g_object_deleter {
  inline void operator()(gpointer obj) {
    g_object_unref(obj);
  }
};

#endif // MISC_H
