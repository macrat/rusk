#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <webkit2/webkit2.h>

#define COOKIEPATH	"./cookie.txt"
#define FAVICONDIR	"/mnt/tmpfs/"

#define SCROLL_STEP	24
#define ZOOM_STEP	0.1


typedef struct {
	WebKitWebView *webview;
	GtkWindow *window;
	GtkEntry *insiteSearch;
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
			gtk_widget_set_visible(GTK_WIDGET(rusk->progressbar), TRUE);
			break;

		case WEBKIT_LOAD_FINISHED:
			gtk_widget_set_visible(GTK_WIDGET(rusk->progressbar), FALSE);
			break;
	}
}

void scroll(RuskWindow *rusk, const int vertical, const int horizonal)
{
	char script[1024];

	snprintf(script, sizeof(script), "window.scrollBy(%d,%d)", horizonal, vertical);

	webkit_web_view_run_javascript(rusk->webview, script, NULL, NULL, NULL);
}

void onFaviconChange(WebKitWebView *webview, GParamSpec *param, RuskWindow *rusk)
{
	cairo_surface_t *favicon;
	int width, height;
	GdkPixbuf *pixbuf;

	if((favicon = webkit_web_view_get_favicon(rusk->webview)) == NULL)
		return;

	width = cairo_image_surface_get_width(favicon);
	height = cairo_image_surface_get_height(favicon);

	if(width <= 0 || height <= 0)
		return;

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

void runInSiteSearch(RuskWindow *rusk, const char *query, const int force)
{
	WebKitFindController *finder = webkit_web_view_get_find_controller(rusk->webview);
	const char *oldquery = webkit_find_controller_get_search_text(finder);

	if(query && (oldquery == NULL || strcmp(query, oldquery) != 0 || force))
		webkit_find_controller_search(finder, query, WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE|WEBKIT_FIND_OPTIONS_WRAP_AROUND, 128);
}

gboolean onInSiteSearchInput(GtkEntry *entry, GdkEventKey *key, RuskWindow *rusk)
{
	if(key->keyval == GDK_KEY_Return)
	{
		gtk_window_set_focus(rusk->window, GTK_WIDGET(rusk->webview));
		return TRUE;
	}else
	{
		runInSiteSearch(rusk, gtk_entry_get_text(rusk->insiteSearch), FALSE);
		return FALSE;
	}
}

void inSiteSearchToggle(RuskWindow *rusk)
{
	if(!gtk_widget_is_visible(GTK_WIDGET(rusk->insiteSearch)))
	{
		gtk_widget_set_visible(GTK_WIDGET(rusk->insiteSearch), TRUE);
		gtk_window_set_focus(rusk->window, GTK_WIDGET(rusk->insiteSearch));

		WebKitFindController *finder = webkit_web_view_get_find_controller(rusk->webview);
		runInSiteSearch(rusk, webkit_find_controller_get_search_text(finder), TRUE);
	}else
	{
		webkit_find_controller_search_finish(webkit_web_view_get_find_controller(rusk->webview));
		gtk_widget_set_visible(GTK_WIDGET(rusk->insiteSearch), FALSE);
		gtk_window_set_focus(rusk->window, GTK_WIDGET(rusk->webview));
	}
}

void inSiteSearchNext(RuskWindow *rusk)
{
	webkit_find_controller_search_next(webkit_web_view_get_find_controller(rusk->webview));
}

void inSiteSearchPrev(RuskWindow *rusk)
{
	webkit_find_controller_search_previous(webkit_web_view_get_find_controller(rusk->webview));
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
			case GDK_KEY_R:
				if(key->state & GDK_SHIFT_MASK)
					webkit_web_view_reload_bypass_cache(rusk->webview);
				else
					webkit_web_view_reload(rusk->webview);
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

			case GDK_KEY_plus:
				webkit_web_view_set_zoom_level(rusk->webview, webkit_web_view_get_zoom_level(rusk->webview)+ZOOM_STEP);
				proceed = TRUE;
				break;
			case GDK_KEY_minus:
				webkit_web_view_set_zoom_level(rusk->webview, webkit_web_view_get_zoom_level(rusk->webview)-ZOOM_STEP);
				proceed = TRUE;
				break;
			case GDK_KEY_0:
				webkit_web_view_set_zoom_level(rusk->webview, 1.0);
				proceed = TRUE;
				break;

			case GDK_KEY_G:
				inSiteSearchToggle(rusk);
				proceed = TRUE;
				break;
			case GDK_KEY_N:
				inSiteSearchNext(rusk);
				proceed = TRUE;
				break;
			case GDK_KEY_P:
				inSiteSearchPrev(rusk);
				proceed = TRUE;
				break;
		}
	}
	return proceed;
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

	rusk->insiteSearch = GTK_ENTRY(gtk_entry_new());
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->insiteSearch), FALSE, FALSE, 0);

	rusk->progressbar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->progressbar), FALSE, FALSE, 0);

	rusk->webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->webview), TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(rusk->window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
	gtk_widget_show_all(GTK_WIDGET(rusk->window));

	gtk_widget_set_visible(GTK_WIDGET(rusk->progressbar), FALSE);
	gtk_widget_set_visible(GTK_WIDGET(rusk->insiteSearch), FALSE);

	g_signal_connect(G_OBJECT(rusk->window), "key-press-event", G_CALLBACK(onKeyPress), rusk);
	g_signal_connect(G_OBJECT(rusk->insiteSearch), "key-release-event", G_CALLBACK(onInSiteSearchInput), rusk);

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
