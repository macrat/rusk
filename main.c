#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <webkit2/webkit2.h>

#define COOKIEPATH	"./cookie.txt"
#define FAVICONDIR	"/mnt/tmpfs/"

#define SCROLL_STEP	24


typedef struct {
	WebKitWebView *webview;
	GtkWindow *window;
	GtkProgressBar *progressbar;
} RuskWindow;


void onTitleChange(WebKitWebView *webview, GParamSpec *param, RuskWindow *rusk)
{
	gtk_window_set_title(rusk->window, webkit_web_view_get_title(rusk->webview));
}

void onProgressChange(WebKitWebView *webview, GParamSpec *parm, RuskWindow *rusk)
{
	const gdouble progress = webkit_web_view_get_estimated_load_progress(rusk->webview);

	gtk_progress_bar_set_fraction(rusk->progressbar, progress);
}

void onLoadChange(WebKitWebView *webview, WebKitLoadEvent event, RuskWindow *rusk)
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

void scroll(RuskWindow *rusk, const int vertical, const int horizonal)
{
	char script[1024];

	snprintf(script, sizeof(script), "window.scrollBy(%d,%d)", horizonal, vertical);

	webkit_web_view_run_javascript(rusk->webview, script, NULL, NULL, NULL);
}

gboolean onKeyPress(GtkWidget *widget, GdkEventKey *key, RuskWindow *rusk)
{
	gboolean proceed = FALSE;

	if(key->state & GDK_CONTROL_MASK)
	{
		switch(gdk_keyval_to_upper(key->keyval))
		{
			case GDK_KEY_B:
				webkit_web_view_go_back(rusk->webview);
				proceed = TRUE;
				break;
			case GDK_KEY_F:
				webkit_web_view_go_forward(rusk->webview);
				proceed = TRUE;
				break;

			case GDK_KEY_H:
				scroll(rusk, 0, -SCROLL_STEP);
				proceed = TRUE;
				break;
			case GDK_KEY_J:
				scroll(rusk, SCROLL_STEP, 0);
				proceed = TRUE;
				break;
			case GDK_KEY_K:
				scroll(rusk, -SCROLL_STEP, 0);
				proceed = TRUE;
				break;
			case GDK_KEY_L:
				scroll(rusk, 0, SCROLL_STEP);
				proceed = TRUE;
				break;
		}
	}
	return proceed;
}

void onFaviconChange(WebKitWebView *webview, GParamSpec *param, RuskWindow *rusk)
{
	cairo_surface_t *favicon;
	int width, height;
	GdkPixbuf *pixbuf;

	if((favicon = webkit_web_view_get_favicon(rusk->webview)) == NULL)
	{
		gtk_window_set_icon_name(rusk->window, "browser");
		return;
	}

	width = cairo_image_surface_get_width(favicon);
	height = cairo_image_surface_get_height(favicon);

	if(width <= 0 || height <= 0)
	{
		gtk_window_set_icon_name(rusk->window, "browser");
		return;
	}

	pixbuf = gdk_pixbuf_get_from_surface(favicon, 0, 0, width, height);

	gtk_window_set_icon(rusk->window, pixbuf);
}

void onLinkHover(WebKitWebView *webview, WebKitHitTestResult *hitTest, guint modifiers, RuskWindow *rusk)
{
	if(webkit_hit_test_result_context_is_link(hitTest))
		gtk_window_set_title(rusk->window, webkit_hit_test_result_get_link_uri(hitTest));
	else
		gtk_window_set_title(rusk->window, webkit_web_view_get_title(rusk->webview));
}

int setupWebView(RuskWindow *rusk)
{
	WebKitCookieManager *cookieManager;
	WebKitWebContext *context;

	context = webkit_web_context_get_default();

	cookieManager = webkit_web_context_get_cookie_manager(context);
	if(cookieManager == NULL)
		return -1;

	webkit_cookie_manager_set_persistent_storage(cookieManager, COOKIEPATH, WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT);

	webkit_web_context_set_favicon_database_directory(context, FAVICONDIR);

	g_signal_connect(G_OBJECT(rusk->webview), "notify::title", G_CALLBACK(onTitleChange), rusk);
	g_signal_connect(G_OBJECT(rusk->webview), "notify::estimated-load-progress", G_CALLBACK(onProgressChange), rusk);
	g_signal_connect(G_OBJECT(rusk->webview), "load-changed", G_CALLBACK(onLoadChange), rusk);
	g_signal_connect(G_OBJECT(rusk->webview), "notify::favicon", G_CALLBACK(onFaviconChange), rusk);
	g_signal_connect(G_OBJECT(rusk->webview), "mouse-target-changed", G_CALLBACK(onLinkHover), rusk);

	return 0;
}

int makeWindow(RuskWindow *rusk)
{
	GtkWidget *box;

	if((rusk->window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL))) == NULL)
		return -1;

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(rusk->window), box);

	rusk->progressbar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->progressbar), FALSE, FALSE, 0);

	rusk->webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->webview), TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(rusk->window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
	gtk_widget_show_all(GTK_WIDGET(rusk->window));

	gtk_widget_hide(GTK_WIDGET(rusk->progressbar));

	g_signal_connect(G_OBJECT(rusk->window), "key-press-event", G_CALLBACK(onKeyPress), rusk);

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
