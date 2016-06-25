#include <stdio.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#define TESTFILE "/mnt/tmpfs/test.txt"


typedef struct {
	gchar *uri;
	gchar *dest;
	GtkProgressBar *progress;
	GtkBox *buttonArea;
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
	if(g_strcmp0(gtk_progress_bar_get_text(task->progress), "failed") != 0)
	{
		gtk_progress_bar_set_text(task->progress, "finished");
		printf("finish\n");  // debug
	}
}

void onFailed(WebKitDownload *download, GError *error, DownloadTask *task)
{
	gtk_progress_bar_set_text(task->progress, "failed");
	printf("failed: %d/%d\n", error->domain == WEBKIT_DOWNLOAD_ERROR, error->code);  // debug
}

void addTaskView(GtkBox *parent, DownloadTask *task)
{
	GtkWidget *outer, *statusArea, *uri, *dest;
	GtkWidget *testButton;
	char *markup;

	outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(parent, outer, FALSE, FALSE, 0);


	statusArea = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(outer), statusArea, TRUE, TRUE, 0);

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


	task->buttonArea = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_pack_start(GTK_BOX(outer), GTK_WIDGET(task->buttonArea), FALSE, FALSE, 0);

	testButton = gtk_button_new_with_label("stop");
	gtk_box_pack_end(task->buttonArea, testButton, FALSE, FALSE, 0);


	gtk_widget_show_all(outer);
}

void startTask(WebKitWebView *webview, DownloadTask *task)
{
	printf("%p, %p\n", task, task->progress);
	WebKitDownload *download = webkit_web_view_download_uri(webview, task->uri);

	g_signal_connect(G_OBJECT(download), "decide-destination", G_CALLBACK(onDecideDestination), task);
	g_signal_connect(G_OBJECT(download), "received-data", G_CALLBACK(onProgress), task);
	g_signal_connect(G_OBJECT(download), "finished", G_CALLBACK(onFinished), task);
	g_signal_connect(G_OBJECT(download), "failed", G_CALLBACK(onFailed), task);
}

int main(int argc, char **argv)
{
	gtk_init(&argc, &argv);

	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	gtk_widget_show_all(window);

	WebKitWebView *webview = WEBKIT_WEB_VIEW(webkit_web_view_new());

	DownloadTask task = {
		.uri= g_strdup("http://localhost:8790/"),
		.dest= g_strdup(TESTFILE)
	};

	printf("uri: %s\ndest: %s\n", task.uri, task.dest);

	addTaskView(GTK_BOX(box), &task);
	startTask(webview, &task);

	gtk_main();
}
