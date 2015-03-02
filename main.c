#include <unistd.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>


GtkWidget *g_webview;


int makeWindow()
{
	GtkWidget *window, *box, *scrolled;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(window), box);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);

	g_webview = webkit_web_view_new();
	gtk_container_add(GTK_CONTAINER(scrolled), g_webview);

	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
	gtk_widget_show_all(window);
}

int main(int argc, char **argv)
{
	gtk_init(&argc, &argv);

	makeWindow();

	webkit_web_view_load_uri(WEBKIT_WEB_VIEW(g_webview), "http://google.com/");  /* debug */

	gtk_main();

	return 0;
}
