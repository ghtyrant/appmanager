#include <dlfcn.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <string.h>

#include "plugin.h"

PluginManager *
plugin_manager_new()
{
    PluginManager *p = g_new0(PluginManager, 1);

    return p;
}

void
plugin_manager_free_plugin_view(void *data)
{
    AppManagerPluginView *view = (AppManagerPluginView*) data;

    g_free(view->name);
    gtk_widget_destroy(GTK_WIDGET(view->container));
    view->destroy();

    g_free(view);
}

void
plugin_manager_free(PluginManager *p)
{
    g_list_free_full(p->plugin_list, plugin_manager_free_plugin_view);
    g_free(p);
}

void
plugin_manager_load_plugin(PluginManager *p, const char *path, GtkNotebook *nb)
{
    void *plugin = dlopen(path, RTLD_LAZY);
    if (!plugin)
        return;

    dlerror();
    initialize_func initialize = dlsym(plugin, "initialize_plugin");
    const char *error;
    if ((error = dlerror()))
    {
        printf("Could not find 'initialize' symbol: %s\n", error);
        return;
    }

    dlerror();
    destroy_func destroy = dlsym(plugin, "destroy_plugin");
    if ((error = dlerror()))
    {
        printf("Could not find 'destroy' symbol: %s\n", error);
        return;
    }

    GList *views = (*initialize)();

    for (GList *i = views; i != NULL; i = i->next)
    {
        PluginViewDefinition *def = (PluginViewDefinition*)i->data;

        GtkSocket *socket = GTK_SOCKET(gtk_socket_new());
        GtkScrolledWindow *win = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
        gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(socket));

        gtk_notebook_append_page(nb, GTK_WIDGET(win), gtk_label_new(def->name));
        gtk_socket_add_id(socket, def->plug_id);

        AppManagerPluginView *plugin = g_new0(AppManagerPluginView, 1);
        plugin->plug_id = def->plug_id;
        plugin->container = win;
        plugin->initialize = initialize;
        plugin->destroy = destroy;
        plugin->name = g_strdup(def->name);

        p->plugin_list = g_list_append(p->plugin_list, p);
    }

    g_list_free(views);
}

void
plugin_manager_load_directory(PluginManager *pm, const gchar* path, GtkNotebook *notebook)
{
    GError *error = NULL;
    GDir *dir = g_dir_open(path, 0, &error);

    if (error)
    {
        g_warning("Error loading plugins from directory %s: %s", path, error->message);
        g_error_free(error);
        return;
    }

    const gchar *file;
    while ((file = g_dir_read_name(dir)) != NULL)
    {
        gchar *file_path = g_build_path(G_DIR_SEPARATOR_S, path, file, NULL);

        g_info("Checking file '%s' ...", file);
        if (strncmp(file + strlen(file) - 3, ".so", 3) == 0)
            plugin_manager_load_plugin(pm, file_path, notebook);

        g_free(file_path);
    }

    g_dir_close(dir);
}


void
app_shutdown(gpointer user_data)
{
    gtk_main_quit();
}

int main(int argc, char *argv[])
{
    GtkBuilder *builder;

    gtk_init(&argc, &argv);

    builder = gtk_builder_new();
    GError *error = NULL;
    gtk_builder_add_from_file(builder, "src/ui/mainwindow.glade", &error);

    if (error != NULL)
    {
        printf("Error loading user interface: %s", error->message);
        g_error_free(error);
        return 0;
    }

    //s->sample_depth     = GTK_COMBO_BOX(gtk_builder_get_object(builder, "Sample Depth"));

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

    PluginManager *pm = plugin_manager_new();

    plugin_manager_load_directory(pm, "/usr/lib/appmanager/plugins/", nb);
    gchar *home_path = g_build_path(G_DIR_SEPARATOR_S, g_get_home_dir(), ".appmanager", NULL);
    plugin_manager_load_directory(pm, home_path, nb);
    g_free(home_path);

    plugin_manager_load_plugin(pm, "../koradapp/koradapp.so", nb);

    gtk_widget_show_all(GTK_WIDGET(window));

    gtk_main();

    plugin_manager_free(pm);

    return 0;
}
