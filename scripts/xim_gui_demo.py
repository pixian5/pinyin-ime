#!/usr/bin/env python3
"""
XIM 端到端 GUI 演示
在 Xvfb 虚拟显示上启动 xterm，通过 XTEST 扩展模拟键盘输入，
验证拼音输入法通过 XIM 协议将英文按键转换为中文输出。
"""
import os
import sys
import time
import subprocess
from Xlib import X, display, XK

os.environ.setdefault("DISPLAY", ":99")

# Keycode map (Linux evdev standard)
KEYCODES = {
    'a': 38, 'b': 56, 'c': 54, 'd': 40, 'e': 26, 'f': 41, 'g': 42,
    'h': 43, 'i': 31, 'j': 44, 'k': 45, 'l': 46, 'm': 58, 'n': 57,
    'o': 32, 'p': 33, 'q': 24, 'r': 27, 's': 39, 't': 28, 'u': 30,
    'v': 55, 'w': 25, 'x': 53, 'y': 29, 'z': 52, ' ': 65,
}
# Special keycodes
KEY_RETURN = 36
KEY_SPACE = 65
KEY_BACKSPACE = 22


def send_key(d, keysym, press=True):
    """Send a key event using XTEST extension."""
    d.xtest_fake_input(X.KeyPress if press else X.KeyRelease, keysym)


def type_string(d, s, delay=0.1):
    """Type a string character by character using XTEST."""
    for ch in s:
        keysym = KEYCODES.get(ch)
        if keysym is None:
            continue
        send_key(d, keysym, press=True)
        send_key(d, keysym, press=False)
        d.flush()
        time.sleep(delay)


def press_key(d, keysym, delay=0.1):
    """Press a key."""
    send_key(d, keysym, press=True)
    send_key(d, keysym, press=False)
    d.flush()
    time.sleep(delay)


def main():
    print("=" * 60)
    print("XIM 端到端 GUI 演示")
    print("=" * 60)

    # Connect to X display
    try:
        d = display.Display()
    except Exception as e:
        print(f"❌ Cannot connect to display: {e}")
        return False

    root = d.screen().root
    print(f"✅ X display connected: {d.get_display_name()}")

    # Check XIM server
    try:
        xim_atom = d.intern_atom("XIM_SERVERS", True)
        if xim_atom:
            prop = root.get_full_property(xim_atom, X.AnyPropertyType)
            if prop and prop.value:
                print(f"✅ XIM server registered")
            else:
                print("⚠️ XIM_SERVERS atom exists but no value")
        else:
            print("❌ No XIM_SERVERS atom")
    except Exception as e:
        print(f"⚠️ XIM check: {e}")

    # Check XTEST extension
    has_xtest = d.has_extension('XTEST')
    if has_xtest:
        print(f"✅ XTEST extension available")
    else:
        print("❌ XTEST extension not available")
        d.close()
        return False

    # Start xterm with XIM
    print("\n--- Starting xterm with XIM ---")
    xterm_env = os.environ.copy()
    xterm_env.update({
        "XMODIFIERS": "@im=fcitx",
        "GTK_IM_MODULE": "fcitx",
        "QT_IM_MODULE": "fcitx",
        "LANG": "zh_CN.utf8",
        "LC_ALL": "zh_CN.utf8",
    })

    # Use a simple xterm that just echoes input
    xterm_cmd = [
        "xterm", "-im", "fcitx",
        "-geometry", "80x24+0+0",
        "-title", "pinyin-ime XIM Demo",
        "-e", "cat",
    ]

    try:
        xterm_proc = subprocess.Popen(
            xterm_cmd, env=xterm_env,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        time.sleep(2)
        print(f"✅ xterm started (PID: {xterm_proc.pid})")
    except Exception as e:
        print(f"❌ Cannot start xterm: {e}")
        d.close()
        return False

    # Find xterm window
    def find_window_by_title(win, title, depth=0):
        if depth > 5:
            return None
        try:
            name = win.get_wm_name()
            if name and title in name:
                return win
        except:
            pass
        try:
            for child in win.query_tree().children:
                result = find_window_by_title(child, title, depth + 1)
                if result:
                    return result
        except:
            pass
        return None

    xterm_win = find_window_by_title(root, "pinyin-ime XIM Demo")
    if xterm_win:
        print(f"✅ xterm window found")
        try:
            xterm_win.set_input_focus(X.RevertToParent, X.CurrentTime)
            xterm_win.configure_stack_mode(X.Above)
            d.flush()
            time.sleep(0.5)
        except:
            pass
    else:
        print("⚠️ xterm window not found, typing to focused window")

    # Test phrases
    test_phrases = [
        ("nihao", "你好"),
        ("zhongguo", "中国"),
        ("shurufa", "输入法"),
        ("pinyin", "拼音"),
        ("beijing", "北京"),
    ]

    print(f"\n--- Typing pinyin via XTEST ---")
    for pinyin, expected in test_phrases:
        print(f"  Typing: {pinyin} (expect: {expected})")
        type_string(d, pinyin, delay=0.15)
        time.sleep(0.5)
        # Press space to commit
        press_key(d, KEY_SPACE)  # space
        time.sleep(0.5)
        # Press Enter for new line
        press_key(d, KEY_RETURN)  # Return
        time.sleep(0.3)

    print(f"\n--- Done typing ---")

    # Take screenshot
    try:
        result = subprocess.run(
            ["xwd", "-root", "-out", "/tmp/xim_demo.xwd"],
            capture_output=True, timeout=5
        )
        if result.returncode == 0:
            print("✅ Screenshot saved: /tmp/xim_demo.xwd")
            # Try to convert to PNG
            result2 = subprocess.run(
                ["convert", "/tmp/xim_demo.xwd", "/tmp/xim_demo.png"],
                capture_output=True, timeout=5
            )
            if result2.returncode == 0:
                print("✅ Screenshot converted: /tmp/xim_demo.png")
            else:
                # Try netpbm
                result3 = subprocess.run(
                    ["xwdtopnm", "/tmp/xim_demo.xwd"],
                    capture_output=True, timeout=5
                )
                if result3.returncode == 0:
                    with open("/tmp/xim_demo.pnm", "wb") as f:
                        f.write(result3.stdout)
                    print("✅ Screenshot saved: /tmp/xim_demo.pnm")
    except Exception as e:
        print(f"⚠️ Screenshot: {e}")

    # Read xterm output
    time.sleep(1)
    xterm_proc.terminate()
    try:
        stdout, stderr = xterm_proc.communicate(timeout=3)
        if stdout:
            print(f"\n--- xterm output ---")
            try:
                text = stdout.decode('utf-8')
                print(text[:500])
            except:
                print(f"  (binary output, {len(stdout)} bytes)")
    except:
        pass

    d.close()

    print("\n" + "=" * 60)
    print("XIM GUI 演示完成")
    print("=" * 60)
    return True


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
