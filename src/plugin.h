#ifndef __PLUGIN_H_
#define __PLUGIN_H_

typedef GList* (*initialize_func)(void);
typedef void (*destroy_func)(void);

typedef struct _App
{
    gchar *name;
    gchar *path;
    GList *views;
    GPid pid;
} App;

typedef struct _AppView
{
    gchar *name;
    gchar *label;
    GtkScrolledWindow *container;

} AppView;

typedef struct
{
    GList *plugin_list;
} PluginManager;

typedef struct
{
    const gchar* name;
    const gchar* icon;
    gulong plug_id;
} PluginViewDefinition;

#endif //__PLUGIN_H_
