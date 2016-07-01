#include <fcntl.h>
#include <gtk/gtk.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <webkit2/webkit2.h>

#ifdef __linux__
#include <linux/limits.h>
#endif


typedef struct {
	GtkBox *box;
	WebKitWebView *webview;
	int sock_fd;
} RuskDL;

typedef struct {
	gchar *uri, *dest;
	WebKitDownload *download;
	RuskDL *downloader;
	GtkBox *outer;
	GtkProgressBar *progress;
	GtkButton *cancel, *close;
} DownloadTask;


char *getPipePath(char *buf, const size_t len)
{
	snprintf(buf, len, "%s/rusk/dl.pipe", g_get_user_cache_dir());
	return buf;
}

gboolean onDecideDestination(WebKitDownload *download, const char *suggest, DownloadTask *task)
{
	if(access(task->dest, F_OK) == 0)
	{
		remove(task->dest);
	}

	gchar *target = g_strdup_printf("file://%s", task->dest);
	webkit_download_set_destination(download, target);
	g_free(target);

	printf("start\n");  // debug
	return TRUE;
}

void onProgress(WebKitDownload *download, guint64 data_length, DownloadTask *task)
{
	const double progress = webkit_download_get_estimated_progress(download);
	const double elapsed  = webkit_download_get_elapsed_time(download);
	const double remain   = elapsed/progress - elapsed;

	gtk_progress_bar_set_fraction(task->progress, progress);

	gchar *p_str = g_strdup_printf("%.1lf%% [%.0lf sec @ %.0lf sec]", progress*100, elapsed, remain);
	gtk_progress_bar_set_text(task->progress, p_str);
	g_free(p_str);

	printf("progress: %.1lf%% [%.0lf sec @ %.0lf sec]\n", progress*100, elapsed, remain);  // debug
}

void onFinished(WebKitDownload *download, DownloadTask *task)
{
	if(g_strcmp0(gtk_progress_bar_get_text(task->progress), "failed") != 0
	&& g_strcmp0(gtk_progress_bar_get_text(task->progress), "canceled") != 0)
	{
		gtk_progress_bar_set_text(task->progress, "finished");
		printf("finish\n");  // debug
	}

	gtk_widget_set_visible(GTK_WIDGET(task->cancel), FALSE);
	gtk_widget_set_visible(GTK_WIDGET(task->close), TRUE);
}

void onFailed(WebKitDownload *download, GError *error, DownloadTask *task)
{
	if(g_strcmp0(gtk_progress_bar_get_text(task->progress), "canceled") != 0)
	{
		gtk_progress_bar_set_text(task->progress, "failed");
		printf("failed: %d/%d\n", error->domain == WEBKIT_DOWNLOAD_ERROR, error->code);  // debug
	}
}

void onCancelTask(GtkButton *button, DownloadTask *task)
{
	webkit_download_cancel(task->download);
	gtk_progress_bar_set_text(task->progress, "canceled");
}

void closeWindow(GtkWidget *widget, RuskDL *dl)
{
	char buf[PATH_MAX];
	remove(getPipePath(buf, sizeof(buf)));

	gtk_main_quit();
}

void onCloseTask(GtkButton *button, DownloadTask *task)
{
	gtk_container_remove(GTK_CONTAINER(task->downloader->box), GTK_WIDGET(task->outer));
	g_free(task->uri);
	g_free(task->dest);

	if(g_list_length(gtk_container_get_children(GTK_CONTAINER(task->downloader->box))) == 0)
	{
		closeWindow(NULL, task->downloader);
	}
}

void addTaskView(RuskDL *downloader, DownloadTask *task)
{
	GtkWidget *statusArea, *uri, *dest;
	GtkBox *buttonArea;
	char *markup;

	task->downloader = downloader;
	task->outer = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
	gtk_box_pack_start(task->downloader->box, GTK_WIDGET(task->outer), FALSE, FALSE, 0);


	statusArea = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(task->outer, statusArea, TRUE, TRUE, 0);

	uri = gtk_label_new(NULL);
	markup = g_markup_printf_escaped("<span size=\"large\">%s</span>", task->uri);
	gtk_label_set_markup(GTK_LABEL(uri), markup);
	g_free(markup);
	gtk_widget_set_halign(uri, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(statusArea), uri, FALSE, FALSE, 0);

	task->progress = GTK_PROGRESS_BAR(gtk_progress_bar_new());
	gtk_box_pack_start(GTK_BOX(statusArea), GTK_WIDGET(task->progress), TRUE, TRUE, 0);
	gtk_progress_bar_set_show_text(task->progress, TRUE);

	gtk_progress_bar_set_fraction(task->progress, 0.0);
	gtk_progress_bar_set_text(task->progress, "0%");

	dest = gtk_link_button_new(task->dest);
	gtk_widget_set_halign(dest, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(statusArea), dest, FALSE, FALSE, 0);


	buttonArea = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_pack_start(task->outer, GTK_WIDGET(buttonArea), FALSE, FALSE, 0);

	task->cancel = GTK_BUTTON(gtk_button_new_with_label("cancel"));
	gtk_box_pack_end(buttonArea, GTK_WIDGET(task->cancel), FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(task->cancel), "clicked", G_CALLBACK(onCancelTask), task);

	task->close = GTK_BUTTON(gtk_button_new_with_label("close"));
	gtk_box_pack_end(buttonArea, GTK_WIDGET(task->close), FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(task->close), "clicked", G_CALLBACK(onCloseTask), task);


	gtk_widget_show_all(GTK_WIDGET(task->outer));
	gtk_widget_set_visible(GTK_WIDGET(task->close), FALSE);
}

