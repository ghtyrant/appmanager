#include <dlfcn.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <string.h>
#include <sys/wait.h>

#include "plugin.h"

void
app_shutdown(gpointer user_data)
{
    gtk_main_quit();
}

void
app_view_free(AppView *view)
{
    g_free(view->name);
    g_free(view);
}

void
app_free(App *app)
{
    g_free(app->name);
    g_free(app->path);

    kill(app->pid, SIGTERM);
    waitpid(app->pid, NULL, WNOHANG);

    g_list_free_full(app->views, app_view_free);
    g_free(app);
}

void
app_add_view(App *app, AppView *view)
{
    app->views = g_list_append(app->views, view);
}

void
app_start(App *app, GtkNotebook *notebook)
{
    GList *arg_views = NULL;
    for (GList *i = app->views; i != NULL; i = i->next)
    {
        AppView *view = i->data;

        g_info("Preparing view '%s' ...", view->name);

        GtkSocket *socket = GTK_SOCKET(gtk_socket_new());
        view->container = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
        gtk_container_add(GTK_CONTAINER(view->container), GTK_WIDGET(socket));

        gtk_notebook_append_page(notebook, GTK_WIDGET(view->container), gtk_label_new(view->name));
        gulong sock_id = gtk_socket_get_id(socket);

        arg_views = g_list_append(arg_views, g_strdup_printf("%s:%lu", view->name, sock_id));
    }

    guint child_argc = g_list_length(arg_views) + 2;
    gchar **child_argv = g_new0(gchar*, child_argc);
    child_argv[0] = g_strdup(app->path);

    guint index = 1;
    for (GList *i = arg_views; i != NULL; i = i->next)
        child_argv[index++] = g_strdup((gchar*)i->data);

    g_info("Parameters:");
    for (guint i = 0; i < child_argc; ++i)
        g_info("[%d] %s", i, child_argv[i]);

    GError *error = NULL;
    g_spawn_async_with_pipes(NULL,
            child_argv,
            NULL,
            G_SPAWN_DEFAULT,
            NULL, NULL,
            &app->pid,
            NULL,
            NULL,
            NULL,
            &error);

    g_list_free_full(arg_views, g_free);
    for (guint i = 0; i < child_argc; ++i)
        g_free(child_argv[i]);

    g_free(child_argv);

    if (error)
    {
        printf("Error loading app: %s", error->message);
        g_error_free(error);
        return;
    }
}

AppView *
config_load_view(GKeyFile *kf, App *app, const gchar *name)
{
    gchar *real_name = g_strdup_printf("%s:%s", app->name, name);

    if (!g_key_file_has_group(kf, real_name))
    {
        g_error("Error parsing view for app '%s': Missing group '%s'!", app->name, real_name);
        return NULL;
    }

    gchar *label = g_key_file_get_string(kf, real_name, "name", NULL);

    g_info("    Loaded view '%s' (%s)", name, label);

    AppView *view = g_new0(AppView, 1);
    view->name = g_strdup(name);
    view->label = g_strdup(label);
    return view;
}

App *
config_load_app(GKeyFile *kf, const gchar *name)
{
    if (!g_key_file_has_group(kf, name))
    {
        g_error("Error parsing app: Missing group '%s'!", name);
        return NULL;
    }

    gchar *path = g_key_file_get_string(kf, name, "path", NULL);
    gchar **views = g_key_file_get_string_list(kf, name, "views", NULL, NULL);

    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR))
    {
        g_error("App '%s': Can't open binary '%s': File does not exist!", name, path);
        return FALSE;
    }

    App *app = g_new0(App, 1);
    app->path = g_strdup(path);
    app->name = g_strdup(name);

    g_info("Loaded app '%s' (%s), loading views ...", app->name, app->path);

    for (gchar **i = views; *i != NULL; i++)
    {
        AppView *view = config_load_view(kf, app, *i);

        if (view == NULL)
        {
            app_free(app);
            return NULL;
        }
        else
            app_add_view(app, view);
    }

    return app;
}

GList *
parse_config()
{
    gchar *config_path = g_build_path(G_DIR_SEPARATOR_S, g_get_home_dir(), ".config", "appmanager", "config.ini", NULL);

    GError *error = NULL;
    GKeyFile *kf = g_key_file_new();
    g_key_file_load_from_file(kf, config_path, G_KEY_FILE_NONE, &error);
    g_free(config_path);

    if (error)
    {
        g_error("Error loading configuration: %s", error->message);
        g_error_free(error);
        return;
    }

    gchar **apps = g_key_file_get_string_list(kf, "core", "apps", NULL, &error);

    GList *app_list = NULL;
    for (gchar **i = apps; *i != NULL; i++)
    {
        g_info("Loading app '%s' ...", *i);
        App *app = config_load_app(kf, *i);
        if (app == NULL)
        {
            g_list_free_full(app_list, app_free);
            return NULL;
        }
        else
            app_list = g_list_append(app_list, app);
    }

    g_strfreev(apps);

    return app_list;
}

int main(int argc, char *argv[])
{
    GtkBuilder *builder;

    gtk_init(&argc, &argv);

    builder = gtk_builder_new();
    GError *error = NULL;
    gtk_builder_add_from_file(builder, "/home/fabian/projects/privat/appmanager/src/ui/mainwindow.glade", &error);

    if (error != NULL)
    {
        printf("Error loading user interface: %s", error->message);
        g_error_free(error);
        return 0;
    }

    GObject *window = gtk_builder_get_object(builder, "Main Window");
    g_signal_connect(window, "destroy", G_CALLBACK(app_shutdown), NULL);

    GtkNotebook *nb = GTK_NOTEBOOK(gtk_builder_get_object(builder, "Notebook"));

    /* Set up CSS style provider */
    /*GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);

    gtk_style_context_add_provider_for_screen(screen,
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_css_provider_load_from_path(GTK_CSS_PROVIDER(provider),
        "src/ui/style.css", &error);

    if (error != NULL)
    {
        printf("Error loading user interface style: %s", error->message);
        g_error_free(error);
        return 0;
    }

    g_object_unref(provider);
    */

    GList *apps = parse_config();
    g_info("Finished parsing config!");
    for (GList *i = apps; i != NULL; i = i->next)
    {
        App *app = i->data;
        g_info("Starting app '%s' ...", app->name);
        app_start(app, nb);
    }

    gtk_widget_show_all(GTK_WIDGET(window));

    gtk_main();

    g_list_free_full(apps, app_free);

    return 0;
}
