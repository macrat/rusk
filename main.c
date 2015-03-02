#include <unistd.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#define COOKIEPATH "./cookie.txt"

typedef struct {
	WebKitWebView *webview;
	GtkWindow *window;
	GtkProgressBar *progressbar;
} RuskWindow;


void onChangeTitle(WebKitWebView *webview, GParamSpec *param, RuskWindow *rusk)
{
	const gchar *title = webkit_web_view_get_title(rusk->webview);
	gtk_window_set_title(rusk->window, title);
}

void onChangeProgress(WebKitWebView *webview, GParamSpec *parm, RuskWindow *rusk)
{
	const gdouble progress = webkit_web_view_get_estimated_load_progress(rusk->webview);

	gtk_progress_bar_set_fraction(rusk->progressbar, progress);
}

void onChangeLoad(WebKitWebView *webview, WebKitLoadEvent event, RuskWindow *rusk)
{
	switch(event)
	{
		case WEBKIT_LOAD_STARTED:
			gtk_widget_show(GTK_WIDGET(rusk->progressbar));
			break;

		case WEBKIT_LOAD_FINISHED:
			gtk_widget_hide(GTK_WIDGET(rusk->progressbar));
			break;
	}
}

int setupWebView(RuskWindow *rusk)
{
	WebKitCookieManager *cookieManager;

	cookieManager = webkit_web_context_get_cookie_manager(webkit_web_context_get_default());
	if(cookieManager == NULL)
		return -1;

	webkit_cookie_manager_set_persistent_storage(cookieManager, COOKIEPATH, WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT);

	g_signal_connect(G_OBJECT(rusk->webview), "notify::title", G_CALLBACK(onChangeTitle), rusk);
	g_signal_connect(G_OBJECT(rusk->webview), "notify::estimated-load-progress", G_CALLBACK(onChangeProgress), rusk);
	g_signal_connect(G_OBJECT(rusk->webview), "load-changed", G_CALLBACK(onChangeLoad), rusk);

	return 0;
}

int makeWindow(RuskWindow *rusk)
{
	GtkWidget *box, *scrolled;

	if((rusk->window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL))) == NULL)
		return -1;

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(rusk->window), box);

	rusk->progressbar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->progressbar), FALSE, FALSE, 0);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);

	rusk->webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
	gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(rusk->webview));

	g_signal_connect(G_OBJECT(rusk->window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
	gtk_widget_show_all(GTK_WIDGET(rusk->window));

	gtk_widget_hide(GTK_WIDGET(rusk->progressbar));

	return 0;
}

int main(int argc, char **argv)
{
	RuskWindow rusk;

	gtk_init(&argc, &argv);

	if(makeWindow(&rusk) != 0)
		return -1;

	if(setupWebView(&rusk) != 0)
		return -1;

	webkit_web_view_load_uri(WEBKIT_WEB_VIEW(rusk.webview), "http://google.com/");  /* debug */

	gtk_main();

	return 0;
}
