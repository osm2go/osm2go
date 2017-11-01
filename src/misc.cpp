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

#include "misc.h"

#include "appdata.h"
#include "pos.h"
#include "settings.h"

#ifdef FREMANTLE
#include <dbus.h>

#include <hildon/hildon-check-button.h>
#include <hildon/hildon-pannable-area.h>
#include <hildon/hildon-picker-button.h>
#include <hildon/hildon-entry.h>
#include <hildon/hildon-touch-selector-entry.h>
#include <hildon/hildon-note.h>
#include <tablet-browser-interface.h>
#else
#include <gtk/gtk.h>
#endif

#include <cassert>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>

double xml_get_prop_float(xmlNode *node, const char *prop) {
  xmlChar *str = xmlGetProp(node, BAD_CAST prop);
  double value = NAN;
  if(str) {
    value = g_ascii_strtod(reinterpret_cast<gchar *>(str), O2G_NULLPTR);
    xmlFree(str);
  }
  return value;
}

bool xml_get_prop_is(xmlNode *node, const char *prop, const char *str) {
  xmlChar *prop_str = xmlGetProp(node, BAD_CAST prop);
  if(!prop_str)
    return false;

  bool match = (strcasecmp(reinterpret_cast<char *>(prop_str), str) == 0);
  xmlFree(prop_str);
  return match;
}

pos_t xml_get_prop_pos(xmlNode *node) {
  return pos_t(xml_get_prop_float(node, "lat"),
               xml_get_prop_float(node, "lon"));
}

void xml_set_prop_pos(xmlNode *node, const pos_t *pos) {
  char str[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_formatd(str, sizeof(str), LL_FORMAT, pos->lat);
  remove_trailing_zeroes(str);
  xmlNewProp(node, BAD_CAST "lat", BAD_CAST str);
  g_ascii_formatd(str, sizeof(str), LL_FORMAT, pos->lon);
  remove_trailing_zeroes(str);
  xmlNewProp(node, BAD_CAST "lon", BAD_CAST str);
}

static void vmessagef(GtkWidget *parent, GtkMessageType type, GtkButtonsType buttons,
                      const char *title, const char *fmt, va_list args) {
  GtkWindow *wnd = GTK_WINDOW(parent);
  char *buf = g_strdup_vprintf(fmt, args);

  if(unlikely(wnd == O2G_NULLPTR)) {
    printf("%s", buf);
    g_free(buf);
    return;
  }

#ifndef FREMANTLE
  GtkWidget *dialog = gtk_message_dialog_new(wnd,
			   GTK_DIALOG_DESTROY_WITH_PARENT,
			   type, buttons, "%s", buf);

  gtk_window_set_title(GTK_WINDOW(dialog), title);
#else
  GtkWidget *dialog = hildon_note_new_information(wnd, buf);
  (void)type;
  (void)buttons;
  (void)title;
#endif // FREMANTLE

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);

  g_free(buf);
}

void messagef(GtkWidget *parent, const char *title, const char *fmt, ...) {
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
  int flags = *static_cast<gint *>(data);

  GtkWidget *dialog = gtk_widget_get_toplevel(button);

  if(flags & MISC_AGAIN_FLAG_DONT_SAVE_NO)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
				      RESPONSE_NO, !active);

  if(flags & MISC_AGAIN_FLAG_DONT_SAVE_YES)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
				      RESPONSE_YES, !active);
}

bool yes_no_f(GtkWidget *parent, appdata_t &appdata, guint again_bit,
              gint flags, const char *title, const char *fmt, ...) {
  if(again_bit && (appdata.dialog_again.not_again & again_bit))
    return ((appdata.dialog_again.reply & again_bit) != 0);

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

  GtkWidget *cbut = O2G_NULLPTR;
  if(again_bit) {
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

  bool yes = (gtk_dialog_run(GTK_DIALOG(dialog)) == RESPONSE_YES);

  if(cbut && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cbut))) {
    /* the user doesn't want to see this dialog again */

    appdata.dialog_again.not_again |= again_bit;
    if(yes)
      appdata.dialog_again.reply |=  again_bit;
    else
      appdata.dialog_again.reply &= ~again_bit;
  }

  gtk_widget_destroy(dialog);

  g_free(buf);
  return yes;
}

std::vector<datapath> base_paths;

