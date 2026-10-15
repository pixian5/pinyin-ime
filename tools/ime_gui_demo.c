/*
 * ime_gui_demo.c — 真实 GUI 演示 fcitx5 pinyinime（对标搜狗拼音），并截图。
 *
 * 场景：一个真实 X 应用窗口（白底），用 libXft 走 fontconfig/文泉驿渲染中文。
 *   1) Ctrl+Space 激活输入法；
 *   2) 打 "nihao" → fcitx5 classicui 弹出候选窗（预编辑串 + 中文候选）→ 截图；
 *   3) 空格上屏「你好」→ 应用窗口用 Xft 渲染出中文 → 截图；
 *   4) 再打 "shijie" 上屏「世界」→ 窗口显示「你好世界」→ 截图。
 *
 * 关键点（XIM 协议）：输入法只能看到客户端「用 XFilterEvent 过滤过」的按键，
 * 因此每次发键后必须泵事件队列，把按键转发给 fcitx5；上屏文本经 XIM 回灌为
 * KeyPress，用 Xutf8LookupString 取出 UTF-8 提交串。
 *
 * 用法：DISPLAY=:99 XMODIFIERS=@im=fcitx ./ime_gui_demo <pre.ppm> <c1.ppm> <c2.ppm>
 * 编译：gcc -O2 -o ime_gui_demo ime_gui_demo.c -lX11 -lXtst -lXft
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/XTest.h>

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Display *dpy;
static Window win;
static XIM im;
static XIC ic;
static XftDraw *xdraw;
static XftFont *font;
static XftColor ink, accent;

static char committed[2048];
static int  committed_len = 0;

static void redraw(void) {
    XClearWindow(dpy, win);
    const char *h1 = "pinyin-ime (fcitx5 addon)  -  real GUI in-app IME, like Sogou";
    const char *h2 = "Type pinyin, pick candidates, space to commit:";
    XftDrawStringUtf8(xdraw, &ink, font, 16, 32, (const FcChar8 *)h1, (int)strlen(h1));
    XftDrawStringUtf8(xdraw, &accent, font, 16, 62, (const FcChar8 *)h2, (int)strlen(h2));
    if (committed_len > 0)
        XftDrawStringUtf8(xdraw, &ink, font, 16, 118,
                          (const FcChar8 *)committed, committed_len);
    XFlush(dpy);
}

static void append_commit(const char *s, int n) {
    if (committed_len + n >= (int)sizeof(committed)) return;
    memcpy(committed + committed_len, s, n);
    committed_len += n;
    committed[committed_len] = 0;
    fprintf(stderr, "# 上屏: %.*s\n", n, s);
    redraw();
}

/* 处理一个事件：把按键交给 XIM；非消费的按键若带回提交串则累积并重绘。 */
static void handle_event(XEvent *ev) {
    if (ev->type == Expose) { redraw(); return; }
    if (ev->type == KeyPress) {
        if (XFilterEvent(ev, win)) return;   /* 被输入法消费 */
        char buf[128];
        KeySym ks;
        Status st;
        int n = Xutf8LookupString(ic, (XKeyPressedEvent *)ev, buf, sizeof(buf) - 1, &ks, &st);
        if (n > 0) {
            buf[n] = 0;
            if (!(n == 1 && (unsigned char)buf[0] < 0x20))   /* 过滤控制字符 */
                append_commit(buf, n);
        }
    } else {
        XFilterEvent(ev, None);
    }
}

/* 转发按键给 XIM：输入法只在客户端 XFilterEvent 时才能看到按键。 */
static void pump(void) {
    while (XPending(dpy) > 0) {
        XEvent e;
        XNextEvent(dpy, &e);
        handle_event(&e);
    }
    XFlush(dpy);
}

static void send_key(KeySym sym, unsigned int mods) {
    KeyCode kc = XKeysymToKeycode(dpy, sym);
    if (!kc) return;
    KeyCode ctrl = XKeysymToKeycode(dpy, XK_Control_L);
    if (mods & ControlMask) XTestFakeKeyEvent(dpy, ctrl, True, 0);
    XTestFakeKeyEvent(dpy, kc, True, 0);
    XTestFakeKeyEvent(dpy, kc, False, 0);
    if (mods & ControlMask) XTestFakeKeyEvent(dpy, ctrl, False, 0);
    XFlush(dpy);
    usleep(50000);
    pump();
    usleep(50000);
}

static void type_ascii(const char *s) {
    for (; *s; ++s) {
        if (*s >= 'a' && *s <= 'z') send_key((KeySym)(XK_a + (*s - 'a')), 0);
        else if (*s >= '0' && *s <= '9') send_key((KeySym)(XK_0 + (*s - '0')), 0);
        else if (*s == ' ') send_key(XK_space, 0);
    }
}

