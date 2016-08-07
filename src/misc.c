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
#include "misc.h"
#include "settings.h"

#ifdef FREMANTLE
#include <hildon/hildon-check-button.h>
#include <hildon/hildon-picker-button.h>
#include <hildon/hildon-entry.h>
#include <hildon/hildon-touch-selector-entry.h>
#include <hildon/hildon-note.h>
#endif

static void vmessagef(GtkWidget *parent, int type, int buttons,
		      char *title, const char *fmt,
		      va_list args) {

  char *buf = g_strdup_vprintf(fmt, args);

#if !defined(USE_HILDON) || (MAEMO_VERSION_MAJOR < 5)
  GtkWidget *dialog = gtk_message_dialog_new(
			   GTK_WINDOW(parent),
			   GTK_DIALOG_DESTROY_WITH_PARENT,
			   type, buttons, "%s", buf);

  gtk_window_set_title(GTK_WINDOW(dialog), title);
#else
  GtkWidget *dialog =
    hildon_note_new_information(GTK_WINDOW(parent), buf);
#endif

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);

  g_free(buf);
}

void messagef(GtkWidget *parent, char *title, const char *fmt, ...) {
  va_list args;
  va_start( args, fmt );
  vmessagef(parent, GTK_MESSAGE_INFO,
	    GTK_BUTTONS_OK, title, fmt, args);
  va_end( args );
}

void errorf(GtkWidget *parent, const char *fmt, ...) {
  va_list args;
  va_start( args, fmt );

  vmessagef(parent, GTK_MESSAGE_ERROR,
	    GTK_BUTTONS_CLOSE, _("Error"), fmt, args);
  va_end( args );
}

void warningf(GtkWidget *parent, const char *fmt, ...) {
  va_list args;
  va_start( args, fmt );
  vmessagef(parent, GTK_MESSAGE_WARNING,
	    GTK_BUTTONS_CLOSE, _("Warning"), fmt, args);
  va_end( args );
}

#ifndef FREMANTLE
#define RESPONSE_YES  GTK_RESPONSE_YES
#define RESPONSE_NO   GTK_RESPONSE_NO
#else
/* hildon names the yes/no buttons ok/cancel ??? */
#define RESPONSE_YES  GTK_RESPONSE_OK
#define RESPONSE_NO   GTK_RESPONSE_CANCEL
#endif

static void on_toggled(GtkWidget *button, gpointer data) {
  gboolean active = check_button_get_active(button);

  GtkWidget *dialog = gtk_widget_get_toplevel(button);

  if(*(gint*)data & MISC_AGAIN_FLAG_DONT_SAVE_NO)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
				      RESPONSE_NO, !active);

  if(*(gint*)data & MISC_AGAIN_FLAG_DONT_SAVE_YES)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
				      RESPONSE_YES, !active);
}

gboolean yes_no_f(GtkWidget *parent, appdata_t *appdata, gulong again_bit,
		  gint flags, const char *title, const char *fmt, ...) {

  if(appdata && again_bit && (appdata->dialog_again.not & again_bit))
    return((appdata->dialog_again.reply & again_bit) != 0);

  va_list args;
  va_start( args, fmt );
  char *buf = g_strdup_vprintf(fmt, args);
  va_end( args );

  printf("%s: \"%s\"\n", title, buf);

#ifndef FREMANTLE
  GtkWidget *dialog = gtk_message_dialog_new(
		     GTK_WINDOW(parent),
                     GTK_DIALOG_DESTROY_WITH_PARENT,
		     GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                     "%s", buf);

  gtk_window_set_title(GTK_WINDOW(dialog), title);
#else
  GtkWidget *dialog =
    hildon_note_new_confirmation(GTK_WINDOW(parent), buf);
#endif

  GtkWidget *cbut = NULL;
  if(appdata && again_bit) {
#ifdef FREMANTLE
    /* make sure there's some space before the checkbox */
    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox),
				gtk_label_new(" "));
#endif

    GtkWidget *alignment = gtk_alignment_new(0.5, 0, 0, 0);

    cbut = check_button_new_with_label(_("Don't ask this question again"));
    g_signal_connect(cbut, "toggled", G_CALLBACK(on_toggled), &flags);

    gtk_container_add(GTK_CONTAINER(alignment), cbut);
    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), alignment);

    gtk_widget_show_all(dialog);
  }

  gboolean yes = (gtk_dialog_run(GTK_DIALOG(dialog)) == RESPONSE_YES);

  if(cbut && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cbut))) {
    /* the user doesn't want to see this dialog again */

    appdata->dialog_again.not |= again_bit;
    if(yes) appdata->dialog_again.reply |=  again_bit;
    else    appdata->dialog_again.reply &= ~again_bit;
  }

  gtk_widget_destroy(dialog);

  g_free(buf);
  return yes;
}

