/*
 * xim_gui_demo.c — 在 Xvfb 里展示真实 fcitx5 + pinyinime 的 XIM 输入过程。
 *
 * 与 xim_test_client.c 的区别：本程序把 XIM 真正提交的中文绘制在窗口里，
 * 便于截图确认「候选窗 → 空格上屏 → 应用窗口出现中文」的完整链路。
 *
 * 用法:
 *   DISPLAY=:99 XMODIFIERS=@im=fcitx ./xim_gui_demo 'nihao' [演示秒数]
 *   DISPLAY=:99 XMODIFIERS=@im=fcitx ./xim_gui_demo 'nihao\s' 8
 *
 * 编译（头文件在 sysroot 时）:
 *   gcc -O2 -I$SYSROOT/usr/include -o xim_gui_demo xim_gui_demo.c \
 *       -L$SYSROOT/usr/lib64 -Wl,-rpath-link,$SYSROOT/usr/lib64 \
 *       -lX11 -lXtst -lXft -lfontconfig -lfreetype
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <X11/Xft/Xft.h>

#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

static Display *dpy;
static Window win;
static XftDraw *xdraw;
static XftFont *font;
static XftColor fg, muted;
static XIM im;
static XIC ic;
static int width = 900, height = 360;
static char committed[4096];
static char preedit[1024];

static void append_utf8(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(dst);
    if (n + strlen(src) + 1 < cap)
        strcat(dst, src);
}

static void draw_text(XftDraw *d, XftColor *color, int x, int y, const char *s)
{
    XftDrawStringUtf8(d, color, font, x, y, (const FcChar8 *)s, (int)strlen(s));
}

static void redraw(void)
{
    XClearWindow(dpy, win);
    draw_text(xdraw, &muted, 36, 54,
              "pinyin-ime / fcitx5 / libime  —  XIM live demo");
    draw_text(xdraw, &muted, 36, 94,
              "Ctrl+Space 激活；输入拼音；空格上屏");

    if (preedit[0]) {
        draw_text(xdraw, &fg, 36, 180, preedit);
    }
    if (committed[0]) {
        draw_text(xdraw, &fg, 36, 250, committed);
    } else {
        draw_text(xdraw, &muted, 36, 250, "(等待上屏…)");
    }
    XFlush(dpy);
}

static void pump_events(int milliseconds)
{
    int fd = ConnectionNumber(dpy);
    for (;;) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (XFilterEvent(&ev, None))
                continue;
            if (ev.type != KeyPress)
                continue;

            char buf[512];
            KeySym ks = NoSymbol;
            int status = XLookupNone;
            int len = XmbLookupString(ic, (XKeyEvent *)&ev, buf, sizeof(buf) - 1,
                                      &ks, &status);
            if (len > 0) {
                buf[len] = 0;
                /* XIM 的 commit 与普通英文按键都可能到这里；只累计非 ASCII。 */
                if ((unsigned char)buf[0] >= 0x80) {
                    append_utf8(committed, sizeof(committed), buf);
                    preedit[0] = 0;
                    redraw();
                }
            }
        }

        fd_set rfds;
        struct timeval tv;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = milliseconds / 1000;
        tv.tv_usec = (milliseconds % 1000) * 1000;
        int r = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (r <= 0)
            return;
        milliseconds = 0;
    }
}

static KeySym ascii_to_keysym(char c)
{
    if (c >= 'a' && c <= 'z') return (KeySym)(XK_a + (c - 'a'));
    if (c >= '0' && c <= '9') return (KeySym)(XK_0 + (c - '0'));
    switch (c) {
        case ' ': return XK_space;
        case ',': return XK_comma;
        case '.': return XK_period;
        default: return NoSymbol;
    }
}

static void send_key(KeySym sym, unsigned int mods)
{
    KeyCode kc = XKeysymToKeycode(dpy, sym);
    if (!kc) return;
    KeyCode ctrl = XKeysymToKeycode(dpy, XK_Control_L);
    if (mods & ControlMask) XTestFakeKeyEvent(dpy, ctrl, True, 0);
    XTestFakeKeyEvent(dpy, kc, True, 0);
    XTestFakeKeyEvent(dpy, kc, False, 0);
    if (mods & ControlMask) XTestFakeKeyEvent(dpy, ctrl, False, 0);
    XFlush(dpy);
    usleep(90000);
}

static void run_script(const char *script)
{
    for (size_t i = 0; script[i]; ++i) {
        if (script[i] == '\\' && script[i + 1]) {
            char e = script[++i];
            if (e == 's') send_key(XK_space, 0);
            else if (e == 'S') send_key(XK_space, ControlMask);
            else if (e == 'e') send_key(XK_Escape, 0);
            else if (e == ',') send_key(XK_comma, 0);
            else if (e == '.') send_key(XK_period, 0);
            continue;
        }
        KeySym sym = ascii_to_keysym(script[i]);
        if (sym != NoSymbol)
            send_key(sym, 0);
    }
    pump_events(350);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "用法: %s '<按键脚本>' [演示秒数]\n", argv[0]);
        return 2;
    }
    int hold = argc > 2 ? atoi(argv[2]) : 6;

    setlocale(LC_ALL, "");
    XSetLocaleModifiers("");
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "无法打开 DISPLAY=%s\n", getenv("DISPLAY"));
        return 1;
    }

    int screen = DefaultScreen(dpy);
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 40, 40,
                              width, height, 1, 0x2b3a4a, 0x0d141c);
    XStoreName(dpy, win, "pinyin-ime XIM GUI Demo");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | FocusChangeMask |
                             StructureNotifyMask);
    XMapWindow(dpy, win);

    XRenderColor rc_fg = {0xffff, 0xffff, 0xffff, 0xffff};
    XRenderColor rc_muted = {0x9f9f, 0xb3b3, 0xc8c8, 0xffff};
    Colormap cmap = DefaultColormap(dpy, screen);
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen), cmap, &rc_fg, &fg);
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen), cmap, &rc_muted, &muted);
    xdraw = XftDrawCreate(dpy, win, DefaultVisual(dpy, screen), cmap);
    font = XftFontOpenName(dpy, screen, "WenQuanYi Micro Hei:size=26");
    if (!font)
        font = XftFontOpenName(dpy, screen, "sans:size=24");
    if (!font) {
        fprintf(stderr, "无法加载 Xft 字体\n");
        return 1;
    }

    XEvent mev;
    for (int i = 0; i < 100; ++i) {
        if (XCheckWindowEvent(dpy, win, StructureNotifyMask, &mev) &&
            mev.type == MapNotify)
            break;
        usleep(10000);
    }

    im = XOpenIM(dpy, NULL, NULL, NULL);
    if (!im) {
        fprintf(stderr, "XOpenIM 失败：fcitx5 未运行？\n");
        return 1;
    }
    ic = XCreateIC(im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                   XNClientWindow, win, XNFocusWindow, win, NULL);
    if (!ic) {
        fprintf(stderr, "XCreateIC 失败\n");
        return 1;
    }
    XSetICFocus(ic);
    XRaiseWindow(dpy, win);
    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
    XFlush(dpy);
    usleep(300000);
    redraw();

    run_script(argv[1]);

    /* 保持窗口，便于外部截图；期间继续接收 XIM 回灌事件。 */
    for (int i = 0; i < hold * 10; ++i)
        pump_events(100);

    XDestroyIC(ic);
    XCloseIM(im);
    XftDrawDestroy(xdraw);
    XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &fg);
    XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &muted);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return committed[0] ? 0 : 1;
}
