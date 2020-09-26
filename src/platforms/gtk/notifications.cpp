/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <notifications.h>

#include "appdata.h"

#include <cstdio>
#ifdef FREMANTLE
#include <hildon/hildon-note.h>
#else
#include <gtk/gtk.h>
#endif

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include "osm2go_platform_gtk.h"

static void
vmessage(osm2go_platform::Widget *parent, GtkMessageType type, GtkButtonsType buttons,
                      trstring::arg_type title, const char *msg)
{
  GtkWindow *wnd = GTK_WINDOW(parent ? parent : appdata_t::window);

  if(unlikely(wnd == nullptr)) {
    g_debug("%s", msg);
    return;
  }

  osm2go_platform::DialogGuard dialog(
#ifndef FREMANTLE
                  gtk_message_dialog_new(wnd, GTK_DIALOG_DESTROY_WITH_PARENT,
                                         type, buttons, "%s", msg));

  gtk_window_set_title(dialog, title);
#else
                  hildon_note_new_information(wnd, msg));
  (void)type;
  (void)buttons;
  (void)title;
#endif // FREMANTLE

  gtk_dialog_run(dialog);
}

void message_dlg(trstring::arg_type title, trstring::arg_type msg, GtkWidget *parent)
{
  vmessage(parent, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, title,
           static_cast<const gchar *>(static_cast<trstring::native_type>(msg)));
}

void error_dlg(trstring::arg_type msg, GtkWidget *parent)
{
  vmessage(parent, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("Error"), static_cast<const gchar *>(static_cast<trstring::native_type>(msg)));
}

void warning_dlg(trstring::arg_type msg, GtkWidget *parent)
{
  vmessage(parent, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, _("Warning"), static_cast<const gchar *>(static_cast<trstring::native_type>(msg)));
}
