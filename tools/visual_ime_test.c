/*
 * visual_ime_test.c — 可视化拼音输入法测试
 * 创建一个可见窗口，通过XIM接收输入，显示preedit和commit文本
 * 截图保存到/tmp/vime-shot-XX.png
 *
 * 编译: gcc -o visual_ime_test visual_ime_test.c -lX11 -lXtst -lXIM
 * 运行: DISPLAY=:99 XMODIFIERS=@im=fcitx ./visual_ime_test
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Display *dpy;
static Window win;
static GC gc;
static XIM im;
static XIC ic;
static char commit_text[4096] = "";
static char preedit_text[1024] = "";
static int win_w = 600, win_h = 400;

static void redraw() {
    XClearWindow(dpy, win);
    // 标题
    XDrawString(dpy, win, gc, 20, 30, "Pinyin IME Visual Test", 22);
    // 分隔线
    XDrawLine(dpy, win, gc, 20, 45, win_w-20, 45);
    // 已提交文本
    XDrawString(dpy, win, gc, 20, 80, "Committed:", 10);
    XDrawString(dpy, win, gc, 20, 110, commit_text, strlen(commit_text));
    // preedit
    XDrawString(dpy, win, gc, 20, 160, "Preedit:", 8);
    XDrawString(dpy, win, gc, 20, 190, preedit_text, strlen(preedit_text));
    // 提示
    XDrawString(dpy, win, gc, 20, win_h-20, "Sending keys via XTest...", 25);
    XFlush(dpy);
}

static void take_screenshot(const char *filename) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "import -window root %s 2>/dev/null", filename);
    system(cmd);
}

static void send_key(KeySym sym, unsigned int mods) {
    KeyCode kc = XKeysymToKeycode(dpy, sym);
    if (kc == 0) return;
    KeyCode ctrl = XKeysymToKeycode(dpy, XK_Control_L);
    if (mods & ControlMask) XTestFakeKeyEvent(dpy, ctrl, True, 0);
    XTestFakeKeyEvent(dpy, kc, True, 0);
    XTestFakeKeyEvent(dpy, kc, False, 0);
    if (mods & ControlMask) XTestFakeKeyEvent(dpy, ctrl, False, 0);
    XFlush(dpy);
    usleep(50000);
}

int main() {
    setlocale(LC_ALL, "");
    XSetLocaleModifiers("");

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open display\n"); return 1; }

    int screen = DefaultScreen(dpy);
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
        50, 50, win_w, win_h, 2,
        BlackPixel(dpy, screen), WhitePixel(dpy, screen));
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | FocusChangeMask);
    XMapWindow(dpy, win);
    XFlush(dpy);
    usleep(100000);

    gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, BlackPixel(dpy, screen));
    XSetBackground(dpy, gc, WhitePixel(dpy, screen));

    im = XOpenIM(dpy, NULL, NULL, NULL);
    if (!im) { fprintf(stderr, "XOpenIM failed\n"); return 1; }

    ic = XCreateIC(im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                   XNClientWindow, win, XNFocusWindow, win, NULL);
    if (!ic) {
        ic = XCreateIC(im, XNInputStyle, XIMPreeditPosition | XIMStatusNothing,
                       XNClientWindow, win, XNFocusWindow, win, NULL);
    }
    XSetICFocus(ic);
    redraw();
    usleep(500000);

    take_screenshot("/tmp/vime-01-initial.png");
    printf("Shot 1: initial\n");

    // Ctrl+Space 激活输入法
    send_key(XK_space, ControlMask);
    usleep(200000);
    take_screenshot("/tmp/vime-02-activated.png");
    printf("Shot 2: IM activated\n");

    // 输入 nihao
    const char *pinyin = "nihao";
    for (int i = 0; pinyin[i]; i++) {
        KeySym sym = (KeySym)(XK_a + (pinyin[i] - 'a'));
        send_key(sym, 0);
        usleep(100000);

        // 处理X事件
        XEvent ev;
        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if (ev.type == KeyPress) {
                wchar_t buf[256];
                Status status;
                int n = XwcLookupString(ic, (XKeyPressedEvent*)&ev, buf, 256, NULL, &status);
                if (status == XLookupChars && n > 0) {
                    // 追加到commit_text
                    char mb[256];
                    wcstombs(mb, buf, 256);
                    strcat(commit_text, mb);
                    preedit_text[0] = 0;
                }
            }
        }
        redraw();

        char fname[64];
        snprintf(fname, sizeof(fname), "/tmp/vime-03-%d-after-%c.png", i+1, pinyin[i]);
        take_screenshot(fname);
        printf("Shot 3-%d: after '%c'\n", i+1, pinyin[i]);
    }

    // 按空格确认
    send_key(XK_space, 0);
    usleep(200000);

    XEvent ev;
    while (XPending(dpy)) {
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress) {
            wchar_t buf[256];
            Status status;
            int n = XwcLookupString(ic, (XKeyPressedEvent*)&ev, buf, 256, NULL, &status);
            if (status == XLookupChars && n > 0) {
                char mb[256];
                wcstombs(mb, buf, 256);
                strcat(commit_text, mb);
            }
        }
    }
    redraw();
    take_screenshot("/tmp/vime-04-committed.png");
    printf("Shot 4: committed\n");
    printf("Result: %s\n", commit_text);

    // 保持窗口5秒
    sleep(5);
    take_screenshot("/tmp/vime-05-final.png");
    printf("Shot 5: final\n");

    XDestroyIC(ic);
    XCloseIM(im);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
