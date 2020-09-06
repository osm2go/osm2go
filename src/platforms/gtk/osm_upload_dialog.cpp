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

#include <osm_api.h>
#include <osm_api_p.h>

#include <appdata.h>
#include <diff.h>
#include <map.h>
#include <net_io.h>
#include <osm.h>
#include <project.h>
#include <settings.h>
#include <uicontrol.h>

#include <cstring>
#include <gtk/gtk.h>

#ifdef FREMANTLE
#include <hildon/hildon-text-view.h>
#endif

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include "osm2go_platform.h"
#include "osm2go_platform_gtk.h"
#include <osm2go_stl.h>

namespace {

class osm_upload_context_gtk : public osm_upload_context_t {
public:
  osm_upload_context_gtk(appdata_t &a, project_t::ref p, const char *c, const char *s);
  osm_upload_context_gtk() O2G_DELETED_FUNCTION;
  osm_upload_context_gtk(const osm_upload_context_gtk &) O2G_DELETED_FUNCTION;
  osm_upload_context_gtk &operator=(const osm_upload_context_gtk &) O2G_DELETED_FUNCTION;
#if __cplusplus >= 201103L
  osm_upload_context_gtk(osm_upload_context_gtk &&) = delete;
  osm_upload_context_gtk &operator=(osm_upload_context_gtk &&) = delete;
#endif

  GtkTextBuffer * const logbuffer;
  GtkTextView * const logview;
};

osm_upload_context_gtk::osm_upload_context_gtk(appdata_t &a, project_t::ref p, const char *c, const char *s)
  : osm_upload_context_t(a, p, c, s)
  , logbuffer(gtk_text_buffer_new(nullptr))
  , logview(GTK_TEXT_VIEW(gtk_text_view_new_with_buffer(logbuffer)))
{
}

} // namespace

osm_upload_context_t::osm_upload_context_t(appdata_t &a, project_t::ref p, const char *c, const char *s)
  : appdata(a)
  , osm(p->osm)
  , project(p)
  , urlbasestr(p->server(settings_t::instance()->server) + '/')
  , comment(c)
  , src(s == nullptr ? s : std::string())
{
}

void osm_upload_context_t::append_str(const char *msg, const char *colorname)
{
  g_debug("%s", msg);

  osm_upload_context_gtk * const gtk_this = static_cast<osm_upload_context_gtk *>(this);
  GtkTextBuffer * const logbuffer = gtk_this->logbuffer;
  GtkTextView * const logview = gtk_this->logview;

  GtkTextIter end;
  gtk_text_buffer_get_end_iter(logbuffer, &end);
  if(colorname != nullptr) {
    GtkTextTag *tag = gtk_text_buffer_create_tag(logbuffer, nullptr,
                                                 "foreground", colorname,
                                                 nullptr);
    gtk_text_buffer_insert_with_tags(logbuffer, &end, msg, -1, tag, nullptr);
  } else
    gtk_text_buffer_insert(logbuffer, &end, msg, -1);

  gtk_text_view_scroll_to_iter(logview, &end, 0.0, FALSE, 0, 0);

  osm2go_platform::process_events();
}

void osm_upload_context_t::append(trstring::arg_type msg, const char *colorname)
{
  append_str(static_cast<const gchar *>(static_cast<trstring::native_type>(msg)), colorname);
}

namespace {

GtkWidget *
table_attach_label_c(GtkWidget *table, const char *str, int x, int y)
{
  GtkWidget *label =  gtk_label_new(str);
  gtk_table_attach_defaults(GTK_TABLE(table), label, x, x + 1, y, y + 1);
  return label;
}

void
table_attach_label_l(GtkWidget *table, int y, const char *str)
{
  GtkWidget *label = table_attach_label_c(table, str, 0, y);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
}

void
table_attach_int(GtkWidget *table, int x, int y, unsigned int num)
{
  char str[G_ASCII_DTOSTR_BUF_SIZE];
  snprintf(str, sizeof(str), "%u", num);
  table_attach_label_c(table, str, x, y);
}

/* comment buffer has been edited, allow upload if the buffer is not empty */
void
callback_buffer_modified(GtkTextBuffer *buffer, GtkDialog *dialog)
{
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
  gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_ACCEPT,
                                    (text != nullptr && *text != '\0') ? TRUE : FALSE);
}

