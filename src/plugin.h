#ifndef __PLUGIN_H_
#define __PLUGIN_H_

typedef GList* (*initialize_func)(void);
typedef void (*destroy_func)(void);
typedef struct _AppManagerPluginView
{
    gchar *name;
    gulong plug_id;
    GtkScrolledWindow *container;

    initialize_func initialize;
    destroy_func destroy;
} AppManagerPluginView;

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
