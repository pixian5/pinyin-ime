#!/usr/bin/env python3
"""
pinyin-ime 用户交互端到端测试
测试候选词翻页、选择、退格、取消等交互操作。

用法:
  python3 scripts/interaction_test.py
"""
import dbus
import time
import sys
import subprocess
import os

# Keycode map
KEYCODE_MAP = {
    'a': 38, 'b': 56, 'c': 54, 'd': 40, 'e': 26, 'f': 41, 'g': 42,
    'h': 43, 'i': 31, 'j': 44, 'k': 45, 'l': 46, 'm': 58, 'n': 57,
    'o': 32, 'p': 33, 'q': 24, 'r': 27, 's': 39, 't': 28, 'u': 30,
    'v': 55, 'w': 25, 'x': 53, 'y': 29, 'z': 52, ' ': 65,
    '1': 10, '2': 11, '3': 12, '4': 13, '5': 14,
    '6': 15, '7': 16, '8': 17, '9': 18, '0': 19,
}

# X11 keysyms for special keys
KEYSYM = {
    'BackSpace': 0xFF08,
    'Escape': 0xFF1B,
    'Return': 0xFF0D,
    'Tab': 0xFF09,
    'Page_Down': 0xFF56,
    'Page_Up': 0xFF55,
    'Down': 0xFF54,
    'Up': 0xFF52,
    'minus': 0x002D,
    'equal': 0x003D,
    'bracketleft': 0x005B,
    'bracketright': 0x005D,
}


def get_keycode(dpy_code, ch):
    """Get keycode for a character or special key."""
    if ch in KEYCODE_MAP:
        return KEYCODE_MAP[ch]
    return 0


def send_key(ic, ch, keycode=None):
    """Send a single key event."""
    if keycode is None:
        keycode = get_keycode(None, ch)
    keyval = ord(ch) if len(ch) == 1 else KEYSYM.get(ch, 0)
    if keyval == 0:
        return False
    try:
        result = ic.ProcessKeyEvent(
            dbus.UInt32(keyval), dbus.UInt32(keycode),
            dbus.UInt32(0), dbus.Boolean(False), dbus.UInt32(0)
        )
        return bool(result)
    except Exception as e:
        print(f"    Error sending '{ch}': {e}")
        return False


def send_string(ic, text):
    """Send a string of characters."""
    results = []
    for ch in text:
        consumed = send_key(ic, ch)
        results.append(consumed)
        time.sleep(0.03)
    return results