/* all entries must contain a trailing '/' ! */
static const char *data_paths[] = {
  "~/." PACKAGE "/",           // in home directory
  DATADIR "/",                 // final installation path
#ifdef USE_HILDON
  "/media/mmc1/" PACKAGE "/",  // path to external memory card
  "/media/mmc2/" PACKAGE "/",  // path to internal memory card
#endif
  "./data/", "../data/",       // local paths for testing
  NULL
};

gchar *find_file(const char *n1, const char *n2, const char *n3) {
  const char **path = data_paths;
  char *p = getenv("HOME");

  while(*path) {
    gchar *full_path = NULL;

    if(*path[0] == '~')
      full_path = g_strconcat(p, *path + 1, n1, n2, n3, NULL);
    else
      full_path = g_strconcat(*path, n1, n2, n3, NULL);

    if(g_file_test(full_path, G_FILE_TEST_IS_REGULAR))
      return full_path;

    g_free(full_path);
    path++;
  }

  return NULL;
}

/* scan all data directories for the given file extension and */
/* return a list of files matching this extension */
file_chain_t *file_scan(const char *extension) {
  file_chain_t *chain = NULL, **chainP = &chain;

  const char **path = data_paths;
  char *p = getenv("HOME");

  while(*path) {
    GDir *dir = NULL;

    /* scan for projects */
    const char *dirname = *path;

    if(*path[0] == '~')
      dirname = g_strjoin(p, *path + 1, NULL);

    if((dir = g_dir_open(dirname, 0, NULL))) {
      const char *name = NULL;
      do {
	name = g_dir_read_name(dir);

	if(name) {
	  if(g_str_has_suffix(name, extension)) {
	    gchar *fullname = g_strconcat(dirname, name, NULL);
	    if(g_file_test(fullname, G_FILE_TEST_IS_REGULAR)) {
	      *chainP = g_new0(file_chain_t, 1);
	      (*chainP)->name = fullname;
	      chainP = &(*chainP)->next;
	    } else
	      g_free(fullname);
	  }
	}
      } while(name);

      g_dir_close(dir);

      if(*path[0] == '~')
	g_free((char*)dirname);
    }

    path++;
  }

  return chain;
}


#ifdef USE_HILDON
static const gint dialog_sizes[][2] = {
  { 400, 100 },  // SMALL
#if MAEMO_VERSION_MAJOR < 5
  { 450, 300 },  // MEDIUM
  { 800, 480 },  // LARGE
#else
  /* in maemo5 most dialogs are full screen */
  { 800, 480 },  // MEDIUM
  { 790, 380 },  // LARGE
#endif
  { 640, 100 },  // WIDE
  { 450, 480 },  // HIGH
};
#else
static const gint dialog_sizes[][2] = {
  { 300, 100 },  // SMALL
  { 400, 300 },  // MEDIUM
  { 500, 350 },  // LARGE
  { 450, 100 },  // WIDE
  { 200, 350 },  // HIGH
};
#endif

/* create a modal dialog using one of the predefined size hints */
GtkWidget *misc_dialog_new(guint hint, const gchar *title,
			   GtkWindow *parent, ...) {
  va_list args;
  va_start( args, parent );

  /* create dialog itself */
  GtkWidget *dialog = gtk_dialog_new();

  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);

  const gchar *button_text = va_arg(args, const gchar *);
  while(button_text) {
    gtk_dialog_add_button(GTK_DIALOG(dialog), button_text, va_arg(args, gint));
    button_text = va_arg(args, const gchar *);
  }

  va_end( args );

  if(hint != MISC_DIALOG_NOSIZE)
    gtk_window_set_default_size(GTK_WINDOW(dialog),
			dialog_sizes[hint][0], dialog_sizes[hint][1]);

  return dialog;
}

#if defined(USE_HILDON) && (MAEMO_VERSION_MAJOR == 5)
#include <hildon/hildon-pannable-area.h>
/* create a pannable area */
GtkWidget *misc_scrolled_window_new(gboolean etched_in) {
  return hildon_pannable_area_new();
}

void misc_scrolled_window_add_with_viewport(GtkWidget *win, GtkWidget *child) {
  hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(win), child);
}

#else
/* create a scrolled window */
GtkWidget *misc_scrolled_window_new(gboolean etched_in) {
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
  				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  if(etched_in)
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window),
					GTK_SHADOW_ETCHED_IN);
  return scrolled_window;
}

void misc_scrolled_window_add_with_viewport(GtkWidget *win, GtkWidget *child) {
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(win), child);
}


#endif

