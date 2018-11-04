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

#include "osm2go_i18n.h"
#include "osm2go_platform.h"
#include "osm2go_platform_gtk.h"

#include <appdata.h>
#include <color.h>
#include <pos.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>
#ifdef FREMANTLE
#include <hildon/hildon-note.h>
#endif
#include <sys/stat.h>

#include <osm2go_annotations.h>

void osm2go_platform::gtk_widget_deleter::operator()(GtkWidget *mem) {
  gtk_widget_destroy(mem);
}

osm2go_platform::DialogGuard::DialogGuard(GtkWidget *dlg)
  : WidgetGuard(dlg)
{
  assert(GTK_WINDOW(dlg) != nullptr);
  assert(GTK_DIALOG(dlg) != nullptr);
}

void osm2go_platform::DialogGuard::reset(GtkWidget *dlg)
{
  assert(GTK_WINDOW(dlg) != nullptr);
  assert(GTK_DIALOG(dlg) != nullptr);
  WidgetGuard::reset(dlg);
}

GtkBox *osm2go_platform::DialogGuard::vbox()
{
  return GTK_BOX(reinterpret_cast<GtkDialog *>(get())->vbox);
}

void osm2go_platform::process_events()
{
  while(gtk_events_pending() == TRUE)
    gtk_main_iteration();
}

void osm2go_platform::Timer::restart(unsigned int seconds, GSourceFunc callback, void *data)
{
  assert_cmpnum(id, 0);
  id = g_timeout_add_seconds(seconds, callback, data);
}

void osm2go_platform::Timer::stop()
{
  if(likely(id != 0)) {
    g_source_remove(id);
    id = 0;
  }
}

void osm2go_platform::dialog_size_hint(GtkWindow *window, osm2go_platform::DialogSizeHint hint)
{
  static const std::array<std::array<gint, 2>, _MISC_DIALOG_SIZEHINT_COUNT> dialog_sizes = { {
#ifdef FREMANTLE
    { { 400, 100 } },  // SMALL
    /* in maemo5 most dialogs are full screen */
    { { 800, 480 } },  // MEDIUM
    { { 790, 380 } },  // LARGE
    { { 640, 100 } },  // WIDE
    { { 450, 480 } },  // HIGH
#else
    { { 300, 100 } },  // SMALL
    { { 400, 300 } },  // MEDIUM
    { { 500, 350 } },  // LARGE
    { { 450, 100 } },  // WIDE
    { { 200, 350 } },  // HIGH
#endif
  }};

  gtk_window_set_default_size(window, dialog_sizes.at(hint).at(0), dialog_sizes.at(hint).at(1));
}

osm2go_platform::MappedFile::MappedFile(const char *fname)
  : map(g_mapped_file_new(fname, FALSE, nullptr))
{
}

const char *osm2go_platform::MappedFile::data()
{
  return g_mapped_file_get_contents(map);
}

size_t osm2go_platform::MappedFile::length()
{
  return g_mapped_file_get_length(map);
}

void osm2go_platform::MappedFile::reset()
{
  if(likely(map != nullptr)) {
#if GLIB_CHECK_VERSION(2,22,0)
    g_mapped_file_unref(map);
#else
    g_mapped_file_free(map);
#endif
    map = nullptr;
  }
}

bool osm2go_platform::parse_color_string(const char *str, color_t &color)
{
  /* we parse this directly since gdk_color_parse doesn't cope */
  /* with the alpha channel that may be present */
  if (strlen(str + 1) == 8) {
    char *err;

    color = strtoul(str + 1, &err, 16);

    return (*err == '\0');
  } else {
    GdkColor gdk_color;
    if(gdk_color_parse(str, &gdk_color) == TRUE) {
      color = color_t(gdk_color.red, gdk_color.green, gdk_color.blue);
      return true;
    }
    return false;
  }
}

static GdkColor parseRed()
{
  GdkColor color;
  gdk_color_parse("red", &color);
  return color;
}

const GdkColor *osm2go_platform::invalid_text_color()
{
  static const GdkColor red = parseRed();

  return &red;
}

double osm2go_platform::string_to_double(const char *str)
{
  if(likely(str != nullptr))
    return g_ascii_strtod(str, nullptr);
  else
    return NAN;
}

std::string trstring::argn(const std::string &spattern, const std::string &a, std::string::size_type pos) const
{
  std::string smsg = *this;

  while (pos != std::string::npos) {
    smsg.replace(pos, spattern.size(), a);
    pos = smsg.find(spattern, pos + a.size());
  }

  return trstring(smsg);
}

trstring trstring::arg(const std::string &a) const
{
  // increase as needed
  std::string spattern = "%1";
  std::string::size_type pos = find(spattern);
  for (int i = 1; i < 3 && pos == std::string::npos; i++) {
    spattern = '%' + std::to_string(i);
    pos = find(spattern);
  }

  if(unlikely(pos == std::string::npos))
    g_debug("no placeholder found in string: '%s'", c_str());

  return trstring(argn(spattern, a, pos));
}

