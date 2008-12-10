/*
 */

#include <gtk/gtk.h>

GtkWidget *statusbartest(void) {
  GdkColor color;
  gdk_color_parse("red", &color);

  GtkWidget *eventbox = gtk_event_box_new();
  GtkWidget *statusbar = gtk_statusbar_new();  
  gtk_container_add(GTK_CONTAINER(eventbox), statusbar);

  gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &color);

  gtk_statusbar_push(GTK_STATUSBAR(statusbar),
	     gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "Msg"),
	     "This message is meant to have a red background");

  return eventbox;
}

int main(int argc, char *argv[]) {
  gtk_init (&argc, &argv);

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size(GTK_WINDOW(window), 500, 50);

  gtk_container_add(GTK_CONTAINER(window), statusbartest());

  g_signal_connect(G_OBJECT(window), "destroy", 
		   G_CALLBACK(gtk_main_quit), NULL);

  gtk_widget_show_all(GTK_WIDGET(window));
  gtk_main();

  return 0;
}
