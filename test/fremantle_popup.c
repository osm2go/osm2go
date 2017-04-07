/*
 */

#include <hildon/hildon.h>

enum { LIST_ITEM = 0, N_COLUMNS };

static void
add_to_list(GtkWidget *list, const gchar *str) {
  GtkListStore *store;
  GtkTreeIter iter;

  store = GTK_LIST_STORE(gtk_tree_view_get_model
      (GTK_TREE_VIEW(list)));

  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter, LIST_ITEM, str, -1);
}

static void on_menu_activated(GtkButton *button, GtkWidget *list) {
  GtkTreeIter iter;
  GtkTreeModel *model;
  char *value = NULL;

  GtkTreeSelection *selection = 
    gtk_tree_view_get_selection(GTK_TREE_VIEW(list));

  if (gtk_tree_selection_get_selected(
      selection, &model, &iter)) {

    gtk_tree_model_get(model, &iter, LIST_ITEM, &value,  -1);
  } else
    value = "<nothing>";

  char *msg = g_strdup_printf("%s is selected", value);

  GtkWidget *dialog = 
    gtk_message_dialog_new(NULL,
			   GTK_DIALOG_DESTROY_WITH_PARENT,
			   GTK_MESSAGE_INFO,
			   GTK_BUTTONS_CLOSE,
			   msg);

 gtk_dialog_run (GTK_DIALOG (dialog));
 gtk_widget_destroy (dialog);

 g_free(msg);
}

static void
init_list(GtkWidget *list) {
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkListStore *store;

  hildon_gtk_tree_view_set_ui_mode(GTK_TREE_VIEW(list), HILDON_UI_MODE_EDIT);
  
  /* Create some simple popup menu */
  GtkMenu *menu = GTK_MENU(gtk_menu_new());
  GtkWidget *menu_item = gtk_menu_item_new_with_label("Click me");
  gtk_menu_append(menu, menu_item);
  hildon_gtk_widget_set_theme_size(menu_item, 
   (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));
  g_signal_connect(GTK_OBJECT(menu_item), "activate", 
                   G_CALLBACK(on_menu_activated), list);
  gtk_widget_show_all(GTK_WIDGET(menu));
  
  gtk_widget_tap_and_hold_setup(list, GTK_WIDGET(menu), NULL, 0);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes("List Items",
		    renderer, "text", LIST_ITEM, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
  
  store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING);

  gtk_tree_view_set_model(GTK_TREE_VIEW(list), 
      GTK_TREE_MODEL(store));

  g_object_unref(store);

  add_to_list(list, "Aliens");
  add_to_list(list, "Leon");
  add_to_list(list, "Capote");
  add_to_list(list, "Saving private Ryan");
  add_to_list(list, "Der Untergang");
  add_to_list(list, "Jurassic Park");
  add_to_list(list, "Die wunderbare Welt der Amelie");
  add_to_list(list, "Titanic");
}

static void on_button_clicked(GtkButton *button, gpointer data) {
  printf("button clicked\n");
  GtkWidget *dialog = 
    gtk_dialog_new_with_buttons("Dialog test",
		   GTK_WINDOW(data),
		   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		   GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		   NULL);

  gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 300);
 
  /* create a treeview and place it in a pannable area */
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), 
      gtk_label_new ("Please tap and hold on an item below"), FALSE, FALSE, 5);
  GtkWidget *pan = hildon_pannable_area_new();
  GtkWidget *list = gtk_tree_view_new();
  init_list(list);
  gtk_container_add(GTK_CONTAINER(pan), list);
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), pan);

  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

int
main (int argc, char **argv) {
  hildon_gtk_init (&argc, &argv);
  
  GtkWidget *window;
  window = hildon_stackable_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Popup test");

  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

   /* add a button to open a seperate dialog doing the same thing */
  /* the main screen does */
  GtkWidget *button = gtk_button_new_with_label("Open Dialog");
  g_signal_connect(GTK_OBJECT(button), "clicked", 
                   G_CALLBACK(on_button_clicked), window);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 5);


  /* create a treeview and place it in a pannable area */
  gtk_box_pack_start(GTK_BOX(vbox), 
      gtk_label_new ("Please tap and hold on an item below"), FALSE, FALSE, 5);
  GtkWidget *pan = hildon_pannable_area_new();
  GtkWidget *list = gtk_tree_view_new();
  init_list(list);
  gtk_container_add(GTK_CONTAINER(pan), list);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), pan);
 
  gtk_container_add (GTK_CONTAINER (window), vbox);
  
  g_signal_connect (G_OBJECT (window), "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);
  
  gtk_widget_show_all (window);
  
  gtk_main ();
  return 0;
}

            