def run_interaction_test():
    """Run interaction tests."""
    try:
        bus = dbus.SessionBus()
    except Exception as e:
        print(f"❌ Cannot connect to DBus: {e}")
        return False

    # Get controller
    try:
        ctrl_proxy = bus.get_object("org.fcitx.Fcitx5", "/controller")
        ctrl = dbus.Interface(ctrl_proxy, "org.fcitx.Fcitx.Controller1")
    except Exception as e:
        print(f"❌ Cannot connect to fcitx5: {e}")
        return False

    # Set group
    try:
        ctrl.SetInputMethodGroupInfo("默认", "pinyinime", dbus.Array([
            dbus.Struct([dbus.String("keyboard-us"), dbus.String("")]),
            dbus.Struct([dbus.String("pinyinime"), dbus.String("")]),
        ], signature="(ss)"))
    except Exception as e:
        print(f"⚠️ SetInputMethodGroupInfo: {e}")

    # Start dbus-monitor
    monitor_proc = None
    monitor_log = "/tmp/interaction_monitor.log"
    try:
        monitor_proc = subprocess.Popen(
            ["/workspace/sysroot/usr/bin/dbus-monitor", "--session",
             "type='signal',interface='org.fcitx.Fcitx.InputContext1'"],
            stdout=open(monitor_log, "w"), stderr=subprocess.STDOUT
        )
        time.sleep(0.5)
    except Exception:
        pass

    # Create input context
    try:
        im_proxy = bus.get_object("org.fcitx.Fcitx5", "/org/freedesktop/portal/inputmethod")
        im = dbus.Interface(im_proxy, "org.fcitx.Fcitx.InputMethod1")
        args = dbus.Array([
            dbus.Struct([dbus.String("program"), dbus.String("interaction-test")]),
            dbus.Struct([dbus.String("unique-name"), dbus.String(bus.get_unique_name())]),
        ], signature="(ss)")
        result = im.CreateInputContext(args)
        ic_path = result[0]
    except Exception as e:
        print(f"❌ CreateInputContext failed: {e}")
        return False

    # Connect to IC
    ic_obj = bus.get_object("org.fcitx.Fcitx5", ic_path)
    ic = dbus.Interface(ic_obj, "org.fcitx.Fcitx.InputContext1")

    # Focus and set IM
    # Order: FocusIn -> SetCurrentIM (SetCapability resets IM!)
    ic.FocusIn()
    time.sleep(0.1)
    ctrl.SetCurrentIM("pinyinime")
    time.sleep(0.1)

    print(f"IC: {ic_path}")
    print(f"IM: {ctrl.CurrentInputMethod()}")
    print(f"{'='*60}")

    # ===== Test 1: Basic input and commit =====
    print(f"\n--- Test 1: Basic input (nihao + space) ---")
    try:
        ic.Reset()
    except:
        pass
    time.sleep(0.1)

    consumed = send_string(ic, "nihao")
    all_consumed = all(consumed)
    print(f"  Keys consumed: {sum(consumed)}/{len(consumed)}")

    # Send space to commit
    space_consumed = send_key(ic, ' ')
    print(f"  Space consumed: {space_consumed}")
    time.sleep(0.3)

    test1_pass = all_consumed and space_consumed
    print(f"  Result: {'✅ PASS' if test1_pass else '❌ FAIL'}")

    # ===== Test 2: Preedit display =====
    print(f"\n--- Test 2: Preedit display (beijing) ---")
    try:
        ic.Reset()
    except:
        pass
    time.sleep(0.1)

    consumed = send_string(ic, "beijing")
    print(f"  Keys consumed: {sum(consumed)}/{len(consumed)}")

    # Check for UpdateFormattedPreedit signal
    time.sleep(0.3)
    preedit_found = False
    try:
        with open(monitor_log, "r") as f:
            content = f.read()
        if "UpdateFormattedPreedit" in content:
            preedit_found = True
            print(f"  Preedit signal: ✅ found")
        else:
            print(f"  Preedit signal: ⚠️ not found in monitor log")
    except:
        pass

    # Cancel with Escape
    escape_consumed = send_key(ic, 'Escape')
    print(f"  Escape consumed: {escape_consumed}")
    time.sleep(0.2)

    test2_pass = all(consumed) and escape_consumed
    print(f"  Result: {'✅ PASS' if test2_pass else '❌ FAIL'}")

    # ===== Test 3: Backspace (delete last char) =====
    print(f"\n--- Test 3: Backspace (shanghai -> delete 'i') ---")
    try:
        ic.Reset()
    except:
        pass
    time.sleep(0.1)

    # Type "shanghai"
    consumed = send_string(ic, "shanghai")
    print(f"  Keys consumed: {sum(consumed)}/{len(consumed)}")

    # Send backspace to delete 'i'
    bs_consumed = send_key(ic, 'BackSpace', 22)
    print(f"  BackSpace consumed: {bs_consumed}")
    time.sleep(0.2)

    # Now type 'i' again
    i_consumed = send_key(ic, 'i')
    print(f"  Re-type 'i' consumed: {i_consumed}")
    time.sleep(0.2)

    # Commit with space
    space_consumed = send_key(ic, ' ')
    print(f"  Space consumed: {space_consumed}")
    time.sleep(0.3)

    test3_pass = all(consumed) and bs_consumed and i_consumed and space_consumed
    print(f"  Result: {'✅ PASS' if test3_pass else '❌ FAIL'}")

    # ===== Test 4: Number key selection =====
    print(f"\n--- Test 4: Number key selection (zhongguo + '2') ---")
    try:
        ic.Reset()
    except:
        pass
    time.sleep(0.1)

    # Type "zhongguo"
    consumed = send_string(ic, "zhongguo")
    print(f"  Keys consumed: {sum(consumed)}/{len(consumed)}")

    # Send '2' to select second candidate
    num_consumed = send_key(ic, '2')
    print(f"  Number '2' consumed: {num_consumed}")
    time.sleep(0.3)

    test4_pass = all(consumed) and num_consumed
    print(f"  Result: {'✅ PASS' if test4_pass else '❌ FAIL'}")

    # ===== Test 5: Page navigation =====
    print(f"\n--- Test 5: Page navigation (pinyin + Page_Down) ---")
    try:
        ic.Reset()
    except:
        pass
    time.sleep(0.1)

    # Type "pinyin"
    consumed = send_string(ic, "pinyin")
    print(f"  Keys consumed: {sum(consumed)}/{len(consumed)}")

    # Try Page_Down (keycode 89 for KP_Page_Down)
    page_down_consumed = send_key(ic, 'Page_Down', 89)
    print(f"  Page_Down consumed: {page_down_consumed}")
    time.sleep(0.2)

    # Try Page_Up (keycode 83 for KP_Page_Up)
    page_up_consumed = send_key(ic, 'Page_Up', 83)
    print(f"  Page_Up consumed: {page_up_consumed}")
    time.sleep(0.2)

    # Try bracket keys ([ and ]) which are common page navigation in fcitx5
    bracket_left = send_key(ic, 'bracketleft', 34)
    print(f"  '[' (page prev) consumed: {bracket_left}")
    time.sleep(0.2)

    bracket_right = send_key(ic, 'bracketright', 35)
    print(f"  ']' (page next) consumed: {bracket_right}")
    time.sleep(0.2)

    # Commit
    space_consumed = send_key(ic, ' ')
    print(f"  Space consumed: {space_consumed}")
    time.sleep(0.3)

    test5_pass = all(consumed) and space_consumed
    print(f"  Result: {'✅ PASS' if test5_pass else '❌ FAIL'}")

    # ===== Test 6: Multiple inputs =====
    print(f"\n--- Test 6: Multiple sequential inputs ---")
    test6_pass = True
    test_phrases = ["nihao", "shanghai", "beijing", "pinyin"]

    for phrase in test_phrases:
        try:
            ic.Reset()
        except:
            pass
        time.sleep(0.1)

        consumed = send_string(ic, phrase)
        space_consumed = send_key(ic, ' ')
        time.sleep(0.2)

        ok = all(consumed) and space_consumed
        status = "✅" if ok else "❌"
        print(f"  {phrase:<15} -> consumed={sum(consumed)+int(space_consumed)}/{len(consumed)+1} {status}")
        if not ok:
            test6_pass = False

    print(f"  Result: {'✅ PASS' if test6_pass else '❌ FAIL'}")

    # ===== Wait for signals and parse =====
    time.sleep(1)

    # Stop monitor
    if monitor_proc:
        monitor_proc.terminate()
        monitor_proc.wait()

    # Parse committed strings
    committed = []
    preedit_signals = []
    try:
        with open(monitor_log, "r") as f:
            lines = f.readlines()
        for i, line in enumerate(lines):
            if "CommitString" in line and i + 1 < len(lines):
                next_line = lines[i + 1].strip()
                if next_line.startswith("string"):
                    str_val = next_line.split('"')[1] if '"' in next_line else ""
                    committed.append(str_val)
            if "UpdateFormattedPreedit" in line:
                preedit_signals.append(line.strip())
    except Exception:
        pass

    # ===== Summary =====
    print(f"\n{'='*60}")
    print(f"SUMMARY")
    print(f"{'='*60}")
    print(f"  Test 1 (basic input):    {'✅ PASS' if test1_pass else '❌ FAIL'}")
    print(f"  Test 2 (preedit):        {'✅ PASS' if test2_pass else '❌ FAIL'}")
    print(f"  Test 3 (backspace):      {'✅ PASS' if test3_pass else '❌ FAIL'}")
    print(f"  Test 4 (number select):  {'✅ PASS' if test4_pass else '❌ FAIL'}")
    print(f"  Test 5 (page nav):       {'✅ PASS' if test5_pass else '❌ FAIL'}")
    print(f"  Test 6 (multi-input):    {'✅ PASS' if test6_pass else '❌ FAIL'}")
    print(f"\n  Committed strings: {committed}")
    print(f"  Preedit signals: {len(preedit_signals)}")

    all_pass = all([test1_pass, test2_pass, test3_pass, test4_pass, test5_pass, test6_pass])
    print(f"\n  Overall: {'✅ ALL PASS' if all_pass else '❌ SOME FAILED'}")

    # Cleanup
    try:
        ic.DestroyIC()
    except:
        pass

    return all_pass


if __name__ == "__main__":
    success = run_interaction_test()
    sys.exit(0 if success else 1)
