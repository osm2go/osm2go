/*
 * Copyright (C) 2008-2009 Till Harbaum <till@harbaum.org>.
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


#include "appdata.h"
#include "map.h"
#include "project.h"

typedef struct {
  appdata_t *appdata;
  GtkWidget *window;
  GMainLoop *loop;
} popup_context_t;


#ifndef USE_HILDON
#define HEIGHT 100
#else
#define HEIGHT 200
#endif

static gboolean
pointer_in_window(GtkWidget *widget, gint x_root, gint y_root) {
  if(GTK_WIDGET_MAPPED(gtk_widget_get_toplevel(widget))) {
    gint window_x, window_y;

    gdk_window_get_position(gtk_widget_get_toplevel(widget)->window,
			    &window_x, &window_y);

    if(x_root >= window_x && x_root < window_x + widget->allocation.width &&
	y_root >= window_y && y_root < window_y + widget->allocation.height)
      return TRUE;
  }

  return FALSE;
}

static gboolean
on_button_press_event(GtkWidget *widget,
			  GdkEventButton *event, G_GNUC_UNUSED popup_context_t *context) {
  gboolean in = pointer_in_window(widget, event->x_root, event->y_root);

  printf("overlay button press(in = %d)\n", in);
  return !in;
}

static gboolean
on_button_release_event(GtkWidget *widget,
			  GdkEventButton *event, G_GNUC_UNUSED popup_context_t *context) {
  gboolean in = pointer_in_window(widget, event->x_root, event->y_root);

  printf("overlay button release(in = %d)\n", in);

  if(!in) {
    printf("destroying popup\n");
    gtk_widget_destroy(gtk_widget_get_toplevel(widget));
  }

  return !in;
}

static void
shutdown_loop(popup_context_t *context) {
  if(g_main_loop_is_running(context->loop))
    g_main_loop_quit(context->loop);
}

static gint
run_delete_handler(G_GNUC_UNUSED GtkWindow *window, G_GNUC_UNUSED GdkEventAny *event,
		   popup_context_t *context) {
  shutdown_loop(context);
  return TRUE; /* Do not destroy */
}

static void
run_destroy_handler(G_GNUC_UNUSED GtkWindow *window, G_GNUC_UNUSED popup_context_t *context) {
  /* shutdown_loop will be called by run_unmap_handler */
  printf("popup destroyed\n");
}

static void
run_unmap_handler(G_GNUC_UNUSED GtkWindow *window, popup_context_t *context) {
  shutdown_loop(context);
}

static void
on_value_changed(GtkAdjustment *adjustment,  popup_context_t *context) {
  printf("value changed to %f (%f)\n",
	 gtk_adjustment_get_value(adjustment),
	 pow(MAP_DETAIL_STEP, -gtk_adjustment_get_value(adjustment)));
  map_detail_change(context->appdata->map, pow(MAP_DETAIL_STEP,
				      -gtk_adjustment_get_value(adjustment)));
}

void scale_popup(GtkWidget *button, appdata_t *appdata) {
  popup_context_t context;
  context.appdata = appdata;

  if(!appdata->project || !appdata->project->map_state)
    return;

  float lin =
    -rint(log(appdata->project->map_state->detail)/log(MAP_DETAIL_STEP));

  context.window = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_widget_realize(context.window);
  gtk_window_set_default_size(GTK_WINDOW(context.window),
			      button->allocation.width, HEIGHT);
  gtk_window_resize(GTK_WINDOW(context.window),
		    button->allocation.width, HEIGHT);
  //  gtk_window_set_resizable(GTK_WINDOW(context.window), FALSE);
  gtk_window_set_transient_for(GTK_WINDOW(context.window),
			       GTK_WINDOW(appdata->window));
  gtk_window_set_keep_above(GTK_WINDOW(context.window), TRUE);
  gtk_window_set_destroy_with_parent(GTK_WINDOW(context.window), TRUE);
  gtk_window_set_gravity(GTK_WINDOW(context.window), GDK_GRAVITY_STATIC);
  gtk_window_set_modal(GTK_WINDOW(context.window), TRUE);

  /* connect events */
  g_signal_connect(G_OBJECT(context.window), "button-press-event",
		   G_CALLBACK(on_button_press_event), &context);
  g_signal_connect(G_OBJECT(context.window), "button-release-event",
		   G_CALLBACK(on_button_release_event), &context);
  g_signal_connect(G_OBJECT(context.window), "delete-event",
		   G_CALLBACK(run_delete_handler), &context);
  g_signal_connect(G_OBJECT(context.window), "destroy",
		   G_CALLBACK(run_destroy_handler), &context);
  g_signal_connect(G_OBJECT(context.window), "unmap",
		   G_CALLBACK(run_unmap_handler), &context);

  gdk_pointer_grab(context.window->window, TRUE,
     GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK,
		   NULL, NULL, GDK_CURRENT_TIME);
  gtk_grab_add(context.window);

  gint x, y;
  gdk_window_get_origin(button->window, &x, &y);

  gtk_window_move(GTK_WINDOW(context.window),
		  x + button->allocation.x,
  		  y + button->allocation.y - HEIGHT);

  /* a frame with a vscale inside */
  GtkWidget *frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
  GtkWidget *ivbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(ivbox), gtk_label_new("D"));
  gtk_box_pack_start_defaults(GTK_BOX(ivbox), gtk_label_new("e"));
  gtk_box_pack_start_defaults(GTK_BOX(ivbox), gtk_label_new("t"));
  gtk_box_pack_start_defaults(GTK_BOX(ivbox), gtk_label_new("a"));
  gtk_box_pack_start_defaults(GTK_BOX(ivbox), gtk_label_new("i"));
  gtk_box_pack_start_defaults(GTK_BOX(ivbox), gtk_label_new("l"));
  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), vbox);

  GtkObject *adjustment = gtk_adjustment_new(lin, -5.0, +6.0, +1.0, +1.0, 1.0);
  GtkWidget *scale = gtk_vscale_new(GTK_ADJUSTMENT(adjustment));
  gtk_scale_set_digits(GTK_SCALE(scale), 0);
  gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
  g_signal_connect(G_OBJECT(adjustment), "value-changed",
		   G_CALLBACK(on_value_changed), &context);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), scale);
  gtk_container_add(GTK_CONTAINER(frame), hbox);
  gtk_container_add(GTK_CONTAINER(context.window), frame);

  gtk_widget_show_all(context.window);

  /* handle this popup until it's gone */

  context.loop = g_main_loop_new(NULL, FALSE);

  GDK_THREADS_LEAVE();
  g_main_loop_run(context.loop);
  GDK_THREADS_ENTER();

  g_main_loop_unref(context.loop);

  printf("scale popup removed\n");
}