#ifndef FREMANTLE
#define RESPONSE_YES  GTK_RESPONSE_YES
#define RESPONSE_NO   GTK_RESPONSE_NO
#else
/* hildon names the yes/no buttons ok/cancel ??? */
#define RESPONSE_YES  GTK_RESPONSE_OK
#define RESPONSE_NO   GTK_RESPONSE_CANCEL
#endif

static void on_toggled(GtkWidget *button, const int *flags)
{
  gboolean not_active = osm2go_platform::check_button_get_active(button) ? FALSE : TRUE;

  GtkDialog *dialog = GTK_DIALOG(gtk_widget_get_toplevel(button));

  if(GPOINTER_TO_UINT(flags) & MISC_AGAIN_FLAG_DONT_SAVE_NO)
    gtk_dialog_set_response_sensitive(dialog, RESPONSE_NO, not_active);

  else if(GPOINTER_TO_UINT(flags) & MISC_AGAIN_FLAG_DONT_SAVE_YES)
    gtk_dialog_set_response_sensitive(dialog, RESPONSE_YES, not_active);
}

bool osm2go_platform::yes_no(const char *title, const char *msg, unsigned int again_flags,
                             osm2go_platform::Widget *parent)
{
  /* flags used to prevent re-appearence of dialogs */
  static struct {
    unsigned int not_again;     /* bit is set if dialog is not to be displayed again */
    unsigned int reply;         /* reply to be assumed if "not_again" bit is set */
  } dialog_again;
  const unsigned int again_bit = again_flags & ~(MISC_AGAIN_FLAG_DONT_SAVE_NO | MISC_AGAIN_FLAG_DONT_SAVE_YES);

  if(dialog_again.not_again & again_bit)
    return ((dialog_again.reply & again_bit) != 0);

  printf("%s: \"%s\"\n", title, msg);

  GtkWindow *p = GTK_WINDOW(parent ? parent : appdata_t::window);

  osm2go_platform::DialogGuard dialog(
#ifndef FREMANTLE
                  gtk_message_dialog_new(p, GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                                         "%s", msg));

  gtk_window_set_title(dialog, title);
#else
                  hildon_note_new_confirmation(p, msg));
#endif

  osm2go_platform::Widget *cbut = nullptr;
  if(again_bit) {
#ifdef FREMANTLE
    /* make sure there's some space before the checkbox */
    gtk_box_pack_start(dialog.vbox(), gtk_label_new(" "), TRUE, TRUE, 0);
#endif

    GtkWidget *alignment = gtk_alignment_new(0.5, 0, 0, 0);

    cbut = osm2go_platform::check_button_new_with_label(_("Don't ask this question again"));
    g_signal_connect(cbut, "toggled", G_CALLBACK(on_toggled), GUINT_TO_POINTER(again_flags));

    gtk_container_add(GTK_CONTAINER(alignment), cbut);
    gtk_box_pack_start(dialog.vbox(), alignment, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog.get());
  }

  bool yes = (gtk_dialog_run(dialog) == RESPONSE_YES);

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

bool osm2go_platform::yes_no(const char *title, const trstring &msg, unsigned int again_flags,
                             osm2go_platform::Widget *parent)
{
  return yes_no(title, static_cast<const gchar *>(msg), again_flags, parent);
}

bool osm2go_platform::yes_no(const trstring &title, const trstring &msg, unsigned int again_flags,
                             osm2go_platform::Widget *parent)
{
  return yes_no(static_cast<const gchar *>(title), static_cast<const gchar *>(msg), again_flags, parent);
}

const std::vector<dirguard> &osm2go_platform::base_paths()
{
/* all entries must contain a trailing '/' ! */
  static std::vector<dirguard> ret;

  if(unlikely(ret.empty())) {
    std::vector<std::string> pathnames;

    const char *home = g_get_home_dir();
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
      dirguard dfd(pathnames[i].c_str());
      if(dfd.valid()) {
#if __cplusplus >= 201103L
        ret.emplace_back(std::move(dfd));
#else
        ret.push_back(dfd);
#endif
      }
    }

    assert(!ret.empty());
  }

  return ret;
}

std::string osm2go_platform::find_file(const std::string &n)
{
  assert(!n.empty());

  struct stat st;

  if(unlikely(n[0] == '/')) {
    if(stat(n.c_str(), &st) == 0 && S_ISREG(st.st_mode))
      return n;
    return std::string();
  }

  const std::vector<dirguard> &paths = osm2go_platform::base_paths();
  const std::vector<dirguard>::const_iterator itEnd = paths.end();

  for(std::vector<dirguard>::const_iterator it = paths.begin(); it != itEnd; it++) {
    if(fstatat(it->dirfd(), n.c_str(), &st, 0) == 0 && S_ISREG(st.st_mode))
      return it->path() + n;
  }

  return std::string();
}

dirguard osm2go_platform::userdatapath()
{
  return dirguard(std::string(g_get_user_data_dir()) + "/osm2go/presets/");
}

bool osm2go_platform::create_directories(const std::string &path)
{
  return g_mkdir_with_parents(path.c_str(), S_IRWXU) == 0;
}
