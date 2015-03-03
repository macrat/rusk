#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <webkit2/webkit2.h>

#define COOKIEPATH	"./cookie.txt"
#define HISTORYPATH	"./history.txt"
#define FAVICONDIR	"/mnt/tmpfs/"

#define SCROLL_STEP	24
#define ZOOM_STEP	0.1

#define BORDER_COLOR_NORMAL	(&((GdkRGBA){1.0, 1.0, 1.0, 1.0}))
#define BORDER_COLOR_TLS_ERROR	(&((GdkRGBA){1.0, 0.5, 0.5, 1.0}))
#define BORDER_COLOR_SECURE	(&((GdkRGBA){0.5, 1.0, 0.5, 1.0}))

#define VERSION	"alpha"


typedef struct {
	WebKitWebView *webview;
	GtkWindow *window;
	GtkEntry *insiteSearch;
	GtkEntry *addressbar;
	GtkEntry *globalSearch;
	GtkProgressBar *progressbar;
	FILE *historyFile;
} RuskWindow;


void openURI(RuskWindow *rusk, const char *uri)
{
	char *buf, *realURI;

	if(uri[0] == '/' || strncmp(uri, "./", 2) == 0 || strncmp(uri, "~/", 2) == 0)
	{
		buf = realpath(uri, NULL);
		realURI = g_strdup_printf("file://%s", buf);
		free(buf);
	}else if(strstr(uri, "://"))
	{
		realURI = g_strdup(uri);
	}else
	{
		realURI = g_strdup_printf("http://%s", uri);
	}

	webkit_web_view_load_uri(rusk->webview, realURI);
	g_free(realURI);
}

void onTitleChange(WebKitWebView *webview, GParamSpec *param, RuskWindow *rusk)
{
	gtk_window_set_title(rusk->window, webkit_web_view_get_title(rusk->webview));
}

void onProgressChange(WebKitWebView *webview, GParamSpec *parm, RuskWindow *rusk)
{
	const gdouble progress = webkit_web_view_get_estimated_load_progress(rusk->webview);

	gtk_progress_bar_set_fraction(rusk->progressbar, progress);
}

void checkTLSInfo(RuskWindow *rusk)
{
	if(strncasecmp(webkit_web_view_get_uri(rusk->webview), "https://", strlen("https://")) != 0)
	{
		gtk_widget_override_background_color(GTK_WIDGET(rusk->window), GTK_STATE_FLAG_NORMAL, BORDER_COLOR_NORMAL);
	}else
	{
		GTlsCertificate *certificate;
		GTlsCertificateFlags errors;

		if(webkit_web_view_get_tls_info(rusk->webview, &certificate, &errors) == FALSE || errors)
			gtk_widget_override_background_color(GTK_WIDGET(rusk->window), GTK_STATE_FLAG_NORMAL, BORDER_COLOR_TLS_ERROR);
		else
			gtk_widget_override_background_color(GTK_WIDGET(rusk->window), GTK_STATE_FLAG_NORMAL, BORDER_COLOR_SECURE);
	}
}

