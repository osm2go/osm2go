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

#ifndef MISC_H
#define MISC_H

#define MISC_AGAIN_ID_DELETE           (1<<0)
#define MISC_AGAIN_ID_JOIN_NODES       (1<<1)
#define MISC_AGAIN_ID_JOIN_WAYS        (1<<2)
#define MISC_AGAIN_ID_OVERWRITE_TAGS   (1<<3)
#define MISC_AGAIN_ID_EXTEND_WAY       (1<<4)
#define MISC_AGAIN_ID_EXTEND_WAY_END   (1<<5)
#define MISC_AGAIN_ID_EXPORT_OVERWRITE (1<<6)

typedef struct file_chain_s {
  char *name;
  struct file_chain_s *next;
} file_chain_t;

/* these flags prevent you from leaving the dialog with no (or yes) */
/* if the "dont show me this dialog again" checkbox is selected. This */
/* makes sure, that you can't permanently switch certain things in, but */
/* only on. e.g. it doesn't make sense to answer a "do you really want to */
/* delete this" dialog with "no and don't ask me again". You'd never be */
/* able to delete anything again */
#define MISC_AGAIN_FLAG_DONT_SAVE_NO  (1<<0)
#define MISC_AGAIN_FLAG_DONT_SAVE_YES (1<<1)

void errorf(GtkWidget *parent, const char *fmt, ...);
void warningf(GtkWidget *parent, const char *fmt, ...);
void messagef(GtkWidget *parent, char *title, const char *fmt, ...);
gboolean yes_no_f(GtkWidget *parent, 
		  appdata_t *appdata, gulong again_bit, gint flags,
		  char *title, const char *fmt, ...);

char *find_file(char *name);
file_chain_t *file_scan(char *pattern);

/* dialog size are specified rather fuzzy */
#define MISC_DIALOG_NOSIZE  -1
#define MISC_DIALOG_SMALL    0
#define MISC_DIALOG_MEDIUM   1
#define MISC_DIALOG_LARGE    2
#define MISC_DIALOG_WIDE     3
#define MISC_DIALOG_HIGH     4

GtkWidget *misc_dialog_new(guint hint, const char *title, GtkWindow *parent, ...);
GtkWidget *misc_scrolled_window_new(gboolean etched_in);
void misc_scrolled_window_add_with_viewport(GtkWidget *win, GtkWidget *child);
const char *misc_get_proxy_uri(settings_t *settings);
void misc_table_attach(GtkWidget *table, GtkWidget *widget, int x, int y);
#endif // MISC_H
