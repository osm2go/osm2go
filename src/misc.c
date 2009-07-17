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

static void vmessagef(GtkWidget *parent, int type, int buttons,
		      char *title, const char *fmt, 
		      va_list args) {

  char *buf = g_strdup_vprintf(fmt, args);

  GtkWidget *dialog = gtk_message_dialog_new(
		     GTK_WINDOW(parent),
                     GTK_DIALOG_DESTROY_WITH_PARENT,
                     type, buttons, buf);

  gtk_window_set_title(GTK_WINDOW(dialog), title);

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);

  g_free(buf);
}


void messagef(GtkWidget *parent, char *title, const char *fmt, ...) {
  va_list args;
  va_start( args, fmt );
  vmessagef(parent, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, title, fmt, args);
  va_end( args );
}

void errorf(GtkWidget *parent, const char *fmt, ...) {
  va_list args;
  va_start( args, fmt );
  vmessagef(parent, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, 
	    _("Error"), fmt, args);
  va_end( args );
}

void warningf(GtkWidget *parent, const char *fmt, ...) {
  va_list args;
  va_start( args, fmt );
  vmessagef(parent, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, 
	    _("Warning"), fmt, args);
  va_end( args );
}

static void on_toggled(GtkWidget *button, gpointer data) {
  gboolean active = 
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

  GtkWidget *dialog = gtk_widget_get_toplevel(button);

  if(*(gint*)data & MISC_AGAIN_FLAG_DONT_SAVE_NO)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
				      GTK_RESPONSE_NO, !active);

  if(*(gint*)data & MISC_AGAIN_FLAG_DONT_SAVE_YES)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
				      GTK_RESPONSE_YES, !active);
}

gboolean yes_no_f(GtkWidget *parent, appdata_t *appdata, gulong again_bit, 
		  gint flags, char *title, const char *fmt, ...) {

  if(appdata && again_bit && (appdata->dialog_again.not & again_bit)) 
    return((appdata->dialog_again.reply & again_bit) != 0);

  va_list args;
  va_start( args, fmt );
  char *buf = g_strdup_vprintf(fmt, args);
  va_end( args );

  printf("%s: \"%s\"\n", title, buf); 

  GtkWidget *dialog = gtk_message_dialog_new(
		     GTK_WINDOW(parent),
                     GTK_DIALOG_DESTROY_WITH_PARENT,
		     GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                     buf);

  gtk_window_set_title(GTK_WINDOW(dialog), title);

  GtkWidget *cbut = NULL;
  if(appdata && again_bit) {
    GtkWidget *alignment = gtk_alignment_new(0.5, 0, 0, 0);

    cbut = gtk_check_button_new_with_label(
            _("Don't ask this question again this session"));
    g_signal_connect(cbut, "toggled", G_CALLBACK(on_toggled), &flags);

    gtk_container_add(GTK_CONTAINER(alignment), cbut);
    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), alignment);

    gtk_widget_show_all(dialog);
  }

  gboolean yes = (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES);

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

static const char *data_paths[] = {
  "~/." PACKAGE,             // in home directory
  PREFIX "/share/" PACKAGE , // final installation path 
#ifdef USE_HILDON
  "/media/mmc1/" PACKAGE,    // path to external memory card
  "/media/mmc2/" PACKAGE,    // path to internal memory card
#endif
  "./data", "../data",       // local paths for testing
  NULL
};

char *find_file(char *name) {
  const char **path = data_paths;
  char *p = getenv("HOME");

  while(*path) {
    char *full_path = NULL;

    if(*path[0] == '~') 
      full_path = g_strdup_printf("%s/%s/%s", p, *path+2, name);
    else
      full_path = g_strdup_printf("%s/%s", *path, name);

    if(g_file_test(full_path, G_FILE_TEST_IS_REGULAR))
      return full_path;

    g_free(full_path);
    path++;
  }

  return NULL;
}

/* scan all data directories for the given file pattern and */
/* return a list of files matching this pattern */
file_chain_t *file_scan(char *pattern) {
  file_chain_t *chain = NULL, **chainP = &chain;

  const char **path = data_paths;
  char *p = getenv("HOME");

  while(*path) {
    GDir *dir = NULL;
    
    /* scan for projects */
    const char *dirname = *path;

    if(*path[0] == '~') 
      dirname = g_strdup_printf("%s/%s", p, *path+2);

    if((dir = g_dir_open(dirname, 0, NULL))) {
      const char *name = NULL;
      do {
	name = g_dir_read_name(dir);
	
	if(name) {
	  char *fullname = g_strdup_printf("%s/%s", dirname, name);
	  if(g_file_test(fullname, G_FILE_TEST_IS_REGULAR)) {
	    if(g_pattern_match_simple(pattern, name)) {
	      *chainP = g_new0(file_chain_t, 1);
	      (*chainP)->name = fullname;
	      chainP = &(*chainP)->next;
	    } else
	      g_free(fullname);
	  } else
	    g_free(fullname);
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
  { 450, 300 },  // MEDIUM
#if MAEMO_VERSION_MAJOR < 5
  { 800, 480 },  // LARGE
#else
  { 800, 380 },  // LARGE
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
GtkWidget *misc_dialog_new(guint hint, const char *title, 
			   GtkWindow *parent, ...) {
  va_list args;
  va_start( args, parent );

  /* create dialog itself */
  GtkWidget *dialog = gtk_dialog_new();
  
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  if(title) gtk_window_set_title(GTK_WINDOW(dialog), title);
  if(parent) gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);

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

  snprintf(proxy_buffer, sizeof(proxy_buffer), "%s%s:%u", 
	   strncmp(settings->proxy->host, "http://", 7)?"http://":"", 
	   settings->proxy->host, settings->proxy->port);

  proxy_buffer[sizeof(proxy_buffer)-1] = 0;
  printf("gconf_proxy: %s\n", proxy_buffer);
  return proxy_buffer;
}

void misc_table_attach(GtkWidget *table, GtkWidget *widget, int x, int y) {
  gtk_table_attach_defaults(GTK_TABLE(table), widget, x, x+1, y, y+1);
}
