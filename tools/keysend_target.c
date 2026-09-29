#include <locale.h>
#include <stdbool.h>
/*
 * keysend_target.c — 向指定 X 窗口发键（XIM 实测辅助）。
 * 用法: keysend_target <window-id-hex> '<按键脚本>'
 * 按键脚本转义同 xim_test_client：\s 空格 \t Tab \n 回车 \e Esc \xNN Ctrl+字母 \S Ctrl+Space
 * 编译: gcc -O2 -o keysend_target keysend_target.c -lX11 -lXtst
 */
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Display *dpy;
static void send_key(KeySym sym, unsigned int mods) {
    KeyCode kc = XKeysymToKeycode(dpy, sym);
    if (!kc) return;
    KeyCode ctrl = XKeysymToKeycode(dpy, XK_Control_L);
    KeyCode shift = XKeysymToKeycode(dpy, XK_Shift_L);
    if (mods & ControlMask) XTestFakeKeyEvent(dpy, ctrl, True, 0);
    if (mods & ShiftMask) XTestFakeKeyEvent(dpy, shift, True, 0);
    XTestFakeKeyEvent(dpy, kc, True, 0);
    XTestFakeKeyEvent(dpy, kc, False, 0);
    if (mods & ShiftMask) XTestFakeKeyEvent(dpy, shift, False, 0);
    if (mods & ControlMask) XTestFakeKeyEvent(dpy, ctrl, False, 0);
    XFlush(dpy);
    usleep(35000);
}
/* 美国布局需要 Shift 的字符：发键时带 Shift 修饰（否则变成基键）。 */
static bool needs_shift(char c) {
    return strchr("!@#$%^&*()_+{}|:\"<>?~", c) != NULL;
}
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
        default: return NoSymbol;
    }
}
static void run_script(const char *s) {
    for (size_t i = 0; s[i]; ++i) {
        char c = s[i];
        if (c != '\\') {
            KeySym sym = ascii_to_keysym(c);
            if (sym != NoSymbol) send_key(sym, needs_shift(c) ? ShiftMask : 0);
            continue;
        }
        ++i;
        char e = s[i];
        switch (e) {
            case 's': send_key(XK_space, 0); break;
            case 't': send_key(XK_Tab, 0); break;
            case 'n': send_key(XK_Return, 0); break;
            case 'e': send_key(XK_Escape, 0); break;
            case ',': send_key(XK_comma, 0); break;
            case '.': send_key(XK_period, 0); break;
            case '[': send_key(XK_bracketleft, 0); break;
            case ']': send_key(XK_bracketright, 0); break;
            case 'S': send_key(XK_space, ControlMask); break;
            case 'l': send_key(XK_Left, ControlMask); break;  /* Ctrl+Left（候选焦点左移） */
            case 'r': send_key(XK_Right, ControlMask); break; /* Ctrl+Right（候选焦点右移） */
            case 'x':
                if (!s[i + 1] || !s[i + 2]) return;
                char hex[3] = {s[i + 1], s[i + 2], 0};
                long v = strtol(hex, NULL, 16);
                i += 2;
                if (v >= 1 && v <= 26) send_key((KeySym)(XK_a + (v - 1)), ControlMask);
                break;
            default: return;
        }
    }
    XFlush(dpy);
    usleep(400000);
}
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "用法: %s <window-id-hex> '<script>'\n", argv[0]);
        return 2;
    }
    Window w = (Window)strtoul(argv[1], NULL, 16);
    setlocale(LC_ALL, "");
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "无法打开 DISPLAY\n"); return 1; }
    XSetInputFocus(dpy, w, RevertToParent, CurrentTime);
    XRaiseWindow(dpy, w);
    XFlush(dpy);
    usleep(250000);
    run_script(argv[2]);
    XCloseDisplay(dpy);
    return 0;
}