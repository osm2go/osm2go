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

#include "misc.h"
#include "xml_helpers.h"

#include "appdata.h"
#include "pos.h"
#include "settings.h"

#ifdef FREMANTLE
#include <hildon/hildon-note.h>
#else
#include <gtk/gtk.h>
#endif
#include <osm2go_platform_gtk.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_platform.h>

double xml_get_prop_float(xmlNode *node, const char *prop) {
  xmlString str(xmlGetProp(node, BAD_CAST prop));
  return xml_parse_float(str);
}

bool xml_get_prop_bool(xmlNode *node, const char *prop) {
  xmlString prop_str(xmlGetProp(node, BAD_CAST prop));
  if(!prop_str)
    return false;

  return (strcasecmp(prop_str, "true") == 0);
}

#ifndef FREMANTLE
#define RESPONSE_YES  GTK_RESPONSE_YES
#define RESPONSE_NO   GTK_RESPONSE_NO
#else
/* hildon names the yes/no buttons ok/cancel ??? */
#define RESPONSE_YES  GTK_RESPONSE_OK
#define RESPONSE_NO   GTK_RESPONSE_CANCEL
#endif

static void on_toggled(GtkWidget *button, int *flags) {
  gboolean active = osm2go_platform::check_button_get_active(button) ? TRUE : FALSE;

  GtkWidget *dialog = gtk_widget_get_toplevel(button);

  if(*flags & MISC_AGAIN_FLAG_DONT_SAVE_NO)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), RESPONSE_NO, !active);

  if(*flags & MISC_AGAIN_FLAG_DONT_SAVE_YES)
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), RESPONSE_YES, !active);
}

bool yes_no_f(osm2go_platform::Widget *parent, unsigned int again_flags, const char *title,
              const char *fmt, ...) {
  /* flags used to prevent re-appearence of dialogs */
  static struct {
    unsigned int not_again;     /* bit is set if dialog is not to be displayed again */
    unsigned int reply;         /* reply to be assumed if "not_again" bit is set */
  } dialog_again;
  const unsigned int again_bit = again_flags & ~(MISC_AGAIN_FLAG_DONT_SAVE_NO | MISC_AGAIN_FLAG_DONT_SAVE_YES);

  if(again_bit && (dialog_again.not_again & again_bit))
    return ((dialog_again.reply & again_bit) != 0);

  va_list args;
  va_start( args, fmt );
  g_string buf(g_strdup_vprintf(fmt, args));
  va_end( args );

  printf("%s: \"%s\"\n", title, buf.get());

  GtkWindow *p = GTK_WINDOW(parent ? parent : appdata_t::window);

  osm2go_platform::WidgetGuard dialog(
#ifndef FREMANTLE
                  gtk_message_dialog_new(p, GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                                         "%s", buf.get()));

  gtk_window_set_title(GTK_WINDOW(dialog.get()), title);
#else
                  hildon_note_new_confirmation(p, buf.get()));
#endif

  osm2go_platform::Widget *cbut = nullptr;
  if(again_bit) {
#ifdef FREMANTLE
    /* make sure there's some space before the checkbox */
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.get())->vbox),
                       gtk_label_new(" "), TRUE, TRUE, 0);
#endif

    GtkWidget *alignment = gtk_alignment_new(0.5, 0, 0, 0);

    cbut = osm2go_platform::check_button_new_with_label(_("Don't ask this question again"));
    g_signal_connect(cbut, "toggled", G_CALLBACK(on_toggled), &again_flags);

    gtk_container_add(GTK_CONTAINER(alignment), cbut);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.get())->vbox), alignment, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog.get());
  }

  bool yes = (gtk_dialog_run(GTK_DIALOG(dialog.get())) == RESPONSE_YES);

  if(cbut != nullptr && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cbut)) == TRUE) {
    /* the user doesn't want to see this dialog again */

    dialog_again.not_again |= again_bit;
    if(yes)
      dialog_again.reply |=  again_bit;
    else
      dialog_again.reply &= ~again_bit;
  }

  return yes;
}

const std::vector<datapath> &base_paths()
{
/* all entries must contain a trailing '/' ! */
  static std::vector<datapath> ret;

  if(unlikely(ret.empty())) {
    std::vector<std::string> pathnames;

    const char *home = getenv("HOME");
    assert(home != nullptr);

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
        ret.emplace_back(datapath(std::move(dfd)));
#else
        ret.push_back(datapath(dfd));
#endif

        ret.back().pathname.swap(pathnames[i]);
      }
    }

    assert(!ret.empty());
  }

  return ret;
}

std::string find_file(const std::string &n) {
  assert(!n.empty());

  struct stat st;

  if(unlikely(n[0] == '/')) {
    if(stat(n.c_str(), &st) == 0 && S_ISREG(st.st_mode))
      return n;
    return std::string();
  }

  const std::vector<datapath> &paths = base_paths();

  for(unsigned int i = 0; i < paths.size(); i++) {
    if(fstatat(paths[i].fd, n.c_str(), &st, 0) == 0 && S_ISREG(st.st_mode))
      return paths[i].pathname + n;
  }

  return std::string();
}
