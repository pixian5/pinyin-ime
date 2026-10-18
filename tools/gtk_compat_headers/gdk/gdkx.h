/*
 * tools/gtk_compat_headers/gdk/gdkx.h — GTK3/GDK「无 devel 包」编译垫片（GDK X11 部分）。
 *
 * 背景：只在「只读 /usr、无 gtk3-devel 无法 dnf install」的沙箱机器上编译
 * tools/gtk_ime_test_client.c 用。真实机器有 gtk3-devel 时用 pkg-config 正常编译，
 * 不需要本目录（-I 本目录与系统头共存时，系统头优先不影响）。
 *
 * 只声明 gtk_ime_test_client.c 实际用到的符号：不允许在本目录追加与真实
 * GTK3 API 不一致的原型，否则会产生隐蔽 ABI 错误。
 */
#ifndef PINYINIME_GDK_COMPAT_H
#define PINYINIME_GDK_COMPAT_H

#include <stddef.h>

/* 不透明类型（只需指针语义）。 */
typedef struct _GdkWindow GdkWindow;

/* 取 GDK X11 窗口的 XID（X11 后端下返回 unsigned long）。 */
extern unsigned long gdk_x11_window_get_xid(GdkWindow *window);

/* 真实宏为 G_TYPE_CHECK_INSTANCE_TYPE(window, GDK_TYPE_X11_WINDOW)；
 * 客户端只在 X11 后端下运行（Xvfb），非空即视为可用。 */
#define GDK_IS_X11_WINDOW(window) ((window) != NULL)

#endif /* PINYINIME_GDK_COMPAT_H */