gboolean
cb_focus_in(GtkTextView *view, GdkEventFocus *, GtkTextBuffer *buffer)
{
  gint first_click = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(view), "first_click"));

  if(first_click == 1) {
    GtkTextIter start, end;
    g_object_set_data(G_OBJECT(view), "first_click", GINT_TO_POINTER(2));
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_delete(buffer, &start, &end);
  }

  return FALSE;
}

template<typename T>
static void table_insert_count(GtkWidget *table, const osm_t::dirty_t::counter<T> &counter, int row)
{
  table_attach_int(table, 1, row, counter.total);
  table_attach_int(table, 2, row, counter.added.size());
  table_attach_int(table, 3, row, counter.changed.size());
  table_attach_int(table, 4, row, counter.deleted.size());
}

void
details_table(osm2go_platform::DialogGuard &dialog, const osm_t::dirty_t &dirty)
{
  GtkWidget *table = gtk_table_new(4, 5, TRUE);

  table_attach_label_c(table, _("Total"),    1, 0);
  table_attach_label_c(table, _("New"),      2, 0);
  table_attach_label_c(table, _("Modified"), 3, 0);
  table_attach_label_c(table, _("Deleted"),  4, 0);

  int row = 1;
  table_attach_label_l(table, row, _("Nodes:"));
  table_insert_count(table, dirty.nodes, row++);

  table_attach_label_l(table, row, _("Ways:"));
  table_insert_count(table, dirty.ways, row++);

  table_attach_label_l(table, row, _("Relations:"));
  table_insert_count(table, dirty.relations, row++);

  gtk_box_pack_start(dialog.vbox(), table, FALSE, FALSE, 0);
}

#ifdef FREMANTLE
/* put additional infos into a seperate dialog for fremantle as */
/* screen space is sparse there */
void
info_more(const osm_t::dirty_t &context, GtkWidget *parent)
{
  osm2go_platform::DialogGuard dialog(gtk_dialog_new_with_buttons(_("Changeset details"),
                                      GTK_WINDOW(parent), GTK_DIALOG_MODAL,
                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, nullptr));

  osm2go_platform::dialog_size_hint(dialog, osm2go_platform::MISC_DIALOG_SMALL);
  gtk_dialog_set_default_response(dialog, GTK_RESPONSE_CANCEL);

  details_table(dialog, context);
  gtk_widget_show_all(dialog.get());
  gtk_dialog_run(dialog);
}
#endif

} // namespace

void osm_upload_dialog(appdata_t &appdata, const osm_t::dirty_t &dirty)
{
  osm2go_platform::DialogGuard dialog(gtk_dialog_new_with_buttons(_("Upload to OSM"),
                                              GTK_WINDOW(appdata_t::window),
                                              GTK_DIALOG_MODAL,
#ifdef FREMANTLE
                                              _("More"), GTK_RESPONSE_HELP,
#endif
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              nullptr));

  osm2go_platform::dialog_size_hint(dialog, osm2go_platform::MISC_DIALOG_MEDIUM);

#ifndef FREMANTLE
  details_table(dialog, dirty);

  /* ------------------------------------------------------ */

  gtk_box_pack_start(dialog.vbox(), gtk_hseparator_new(), FALSE, FALSE, 0);
#endif

  /* ------- add username and password entries ------------ */

  GtkWidget *table = gtk_table_new(2, 2, FALSE);
  table_attach_label_l(table, 0, _("Username:"));
  GtkWidget *uentry = osm2go_platform::entry_new(osm2go_platform::EntryFlagsNoAutoCap);

  settings_t::ref settings = settings_t::instance();
  osm2go_platform::setEntryText(GTK_ENTRY(uentry), settings->username.c_str(),
                                _("<your osm username>"));

  gtk_table_attach_defaults(GTK_TABLE(table),  uentry, 1, 2, 0, 1);
  table_attach_label_l(table, 1, _("Password:"));
  GtkWidget *pentry = osm2go_platform::entry_new(osm2go_platform::EntryFlagsNoAutoCap);
  if(!settings->password.empty())
    gtk_entry_set_text(GTK_ENTRY(pentry), settings->password.c_str());
  gtk_entry_set_visibility(GTK_ENTRY(pentry), FALSE);
  gtk_table_attach_defaults(GTK_TABLE(table),  pentry, 1, 2, 1, 2);
  gtk_box_pack_start(dialog.vbox(), table, FALSE, FALSE, 0);

  table_attach_label_l(table, 2, _("Source:"));
  GtkWidget *sentry = osm2go_platform::entry_new(osm2go_platform::EntryFlagsNoAutoCap);
  gtk_table_attach_defaults(GTK_TABLE(table),  sentry, 1, 2, 2, 3);
  gtk_box_pack_start(dialog.vbox(), table, FALSE, FALSE, 0);

  GtkTextBuffer *buffer = gtk_text_buffer_new(nullptr);
  trstring::native_type placeholder_comment = _("Please add a comment");

  /* disable ok button until user edited the comment */
  gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_ACCEPT, FALSE);

  g_signal_connect(buffer, "changed", G_CALLBACK(callback_buffer_modified), dialog.get());

  GtkTextView *view = GTK_TEXT_VIEW(
#ifndef FREMANTLE
                    gtk_text_view_new_with_buffer(buffer));
  gtk_text_buffer_set_text(buffer, placeholder_comment, -1);
