#include <stdio.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#define TESTFILE "/mnt/tmpfs/test.txt"

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

int main(int argc, char **argv)
{
	GtkWidget *webview;
	WebKitDownload *download;

	gtk_init(&argc, &argv);

	webview = webkit_web_view_new();

	download = webkit_web_view_download_uri(WEBKIT_WEB_VIEW(webview), "http://localhost:8790/");

	g_signal_connect(G_OBJECT(download), "decide-destination", G_CALLBACK(decideDestination), NULL);
	g_signal_connect(G_OBJECT(download), "notify::estimated-progress", G_CALLBACK(progress), NULL);
	g_signal_connect(G_OBJECT(download), "finished", G_CALLBACK(finished), NULL);
	g_signal_connect(G_OBJECT(download), "failed", G_CALLBACK(failed), NULL);

	gtk_main();
}
