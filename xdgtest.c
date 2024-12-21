/*
 * xdg_activation_test.c
 * 
 * Simple GTK+ app to test passing xdg-activation tokens among two
 * running instances.
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkwayland.h>
#include <glib/gstdio.h>
#include <glib-unix.h>
#include <unistd.h>

int pipefd[2];
GdkDisplay *dsp;
GAppInfo *app;
GtkSpinButton *timeout;
FILE *f_pipe_write;

static gboolean send_token(gpointer data)
{
	char *id = (char*)data;
	fprintf(f_pipe_write, "%s\n", id);
	fflush(f_pipe_write);
	g_free(id);
	return FALSE; // do not repeat
}

static void token1(GtkButton*, void*)
{
	GdkAppLaunchContext *ctx = gdk_display_get_app_launch_context(dsp);
	char *id = g_app_launch_context_get_startup_notify_id(G_APP_LAUNCH_CONTEXT(ctx), app, NULL);
	fprintf(stderr, "[parent] got token: %s\n", id);
	double delay = gtk_spin_button_get_value(timeout); // in seconds
	if (delay > 0.0)
		g_timeout_add((unsigned int)(1000.0 * delay), send_token, id);
	else send_token(id);
	g_object_unref(ctx);
}

static void token2(GtkButton*, void*)
{
	GdkAppLaunchContext *ctx = gdk_display_get_app_launch_context(dsp);
	char *id = g_app_launch_context_get_startup_notify_id(G_APP_LAUNCH_CONTEXT(ctx), app, NULL);
	GdkAppLaunchContext *ctx2 = gdk_display_get_app_launch_context(dsp);
	char *id2 = g_app_launch_context_get_startup_notify_id(G_APP_LAUNCH_CONTEXT(ctx), app, NULL);
	fprintf(stderr, "[parent] got tokens: %s, %s\n", id, id2);
	double delay = gtk_spin_button_get_value(timeout); // in seconds
	if (delay > 0.0)
		g_timeout_add((unsigned int)(1000.0 * delay), send_token, id);
	else send_token(id);
	g_free(id2);
	g_object_unref(ctx);
	g_object_unref(ctx2);
}

static void token0(GtkButton*, void*)
{
	fprintf(f_pipe_write, "NO_TOKEN\n");
	fflush(f_pipe_write);
}

static gboolean child_read_pipe(GIOChannel *src, GIOCondition, gpointer data)
{
	GString *str = g_string_new(NULL);
	gsize pos;
	GIOStatus st = g_io_channel_read_line_string(src, str, &pos, NULL);
	if (st == G_IO_STATUS_EOF)
	{
		gtk_main_quit();
		return FALSE;
	}
	if (st == G_IO_STATUS_NORMAL)
	{
		str->str[pos] = 0;
		fprintf(stderr, "[child] read token: %s\n", str->str);
		GtkWindow *win = (GtkWindow*)data;
		if (strcmp(str->str, "NO_TOKEN"))
			gdk_wayland_display_set_startup_notification_id(dsp, str->str);
		gtk_window_present(win);
		g_string_free(str, TRUE);
	}
	return TRUE;
}

static void dialog_activate(GtkButton*, void *data)
{
	GtkWindow *win = (GtkWindow*)data;
	token1(NULL, NULL);
	gtk_window_close(win);
}

static void dialog(GtkButton*, void *data)
{
	GtkWindow *parent = (GtkWindow*)data;
	GtkWindow *win = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_title(win, "Dialog window");
	gtk_window_set_transient_for(win, parent);
	gtk_window_set_modal(win, TRUE);
	
	GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 20));
	GtkWidget *btn = gtk_button_new_with_label("Activate child");
	gtk_box_pack_start(box, btn, TRUE, FALSE, 20);
	g_signal_connect(btn, "clicked", G_CALLBACK(dialog_activate), win);
	
	gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(box));
	gtk_widget_show_all(GTK_WIDGET(win));
}

int main(int argc, char **argv)
{
	if (pipe(pipefd)) return -1;
	
	pid_t pid = fork();
	if (pid < 0) return -1;
	
	if (pid > 0)
	{
		// parent process
		close(pipefd[0]);
		f_pipe_write = fdopen(pipefd[1], "w");
		
		gtk_init(&argc, &argv);
		
		// need to create a dummy GAppInfo to give as a parameter to
		// g_app_launch_context_get_startup_notify_id(), even though it is not used
		app = g_app_info_create_from_commandline("dummy", "dummy", G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION, NULL);
		
		dsp = gdk_display_get_default();
		
		GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(win), "Parent");
		
		GtkWidget *lbl = gtk_label_new("Activate child with:");
		GtkWidget *btn1 = gtk_button_new_with_label("Valid token");
		GtkWidget *btn2 = gtk_button_new_with_label("Expired token");
		GtkWidget *btn3 = gtk_button_new_with_label("No token");
		GtkWidget *btn4 = gtk_button_new_with_label("Show dialog");
		GtkWidget *timeout2 = gtk_spin_button_new_with_range (0.0, 10.0, 0.1);
		
		GtkWidget *menubutton = gtk_menu_button_new();
		gtk_button_set_label(GTK_BUTTON(menubutton), "Popup menu");
		GtkWidget *menu = gtk_menu_new();
		GtkWidget *item = gtk_menu_item_new_with_label("Activate child");
		gtk_widget_show_all (item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_menu_button_set_popup(GTK_MENU_BUTTON(menubutton), menu);
		
		g_signal_connect(btn1, "clicked", G_CALLBACK(token1), NULL);
		g_signal_connect(btn2, "clicked", G_CALLBACK(token2), NULL);
		g_signal_connect(btn3, "clicked", G_CALLBACK(token0), NULL);
		g_signal_connect(btn4, "clicked", G_CALLBACK(dialog), win);
		g_signal_connect(item, "activate", G_CALLBACK (token1), NULL);
		g_signal_connect(win, "destroy", gtk_main_quit, NULL);
		
		GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 10));
		gtk_box_pack_start(box, lbl, TRUE, FALSE, 20);
		gtk_box_pack_start(box, btn1, TRUE, FALSE, 5);
		gtk_box_pack_start(box, btn2, TRUE, FALSE, 5);
		gtk_box_pack_start(box, btn3, TRUE, FALSE, 5);
		gtk_box_pack_start(box, btn4, TRUE, FALSE, 5);
		gtk_box_pack_start(box, menubutton, TRUE, FALSE, 5);
		gtk_box_pack_start(box, gtk_label_new("Delay before activating:"), TRUE, FALSE, 20);
		gtk_box_pack_start(box, timeout2, TRUE, FALSE, 5);
		timeout = GTK_SPIN_BUTTON(timeout2);
		
		gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(box));
		gtk_widget_show_all(win);
		
		gtk_main();
		fclose(f_pipe_write);
		g_object_unref(app);
	}
	else
	{
		// child process
		close(pipefd[1]);
		
		gtk_init(&argc, &argv);
		
		dsp = gdk_display_get_default();
		
		GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(win), "Child");
		gtk_window_set_default_size(GTK_WINDOW(win), 500, 400);
		g_signal_connect(win, "destroy", gtk_main_quit, NULL);
		
		GtkWidget *lbl = gtk_label_new("Child window");
		gtk_container_add(GTK_CONTAINER(win), lbl);
		
		GIOChannel *src = g_io_channel_unix_new(pipefd[0]);
		g_io_add_watch(src, G_IO_IN, child_read_pipe, GTK_WINDOW(win));
		
		gtk_widget_show_all(win);
		
		gtk_main();
		close(pipefd[0]);
	}
	
	return 0;
}

