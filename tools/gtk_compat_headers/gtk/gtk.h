/*
 * tools/gtk_compat_headers/gtk/gtk.h — GTK3「无 devel 包」编译垫片（GTK 部分）。
 *
 * 见 gdk/gdkx.h 顶部注释。只覆盖 tools/gtk_ime_test_client.c 用到的 API，
 * 原型与 GTK 3.24 头文件（gtk/gtk.h、glib/gobject）一致（消费者侧 ABI 相同）。
 */
#ifndef PINYINIME_GTK_COMPAT_H
#define PINYINIME_GTK_COMPAT_H

#include <stddef.h>
#include <stdlib.h> /* atoi 等（真实 gtk.h 亦间接引入） */
#include "../gdk/gdkx.h"  /* 相对包含：本文件位于 gtk/ 子目录 */

/* ---- 基础类型（GLib/GObject 的 typedef 子集）---- */
typedef int gboolean;
typedef unsigned int guint;
typedef char gchar;
typedef void *gpointer;

/* ---- 不透明 GTK 类型 ---- */
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;
typedef struct _GtkContainer GtkContainer;
typedef struct _GtkEntry GtkEntry;
typedef struct _GtkEntryBuffer GtkEntryBuffer;

/* ---- 常量/宏 ---- */
#define GTK_WINDOW_TOPLEVEL 0
#define GTK_WINDOW(obj) ((GtkWindow *)(obj))
#define GTK_CONTAINER(obj) ((GtkContainer *)(obj))
#define GTK_ENTRY(obj) ((GtkEntry *)(obj))
#define G_OBJECT(obj) ((gpointer)(obj))
#define G_CALLBACK(fn) ((gpointer)(fn))
#define G_SOURCE_REMOVE 0

/* g_signal_connect 在真实头里是宏：展开为 g_signal_connect_data(..., 0)。 */
#define g_signal_connect(instance, signal, callback, data)                     \
    g_signal_connect_data((gpointer)(instance), (signal), G_CALLBACK(callback),\
                          (gpointer)(data), NULL, 0)

/* ---- 函数原型（gtk_ime_test_client.c 用到的子集）---- */
extern void gtk_init(int *argc, char ***argv);
extern GtkWidget *gtk_window_new(int type);
extern void gtk_window_set_title(gpointer window, const char *title);
extern void gtk_window_set_default_size(gpointer window, int width, int height);
extern GtkWidget *gtk_entry_new(void);
extern GtkEntryBuffer *gtk_entry_get_buffer(gpointer entry);
extern unsigned long g_signal_connect_data(gpointer instance,
                                           const char *detailed_signal,
                                           gpointer c_handler, gpointer data,
                                           gpointer destroy_data, int flags);
extern void gtk_container_add(gpointer container, gpointer widget);
extern void gtk_widget_show_all(gpointer widget);
extern void gtk_widget_grab_focus(gpointer widget);
extern GdkWindow *gtk_widget_get_window(gpointer widget);
extern guint g_timeout_add_seconds(guint interval,
                                   gboolean (*function)(gpointer),
                                   gpointer data);
extern void gtk_main(void);
extern void gtk_main_quit(void);

#endif /* PINYINIME_GTK_COMPAT_H */