/* all entries must contain a trailing '/' ! */
static void init_paths() {
  std::vector<std::string> pathnames;

  const char *home = getenv("HOME");
  assert(home != O2G_NULLPTR);

  // in home directory
  pathnames.push_back(home + std::string("/." PACKAGE "/"));
  // final installation path
  pathnames.push_back(DATADIR "/");
#ifdef FREMANTLE
  // path to external memory card
  pathnames.push_back("/media/mmc1/" PACKAGE "/");
  // path to internal memory card
  pathnames.push_back("/media/mmc2/" PACKAGE "/");
#endif
  // local paths for testing
  pathnames.push_back("./data/");
  pathnames.push_back("../data/");

  for (unsigned int i = 0; i < pathnames.size(); i++) {
    assert(pathnames[i][pathnames[i].size() - 1] == '/');
    fdguard dfd(pathnames[i].c_str(), O_DIRECTORY);
    if(dfd.valid()) {
#if __cplusplus >= 201103L
      base_paths.emplace_back(datapath(std::move(dfd)));
#else
      base_paths.push_back(datapath(dfd));
#endif

      base_paths.back().pathname.swap(pathnames[i]);
    }
  }

  assert(!base_paths.empty());
}

std::string find_file(const std::string &n) {
  assert(!n.empty());

  struct stat st;

  if(unlikely(n[0] == '/')) {
    if(stat(n.c_str(), &st) == 0 && S_ISREG(st.st_mode))
      return n;
    return std::string();
  }

  for(unsigned int i = 0; i < base_paths.size(); i++) {
    if(fstatat(base_paths[i].fd, n.c_str(), &st, 0) == 0 && S_ISREG(st.st_mode))
      return base_paths[i].pathname + n;
  }

  return std::string();
}

static const gint dialog_sizes[][2] = {
#ifdef FREMANTLE
  { 400, 100 },  // SMALL
  /* in maemo5 most dialogs are full screen */
  { 800, 480 },  // MEDIUM
  { 790, 380 },  // LARGE
  { 640, 100 },  // WIDE
  { 450, 480 },  // HIGH
#else
  { 300, 100 },  // SMALL
  { 400, 300 },  // MEDIUM
  { 500, 350 },  // LARGE
  { 450, 100 },  // WIDE
  { 200, 350 },  // HIGH
#endif
};

/* create a modal dialog using one of the predefined size hints */
GtkWidget *misc_dialog_new(DialogSizeHing hint, const gchar *title,
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

#ifdef FREMANTLE
/* create a pannable area */
GtkWidget *misc_scrolled_window_new(gboolean) {
  return hildon_pannable_area_new();
}

#else
/* create a scrolled window */
GtkWidget *misc_scrolled_window_new(gboolean etched_in) {
  GtkWidget *scrolled_window = gtk_scrolled_window_new(O2G_NULLPTR, O2G_NULLPTR);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
  				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  if(etched_in)
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window),
					GTK_SHADOW_ETCHED_IN);
  return scrolled_window;
}

#endif

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

GtkWidget *button_new_with_label(const gchar *label) {
  GtkWidget *button = gtk_button_new_with_label(label);
#ifdef FREMANTLE
  hildon_gtk_widget_set_theme_size(button,
           static_cast<HildonSizeType>(HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));
#endif
  return button;
}

