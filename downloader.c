#include <stdio.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>


typedef struct {
	gchar *uri, *dest;
	WebKitDownload *download;
	GtkBox *parent, *outer;
	GtkProgressBar *progress;
	GtkButton *cancel, *close;
} DownloadTask;


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

void onCloseTask(GtkButton *button, DownloadTask *task)
{
	gtk_container_remove(GTK_CONTAINER(task->parent), GTK_WIDGET(task->outer));
	g_free(task->uri);
	g_free(task->dest);
}

void addTaskView(GtkBox *parent, DownloadTask *task)
{
	GtkWidget *statusArea, *uri, *dest;
	GtkBox *buttonArea;
	char *markup;

	task->parent = parent;
	task->outer = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
	gtk_box_pack_start(parent, GTK_WIDGET(task->outer), FALSE, FALSE, 0);


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

void startTask(WebKitWebView *webview, DownloadTask *task)
{
	task->download = webkit_web_view_download_uri(webview, task->uri);

	g_signal_connect(G_OBJECT(task->download), "decide-destination", G_CALLBACK(onDecideDestination), task);
	g_signal_connect(G_OBJECT(task->download), "received-data", G_CALLBACK(onProgress), task);
	g_signal_connect(G_OBJECT(task->download), "finished", G_CALLBACK(onFinished), task);
	g_signal_connect(G_OBJECT(task->download), "failed", G_CALLBACK(onFailed), task);
}

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		fprintf(stderr, "$ %s [URI] [DESTNATION]\n", argv[0]);
		return 1;
	}

	gtk_init(&argc, &argv);

	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	gtk_container_add(GTK_CONTAINER(window), box);
	g_signal_connect(G_OBJECT(window), "destroy", gtk_main_quit, NULL);

	gtk_widget_show_all(window);

	WebKitWebView *webview = WEBKIT_WEB_VIEW(webkit_web_view_new());

	DownloadTask task = {
		.uri= g_strdup(argv[1]),
		.dest= g_strdup(argv[2])
	};

	printf("uri: %s\ndest: %s\n", task.uri, task.dest);

	addTaskView(GTK_BOX(box), &task);
	startTask(webview, &task);

	gtk_main();
}
