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

#include <notifications.h>

#include "appdata.h"
#include "misc.h"

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
                      const char *title, const char *msg) {
  GtkWindow *wnd = GTK_WINDOW(parent ? parent : appdata_t::window);

  if(unlikely(wnd == nullptr)) {
    g_debug("%s", msg);
    return;
  }

  osm2go_platform::WidgetGuard dialog(
#ifndef FREMANTLE
                  gtk_message_dialog_new(wnd, GTK_DIALOG_DESTROY_WITH_PARENT,
                                         type, buttons, "%s", msg));

  gtk_window_set_title(GTK_WINDOW(dialog.get()), title);
#else
                  hildon_note_new_information(wnd, msg));
  (void)type;
  (void)buttons;
  (void)title;
#endif // FREMANTLE

  gtk_dialog_run(GTK_DIALOG(dialog.get()));
}

void message_dlg(const char *title, const char *msg, GtkWidget *parent)
{
  vmessage(parent, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, title, msg);
}

void message_dlg(const char *title, const trstring &msg, GtkWidget *parent)
{
  vmessage(parent, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, title, msg.c_str());
}

void error_dlg(const char *msg, GtkWidget *parent)
{
  vmessage(parent, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("Error"), msg);
}

void error_dlg(const trstring &msg, GtkWidget *parent)
{
  vmessage(parent, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("Error"), msg.c_str());
}

void warning_dlg(const char *msg, GtkWidget *parent)
{
  vmessage(parent, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, _("Warning"), msg);
}

void warning_dlg(const trstring &msg, GtkWidget *parent)
{
  vmessage(parent, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, _("Warning"), msg.c_str());
}