const char *misc_get_proxy_uri(settings_t *settings) {
  static char proxy_buffer[64];

  /* use environment settings if preset */
  const char *proxy = g_getenv("http_proxy");
  if(proxy) {
    printf("http_proxy: %s\n", proxy);
    return proxy;
  }

  /* otherwise try settings */
  if(!settings || !settings->proxy ||
     !settings->proxy->host) return NULL;

  const char *protocol = strncmp(settings->proxy->host, "http://", 7) ? "" :
			 strncmp(settings->proxy->host, "https://", 8) ? "" :
			 "http://";

  snprintf(proxy_buffer, sizeof(proxy_buffer), "%s%s:%u",
	   protocol, settings->proxy->host, settings->proxy->port);

  proxy_buffer[sizeof(proxy_buffer)-1] = 0;
  printf("gconf_proxy: %s\n", proxy_buffer);
  return proxy_buffer;
}

void misc_table_attach(GtkWidget *table, GtkWidget *widget, int x, int y) {
  gtk_table_attach_defaults(GTK_TABLE(table), widget, x, x+1, y, y+1);
}

/* ---------- unified widgets for fremantle/others --------------- */

GtkWidget *entry_new(void) {
#ifndef FREMANTLE
  return gtk_entry_new();
#else
  return hildon_entry_new(HILDON_SIZE_AUTO);
#endif
}

GType entry_type(void) {
#ifndef FREMANTLE
  return GTK_TYPE_ENTRY;
#else
  return HILDON_TYPE_ENTRY;
#endif
}

GtkWidget *button_new(void) {
  GtkWidget *button = gtk_button_new();
#ifdef FREMANTLE
  hildon_gtk_widget_set_theme_size(button,
           (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));
#endif
  return button;
}

GtkWidget *button_new_with_label(const gchar *label) {
  GtkWidget *button = gtk_button_new_with_label(label);
#ifdef FREMANTLE
  hildon_gtk_widget_set_theme_size(button,
           (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));
#endif
  return button;
}

GtkWidget *check_button_new_with_label(const gchar *label) {
#ifndef FREMANTLE
  return gtk_check_button_new_with_label(label);
#else
  GtkWidget *cbut =
    hildon_check_button_new(HILDON_SIZE_FINGER_HEIGHT |
                            HILDON_SIZE_AUTO_WIDTH);
  gtk_button_set_label(GTK_BUTTON(cbut), label);
  return cbut;
#endif
}

GType check_button_type(void) {
#ifndef FREMANTLE
  return GTK_TYPE_CHECK_BUTTON;
#else
  return HILDON_TYPE_CHECK_BUTTON;
#endif
}

void check_button_set_active(GtkWidget *button, gboolean active) {
#ifndef FREMANTLE
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), active);
#else
  hildon_check_button_set_active(HILDON_CHECK_BUTTON(button), active);
#endif
}

gboolean check_button_get_active(GtkWidget *button) {
#ifndef FREMANTLE
  return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
#else
  return hildon_check_button_get_active(HILDON_CHECK_BUTTON(button));
#endif
}

GtkWidget *notebook_new(void) {
#ifdef FREMANTLE
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  GtkWidget *notebook =  gtk_notebook_new();

  /* solution for fremantle: we use a row of ordinary buttons instead */
  /* of regular tabs */

  /* hide the regular tabs */
  gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);

  gtk_box_pack_start_defaults(GTK_BOX(vbox), notebook);

  /* store a reference to the notebook in the vbox */
  g_object_set_data(G_OBJECT(vbox), "notebook", (gpointer)notebook);

  /* create a hbox for the buttons */
  GtkWidget *hbox = gtk_hbox_new(TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  g_object_set_data(G_OBJECT(vbox), "hbox", (gpointer)hbox);

  return vbox;
#else
  return gtk_notebook_new();
#endif
}

GtkWidget *notebook_get_gtk_notebook(GtkWidget *notebook) {
#ifdef FREMANTLE
  return GTK_WIDGET(g_object_get_data(G_OBJECT(notebook), "notebook"));
#else
  return notebook;
#endif
}


#ifdef FREMANTLE
static void on_notebook_button_clicked(GtkWidget *button, gpointer data) {
  GtkNotebook *nb =
    GTK_NOTEBOOK(g_object_get_data(G_OBJECT(data), "notebook"));

  gint page = (gint)g_object_get_data(G_OBJECT(button), "page");
  gtk_notebook_set_current_page(nb, page);
}
#endif