static void settle(int iters) { for (int i = 0; i < iters; ++i) { pump(); usleep(40000); } }

static void capture_root(const char *path) {
    int w = DisplayWidth(dpy, DefaultScreen(dpy));
    int h = DisplayHeight(dpy, DefaultScreen(dpy));
    XImage *img = XGetImage(dpy, DefaultRootWindow(dpy), 0, 0, w, h, AllPlanes, ZPixmap);
    if (!img) { fprintf(stderr, "!! XGetImage 失败\n"); return; }
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "!! 打不开 %s\n", path); XDestroyImage(img); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            unsigned long p = XGetPixel(img, x, y);
            fputc((p >> 16) & 0xff, f); fputc((p >> 8) & 0xff, f); fputc(p & 0xff, f);
        }
    fclose(f); XDestroyImage(img);
    fprintf(stderr, "# 已截图 %s (%dx%d)\n", path, w, h);
}

int main(int argc, char **argv) {
    const char *o_pre = argc > 1 ? argv[1] : "/tmp/ime_pre.ppm";
    const char *o_c1  = argc > 2 ? argv[2] : "/tmp/ime_c1.ppm";
    const char *o_c2  = argc > 3 ? argv[3] : "/tmp/ime_c2.ppm";

    setlocale(LC_ALL, "");
    XSetLocaleModifiers("");

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "!! 无法打开 DISPLAY\n"); return 1; }
    int screen = DefaultScreen(dpy);

    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 60, 60, 780, 260, 2,
                              BlackPixel(dpy, screen), WhitePixel(dpy, screen));
    XStoreName(dpy, win, "pinyin-ime on fcitx5 - type pinyin (like Sogou)");
    XSelectInput(dpy, win, KeyPressMask | FocusChangeMask |
                          StructureNotifyMask | ExposureMask);
    XSetWindowBackground(dpy, win, WhitePixel(dpy, screen));
    XMapWindow(dpy, win);
    XFlush(dpy);
    for (int i = 0; i < 100; ++i) {
        XEvent e;
        if (XCheckWindowEvent(dpy, win, StructureNotifyMask, &e) && e.type == MapNotify) break;
        usleep(10000);
    }

    /* Xft：走 fontconfig，使用文泉驿中文字体 */
    xdraw = XftDrawCreate(dpy, win, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
    font = XftFontOpenName(dpy, screen, "WenQuanYi Micro Hei:size=16");
    if (!font) font = XftFontOpenName(dpy, screen, "sans:size=16");
    XftColorAllocName(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), "black", &ink);
    XftColorAllocName(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), "#2255dd", &accent);
    redraw();

    im = XOpenIM(dpy, NULL, NULL, NULL);
    if (!im) { fprintf(stderr, "!! XOpenIM 失败（fcitx5 未运行？）\n"); return 1; }
    ic = XCreateIC(im, XNInputStyle, XIMPreeditCallbacks | XIMStatusCallbacks,
                   XNClientWindow, win, XNFocusWindow, win, NULL);
    if (!ic)
        ic = XCreateIC(im, XNInputStyle, XIMPreeditPosition | XIMStatusNothing,
                       XNClientWindow, win, XNFocusWindow, win, NULL);
    if (!ic)
        ic = XCreateIC(im, XNInputStyle, XIMPreeditNone | XIMStatusNone,
                       XNClientWindow, win, XNFocusWindow, win, NULL);
    if (!ic) { fprintf(stderr, "!! XCreateIC 失败\n"); return 1; }

    XSetICFocus(ic);
    XRaiseWindow(dpy, win);
    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
    XFlush(dpy);
    settle(8);

    fprintf(stderr, "# Ctrl+Space 激活输入法\n");
    send_key(XK_space, ControlMask);
    settle(6);

    fprintf(stderr, "# 输入 nihao（候选窗应出现）\n");
    type_ascii("nihao");
    settle(25);
    capture_root(o_pre);

    fprintf(stderr, "# 空格上屏\n");
    send_key(XK_space, 0);
    settle(15);
    capture_root(o_c1);

    fprintf(stderr, "# 再输入 shijie 并上屏\n");
    type_ascii("shijie");
    settle(20);
    send_key(XK_space, 0);
    settle(15);
    capture_root(o_c2);

    fprintf(stderr, "# 最终窗口文本: %s\n", committed);
    XDestroyIC(ic);
    XCloseIM(im);
    XCloseDisplay(dpy);
    return 0;
}