GtkWidget *check_button_new_with_label(const gchar *label) {
#ifndef FREMANTLE
  return gtk_check_button_new_with_label(label);
#else
  GtkWidget *cbut =
    hildon_check_button_new(static_cast<HildonSizeType>(HILDON_SIZE_FINGER_HEIGHT |
                                                        HILDON_SIZE_AUTO_WIDTH));
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
  g_object_set_data(G_OBJECT(vbox), "notebook", notebook);

  /* create a hbox for the buttons */
  GtkWidget *hbox = gtk_hbox_new(TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  g_object_set_data(G_OBJECT(vbox), "hbox", hbox);

  return vbox;
#else
  return gtk_notebook_new();
#endif
}

GtkNotebook *notebook_get_gtk_notebook(GtkWidget *notebook) {
#ifdef FREMANTLE
  return GTK_NOTEBOOK(g_object_get_data(G_OBJECT(notebook), "notebook"));
#else
  return GTK_NOTEBOOK(notebook);
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

void notebook_append_page(GtkWidget *notebook, GtkWidget *page, const gchar *label) {
  GtkNotebook *nb = notebook_get_gtk_notebook(notebook);
  gint page_num = gtk_notebook_append_page(nb, page, gtk_label_new(label));

#ifdef FREMANTLE
  GtkWidget *button = O2G_NULLPTR;

  /* select button for page 0 by default */
  if(!page_num) {
    button = gtk_radio_button_new_with_label(O2G_NULLPTR, label);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    g_object_set_data(G_OBJECT(notebook), "group_master", (gpointer)button);
  } else {
    GtkWidget *master = static_cast<GtkWidget *>(g_object_get_data(G_OBJECT(notebook), "group_master"));
    button = gtk_radio_button_new_with_label_from_widget(
				 GTK_RADIO_BUTTON(master), label);
  }

  gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
  g_object_set_data(G_OBJECT(button), "page", (gpointer)page_num);

  g_signal_connect(GTK_OBJECT(button), "clicked",
                   G_CALLBACK(on_notebook_button_clicked), notebook);

#ifdef FREMANTLE
  hildon_gtk_widget_set_theme_size(button,
	   static_cast<HildonSizeType>(HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));
#endif

  gtk_box_pack_start_defaults(
	      GTK_BOX(g_object_get_data(G_OBJECT(notebook), "hbox")),
	      button);
#else
  (void)page_num;
#endif
}

#ifdef FREMANTLE
void on_value_changed(HildonPickerButton *widget, gpointer) {
  g_signal_emit_by_name(widget, "changed");
}

static GtkWidget *combo_box_new_with_selector(const gchar *title, GtkWidget *selector) {
  GtkWidget *button =
    hildon_picker_button_new(static_cast<HildonSizeType>(HILDON_SIZE_FINGER_HEIGHT |
                                                         HILDON_SIZE_AUTO_WIDTH),
			     HILDON_BUTTON_ARRANGEMENT_VERTICAL);

  hildon_button_set_title_alignment(HILDON_BUTTON(button), 0.5, 0.5);
  hildon_button_set_value_alignment(HILDON_BUTTON(button), 0.5, 0.5);

  /* allow button to emit "changed" signal */
  g_signal_connect(button, "value-changed", G_CALLBACK(on_value_changed), O2G_NULLPTR);

  hildon_button_set_title(HILDON_BUTTON (button), title);

  hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(button),
				    HILDON_TOUCH_SELECTOR(selector));

  return button;
}
#endif

/* the title is only used on fremantle with the picker widget */
GtkWidget *combo_box_new(const gchar *title) {
#ifndef FREMANTLE
  (void)title;
  return gtk_combo_box_new_text();
#else
  GtkWidget *selector = hildon_touch_selector_new_text();
  return combo_box_new_with_selector(title, selector);
#endif
}

GtkWidget *combo_box_entry_new(const gchar *title) {
#ifndef FREMANTLE
  (void)title;
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

std::string combo_box_get_active_text(GtkWidget *cbox) {
#ifndef FREMANTLE
  char *ptr;
  ptr = gtk_combo_box_get_active_text(GTK_COMBO_BOX(cbox));
  std::string ret = ptr;
  g_free(ptr);
  return ret;
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

/* ---------- simple interface to the systems web browser ---------- */
void open_url(appdata_t &appdata, const char *url)
{
#ifndef FREMANTLE
  gtk_show_uri(O2G_NULLPTR, url, GDK_CURRENT_TIME, O2G_NULLPTR);
  (void)appdata;
#else
  osso_rpc_run_with_defaults(appdata.osso_context, "osso_browser",
                             OSSO_BROWSER_OPEN_NEW_WINDOW_REQ, O2G_NULLPTR,
                             DBUS_TYPE_STRING, url,
                             DBUS_TYPE_BOOLEAN, FALSE, DBUS_TYPE_INVALID);
#endif
}

void misc_init(void) {
  init_paths();
#ifdef FREMANTLE
  g_signal_new ("changed", HILDON_TYPE_PICKER_BUTTON,
		G_SIGNAL_RUN_FIRST, 0, O2G_NULLPTR, O2G_NULLPTR,
		g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  dbus_register();
#endif
}

struct combo_add_string {
  GtkWidget * const cbox;
  explicit combo_add_string(GtkWidget *w) : cbox(w) {}
  void operator()(const std::string &entry) {
    combo_box_append_text(cbox, entry.c_str());
  }
};

GtkWidget *string_select_widget(const char *title, const std::vector<std::string> &entries, int match) {
  GtkWidget *cbox = combo_box_new(title);

  /* fill combo box with entries */
  std::for_each(entries.begin(), entries.end(), combo_add_string(cbox));

  if(match >= 0)
    combo_box_set_active(cbox, match);

  return cbox;
}
