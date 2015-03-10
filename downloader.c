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


gboolean decideDestination(WebKitDownload *download, const char *suggest, gpointer data)
{
	if(access(TESTFILE, F_OK) == 0)
	{
		remove(TESTFILE);
	}
	webkit_download_set_destination(download, "file://"TESTFILE);

	printf("start\n");
	return FALSE;
}

void progress(WebKitDownload *download, gpointer data)
{
	const double progress = webkit_download_get_estimated_progress(download);
	const double elapsed  = webkit_download_get_elapsed_time(download);
	const double remain   = elapsed/progress - elapsed;

	printf("progress: %.1lf%% [%.0lf sec][@ %.0lf sec]\n", progress*100, elapsed, remain);
}

void finished(WebKitDownload *download, gpointer data)
{
	printf("finish\n");
}

void failed(WebKitDownload *download, gpointer error, gpointer data)
{
	printf("failed\n");
}

void makeTaskView(GtkBox *parent, DownloadTask *task)
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

	dest = gtk_link_button_new(task->dest);
	gtk_widget_set_halign(dest, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(statusArea), dest, FALSE, FALSE, 0);


	task->buttonArea = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_pack_start(GTK_BOX(outer), GTK_WIDGET(task->buttonArea), FALSE, FALSE, 0);

	testButton = gtk_button_new_with_label("stop");
	gtk_box_pack_end(task->buttonArea, testButton, FALSE, FALSE, 0);
}

int main(int argc, char **argv)
{
	gtk_init(&argc, &argv);


	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	DownloadTask task;

	task.uri = g_strdup("test_file.txt");
	task.dest = g_strdup("http://localhost:8790");
	makeTaskView(GTK_BOX(box), &task);

	task.uri = g_strdup("this-is-test.txt");
	task.dest = g_strdup("http://localhost:8790/path/to/file.txt");
	makeTaskView(GTK_BOX(box), &task);

	gtk_widget_show_all(window);


/*
	GtkWidget *webview;
	WebKitDownload *download;

	webview = webkit_web_view_new();

	download = webkit_web_view_download_uri(WEBKIT_WEB_VIEW(webview), "http://localhost:8790/");

	g_signal_connect(G_OBJECT(download), "decide-destination", G_CALLBACK(decideDestination), NULL);
	g_signal_connect(G_OBJECT(download), "notify::estimated-progress", G_CALLBACK(progress), NULL);
	g_signal_connect(G_OBJECT(download), "finished", G_CALLBACK(finished), NULL);
	g_signal_connect(G_OBJECT(download), "failed", G_CALLBACK(failed), NULL);
*/

	gtk_main();
}
