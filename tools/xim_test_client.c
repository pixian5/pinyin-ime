/*
 * xim_test_client.c — 虚拟桌面（Xvfb）下 Fcitx5 pinyinime addon 的 XIM 实测客户端。
 *
 * 用途：headless 沙箱里真正把 pinyinime 跑起来——连接 DISPLAY（Xvfb）上的 fcitx5
 * XIM 服务，用 XTest 模拟真实按键，验证 addon 的 keyEvent 链路（拼音→候选→上屏、
 * 简拼、中英切换、中文标点、Ctrl-E 表情）在「真实 XIM 会话」里的 commit 结果。
 *
 * 用法：
 *   DISPLAY=:99 ./xim_test_client 'nihao\s'        # 发送 n i h a o + 空格
 *   脚本支持转义：
 *     \s 空格   \t Tab   \n 回车   \e Esc   \xNN 带 Ctrl 的字符（如 \x05 = Ctrl-E）
 *     \, 逗号   \. 句号   \[ 左方括号   \] 右方括号   \1..\9 数字（无修饰）
 *     \u PageUp   \d PageDown
 *   输出的每行 COMMIT:<text> 即输入法真正上屏的文本（XmbLookupString 结果）。
 *
 * 编译：gcc -O2 -o xim_test_client xim_test_client.c -lX11 -lXtst
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

static Display *dpy;
static Window win;
static XIM im;
static XIC ic;

static void send_key(KeySym sym, unsigned int mods) {
    KeyCode kc = XKeysymToKeycode(dpy, sym);
    if (kc == 0) {
        fprintf(stderr, "!! 无法映射 keysym 0x%lx 到 keycode\n", (unsigned long)sym);
        return;
    }
    KeyCode ctrl = XKeysymToKeycode(dpy, XK_Control_L);
    KeyCode shift = XKeysymToKeycode(dpy, XK_Shift_L);
    if (mods & ControlMask) {
        XTestFakeKeyEvent(dpy, ctrl, True, 0);
    }
    if (mods & ShiftMask) {
        XTestFakeKeyEvent(dpy, shift, True, 0);
    }
    XTestFakeKeyEvent(dpy, kc, True, 0);
    XTestFakeKeyEvent(dpy, kc, False, 0);
    if (mods & ShiftMask) {
        XTestFakeKeyEvent(dpy, shift, False, 0);
    }
    if (mods & ControlMask) {
        XTestFakeKeyEvent(dpy, ctrl, False, 0);
    }
    XFlush(dpy);
    usleep(30000); /* 30ms：让 fcitx5 有时间解码并回灌 commit 事件 */
}

/* 单个 ASCII 可见字符 → KeySym（小写 a-z/0-9/常见标点）。 */
static KeySym ascii_to_keysym(char c) {
    if (c >= 'a' && c <= 'z') return (KeySym)(XK_a + (c - 'a'));
    if (c >= '0' && c <= '9') return (KeySym)(XK_0 + (c - '0'));
    switch (c) {
        case ' ': return XK_space;
        case ',': return XK_comma;
        case '.': return XK_period;
        case '[': return XK_bracketleft;
        case ']': return XK_bracketright;
        case ';': return XK_semicolon;
        case ':': return XK_colon;
        case '?': return XK_question;
        case '!': return XK_exclam;
        case '\'': return XK_apostrophe;
        case '"': return XK_quotedbl;
        case '-': return XK_minus;
        case '=': return XK_equal;
        case '/': return XK_slash;
        case '\\': return XK_backslash;
        case '(': return XK_parenleft;
        case ')': return XK_parenright;
        default: return NoSymbol;
    }
}

