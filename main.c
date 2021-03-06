/*
 *		rusk browser
 *			- here is nothing! -
 *
 *	the browser for minimalist and keyboard believer.
 *	this browser expect tiling window manager like awesome.
 *
 *					MIT License (c)2015 MacRat
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <webkit2/webkit2.h>
#include <sqlite3.h>
#include <wordexp.h>

#define DOWNLOAD_COMMAND "./rusk-dl"

#define SCRIPT_DOCUMENT_START		"./document_start.js"
#define SCRIPT_DOCUMENT_END			"./document_end.js"

#define HOMEPAGE	"http://google.com/"

#define BORDER_WIDTH	2
#define BORDER_COLOR_NORMAL				(&((GdkRGBA){1.0, 1.0, 1.0, 1.0}))
#define BORDER_COLOR_TLS_ERROR			(&((GdkRGBA){1.0, 0.5, 0.5, 1.0}))
#define BORDER_COLOR_SECURE				(&((GdkRGBA){0.5, 1.0, 0.5, 1.0}))
#define BORDER_COLOR_PRIVATE			(&((GdkRGBA){0.0, 0.0, 0.0, 1.0}))
#define BORDER_COLOR_PRIVATE_TLS_ERROR	(&((GdkRGBA){0.5, 0.0, 0.0, 1.0}))
#define BORDER_COLOR_PRIVATE_SECURE		(&((GdkRGBA){0.0, 0.5, 0.0, 1.0}))

#define VERSION	"alpha"


typedef struct {
	WebKitWebView *webview;
	GtkWindow *window;
	GtkEntry *insiteSearch;
	GtkEntry *addressbar;
	GtkEntry *globalSearch;
	GtkProgressBar *progressbar;
	struct {
		sqlite3 *connection;
		sqlite3_stmt *insertStmt;
	} database;
} RuskWindow;


RuskWindow* makeRusk();


int g_ruskCounter = 0;


gchar* getDatabasePath()
{
	return g_strdup_printf("%s/rusk/rusk.db", g_get_user_data_dir());
}


gchar* getFaviconDir()
{
	return g_strdup_printf("%s/rusk/", g_get_user_cache_dir());
}


char* loadFile(const char *fname)
{
	char buf[1024];

	FILE *fp = fopen(fname, "r");

	fseek(fp, 0, SEEK_END);
	size_t resultSize = ftell(fp) + 1;
	fseek(fp, 0, SEEK_SET);

	char *result = malloc(sizeof(char) * resultSize);
	memset(result, '\0', resultSize);

	size_t read;
	while((read = fread(buf, sizeof(char), sizeof(buf), fp)) > 0)
	{
		memcpy(result+ftell(fp)-read, buf, read);
	}
	fclose(fp);

	return result;
}


void openURI(RuskWindow *rusk, const char *uri)
{
	gchar *realURI;

	if(uri[0] == '/' || strncmp(uri, "./", 2) == 0)
	{
		char *filepath = realpath(uri, NULL);
		realURI = g_strdup_printf("file://%s", filepath);
		free(filepath);
	}else if(strncmp(uri, "~/", 2) == 0)
	{
		gchar *path = g_strdup_printf("%s%s", g_get_home_dir(), uri+1);
		char *real = realpath(path, NULL);
		g_free(path);
		realURI = g_strdup_printf("file://%s", real);
		free(real);
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

void updateBorder(RuskWindow *rusk)
{
	const gboolean privateMode = webkit_settings_get_enable_private_browsing(webkit_web_view_get_settings(rusk->webview));
	GdkRGBA *borderColor;

	if(strncasecmp(webkit_web_view_get_uri(rusk->webview), "https://", strlen("https://")) != 0)
	{
		borderColor = privateMode ? BORDER_COLOR_PRIVATE : BORDER_COLOR_NORMAL;
	}else
	{
		GTlsCertificate *certificate;
		GTlsCertificateFlags errors;

		if(webkit_web_view_get_tls_info(rusk->webview, &certificate, &errors) == FALSE || errors)
		{
			borderColor = privateMode ? BORDER_COLOR_PRIVATE_TLS_ERROR : BORDER_COLOR_TLS_ERROR;
		}else
		{
			borderColor = privateMode ? BORDER_COLOR_PRIVATE_SECURE : BORDER_COLOR_SECURE;
		}
	}

	gtk_widget_override_background_color(GTK_WIDGET(rusk->window), GTK_STATE_FLAG_NORMAL, borderColor);
}

void appendHistory(RuskWindow *rusk)
{
	if(!webkit_settings_get_enable_private_browsing(webkit_web_view_get_settings(rusk->webview)))
	{
		const char *uri = webkit_web_view_get_uri(rusk->webview);

		sqlite3_reset(rusk->database.insertStmt);

		sqlite3_bind_int(rusk->database.insertStmt, sqlite3_bind_parameter_index(rusk->database.insertStmt, ":date"), time(NULL));
		sqlite3_bind_text(rusk->database.insertStmt, sqlite3_bind_parameter_index(rusk->database.insertStmt, ":uri"), uri, strlen(uri), SQLITE_STATIC);

		while(TRUE)
		{
			const int result = sqlite3_step(rusk->database.insertStmt);
			if(result == SQLITE_DONE || result == SQLITE_ERROR)
			{
				break;
			}
		}
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
			updateBorder(rusk);
			appendHistory(rusk);
			break;

		case WEBKIT_LOAD_FINISHED:
			gtk_widget_set_visible(GTK_WIDGET(rusk->progressbar), FALSE);
			break;

		default:
			break;
	}
}

void onFaviconChange(WebKitWebView *webview, GParamSpec *param, RuskWindow *rusk)
{
	cairo_surface_t *favicon;
	int width, height;
	GdkPixbuf *pixbuf;

	if((favicon = webkit_web_view_get_favicon(rusk->webview)) == NULL)
	{
		return;
	}

	width = cairo_image_surface_get_width(favicon);
	height = cairo_image_surface_get_height(favicon);

	if(width <= 0 || height <= 0)
	{
		return;
	}

	pixbuf = gdk_pixbuf_get_from_surface(favicon, 0, 0, width, height);

	gtk_window_set_icon(rusk->window, pixbuf);
}

void onMouseTargetChange(WebKitWebView *webview, WebKitHitTestResult *hitTest, guint modifiers, RuskWindow *rusk)
{
	if(webkit_hit_test_result_context_is_link(hitTest))
	{
		gtk_window_set_title(rusk->window, webkit_hit_test_result_get_link_uri(hitTest));
	}else
	{
		gtk_window_set_title(rusk->window, webkit_web_view_get_title(rusk->webview));
	}
}

void runInSiteSearch(RuskWindow *rusk, const char *query, const int force)
{
	WebKitFindController *finder = webkit_web_view_get_find_controller(rusk->webview);
	const char *oldquery = webkit_find_controller_get_search_text(finder);

	if(query && (oldquery == NULL || strcmp(query, oldquery) != 0 || force))
	{
		webkit_find_controller_search(finder, query, WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE|WEBKIT_FIND_OPTIONS_WRAP_AROUND, 128);
	}
}

void inSiteSearchNext(RuskWindow *rusk)
{
	if(gtk_widget_is_visible(GTK_WIDGET(rusk->insiteSearch)))
	{
		webkit_find_controller_search_next(webkit_web_view_get_find_controller(rusk->webview));
	}
}

void inSiteSearchPrev(RuskWindow *rusk)
{
	if(gtk_widget_is_visible(GTK_WIDGET(rusk->insiteSearch)))
	{
		webkit_find_controller_search_previous(webkit_web_view_get_find_controller(rusk->webview));
	}
}

gboolean onInSiteSearchInput(GtkEntry *entry, GdkEventKey *key, RuskWindow *rusk)
{
	if(key->keyval == GDK_KEY_Return)
	{
		if(key->state & GDK_SHIFT_MASK)
		{
			inSiteSearchPrev(rusk);
		}else
		{
			inSiteSearchNext(rusk);
		}

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

		webkit_find_controller_search_previous(finder);
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
	return FALSE;
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
	{
		uri = "https://google.com/search?q=%s";
	}else if(strstr(site, "maps"))
	{
		uri = "https://google.com/maps?q=%s";
	}else if(strstr(site, "images"))
	{
		uri = "https://google.com/search?tbm=isch&q=%s";
	}

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

void togglePrivateBrowsing(RuskWindow *rusk)
{
	WebKitSettings *settings = webkit_web_view_get_settings(rusk->webview);
	WebKitCookieManager *cookieManager = webkit_web_context_get_cookie_manager(webkit_web_context_get_default());

	if(webkit_settings_get_enable_private_browsing(settings))
	{
		webkit_settings_set_enable_private_browsing(settings, FALSE);
		webkit_cookie_manager_set_accept_policy(cookieManager, WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);
	}else
	{
		webkit_settings_set_enable_private_browsing(settings, TRUE);
		webkit_cookie_manager_set_accept_policy(cookieManager, WEBKIT_COOKIE_POLICY_ACCEPT_NEVER);
	}

	updateBorder(rusk);
}

RuskWindow* createNewWindow(RuskWindow *rusk)
{
	RuskWindow *newRusk = makeRusk();

	WebKitSettings *settings = webkit_web_view_get_settings(rusk->webview);
	WebKitSettings *newRuskSettings = webkit_web_view_get_settings(rusk->webview);
	webkit_settings_set_enable_private_browsing(newRuskSettings, webkit_settings_get_enable_private_browsing(settings));

	return newRusk;
}

void toggleInspector(RuskWindow *rusk)
{
	g_object_set(G_OBJECT(webkit_web_view_get_settings(rusk->webview)), "enable-developer-extras", TRUE, NULL);

	WebKitWebInspector *inspector = webkit_web_view_get_inspector(rusk->webview);

	if(webkit_web_inspector_is_attached(inspector))
	{
		webkit_web_inspector_close(inspector);
	}else
	{
		webkit_web_inspector_show(inspector);
	}
}

GtkWidget* onRequestNewWindow(WebKitWebView *webview, RuskWindow *rusk)
{
	return GTK_WIDGET(makeRusk()->webview);
}

void startDownload(const char *uri, const char *dest)
{
	if(fork() == 0)
	{
		execlp(DOWNLOAD_COMMAND, DOWNLOAD_COMMAND, uri, dest, NULL);
	}
}

gboolean decideDownloadDestination(WebKitDownload *download, const gchar *suggest, RuskWindow *rusk)
{
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new("Save File", rusk->window, GTK_FILE_CHOOSER_ACTION_SAVE,
										  "_Cancel", GTK_RESPONSE_CANCEL,
										  "_Save", GTK_RESPONSE_ACCEPT,
										  NULL);

	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

	char *buf = g_strdup_printf("%s%s", g_get_home_dir(), suggest);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), buf);
	g_free(buf);

	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

		printf("download %s\n", webkit_uri_request_get_uri(webkit_download_get_request(download)));
		printf("suggest: %s\n", suggest);
		printf("save to: %s\n", filename);

		startDownload(
			webkit_uri_request_get_uri(webkit_download_get_request(download)),
			filename
		);

		g_free(filename);
	}

	gtk_widget_destroy(dialog);

	return TRUE;
}

void onDownloadStarted(WebKitWebView *webview, WebKitDownload *download, RuskWindow *rusk)
{
	g_signal_connect(G_OBJECT(download), "decide-destination", G_CALLBACK(decideDownloadDestination), rusk);
}

gboolean onKeyPress(GtkWidget *widget, GdkEventKey *key, RuskWindow *rusk)
{
	gboolean proceed = TRUE;

	char *script = g_strdup_printf(
			"onKeyPress({ctrlKey: %d, shiftKey: %d, superKey: %d, hyperKey: %d, metaKey: %d, string: '%s'})",
			(key->state & GDK_CONTROL_MASK)?1:0,
			(key->state & GDK_SHIFT_MASK)?1:0,
			(key->state & GDK_SUPER_MASK)?1:0,
			(key->state & GDK_HYPER_MASK)?1:0,
			(key->state & GDK_META_MASK)?1:0,
			gdk_keyval_name(key->keyval));
	webkit_web_view_run_javascript(rusk->webview, script, NULL, NULL, NULL);
	g_free(script);

	if(key->state & GDK_CONTROL_MASK)
	{
		switch(gdk_keyval_to_upper(key->keyval))
		{
			case GDK_KEY_S:
				inSiteSearchToggle(rusk);
				break;
			case GDK_KEY_N:
				if(key->state & GDK_SHIFT_MASK)
				{
					openURI(createNewWindow(rusk), HOMEPAGE);
				}else
				{
					inSiteSearchNext(rusk);
				}
				break;
			case GDK_KEY_P:
				if(key->state & GDK_SHIFT_MASK)
				{
					togglePrivateBrowsing(rusk);
				}else
				{
					inSiteSearchPrev(rusk);
				}
				break;

			case GDK_KEY_U:
				addressbarToggle(rusk);
				break;

			case GDK_KEY_G:
				globalSearchToggle(rusk, "google");
				break;
			case GDK_KEY_M:
				globalSearchToggle(rusk, "maps");
				break;
			case GDK_KEY_I:
				if(key->state & GDK_SHIFT_MASK)
				{
					toggleInspector(rusk);
				}else
				{
					globalSearchToggle(rusk, "images");
				}
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

int addScriptByFileName(WebKitUserContentManager *contentManager, const char *scriptPath, WebKitUserScriptInjectionTime InjectionTime)
{
	if(access(scriptPath, F_OK) != 0)
	{
		return 1;
	}else
	{
		char *scriptStr = loadFile(scriptPath);
		if(scriptStr == NULL)
		{
			return -1;
		}

		WebKitUserScript *script = webkit_user_script_new(scriptStr, WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, InjectionTime, NULL, NULL);

		free(scriptStr);

		if(script == NULL)
		{
			return -1;
		}

		webkit_user_content_manager_add_script(contentManager, script);
	}

	return 0;
}

int setupWebView(RuskWindow *rusk)
{
	WebKitCookieManager *cookieManager;
	WebKitWebContext *context;

	context = webkit_web_context_get_default();

	cookieManager = webkit_web_context_get_cookie_manager(context);
	if(cookieManager == NULL)
	{
		return -1;
	}

	gchar *dbpath = getDatabasePath();
	webkit_cookie_manager_set_persistent_storage(cookieManager, dbpath, WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
	g_free(dbpath);

	gchar *favdir = getFaviconDir();
	webkit_web_context_set_favicon_database_directory(context, favdir);
	g_free(favdir);

	webkit_settings_set_user_agent_with_application_details(webkit_web_view_get_settings(rusk->webview), "rusk", VERSION);

	g_signal_connect(G_OBJECT(rusk->webview), "notify::title", G_CALLBACK(onTitleChange), rusk);
	g_signal_connect(G_OBJECT(rusk->webview), "notify::estimated-load-progress", G_CALLBACK(onProgressChange), rusk);
	g_signal_connect(G_OBJECT(rusk->webview), "load-changed", G_CALLBACK(onLoadChange), rusk);
	g_signal_connect(G_OBJECT(rusk->webview), "notify::favicon", G_CALLBACK(onFaviconChange), rusk);
	g_signal_connect(G_OBJECT(rusk->webview), "mouse-target-changed", G_CALLBACK(onMouseTargetChange), rusk);
	g_signal_connect(G_OBJECT(rusk->webview), "create", G_CALLBACK(onRequestNewWindow), rusk);

	g_signal_connect(G_OBJECT(context), "download-started", G_CALLBACK(onDownloadStarted), rusk);

	WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(rusk->webview);
	if(addScriptByFileName(manager, SCRIPT_DOCUMENT_START, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START) < 0)
	{
		return -1;
	}
	if(addScriptByFileName(manager, SCRIPT_DOCUMENT_END, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END) < 0)
	{
		return -1;
	}

	return 0;
}

void closeRusk(GtkWidget *widget, RuskWindow *rusk)
{
	gtk_widget_destroy(GTK_WIDGET(rusk->window));
	free(rusk);

	sqlite3_finalize(rusk->database.insertStmt);
	sqlite3_close(rusk->database.connection);

	g_ruskCounter--;
	if(g_ruskCounter <= 0)
	{
		gtk_main_quit();
	}
}

int makeWindow(RuskWindow *rusk)
{
	GtkWidget *box;

	if((rusk->window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL))) == NULL)
	{
		return -1;
	}

	gtk_container_set_border_width(GTK_CONTAINER(rusk->window), BORDER_WIDTH);
	gtk_widget_override_background_color(GTK_WIDGET(rusk->window), GTK_STATE_FLAG_NORMAL, BORDER_COLOR_NORMAL);

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(rusk->window), box);

	rusk->addressbar = GTK_ENTRY(gtk_entry_new());
	gtk_entry_set_placeholder_text(rusk->addressbar, "URI");
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->addressbar), FALSE, FALSE, 0);

	rusk->globalSearch = GTK_ENTRY(gtk_search_entry_new());
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->globalSearch), FALSE, FALSE, 0);

	rusk->insiteSearch = GTK_ENTRY(gtk_search_entry_new());
	gtk_entry_set_placeholder_text(rusk->insiteSearch, "site search");
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->insiteSearch), FALSE, FALSE, 0);

	rusk->progressbar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->progressbar), FALSE, FALSE, 0);

	rusk->webview = WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(webkit_web_context_new_with_website_data_manager(webkit_website_data_manager_new(
		"base-cache-directory", g_strdup_printf("%s/rusk", g_get_user_cache_dir()),
		"disk-cache-directory", g_strdup_printf("%s/rusk", g_get_user_cache_dir()),
		"base-data-directory", g_strdup_printf("%s/rusk/data", g_get_user_data_dir()),
		"indexeddb-directory", g_strdup_printf("%s/rusk/indexed", g_get_user_data_dir()),
		"local-storage-directory", g_strdup_printf("%s/rusk/local-storage", g_get_user_data_dir()),
		"offline-application-cache-directory", g_strdup_printf("%s/rusk/offline-apps", g_get_user_data_dir()),
		"websql-directory", g_strdup_printf("%s/rusk/websql", g_get_user_data_dir()),
		NULL
	))));
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(rusk->webview), TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(rusk->window), "destroy", G_CALLBACK(closeRusk), rusk);
	gtk_widget_show_all(GTK_WIDGET(rusk->window));

	gtk_widget_set_visible(GTK_WIDGET(rusk->addressbar), FALSE);
	gtk_widget_set_visible(GTK_WIDGET(rusk->globalSearch), FALSE);
	gtk_widget_set_visible(GTK_WIDGET(rusk->progressbar), FALSE);
	gtk_widget_set_visible(GTK_WIDGET(rusk->insiteSearch), FALSE);

	g_signal_connect(G_OBJECT(rusk->window), "key-press-event", G_CALLBACK(onKeyPress), rusk);
	g_signal_connect(G_OBJECT(rusk->insiteSearch), "search-changed", G_CALLBACK(onInSiteSearchInput), rusk);
	g_signal_connect(G_OBJECT(rusk->addressbar), "key-release-event", G_CALLBACK(onAddressbarInput), rusk);
	g_signal_connect(G_OBJECT(rusk->globalSearch), "activate", G_CALLBACK(onGlobalSearchActivate), rusk);

	return 0;
}

int connectDataBase(RuskWindow *rusk)
{
	gchar *dbpath = getDatabasePath();

	if(sqlite3_open(dbpath, &rusk->database.connection) != SQLITE_OK)
	{
		return -1;
	}

	g_free(dbpath);

	char *error = NULL;
	if(sqlite3_exec(rusk->database.connection, "create table if not exists rusk_history (date integer not null check(date > 0), uri text not null unique);", NULL, NULL, &error) != SQLITE_OK)
	{
		sqlite3_free(error);
		return -1;
	}

	const char *insert = "insert or replace into rusk_history values(:date, :uri);";
	sqlite3_prepare(rusk->database.connection, insert, strlen(insert), &rusk->database.insertStmt, NULL);

	return 0;
}

RuskWindow* makeRusk()
{
	RuskWindow *rusk;
	
	if((rusk = malloc(sizeof(RuskWindow))) == NULL)
	{
		return NULL;
	}

	if(makeWindow(rusk) != 0)
	{
		return NULL;
	}

	if(setupWebView(rusk) != 0)
	{
		return NULL;
	}

	if(connectDataBase(rusk) != 0)
	{
		return NULL;
	}

	g_ruskCounter++;

	return rusk;
}

int main(int argc, char **argv)
{
	gtk_init(&argc, &argv);

	if(argc <= 1)
	{
		openURI(makeRusk(), HOMEPAGE);
	}else
	{
		for(int i=1; i<argc; i++)
		{
			openURI(makeRusk(), argv[i]);
		}
	}

	gtk_main();

	return 0;
}