void notebook_append_page(GtkWidget *notebook,
			  GtkWidget *page, const gchar *label) {
#ifdef FREMANTLE
  GtkNotebook *nb =
    GTK_NOTEBOOK(g_object_get_data(G_OBJECT(notebook), "notebook"));

  gint page_num = gtk_notebook_append_page(nb, page, gtk_label_new(label));
  GtkWidget *button = NULL;

  /* select button for page 0 by default */
  if(!page_num) {
    button = gtk_radio_button_new_with_label(NULL, label);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    g_object_set_data(G_OBJECT(notebook), "group_master", (gpointer)button);
  } else {
    GtkWidget *master = g_object_get_data(G_OBJECT(notebook), "group_master");
    button = gtk_radio_button_new_with_label_from_widget(
				 GTK_RADIO_BUTTON(master), label);
  }

  gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
  g_object_set_data(G_OBJECT(button), "page", (gpointer)page_num);

  gtk_signal_connect(GTK_OBJECT(button), "clicked",
	   GTK_SIGNAL_FUNC(on_notebook_button_clicked), notebook);

#if defined(USE_HILDON) && (MAEMO_VERSION_MAJOR == 5)
  hildon_gtk_widget_set_theme_size(button,
	   (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));
#endif

  gtk_box_pack_start_defaults(
	      GTK_BOX(g_object_get_data(G_OBJECT(notebook), "hbox")),
	      button);

#else
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, gtk_label_new(label));
#endif
}

#ifdef FREMANTLE
void on_value_changed(HildonPickerButton *widget, gpointer  user_data) {
  g_signal_emit_by_name(widget, "changed");
}

static GtkWidget *combo_box_new_with_selector(const gchar *title, GtkWidget *selector) {
  GtkWidget *button =
    hildon_picker_button_new(HILDON_SIZE_FINGER_HEIGHT |
			     HILDON_SIZE_AUTO_WIDTH,
			     HILDON_BUTTON_ARRANGEMENT_VERTICAL);

  hildon_button_set_title_alignment(HILDON_BUTTON(button), 0.5, 0.5);
  hildon_button_set_value_alignment(HILDON_BUTTON(button), 0.5, 0.5);

  /* allow button to emit "changed" signal */
  g_signal_connect(button, "value-changed", G_CALLBACK(on_value_changed), NULL);

  hildon_button_set_title(HILDON_BUTTON (button), title);

  hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(button),
				    HILDON_TOUCH_SELECTOR(selector));

  return button;
}
#endif

/* the title is only used on fremantle with the picker widget */
GtkWidget *combo_box_new(const gchar *title) {
#ifndef FREMANTLE
  return gtk_combo_box_new_text();
#else
  GtkWidget *selector = hildon_touch_selector_new_text();
  return combo_box_new_with_selector(title, selector);
#endif
}

GtkWidget *combo_box_entry_new(const gchar *title) {
#ifndef FREMANTLE
  return gtk_combo_box_entry_new_text();
#else
  GtkWidget *selector = hildon_touch_selector_entry_new_text();
  return combo_box_new_with_selector(title, selector);
#endif
}

void combo_box_append_text(GtkWidget *cbox, const gchar *text) {
#ifndef FREMANTLE
  gtk_combo_box_append_text(GTK_COMBO_BOX(cbox), text);
#else
  HildonTouchSelector *selector =
    hildon_picker_button_get_selector(HILDON_PICKER_BUTTON(cbox));

  hildon_touch_selector_append_text(selector, text);
#endif
}

void combo_box_set_active(GtkWidget *cbox, int index) {
#ifndef FREMANTLE
  gtk_combo_box_set_active(GTK_COMBO_BOX(cbox), index);
#else
  hildon_picker_button_set_active(HILDON_PICKER_BUTTON(cbox), index);
#endif
}

int combo_box_get_active(GtkWidget *cbox) {
#ifndef FREMANTLE
  return gtk_combo_box_get_active(GTK_COMBO_BOX(cbox));
#else
  return hildon_picker_button_get_active(HILDON_PICKER_BUTTON(cbox));
#endif
}

const char *combo_box_get_active_text(GtkWidget *cbox) {
#ifndef FREMANTLE
  return gtk_combo_box_get_active_text(GTK_COMBO_BOX(cbox));
#else
  return hildon_button_get_value(HILDON_BUTTON(cbox));
#endif
}

GType combo_box_type(void) {
#ifndef FREMANTLE
  return GTK_TYPE_COMBO_BOX;
#else
  return HILDON_TYPE_PICKER_BUTTON;
#endif
}

GType combo_box_entry_type(void) {
#ifndef FREMANTLE
  return GTK_TYPE_COMBO_BOX_ENTRY;
#else
  return HILDON_TYPE_PICKER_BUTTON;
#endif
}

void misc_init(void) {
#ifdef FREMANTLE
  g_signal_new ("changed", HILDON_TYPE_PICKER_BUTTON,
		G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
#endif
}
