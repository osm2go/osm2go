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

#include <uicontrol.h>

#include "appdata.h"
#include "icon.h"

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
#include <string>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include "osm2go_i18n.h"
#include "osm2go_platform.h"
#include "osm2go_platform_gtk.h"

namespace {

GtkWidget *
link_new(const char *url)
{
  GtkWidget *label = gtk_label_new(nullptr);
  std::string str = "<span color=\"" LINK_COLOR "\"><u>" + std::string(url) +
                          "</u></span>";
  gtk_label_set_markup(GTK_LABEL(label), str.c_str());

  GtkWidget *eventbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox), label);

  g_signal_connect_swapped(eventbox, "button-press-event",
                   G_CALLBACK(osm2go_platform::open_url), const_cast<char *>(url));
  return eventbox;
}

GtkWidget *
label_scale(trstring::native_type_arg str, double scale_factor)
{
  PangoAttrList *attrs = pango_attr_list_new();
  pango_attr_list_change(attrs, pango_attr_scale_new(scale_factor));
  GtkWidget *label = gtk_label_new(str);
  gtk_label_set_attributes(GTK_LABEL(label), attrs);
  pango_attr_list_unref(attrs);
  return label;
}

void
on_label_realize(GtkWidget *widget)
{
  /* get parent size (which is a container) */
  gtk_widget_set_size_request(widget, widget->parent->allocation.width, -1);
}

GtkWidget *
label_wrap(const char *str)
{
  GtkWidget *label = gtk_label_new(str);

  gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

  g_signal_connect(label, "realize", G_CALLBACK(on_label_realize), nullptr);

  return label;
}

inline GtkWidget *
label_wrap(trstring::native_type_arg str)
{
  return label_wrap(static_cast<const gchar *>(str));
}

GtkWidget *
license_page_new()
{
  GtkWidget *label = nullptr;

  const std::string &name = osm2go_platform::find_file("COPYING");
  if(likely(!name.empty())) {
    osm2go_platform::MappedFile licMap(name);

    if(licMap) {
      const std::string buffer(licMap.data(), licMap.length());

      label = label_wrap(buffer.c_str());
    }
  }

  if(unlikely(label == nullptr))
    label = label_wrap(_("Load error"));

  GtkWidget *ret;
#ifndef FREMANTLE
  ret = gtk_scrolled_window_new(nullptr, nullptr);
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW(ret);
  gtk_scrolled_window_set_policy(scrolled_window, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_add_with_viewport(scrolled_window, label);
  gtk_scrolled_window_set_shadow_type(scrolled_window, GTK_SHADOW_IN);
#else
  ret = hildon_pannable_area_new();
  hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(ret), label);
#endif
  return ret;
}

GtkWidget *
copyright_page_new(icon_t &icons)
{
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  /* ------------------------ */
  GtkWidget *ivbox = gtk_vbox_new(FALSE, 0);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
  GtkWidget *ihbox = gtk_hbox_new(FALSE, 20);
  gtk_box_pack_start(GTK_BOX(ihbox), icons.widget_load(OSM2GO_ICON), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ihbox), label_scale(_("OSM2Go"), PANGO_SCALE_XX_LARGE),
		     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), ihbox, TRUE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ivbox), hbox, TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(ivbox),
                     label_scale(_("Mobile OpenStreetMap Editor"), PANGO_SCALE_X_LARGE),
                     TRUE, TRUE, 0);

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
GtkWidget *
left_label(const char *str)
{
  GtkWidget *widget = gtk_label_new(str);
  gtk_misc_set_alignment(GTK_MISC(widget), 0.0f, 0.5f);
  return widget;
}

void
author_add(GtkWidget *box, const char *str)
{
  gtk_box_pack_start(GTK_BOX(box), left_label(str), FALSE, FALSE, 0);
}

inline void
author_add(GtkWidget *box, trstring::native_type_arg str)
{
  author_add(box, static_cast<const gchar *>(str));
}

GtkWidget *
authors_page_new()
{
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
  GtkWidget *scrolled_window = gtk_scrolled_window_new(nullptr, nullptr);
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

GtkWidget *
donate_page_new(icon_t &icons)
{
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start(GTK_BOX(vbox),
      label_wrap(_("If you like OSM2Go and want to support its future development "
                   "please consider donating to the developer. You can either "
                   "donate via paypal to")), TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), link_new("till@harbaum.org"), TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(vbox),
                     label_wrap(_("or you can just click the button below which will "
                                  "open the appropriate web page in your browser.")),
                     TRUE, TRUE, 0);

  GtkWidget *ihbox = gtk_hbox_new(FALSE, 0);
  GtkWidget *button = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(button), icons.widget_load(PAYPAL_ICON));
  gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
  g_signal_connect_swapped(button, "clicked",
                           G_CALLBACK(osm2go_platform::open_url),
                           const_cast<char *>("https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=7400558"));

  gtk_box_pack_start(GTK_BOX(ihbox), button, TRUE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), ihbox, TRUE, TRUE, 0);

  return vbox;
}

GtkWidget *
bugs_page_new()
{
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start(GTK_BOX(vbox),
      label_wrap(_("Please report bugs or feature requests via the OSM2Go "
                   "bug tracker. This bug tracker can directly be reached via "
                   "the following link:")), TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(vbox),
                     link_new("https://github.com/osm2go/osm2go/issues"),
                     TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(vbox),
                     label_wrap(_("You might also be interested in joining the mailing lists "
                                  "or the forum:")), TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), link_new("https://garage.maemo.org/projects/osm2go/"),
                     TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), label_wrap(_("Thank you for contributing!")),
                     TRUE, TRUE, 0);

  return vbox;
}

} // namespace

void MainUi::about_box()
{
  osm2go_platform::DialogGuard dialog(gtk_dialog_new_with_buttons(static_cast<const gchar *>(_("About OSM2Go")),
                                              GTK_WINDOW(appdata_t::window), GTK_DIALOG_MODAL,
                                              GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, nullptr));

  gtk_window_set_default_size(dialog,
#ifdef FREMANTLE
                              640, 480);
#else
                              400, 200);
#endif

  GtkWidget *notebook = osm2go_platform::notebook_new();

  icon_t &icons = icon_t::instance();
  osm2go_platform::notebook_append_page(notebook, copyright_page_new(icons), _("Copyright"));
  osm2go_platform::notebook_append_page(notebook, license_page_new(),        _("License"));
  osm2go_platform::notebook_append_page(notebook, authors_page_new(),        _("Authors"));
  osm2go_platform::notebook_append_page(notebook, donate_page_new(icons),    _("Donate"));
  osm2go_platform::notebook_append_page(notebook, bugs_page_new(),           _("Bugs"));

  gtk_box_pack_start(dialog.vbox(), notebook, TRUE, TRUE, 0);

  gtk_widget_show_all(dialog.get());

  gtk_dialog_run(dialog);
}
