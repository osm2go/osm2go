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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
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

#ifdef FREMANTLE
#include <hildon/hildon-pannable-area.h>
#endif
#include <gtk/gtk.h>

#include <osm2go_cpp.h>
#include "osm2go_i18n.h"
#include <osm2go_platform.h>

static gboolean on_link_clicked(GtkWidget *widget) {
  const char *str = gtk_label_get_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(widget))));

  osm2go_platform::open_url(str);
  return TRUE;
}

static GtkWidget *link_new(const char *url) {
  GtkWidget *label = gtk_label_new(O2G_NULLPTR);
  std::string str = "<span color=\"" LINK_COLOR "\"><u>" + std::string(url) +
                          "</u></span>";
  gtk_label_set_markup(GTK_LABEL(label), str.c_str());

  GtkWidget *eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);

  g_signal_connect(eventbox, "button-press-event",
                   G_CALLBACK(on_link_clicked), O2G_NULLPTR);
  return eventbox;
}

static void on_paypal_button_clicked() {
  osm2go_platform::open_url("https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=7400558");
}

static GtkWidget *label_scale(const char *str, double scale_factor) {
  PangoAttrList *attrs = pango_attr_list_new();
  pango_attr_list_change(attrs, pango_attr_scale_new(scale_factor));
  GtkWidget *label = gtk_label_new(str);
  gtk_label_set_attributes(GTK_LABEL(label), attrs);
  pango_attr_list_unref(attrs);
  return label;
}

static void
on_label_realize(GtkWidget *widget) {
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
    const std::string buffer(g_mapped_file_get_contents(licMap),
                             g_mapped_file_get_length(licMap));
#if GLIB_CHECK_VERSION(2,22,0)
    g_mapped_file_unref(licMap);
#else
    g_mapped_file_free(licMap);
#endif

    gtk_label_set_text(GTK_LABEL(label), buffer.c_str());
  } else
    gtk_label_set_text(GTK_LABEL(label), _("Load error"));

#ifndef FREMANTLE
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

static GtkWidget *copyright_page_new(icon_t &icons) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  /* ------------------------ */
  GtkWidget *ivbox = gtk_vbox_new(FALSE, 0);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
  GtkWidget *ihbox = gtk_hbox_new(FALSE, 20);
  gtk_box_pack_start(GTK_BOX(ihbox), icons.widget_load(OSM2GO_ICON), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ihbox), label_scale("OSM2Go", PANGO_SCALE_XX_LARGE),
		     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), ihbox, TRUE, FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(ivbox), hbox);

  gtk_box_pack_start_defaults(GTK_BOX(ivbox),
                              label_scale(_("Mobile OpenStreetMap Editor"), PANGO_SCALE_X_LARGE));

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
                     link_new("http://www.harbaum.org/till/maemo#osm2go"),
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
  author_add(ivbox, "Rolf Eike Beer <eike@sf-mail.de>");
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  /* -------------------------------------------- */
  ivbox = gtk_vbox_new(FALSE, 0);
  author_add(ivbox, _("Patches by:"));
  author_add(ivbox, "Rolf Bode-Meyer <robome@gmail.com>");
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

#ifndef FREMANTLE
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

static GtkWidget *donate_page_new(icon_t &icons) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
      label_wrap(_("If you like OSM2Go and want to support its future development "
		   "please consider donating to the developer. You can either "
		   "donate via paypal to")));

  gtk_box_pack_start_defaults(GTK_BOX(vbox), link_new("till@harbaum.org"));

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
      label_wrap(_("or you can just click the button below which will open "
		   "the appropriate web page in your browser.")));

  GtkWidget *ihbox = gtk_hbox_new(FALSE, 0);
  GtkWidget *button = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(button), icons.widget_load(PAYPAL_ICON));
  gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
  g_signal_connect_swapped(button, "clicked",
                           G_CALLBACK(on_paypal_button_clicked), O2G_NULLPTR);
  gtk_box_pack_start(GTK_BOX(ihbox), button, TRUE, FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), ihbox);

  return vbox;
}

static GtkWidget *bugs_page_new() {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
      label_wrap(_("Please report bugs or feature requests via the OSM2Go "
		   "bug tracker. This bug tracker can directly be reached via "
		   "the following link:")));

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
                              link_new("https://github.com/osm2go/osm2go/issues"));

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
      label_wrap(_("You might also be interested in joining the mailing lists "
		   "or the forum:")));

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
                              link_new("https://garage.maemo.org/projects/osm2go/"));

  gtk_box_pack_start_defaults(GTK_BOX(vbox),
      label_wrap(_("Thank you for contributing!")));

  return vbox;
}

void about_box(appdata_t *appdata) {
  g_widget dialog(gtk_dialog_new_with_buttons(_("About OSM2Go"),
                                              GTK_WINDOW(appdata->window), GTK_DIALOG_MODAL,
                                              GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, O2G_NULLPTR));

  gtk_window_set_default_size(GTK_WINDOW(dialog.get()),
#ifdef FREMANTLE
                              640, 480);
#else
                              400, 200);
#endif

  GtkWidget *notebook = notebook_new();

  notebook_append_page(notebook, copyright_page_new(appdata->icons), _("Copyright"));
  notebook_append_page(notebook, license_page_new(),                 _("License"));
  notebook_append_page(notebook, authors_page_new(),                 _("Authors"));
  notebook_append_page(notebook, donate_page_new(appdata->icons),    _("Donate"));
  notebook_append_page(notebook, bugs_page_new(),                    _("Bugs"));

  gtk_box_pack_start_defaults(GTK_BOX((GTK_DIALOG(dialog.get()))->vbox),
			      notebook);

  gtk_widget_show_all(dialog.get());

  gtk_dialog_run(GTK_DIALOG(dialog.get()));
}
