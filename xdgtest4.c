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
#include <gdk/wayland/gdkwayland.h>
#include <glib/gstdio.h>
#include <glib-unix.h>
#include <gtk4-layer-shell.h>
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
	GtkWindow *win = (GtkWindow*)data;
	GString *str = g_string_new(NULL);
	gsize pos;
	GIOStatus st = g_io_channel_read_line_string(src, str, &pos, NULL);
	if (st == G_IO_STATUS_EOF)
	{
		gtk_window_close (win);
		return FALSE;
	}
	if (st == G_IO_STATUS_NORMAL)
	{
		str->str[pos] = 0;
		fprintf(stderr, "[child] read token: %s\n", str->str);
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
	GtkWindow *win = GTK_WINDOW(gtk_window_new());
	gtk_window_set_title(win, "Dialog window");
	gtk_window_set_transient_for(win, parent);
	gtk_window_set_modal(win, TRUE);
	
	GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 20));
	GtkWidget *btn = gtk_button_new_with_label("Activate child");
	gtk_box_append(box, btn);
	g_signal_connect(btn, "clicked", G_CALLBACK(dialog_activate), win);
	
	gtk_window_set_child(win, GTK_WIDGET(box));
	gtk_window_present(win);
}

static void token1_wrap(void*)
{
	token1(NULL, NULL);
}

static void menu_activate_immed(GSimpleAction*, GVariant*, gpointer)
{
	token1(NULL, NULL);
}

static void menu_activate_delayed(GSimpleAction*, GVariant*, gpointer)
{
	g_timeout_add_once(1000, token1_wrap, NULL);
}

static void open_popover(GtkButton*, void *data)
{
	GtkPopover *popover = (GtkPopover*)data;
	gtk_popover_popup(popover);
}

static void token_popover(GtkButton*, void *data)
{
	token1(NULL, NULL);
	GtkPopover *popover = (GtkPopover*)data;
	gtk_popover_popdown(popover);
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
		
		gboolean use_layer_shell = FALSE;
		int i;
		for (i = 1; i < argc; i++) if (argv[i][0] == '-' && argv[i][1] == 'l')
		{
			use_layer_shell = TRUE;
			break;
		}
		
		gtk_init();
		
		// need to create a dummy GAppInfo to give as a parameter to
		// g_app_launch_context_get_startup_notify_id(), even though it is not used
		app = g_app_info_create_from_commandline("dummy", "dummy", G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION, NULL);
		
		dsp = gdk_display_get_default();
		
		GtkWindow *win = GTK_WINDOW(gtk_window_new());
		if (use_layer_shell)
		{
			gtk_layer_init_for_window(win);
			gtk_layer_set_keyboard_mode(win, 2);
		}
		gtk_window_set_title(win, "Parent");
		
		GtkWidget *lbl = gtk_label_new("Activate child with:");
		GtkWidget *btn1 = gtk_button_new_with_label("Valid token");
		GtkWidget *btn2 = gtk_button_new_with_label("Expired token");
		GtkWidget *btn3 = gtk_button_new_with_label("No token");
		GtkWidget *btn4 = gtk_button_new_with_label("Show dialog");
		GtkWidget *btn5 = gtk_button_new_with_label("Show popover");
		GtkWidget *timeout2 = gtk_spin_button_new_with_range (0.0, 10.0, 0.1);
		
		GtkWidget *menubutton = gtk_menu_button_new();
		gtk_menu_button_set_label(GTK_MENU_BUTTON(menubutton), "Popup menu");
		
		GMenu *menu = g_menu_new();
		g_menu_append(menu, "Activate child (immediate)", "menu1.activate_immed");
		g_menu_append(menu, "Activate child (delayed)", "menu1.activate_delayed");
		gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menubutton), G_MENU_MODEL(menu));
		
		GSimpleActionGroup *actions = g_simple_action_group_new();
		const GActionEntry entries[] = {
			{ "activate_immed", menu_activate_immed},
			{ "activate_delayed", menu_activate_delayed}
		};
		g_action_map_add_action_entries(G_ACTION_MAP(actions), entries, G_N_ELEMENTS(entries), NULL);
		gtk_widget_insert_action_group(menubutton, "menu1", G_ACTION_GROUP(actions));
		
		GtkWidget *popover = gtk_popover_new();
		
		g_signal_connect(btn1, "clicked", G_CALLBACK(token1), NULL);
		g_signal_connect(btn2, "clicked", G_CALLBACK(token2), NULL);
		g_signal_connect(btn3, "clicked", G_CALLBACK(token0), NULL);
		g_signal_connect(btn4, "clicked", G_CALLBACK(dialog), win);
		g_signal_connect(btn5, "clicked", G_CALLBACK(open_popover), popover);
		
		GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 10));
		gtk_box_append(box, lbl);
		gtk_box_append(box, btn1);
		gtk_box_append(box, btn2);
		gtk_box_append(box, btn3);
		gtk_box_append(box, btn4);
		gtk_box_append(box, menubutton);
		gtk_box_append(box, btn5);
		gtk_box_append(box, gtk_label_new("Delay before activating:"));
		gtk_box_append(box, timeout2);
		timeout = GTK_SPIN_BUTTON(timeout2);
		
		gtk_window_set_child(win, GTK_WIDGET(box));
		gtk_window_present(win);
		
		// popover contents
		box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 10));
		gtk_popover_set_child(GTK_POPOVER(popover), GTK_WIDGET(box));
		GtkWidget *lblp1 = gtk_label_new("Custom popover");
		GtkWidget *btnp1 = gtk_button_new_with_label("Activate child");
		gtk_box_append(box, lblp1);
		gtk_box_append(box, btnp1);
		g_signal_connect(btnp1, "clicked", G_CALLBACK(token_popover), popover);
		gtk_widget_set_parent(popover, btn5);
		
		while (g_list_model_get_n_items(gtk_window_get_toplevels()) > 0)
			g_main_context_iteration(NULL, TRUE);
		fclose(f_pipe_write);
		g_object_unref(app);
	}
	else
	{
		// child process
		close(pipefd[1]);
		
		gtk_init();
		
		dsp = gdk_display_get_default();
		
		GtkWindow *win = GTK_WINDOW(gtk_window_new());
		gtk_window_set_title(win, "Child");
		gtk_window_set_default_size(win, 500, 400);
		
		GtkWidget *lbl = gtk_label_new("Child window");
		gtk_window_set_child(win, lbl);
		
		GIOChannel *src = g_io_channel_unix_new(pipefd[0]);
		g_io_add_watch(src, G_IO_IN, child_read_pipe, GTK_WINDOW(win));
		
		gtk_window_present(win);
		
		while (g_list_model_get_n_items(gtk_window_get_toplevels()) > 0)
			g_main_context_iteration(NULL, TRUE);
		close(pipefd[0]);
	}
	
	return 0;
}

