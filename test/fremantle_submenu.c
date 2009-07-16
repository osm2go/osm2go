/*
 */

#include <hildon/hildon.h>

#define SIZE (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH)

void on_about_clicked(GtkButton *button, gpointer data) {
  GtkWidget *dialog = 
    gtk_message_dialog_new(NULL,
			   GTK_DIALOG_DESTROY_WITH_PARENT,
			   GTK_MESSAGE_INFO,
			   GTK_BUTTONS_CLOSE,
			   "Hello!!!");

 gtk_dialog_run (GTK_DIALOG (dialog));
 gtk_widget_destroy (dialog);
}

void on_submenu_clicked(GtkButton *button, GtkWidget *submenu) {
  GtkWidget *top = hildon_window_stack_peek(hildon_window_stack_get_default());

#if 0  // enabling this makes the submenu work in the sdk
  int start, end;
  GTimeVal tv;
  g_get_current_time(&tv);
  start = tv.tv_sec * 1000 + tv.tv_usec / 1000; 
  do {
    if(gtk_events_pending()) 
      while(gtk_events_pending()) {
	putchar('.'); fflush(stdout);
	gtk_main_iteration();
      }
    else
      usleep(100);

    g_get_current_time(&tv);
    end = tv.tv_sec * 1000 + tv.tv_usec / 1000; 
  } while(end-start < 1000);
#endif

  hildon_app_menu_popup(HILDON_APP_MENU(submenu), GTK_WINDOW(top));
}

static HildonAppMenu *build_submenu(void) {
  HildonAppMenu *menu = HILDON_APP_MENU(hildon_app_menu_new());
  GtkButton *button;

  button = GTK_BUTTON(hildon_gtk_button_new(SIZE));
  gtk_button_set_label(button, "About");
  g_signal_connect_after(G_OBJECT (button), "clicked",
		   G_CALLBACK(on_about_clicked), NULL);
  hildon_app_menu_append(menu, GTK_BUTTON (button));
  
  gtk_widget_show_all(GTK_WIDGET(menu));

  return menu;
}

static HildonAppMenu *build_main_menu(void) {
  HildonAppMenu *menu = HILDON_APP_MENU(hildon_app_menu_new());
  HildonAppMenu *submenu = build_submenu();

  GtkButton *button = GTK_BUTTON(hildon_gtk_button_new(SIZE));
  gtk_button_set_label(button, "About");
  g_signal_connect_after(G_OBJECT (button), "clicked",
		   G_CALLBACK(on_about_clicked), NULL);
  hildon_app_menu_append(menu, GTK_BUTTON (button));
  
  button = GTK_BUTTON(hildon_gtk_button_new(SIZE));
  gtk_button_set_label(button, "Submenu");
  g_signal_connect_after(G_OBJECT (button), "clicked",
		   G_CALLBACK(on_submenu_clicked), submenu);
  hildon_app_menu_append(menu, GTK_BUTTON (button));
  
  gtk_widget_show_all(GTK_WIDGET(menu));

  return menu;
}

int
main (int argc, char **argv) {
  hildon_gtk_init (&argc, &argv);
  
  GtkWidget *window;
  window = hildon_stackable_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Submenu test");
  
  HildonAppMenu *menu = build_main_menu();
  hildon_stackable_window_set_main_menu (HILDON_STACKABLE_WINDOW (window), menu);
  
  GtkWidget *contents = gtk_label_new ("Submenu test");
  gtk_container_add (GTK_CONTAINER (window), contents);
  
  g_signal_connect (G_OBJECT (window), "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);
  
  gtk_widget_show_all (window);
  
  gtk_main ();
  return 0;
}

            
