#ifndef __PLUGIN_H_
#define __PLUGIN_H_

typedef gulong* (*initialize_func)(void);
typedef void (*destroy_func)(void);
typedef struct _AppManagerPlugin
{
    const char *name;
    gulong plug_id;
    GtkScrolledWindow *container;

    initialize_func initialize;
    destroy_func destroy;

    struct _AppManagerPlugin *next;
} AppManagerPlugin;

typedef struct
{
    AppManagerPlugin *plugin_list;
    AppManagerPlugin *plugin_list_end;
} PluginManager;

#endif //__PLUGIN_H_
