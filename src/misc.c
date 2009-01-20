/*
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

void messagef(GtkWidget *parent, char *title, const char *fmt, ...) {
  va_list args;
  va_start( args, fmt );
  char *buf = g_strdup_vprintf(fmt, args);
  va_end( args );

  printf("%s: \"%s\"\n", title, buf); 
  
  GtkWidget *dialog = gtk_message_dialog_new(
		     GTK_WINDOW(parent),
                     GTK_DIALOG_DESTROY_WITH_PARENT,
                     GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                     buf);

  gtk_window_set_title(GTK_WINDOW(dialog), title);

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);

  g_free(buf);
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

void errorf(GtkWidget *parent, const char *fmt, ...) {
  va_list args;
  va_start( args, fmt );
  char *buf = g_strdup_vprintf(fmt, args);
  va_end( args );

  printf("errorf(\"%s\")\n", buf); 
  
  GtkWidget *dialog = gtk_message_dialog_new(
		     GTK_WINDOW(parent),
                     GTK_DIALOG_DESTROY_WITH_PARENT,
                     GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                     buf);

  gtk_window_set_title(GTK_WINDOW(dialog), _("ERROR"));

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);

  g_free(buf);
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