void startDownload(WebKitWebView *webview, DownloadTask *task)
{
	task->download = webkit_web_view_download_uri(webview, task->uri);

	g_signal_connect(G_OBJECT(task->download), "decide-destination", G_CALLBACK(onDecideDestination), task);
	g_signal_connect(G_OBJECT(task->download), "received-data", G_CALLBACK(onProgress), task);
	g_signal_connect(G_OBJECT(task->download), "finished", G_CALLBACK(onFinished), task);
	g_signal_connect(G_OBJECT(task->download), "failed", G_CALLBACK(onFailed), task);
}

RuskDL *makeWindow()
{
	RuskDL *dl = (RuskDL *)malloc(sizeof(RuskDL));

	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	dl->box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 5));
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(dl->box));
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(closeWindow), dl);

	gtk_widget_show_all(window);

	dl->webview = WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(webkit_web_context_new_with_website_data_manager(webkit_website_data_manager_new(
		"base-cache-directory", g_strdup_printf("%s/rusk", g_get_user_cache_dir()),
		"disk-cache-directory", g_strdup_printf("%s/rusk", g_get_user_cache_dir()),
		"base-data-directory", g_strdup_printf("%s/rusk/data", g_get_user_data_dir()),
		"indexeddb-directory", g_strdup_printf("%s/rusk/indexed", g_get_user_data_dir()),
		"local-storage-directory", g_strdup_printf("%s/rusk/local-storage", g_get_user_data_dir()),
		"offline-application-cache-directory", g_strdup_printf("%s/rusk/offline-apps", g_get_user_data_dir()),
		"websql-directory", g_strdup_printf("%s/rusk/websql", g_get_user_data_dir()),
		NULL
	))));

	return dl;
}

void startTask(RuskDL *dl, gchar *uri, gchar *dest)
{
	DownloadTask *task = (DownloadTask *)malloc(sizeof(DownloadTask));
	task->uri = uri;
	task->dest = dest;

	addTaskView(dl, task);
	startDownload(dl->webview, task);
}

gboolean onRequest(GIOChannel *source, GIOCondition condition, RuskDL *dl)
{
	gchar *uri, *dest;

	if(g_io_channel_read_line(source, &uri, NULL, NULL, NULL) != G_IO_STATUS_NORMAL)
	{
		fprintf(stderr, "failed read.\n");
		return false;
	}

	if(g_io_channel_read_line(source, &dest, NULL, NULL, NULL) != G_IO_STATUS_NORMAL)
	{
		g_free(uri);
		fprintf(stderr, "failed read.\n");
		return false;
	}

	startTask(dl, uri, dest);

	g_io_add_watch(source, G_IO_IN, (GIOFunc)onRequest, dl);
	return false;
}

bool requestDownload(char *uri, char *dest)
{
	char path[PATH_MAX];
	int fd = open(getPipePath(path, sizeof(path)), O_WRONLY);

	if(fd < 0)
	{
		return false;
	}

	write(fd, uri, strlen(uri)+1);
	write(fd, dest, strlen(dest)+1);

	close(fd);

	return true;
}

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		fprintf(stderr, "$ %s [URI] [DESTNATION]\n", argv[0]);
		return 1;
	}

	if(!requestDownload(argv[1], argv[2]))
	{
		char path[PATH_MAX];

		remove(getPipePath(path, sizeof(path)));
		mkfifo(path, 0600);

		int fd = open(path, O_RDONLY | O_NONBLOCK);
		if(fd < 0)
		{
			perror("open");
			return -1;
		}

		gtk_init(&argc, &argv);

		RuskDL *dl = makeWindow();
		dl->sock_fd = fd;

		startTask(dl, g_strdup(argv[1]), g_strdup(argv[2]));

		GIOChannel *channel = g_io_channel_unix_new(fd);
		g_io_channel_set_line_term(channel, g_strdup("\0"), 1);
		g_io_add_watch(channel, G_IO_IN, (GIOFunc)onRequest, dl);

		gtk_main();
	}
}
