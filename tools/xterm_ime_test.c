/*
 * xterm_ime_test.c — 向xterm窗口发送按键并截图
 * 用法: DISPLAY=:99 ./xterm_ime_test
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Display *dpy;

static void send_key(KeySym sym, unsigned int mods) {
    KeyCode kc = XKeysymToKeycode(dpy, sym);
    if (kc == 0) { fprintf(stderr, "No keycode for keysym 0x%lx\n", (unsigned long)sym); return; }
    KeyCode ctrl = XKeysymToKeycode(dpy, XK_Control_L);
    if (mods & ControlMask) XTestFakeKeyEvent(dpy, ctrl, True, 0);
    XTestFakeKeyEvent(dpy, kc, True, 0);
    XTestFakeKeyEvent(dpy, kc, False, 0);
    if (mods & ControlMask) XTestFakeKeyEvent(dpy, ctrl, False, 0);
    XFlush(dpy);
    usleep(80000);
}

static void screenshot(const char *name) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "import -window root %s 2>/dev/null", name);
    system(cmd);
    fprintf(stderr, "Saved: %s\n", name);
}

/* 递归查找窗口标题包含title的窗口 */
static Window find_window(Window root, const char *title) {
    Window parent, *children;
    unsigned int nchildren;
    if (!XQueryTree(dpy, root, &root, &parent, &children, &nchildren)) return 0;
    
    for (unsigned int i = 0; i < nchildren; i++) {
        char *name = NULL;
        if (XFetchName(dpy, children[i], &name) && name) {
            if (strstr(name, title)) {
                Window found = children[i];
                XFree(name);
                XFree(children);
                return found;
            }
            XFree(name);
        }
        // 递归子窗口
        Window sub = find_window(children[i], title);
        if (sub) { XFree(children); return sub; }
    }
    XFree(children);
    return 0;
}

int main() {
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open display\n"); return 1; }
    
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    
    // 查找xterm窗口
    Window xterm = find_window(root, "Pinyin");
    if (!xterm) xterm = find_window(root, "xterm");
    if (!xterm) {
        fprintf(stderr, "xterm window not found, using root\n");
        xterm = root;
    } else {
        fprintf(stderr, "Found xterm window: 0x%lx\n", (unsigned long)xterm);
        // 聚焦xterm
        XRaiseWindow(dpy, xterm);
        XSetInputFocus(dpy, xterm, RevertToParent, CurrentTime);
        XFlush(dpy);
        usleep(200000);
    }
    
    screenshot("/tmp/xt-01-initial.png");
    
    // Ctrl+Space 激活输入法
    send_key(XK_space, ControlMask);
    usleep(300000);
    screenshot("/tmp/xt-02-activated.png");
    
    // 输入 n
    send_key(XK_n, 0);
    usleep(200000);
    screenshot("/tmp/xt-03-after-n.png");
    
    // 输入 i
    send_key(XK_i, 0);
    usleep(200000);
    screenshot("/tmp/xt-04-after-ni.png");
    
    // 输入 h
    send_key(XK_h, 0);
    usleep(200000);
    screenshot("/tmp/xt-05-after-nih.png");
    
    // 输入 a
    send_key(XK_a, 0);
    usleep(200000);
    screenshot("/tmp/xt-06-after-niha.png");
    
    // 输入 o
    send_key(XK_o, 0);
    usleep(300000);
    screenshot("/tmp/xt-07-after-nihao.png");
    
    // 空格确认
    send_key(XK_space, 0);
    usleep(500000);
    screenshot("/tmp/xt-08-committed.png");
    
    // 输入第二个词 zhongguo
    send_key(XK_z, 0); usleep(100000);
    send_key(XK_h, 0); usleep(100000);
    send_key(XK_o, 0); usleep(100000);
    send_key(XK_n, 0); usleep(100000);
    send_key(XK_g, 0); usleep(100000);
    send_key(XK_u, 0); usleep(100000);
    send_key(XK_o, 0); usleep(300000);
    screenshot("/tmp/xt-09-zhongguo-preedit.png");
    
    send_key(XK_space, 0);
    usleep(500000);
    screenshot("/tmp/xt-10-zhongguo-committed.png");
    
    // 输入第三个词 shurufa
    send_key(XK_s, 0); usleep(100000);
    send_key(XK_h, 0); usleep(100000);
    send_key(XK_u, 0); usleep(100000);
    send_key(XK_r, 0); usleep(100000);
    send_key(XK_u, 0); usleep(100000);
    send_key(XK_f, 0); usleep(100000);
    send_key(XK_a, 0); usleep(300000);
    screenshot("/tmp/xt-11-shurufa-preedit.png");
    
    send_key(XK_space, 0);
    usleep(500000);
    screenshot("/tmp/xt-12-shurufa-committed.png");
    
    fprintf(stderr, "All screenshots taken\n");
    
    // 保持5秒
    sleep(5);
    screenshot("/tmp/xt-13-final.png");
    
    XCloseDisplay(dpy);
    return 0;
}
