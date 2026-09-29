/*
 * gtk_ime_test_client.c — 真桌面端到端实测客户端：一个带 GtkEntry 的 GTK3 应用。
 *
 * 与 tools/xim_test_client.c（走 XIM 协议）不同，本客户端走 GTK3 的 IM module 通道
 * （GTK_IM_MODULE=fcitx → 加载 fcitx5-gtk 装的 im-fcitx5.so → 经 DBus 连 fcitx5 守护进程），
 * 是「真实桌面应用」（如浏览器/编辑器）使用输入法的那条链路。
 *
 * 用法：
 *   GTK_IM_MODULE=fcitx GTK_PATH=<prefix>/lib DISPLAY=:99 \
 *   DBUS_SESSION_BUS_ADDRESS=... ./gtk_ime_test_client
 *   启动后打印一行 `WINDOW_ID=0x...`（供 keysend_target 注入按键用）；
 *   每次 IM 上屏（GtkEntryBuffer inserted-text）打印一行 `GTK_COMMIT:<text>`。
 *
 * 编译：gcc -O2 -o gtk_ime_test_client gtk_ime_test_client.c \
 *        $(pkg-config --cflags --libs gtk+-3.0 gdk-x11-3.0)
 */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <locale.h>
#include <stdio.h>

/* 每次上屏（其实是「插入到输入框缓冲」）都打印一行，供断言脚本 grep。 */
static void on_inserted(GtkEntryBuffer *buf, guint pos,
                        gchar *chars, guint n_chars, gpointer data) {
    (void)pos; (void)n_chars; (void)data;
    printf("GTK_COMMIT:%s\n", chars);
    fflush(stdout);
}

/* 安全兜底：N 秒后自动退出（异常时不让脚本挂死）。 */
static gboolean on_timeout(gpointer data) {
    (void)data;
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    gtk_init(&argc, &argv);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "pinyinime-gtk-quanzi");
    gtk_window_set_default_size(GTK_WINDOW(win), 600, 120);

    GtkWidget *entry = gtk_entry_new();
    GtkEntryBuffer *buf = gtk_entry_get_buffer(GTK_ENTRY(entry));
    g_signal_connect(G_OBJECT(buf), "inserted-text", G_CALLBACK(on_inserted), NULL);

    gtk_container_add(GTK_CONTAINER(win), entry);
    gtk_widget_show_all(win);

    /* 拿到 X11 窗口 XID，打印给外部注入器（keysend_target）。 */
    GdkWindow *gwin = gtk_widget_get_window(win);
    if (gwin && GDK_IS_X11_WINDOW(gwin)) {
        printf("WINDOW_ID=0x%lx\n", (unsigned long)gdk_x11_window_get_xid(gwin));
        fflush(stdout);
    } else {
        printf("WINDOW_ID=0x0\n");
        fflush(stdout);
    }

    gtk_widget_grab_focus(entry);
    g_timeout_add_seconds((argc > 1) ? (guint)atoi(argv[1]) : 30, on_timeout, NULL);
    gtk_main();
    return 0;
}