/*
 * Copyright (C) 2009 Till Harbaum <till@harbaum.org>.
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

#include "about.h"

#include "appdata.h"
#include "icon.h"
#include "misc.h"

#ifndef FREMANTLE
#define LINK_COLOR "blue"
#define PAYPAL_ICON "paypal.64"
#define OSM2GO_ICON "osm2go"
#else
#define LINK_COLOR "lightblue"
#define PAYPAL_ICON "paypal.32"
#define OSM2GO_ICON "osm2go.32"
#endif

#include <gtk/gtk.h>

#include <osm2go_cpp.h>

#ifdef ENABLE_BROWSER_INTERFACE

static gboolean on_link_clicked(GtkWidget *widget, GdkEventButton *,
				gpointer user_data) {

  const char *str =
    gtk_label_get_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(widget))));

  open_url((appdata_t*)user_data, str);
  return TRUE;
}
#endif

static GtkWidget *link_new(appdata_t *appdata, const char *url) {
#ifdef ENABLE_BROWSER_INTERFACE
  GtkWidget *label = gtk_label_new(O2G_NULLPTR);
  char *str = g_strconcat("<span color=\"" LINK_COLOR "\"><u>", url,
                          "</u></span>", O2G_NULLPTR);
  gtk_label_set_markup(GTK_LABEL(label), str);
  g_free(str);

  GtkWidget *eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);

  g_signal_connect(eventbox, "button-press-event",
                   G_CALLBACK(on_link_clicked), appdata);
  return eventbox;
#else
  GtkWidget *label = gtk_label_new(O2G_NULLPTR);
  char *str = g_strconcat("<span color=\"" LINK_COLOR "\">", url, "</span>", O2G_NULLPTR);
  gtk_label_set_markup(GTK_LABEL(label), str);
  g_free(str);
  return label;
#endif
}

#ifdef ENABLE_BROWSER_INTERFACE
static void on_paypal_button_clicked(appdata_t *appdata) {
  open_url(appdata,
	      "https://www.paypal.com/cgi-bin/webscr"
	      "?cmd=_s-xclick&hosted_button_id=7400558");
}
#endif

static GtkWidget *label_big(const char *str) {
  GtkWidget *label = gtk_label_new(O2G_NULLPTR);
  char *markup =
    g_markup_printf_escaped("<span size='x-large'>%s</span>", str);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  return label;
}

static GtkWidget *label_xbig(const char *str) {
  GtkWidget *label = gtk_label_new(O2G_NULLPTR);
  char *markup =
    g_markup_printf_escaped("<span size='xx-large'>%s</span>", str);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  return label;
}

static void
on_label_realize(GtkWidget *widget, gpointer)  {
  /* get parent size (which is a container) */
  gtk_widget_set_size_request(widget, widget->parent->allocation.width, -1);
}

static GtkWidget *label_wrap(const char *str) {
  GtkWidget *label = gtk_label_new(str);

  gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

  g_signal_connect(G_OBJECT(label), "realize",
		   G_CALLBACK(on_label_realize), O2G_NULLPTR);

  return label;
}

static GtkWidget *license_page_new(void) {
  GtkWidget *label = label_wrap(O2G_NULLPTR);
  GMappedFile *licMap = O2G_NULLPTR;

  const std::string &name = find_file("COPYING");
  if(!name.empty()) {
    licMap = g_mapped_file_new(name.c_str(), FALSE, O2G_NULLPTR);
  }

  if(licMap) {
    gchar *buffer = g_strndup(g_mapped_file_get_contents(licMap),
                              g_mapped_file_get_length(licMap));
#if GLIB_CHECK_VERSION(2,22,0)
    g_mapped_file_unref(licMap);
#else
    g_mapped_file_free(licMap);
#endif

    gtk_label_set_text(GTK_LABEL(label), buffer);

    g_free(buffer);
  } else
    gtk_label_set_text(GTK_LABEL(label), _("Load error"));

#ifndef FREMANTLE_PANNABLE_AREA
  GtkWidget *scrolled_window = gtk_scrolled_window_new(O2G_NULLPTR, O2G_NULLPTR);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
  				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window),
					label);
  gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW(scrolled_window),
				       GTK_SHADOW_IN);
  return scrolled_window;
#else
  GtkWidget *pannable_area = hildon_pannable_area_new();
  hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(pannable_area),
					label);
  return pannable_area;
#endif
}

static GtkWidget *copyright_page_new(appdata_t *appdata) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  /* ------------------------ */
  GtkWidget *ivbox = gtk_vbox_new(FALSE, 0);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
  GtkWidget *ihbox = gtk_hbox_new(FALSE, 20);
  gtk_box_pack_start(GTK_BOX(ihbox), appdata->icons.widget_load(OSM2GO_ICON),
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ihbox), label_xbig("OSM2Go"),
		     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), ihbox, TRUE, FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(ivbox), hbox);

  gtk_box_pack_start_defaults(GTK_BOX(ivbox),
		      label_big(_("Mobile OpenStreetMap Editor")));

  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  /* ------------------------ */
  ivbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start(GTK_BOX(ivbox),
		      gtk_label_new("Version " VERSION " (https://github.com/osm2go/osm2go)"), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ivbox),
		      gtk_label_new(__DATE__ " " __TIME__), FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  /* ------------------------ */
  ivbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start(GTK_BOX(ivbox),
	      gtk_label_new(_("Copyright 2008-2017")), FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(ivbox),
      link_new(appdata, "http://www.harbaum.org/till/maemo#osm2go"),
			      FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  return vbox;
}