/* 解析脚本：普通字符直接按键；\s \t \n \e \xNN \, \[ \] 等为转义。 */
/* 美国布局需要 Shift 的字符：发键时带 Shift 修饰（否则变成基键）。 */
static bool needs_shift(char c) {
    return strchr("!@#$%^&*()_+{}|:\"<>?~", c) != NULL;
}
static void run_script(const char *script) {
    for (size_t i = 0; script[i]; ++i) {
        char c = script[i];
        if (c != '\\') {
            KeySym sym = ascii_to_keysym(c);
            if (sym != NoSymbol) {
                send_key(sym, needs_shift(c) ? ShiftMask : 0);
            } else {
                fprintf(stderr, "!! 跳过无法处理的字符: %c\n", c);
            }
            continue;
        }
        ++i; /* consume '\' */
        char e = script[i];
        switch (e) {
            case 's': send_key(XK_space, 0); break;
            case 't': send_key(XK_Tab, 0); break;
            case 'n': send_key(XK_Return, 0); break;
            case 'e': send_key(XK_Escape, 0); break;
            case 'S': send_key(XK_space, ControlMask); break;  /* \S = Ctrl+Space（激活输入法） */
            case ',': send_key(XK_comma, 0); break;
            case '.': send_key(XK_period, 0); break;
            case '[': send_key(XK_bracketleft, 0); break;
            case ']': send_key(XK_bracketright, 0); break;
            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9':
                send_key((KeySym)(XK_1 + (e - '1')), 0);
                break;
            case 'u': send_key(XK_Page_Up, 0); break;   /* \u = PageUp */
            case 'd': send_key(XK_Page_Down, 0); break; /* \d = PageDown */
            case 'l': send_key(XK_Left, ControlMask); break;  /* \l = Ctrl+Left（候选焦点左移） */
            case 'r': send_key(XK_Right, ControlMask); break; /* \r = Ctrl+Right（候选焦点右移） */
            case 'w': usleep(250000); break;            /* \w = 等 250ms（翻页前让解码落定）*/
            case 'x': { /* \xNN：带 Ctrl 的字符（如 \x05 = Ctrl-E） */
                if (!script[i + 1] || !script[i + 2]) {
                    fprintf(stderr, "!! \\x 转义缺少两位 hex\n");
                    return;
                }
                char hex[3] = {script[i + 1], script[i + 2], 0};
                long v = strtol(hex, NULL, 16);
                i += 2;
                if (v >= 0x01 && v <= 0x1a) {
                    KeySym sym = (KeySym)(XK_a + (v - 1));
                    send_key(sym, ControlMask);
                } else {
                    fprintf(stderr, "!! \\x%02lx 不支持的 Ctrl 字符\n", v);
                }
                break;
            }
            case '\0':
                fprintf(stderr, "!! 脚本以 '\\' 结尾\n");
                return;
            default:
                fprintf(stderr, "!! 未知转义 \\%c\n", e);
                return;
        }
    }
    /* 收尾让事件全部到达 */
    XFlush(dpy);
    usleep(300000);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s '<按键脚本>'\n", argv[0]);
        return 2;
    }
    setlocale(LC_ALL, "");
    XSetLocaleModifiers("");

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "!! 无法打开 DISPLAY=%s（Xvfb 没起？）\n", getenv("DISPLAY"));
        return 1;
    }

    int screen = DefaultScreen(dpy);
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0, 400, 200, 0,
                              BlackPixel(dpy, screen), WhitePixel(dpy, screen));
    XSelectInput(dpy, win, KeyPressMask | FocusChangeMask | StructureNotifyMask);
    XMapWindow(dpy, win);
    XFlush(dpy);
    for (int i = 0; i < 100; ++i) {
        XEvent mev;
        if (XCheckWindowEvent(dpy, win, StructureNotifyMask, &mev) && mev.type == MapNotify) break;
        usleep(10000);
    }
    XWindowAttributes attr;
    XGetWindowAttributes(dpy, win, &attr);
    fprintf(stderr, "# Window mapped: viewable=%d map_state=%d\n", attr.map_state == IsViewable, attr.map_state);

    im = XOpenIM(dpy, NULL, NULL, NULL);
    if (!im) {
        fprintf(stderr, "!! XOpenIM 失败——检查 XMODIFIERS / fcitx5 是否在运行\n");
        return 1;
    }
    XIMStyles *styles = NULL;
    XGetIMValues(im, XNQueryInputStyle, &styles, NULL);
    if (styles) {
        fprintf(stderr, "IM styles 支持数: %d\n", styles->count_styles);
        for (int i = 0; i < styles->count_styles; ++i) {
            fprintf(stderr, "  style[%d]: preedit=0x%lx status=0x%lx\n", i,
                    (unsigned long)(styles->supported_styles[i] & 0x1F),
                    (unsigned long)(styles->supported_styles[i] & 0x3C0));
        }
        XFree(styles);
    }
    /* 优先 on-the-spot（Callbacks）：real 应用（xterm 等）最常用，xcb-imdkit 支持度最高。 */
    ic = XCreateIC(im, XNInputStyle, XIMPreeditCallbacks | XIMStatusCallbacks,
                   XNClientWindow, win, XNFocusWindow, win, NULL);
    if (!ic) {
        ic = XCreateIC(im, XNInputStyle, XIMPreeditPosition | XIMStatusNothing,
                       XNClientWindow, win, XNFocusWindow, win, NULL);
    }
    if (!ic) {
        ic = XCreateIC(im, XNInputStyle, XIMPreeditNone | XIMStatusNone,
                       XNClientWindow, win, XNFocusWindow, win, NULL);
    }
    if (!ic) {
        fprintf(stderr, "!! XCreateIC 失败\n");
        return 1;
    }
    {
        unsigned long nstyle = 0;
        XGetICValues(ic, XNInputStyle, &nstyle, NULL);
        if (nstyle)
            fprintf(stderr, "# IC 协商风格 preedit=0x%lx status=0x%lx\n",
                    (unsigned long)(nstyle & 0x1F), (unsigned long)(nstyle & 0x3C0));
    }

    XSetICFocus(ic);
    XRaiseWindow(dpy, win);
    /* Xvfb 无窗口管理器：必须显式给窗口输入焦点，否则 fcitx5 XIM 收不到按键。 */
    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
    XFlush(dpy);
    usleep(200000);

    fprintf(stderr, "# 客户端就绪，执行脚本: %s\n", argv[1]);
    run_script(argv[1]);

    /* 把 X 服务器上已到达的事件（含 fcitx5 回灌的 commit KeyPress）全部读出。 */
    fprintf(stderr, "# 开始读取 commit 事件...\n");
    int commit_count = 0;
    for (int loop = 0; loop < 200; ++loop) {
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == FocusIn) {
                fprintf(stderr, "# FocusIn mode=%d\n", ev.xfocus.mode);
                continue;
            }
            if (ev.type == FocusOut) {
                fprintf(stderr, "# FocusOut mode=%d\n", ev.xfocus.mode);
                continue;
            }
            /* 关键：XFilterEvent 把按键交给 XIM（forward 给 IM 服务器），
             * IM 消费时返回 True，本客户端不再处理。 */
            if (XFilterEvent(&ev, None)) {
                fprintf(stderr, "# XFilterEvent consumed\n");
                continue;
            }
            if (ev.type != KeyPress) {
                continue;
            }
            char buf[256];
            KeySym ks = NoSymbol;
            int status = XLookupNone;
            int len = XmbLookupString(ic, (XKeyEvent *)&ev, buf, sizeof(buf) - 1,
                                      &ks, &status);
            fprintf(stderr, "# KeyPress status=%d len=%d keysym=0x%lx\n", status, len,
                    (unsigned long)ks);
            if (len > 0) {
                buf[len] = '\0';
                printf("COMMIT:%s\n", buf);
                fflush(stdout);
                ++commit_count;
            } else if (ks == XK_Return || ks == XK_space || ks == XK_Tab) {
                /* 原样按键（英文态/未消费）也会回灌，仅打印非 ASCII 的 commit */
            }
        }
        usleep(50000);
        if (commit_count > 0 && XPending(dpy) == 0) {
            /* 拿到一次 commit 且事件流静止后稍等再结束，避免漏掉连续 commit */
            usleep(150000);
            break;
        }
    }

    XDestroyIC(ic);
    XCloseIM(im);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    fprintf(stderr, "# 结束，commit 次数=%d\n", commit_count);
    return commit_count > 0 ? 0 : 1;
}