void onLoadChange(WebKitWebView *webview, WebKitLoadEvent event, RuskWindow *rusk)
{
	switch(event)
	{
		case WEBKIT_LOAD_STARTED:
			gtk_widget_set_visible(GTK_WIDGET(rusk->progressbar), TRUE);
			break;

		case WEBKIT_LOAD_COMMITTED:
			checkTLSInfo(rusk);
			fprintf(rusk->historyFile, "%s\n", webkit_web_view_get_uri(rusk->webview));
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

void inSiteSearchNext(RuskWindow *rusk)
{
	webkit_find_controller_search_next(webkit_web_view_get_find_controller(rusk->webview));
}

void inSiteSearchPrev(RuskWindow *rusk)
{
	webkit_find_controller_search_previous(webkit_web_view_get_find_controller(rusk->webview));
}

gboolean onInSiteSearchInput(GtkEntry *entry, GdkEventKey *key, RuskWindow *rusk)
{
	if(key->keyval == GDK_KEY_Return)
	{
		if(key->state & GDK_SHIFT_MASK)
			inSiteSearchPrev(rusk);
		else
			inSiteSearchNext(rusk);
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

void addressbarToggle(RuskWindow *rusk)
{
	if(!gtk_widget_is_visible(GTK_WIDGET(rusk->addressbar)))
	{
		gtk_entry_set_text(rusk->addressbar, webkit_web_view_get_uri(rusk->webview));
		gtk_widget_set_visible(GTK_WIDGET(rusk->addressbar), TRUE);
		gtk_window_set_focus(rusk->window, GTK_WIDGET(rusk->addressbar));
	}else
	{
		gtk_widget_set_visible(GTK_WIDGET(rusk->addressbar), FALSE);
		gtk_window_set_focus(rusk->window, GTK_WIDGET(rusk->webview));
	}
}

gboolean onAddressbarInput(GtkWidget *widget, GdkEventKey *key, RuskWindow *rusk)
{
	if(key->keyval == GDK_KEY_Return)
	{
		openURI(rusk, gtk_entry_get_text(rusk->addressbar));
		addressbarToggle(rusk);
	}
}

void globalSearchToggle(RuskWindow *rusk, const char *site)
{
	if(!gtk_widget_get_visible(GTK_WIDGET(rusk->globalSearch)))
	{
		gtk_entry_set_placeholder_text(rusk->globalSearch, site);
		gtk_widget_set_visible(GTK_WIDGET(rusk->globalSearch), TRUE);
		gtk_window_set_focus(rusk->window, GTK_WIDGET(rusk->globalSearch));
	}else
	{
		gtk_widget_set_visible(GTK_WIDGET(rusk->globalSearch), FALSE);
		gtk_window_set_focus(rusk->window, GTK_WIDGET(rusk->webview));
	}
}

void runGlobalSearch(RuskWindow *rusk)
{
	char *uri = NULL;
	const char *site = gtk_entry_get_placeholder_text(rusk->globalSearch);

	if(strstr(site, "google"))
		uri = "https://google.com/search?q=%s";
	if(strstr(site, "maps"))
		uri = "https://google.com/maps?q=%s";
	if(strstr(site, "images"))
		uri = "https://google.com/search?tbm=isch&q=%s";

	if(uri)
	{
		char *realURI = g_strdup_printf(uri, gtk_entry_get_text(rusk->globalSearch));
		openURI(rusk, realURI);
		g_free(realURI);

		gtk_entry_set_text(rusk->globalSearch, "");
	}

	globalSearchToggle(rusk, "");
}

void onGlobalSearchActivate(GtkWidget *widget, RuskWindow *rusk)
{
	runGlobalSearch(rusk);
}

gboolean onKeyPress(GtkWidget *widget, GdkEventKey *key, RuskWindow *rusk)
{
	gboolean proceed = TRUE;

	if(key->state & GDK_CONTROL_MASK)
	{
		switch(gdk_keyval_to_upper(key->keyval))
		{
			case GDK_KEY_B:
				webkit_web_view_go_back(rusk->webview);
				break;
			case GDK_KEY_F:
				webkit_web_view_go_forward(rusk->webview);
				break;
			case GDK_KEY_R:
				if(key->state & GDK_SHIFT_MASK)
					webkit_web_view_reload_bypass_cache(rusk->webview);
				else
					webkit_web_view_reload(rusk->webview);
				break;

			case GDK_KEY_H:
				scroll(rusk, 0, -SCROLL_STEP);
				break;
			case GDK_KEY_J:
				scroll(rusk, SCROLL_STEP, 0);
				break;
			case GDK_KEY_K:
				scroll(rusk, -SCROLL_STEP, 0);
				break;
			case GDK_KEY_L:
				scroll(rusk, 0, SCROLL_STEP);
				break;

			case GDK_KEY_plus:
				webkit_web_view_set_zoom_level(rusk->webview, webkit_web_view_get_zoom_level(rusk->webview)+ZOOM_STEP);
				break;
			case GDK_KEY_minus:
				webkit_web_view_set_zoom_level(rusk->webview, webkit_web_view_get_zoom_level(rusk->webview)-ZOOM_STEP);
				break;
			case GDK_KEY_0:
				webkit_web_view_set_zoom_level(rusk->webview, 1.0);
				break;

			case GDK_KEY_S:
				inSiteSearchToggle(rusk);
				break;
			case GDK_KEY_N:
				inSiteSearchNext(rusk);
				break;
			case GDK_KEY_P:
				inSiteSearchPrev(rusk);
				break;

			case GDK_KEY_O:
				addressbarToggle(rusk);
				break;

			case GDK_KEY_G:
				globalSearchToggle(rusk, "google");
				break;
			case GDK_KEY_M:
				globalSearchToggle(rusk, "maps");
				break;
			case GDK_KEY_I:
				globalSearchToggle(rusk, "images");
				break;

			default:
				proceed = FALSE;
		}
	}else
	{
		proceed = FALSE;
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

	webkit_settings_set_user_agent_with_application_details(webkit_web_view_get_settings(rusk->webview), "rusk", VERSION);

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

	gtk_container_set_border_width(GTK_CONTAINER(rusk->window), 2);
	gtk_widget_override_background_color(GTK_WIDGET(rusk->window), GTK_STATE_FLAG_NORMAL, BORDER_COLOR_NORMAL);

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(rusk->window), box);

	rusk->addressbar = GTK_ENTRY(gtk_entry_new());
	gtk_entry_set_placeholder_text(rusk->addressbar, "URI");
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->addressbar), FALSE, FALSE, 0);

	rusk->globalSearch = GTK_ENTRY(gtk_entry_new());
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->globalSearch), FALSE, FALSE, 0);

	rusk->insiteSearch = GTK_ENTRY(gtk_entry_new());
	gtk_entry_set_placeholder_text(rusk->insiteSearch, "site search");
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->insiteSearch), FALSE, FALSE, 0);

	rusk->progressbar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->progressbar), FALSE, FALSE, 0);

	rusk->webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->webview), TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(rusk->window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
	gtk_widget_show_all(GTK_WIDGET(rusk->window));

	gtk_widget_set_visible(GTK_WIDGET(rusk->addressbar), FALSE);
	gtk_widget_set_visible(GTK_WIDGET(rusk->globalSearch), FALSE);
	gtk_widget_set_visible(GTK_WIDGET(rusk->progressbar), FALSE);
	gtk_widget_set_visible(GTK_WIDGET(rusk->insiteSearch), FALSE);

	g_signal_connect(G_OBJECT(rusk->window), "key-press-event", G_CALLBACK(onKeyPress), rusk);
	g_signal_connect(G_OBJECT(rusk->insiteSearch), "key-release-event", G_CALLBACK(onInSiteSearchInput), rusk);
	g_signal_connect(G_OBJECT(rusk->addressbar), "key-release-event", G_CALLBACK(onAddressbarInput), rusk);
	g_signal_connect(G_OBJECT(rusk->globalSearch), "activate", G_CALLBACK(onGlobalSearchActivate), rusk);

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

	if((rusk.historyFile = fopen(HISTORYPATH, "a")) == NULL)
		return -1;

	openURI(&rusk, "google.com");  /* debug */

	gtk_main();

	return 0;
}