/* a label that is left aligned */
static GtkWidget *left_label(const char *str) {
  GtkWidget *widget = gtk_label_new(str);
  gtk_misc_set_alignment(GTK_MISC(widget), 0.0f, 0.5f);
  return widget;
}

static void author_add(GtkWidget *box, const char *str) {
  gtk_box_pack_start(GTK_BOX(box), left_label(str), FALSE, FALSE, 0);
}

static GtkWidget *authors_page_new(void) {
  GtkWidget *ivbox, *vbox = gtk_vbox_new(FALSE, 16);

  /* -------------------------------------------- */
  ivbox = gtk_vbox_new(FALSE, 0);
  author_add(ivbox, _("Main developers:"));
  author_add(ivbox, "Till Harbaum <till@harbaum.org>");
  author_add(ivbox, "Andrew Chadwick <andrewc-osm2go@piffle.org>");
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  /* -------------------------------------------- */
  ivbox = gtk_vbox_new(FALSE, 0);
  author_add(ivbox, _("Patches by:"));
  author_add(ivbox, "Rolf Bode-Meyer <robome@gmail.com>");
  author_add(ivbox, "Rolf Eike Beer <eike@sf-mail.de>");
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  /* -------------------------------------------- */
  ivbox = gtk_vbox_new(FALSE, 0);
  author_add(ivbox, _("Icon artwork by:"));
  author_add(ivbox, "Andrew Zhilin <drew.zhilin@gmail.com>"),
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  /* -------------------------------------------- */
  ivbox = gtk_vbox_new(FALSE, 0);
  author_add(ivbox, _("Original map widget by:"));
  author_add(ivbox, "John Stowers <john.stowers@gmail.com>");
  author_add(ivbox, "Marcus Bauer <marcus.bauer@gmail.com>"),
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  /* -------------------------------------------- */
  ivbox = gtk_vbox_new(FALSE, 0);
  author_add(ivbox, _("Testers:"));
  author_add(ivbox, "Christoph Eckert <ce@christeck.de>");
  author_add(ivbox, "Claudius Henrichs <claudius.h@gmx.de>");
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

#ifndef FREMANTLE_PANNABLE_AREA
  GtkWidget *scrolled_window = gtk_scrolled_window_new(O2G_NULLPTR, O2G_NULLPTR);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
  				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window),
					vbox);
  gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW(scrolled_window),
				       GTK_SHADOW_IN);
  return scrolled_window;
#else
  GtkWidget *pannable_area = hildon_pannable_area_new();
  hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(pannable_area),
					vbox);
  return pannable_area;
#endif
}

static GtkWidget *donate_page_new(appdata_t *appdata) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
      label_wrap(_("If you like OSM2Go and want to support its future development "
		   "please consider donating to the developer. You can either "
		   "donate via paypal to")));

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
			      link_new(O2G_NULLPTR, "till@harbaum.org"));

#ifdef ENABLE_BROWSER_INTERFACE
  gtk_box_pack_start_defaults(GTK_BOX(vbox),
      label_wrap(_("or you can just click the button below which will open "
		   "the appropriate web page in your browser.")));

  GtkWidget *ihbox = gtk_hbox_new(FALSE, 0);
  GtkWidget *button = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(button), appdata->icons.widget_load(PAYPAL_ICON));
  gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
  g_signal_connect_swapped(button, "clicked",
                           G_CALLBACK(on_paypal_button_clicked), appdata);
  gtk_box_pack_start(GTK_BOX(ihbox), button, TRUE, FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), ihbox);
#endif

  return vbox;
}

static GtkWidget *bugs_page_new(appdata_t *appdata) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
      label_wrap(_("Please report bugs or feature requests via the OSM2Go "
		   "bug tracker. This bug tracker can directly be reached via "
		   "the following link:")));

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
       link_new(appdata, "https://github.com/osm2go/osm2go/issues"));

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
      label_wrap(_("You might also be interested in joining the mailing lists "
		   "or the forum:")));

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
	      link_new(appdata, "http://garage.maemo.org/projects/osm2go/"));

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
      label_wrap(_("Thank you for contributing!")));

  return vbox;
}

void about_box(appdata_t *appdata) {
  GtkWidget *dialog = gtk_dialog_new_with_buttons(_("About OSM2Go"),
	  GTK_WINDOW(appdata->window), GTK_DIALOG_MODAL,
          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, O2G_NULLPTR);

#ifdef USE_HILDON
  gtk_window_set_default_size(GTK_WINDOW(dialog), 640, 480);
#else
  gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 200);
#endif

  GtkWidget *notebook = notebook_new();

  notebook_append_page(notebook, copyright_page_new(appdata), _("Copyright"));
  notebook_append_page(notebook, license_page_new(),          _("License"));
  notebook_append_page(notebook, authors_page_new(),          _("Authors"));
  notebook_append_page(notebook, donate_page_new(appdata),    _("Donate"));
  notebook_append_page(notebook, bugs_page_new(appdata),      _("Bugs"));

  gtk_box_pack_start_defaults(GTK_BOX((GTK_DIALOG(dialog))->vbox),
			      notebook);

  gtk_widget_show_all(dialog);

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}