#else
                    hildon_text_view_new());
  gtk_text_view_set_buffer(view, buffer);
  hildon_gtk_text_view_set_placeholder_text(view, placeholder_comment);
#endif

  gtk_text_view_set_wrap_mode(view, GTK_WRAP_WORD);
  gtk_text_view_set_editable(view, TRUE);
  gtk_text_view_set_left_margin(view, 2 );
  gtk_text_view_set_right_margin(view, 2 );

  g_object_set_data(G_OBJECT(view), "first_click", GINT_TO_POINTER(1));
  g_signal_connect(view, "focus-in-event", G_CALLBACK(cb_focus_in), buffer);

  gtk_box_pack_start(dialog.vbox(), osm2go_platform::scrollable_container(GTK_WIDGET(view)), TRUE, TRUE, 0);
  gtk_widget_show_all(dialog.get());

  bool done = false;
  while(!done) {
    switch(gtk_dialog_run(dialog)) {
#ifdef FREMANTLE
    case GTK_RESPONSE_HELP:
      info_more(dirty, dialog.get());
      break;
#endif
    case GTK_RESPONSE_ACCEPT:
      done = true;
      break;
    default:
      g_debug("upload cancelled");
      return;
    }
  }

  g_debug("clicked ok");

  /* retrieve username and password */
  settings->username = gtk_entry_get_text(GTK_ENTRY(uentry));
  settings->password = gtk_entry_get_text(GTK_ENTRY(pentry));

  /* fetch comment from dialog */
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

  project_t::ref project = appdata.project;
  /* server url should not end with a slash */
  if(!project->rserver.empty() && ends_with(project->rserver, '/')) {
    g_debug("removing trailing slash");
    project->rserver.erase(project->rserver.size() - 1);
  }

  osm_upload_context_gtk context(appdata, project, text,
                                 gtk_entry_get_text(GTK_ENTRY(sentry)));

  dialog.reset();
  project->save();

  dialog.reset(gtk_dialog_new_with_buttons(_("Uploading"), GTK_WINDOW(appdata_t::window),
                                           GTK_DIALOG_MODAL,
                                           GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                           nullptr));

  osm2go_platform::dialog_size_hint(dialog, osm2go_platform::MISC_DIALOG_LARGE);
  gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_CLOSE, FALSE);

  /* ------- main ui element is this text view --------------- */

  /* create a scrolled window */
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr,
                                                                                   nullptr));
  gtk_scrolled_window_set_policy(scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_scrolled_window_set_shadow_type(scrolled_window, GTK_SHADOW_IN);

  gtk_text_view_set_editable(context.logview, FALSE);
  gtk_text_view_set_cursor_visible(context.logview, FALSE);
  gtk_text_view_set_wrap_mode(context.logview, GTK_WRAP_WORD);

  gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(context.logview));

  gtk_box_pack_start(dialog.vbox(), GTK_WIDGET(scrolled_window), TRUE, TRUE, 0);
  gtk_widget_show_all(dialog.get());

  context.upload(dirty, dialog.get());

  gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_CLOSE, TRUE);

  gtk_dialog_run(dialog);
}
