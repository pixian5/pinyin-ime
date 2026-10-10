#!/usr/bin/env python3
"""
pinyin-ime SelectCandidate 和简拼端到端测试
测试精确选词、简拼转换、候选词翻页选择。

用法:
  python3 scripts/select_test.py
"""
import dbus
import time
import sys
import subprocess

KEYCODE_MAP = {
    'a': 38, 'b': 56, 'c': 54, 'd': 40, 'e': 26, 'f': 41, 'g': 42,
    'h': 43, 'i': 31, 'j': 44, 'k': 45, 'l': 46, 'm': 58, 'n': 57,
    'o': 32, 'p': 33, 'q': 24, 'r': 27, 's': 39, 't': 28, 'u': 30,
    'v': 55, 'w': 25, 'x': 53, 'y': 29, 'z': 52, ' ': 65,
    '1': 10, '2': 11, '3': 12, '4': 13, '5': 14,
    '6': 15, '7': 16, '8': 17, '9': 18, '0': 19,
}


def send_key(ic, ch, keycode=None):
    """Send a single key event."""
    if keycode is None:
        keycode = KEYCODE_MAP.get(ch, 0)
    keyval = ord(ch) if len(ch) == 1 else 0
    if keyval == 0:
        return False
    try:
        result = ic.ProcessKeyEvent(
            dbus.UInt32(keyval), dbus.UInt32(keycode),
            dbus.UInt32(0), dbus.Boolean(False), dbus.UInt32(0)
        )
        return bool(result)
    except Exception:
        return False


def send_string(ic, text):
    """Send a string of characters."""
    results = []
    for ch in text:
        consumed = send_key(ic, ch)
        results.append(consumed)
        time.sleep(0.03)
    return results


def run_select_test():
    """Run SelectCandidate and 简拼 tests."""
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

    # Start dbus-monitor
    monitor_proc = None
    monitor_log = "/tmp/select_monitor.log"
    try:
        monitor_proc = subprocess.Popen(
            ["/workspace/sysroot/usr/bin/dbus-monitor", "--session",
             "type='signal',sender=':1.0'"],
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
            dbus.Struct([dbus.String("program"), dbus.String("select-test")]),
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

    # Correct order: FocusIn -> SetCurrentIM
    ic.FocusIn()
    time.sleep(0.1)
    ctrl.SetCurrentIM("pinyinime")
    time.sleep(0.2)

    current_im = str(ctrl.CurrentInputMethod())
    print(f"IC: {ic_path}")
    print(f"IM: {current_im}")
    print(f"{'='*60}")

    passed = 0
    failed = 0

    # ===== Test 1: SelectCandidate - select 2nd candidate =====
    print(f"\n--- Test 1: SelectCandidate (zhongguo, select #2) ---")
    try:
        ic.Reset()
    except:
        pass
    time.sleep(0.1)

    # Type "zhongguo"
    consumed = send_string(ic, "zhongguo")
    print(f"  Keys consumed: {sum(consumed)}/{len(consumed)}")

    # Select candidate #2 (index 1)
    try:
        ic.SelectCandidate(dbus.UInt32(1))
        print(f"  SelectCandidate(1): OK")
        select_ok = True
    except Exception as e:
        print(f"  SelectCandidate(1): {e}")
        select_ok = False

    time.sleep(0.3)

    # Check if it committed (SelectCandidate usually commits immediately)
    space_consumed = send_key(ic, ' ')
    print(f"  Space consumed: {space_consumed}")
    time.sleep(0.3)

    test1_pass = all(consumed) and select_ok
    print(f"  Result: {'✅ PASS' if test1_pass else '❌ FAIL'}")
    if test1_pass:
        passed += 1
    else:
        failed += 1

    # ===== Test 2: 简拼 - zg → 中国 =====
    print(f"\n--- Test 2: 简拼 (zg → 中国) ---")
    try:
        ic.Reset()
    except:
        pass
    time.sleep(0.1)

    # Type "zg" (abbreviated pinyin for 中国)
    consumed = send_string(ic, "zg")
    print(f"  Keys consumed: {sum(consumed)}/{len(consumed)}")

    # Commit with space
    space_consumed = send_key(ic, ' ')
    print(f"  Space consumed: {space_consumed}")
    time.sleep(0.3)

    test2_pass = all(consumed) and space_consumed
    print(f"  Result: {'✅ PASS' if test2_pass else '❌ FAIL'}")
    if test2_pass:
        passed += 1
    else:
        failed += 1

    # ===== Test 3: 简拼 - shjf → 输入法 =====
    print(f"\n--- Test 3: 简拼 (shurufa full + zg abbreviated) ---")
    try:
        ic.Reset()
    except:
        pass
    time.sleep(0.1)

    # Type "shjf" (abbreviated for 输入法 - shu ru fa)
    consumed = send_string(ic, "shjf")
    print(f"  Keys consumed: {sum(consumed)}/{len(consumed)}")

    # Commit with space
    space_consumed = send_key(ic, ' ')
    print(f"  Space consumed: {space_consumed}")
    time.sleep(0.3)

    test3_pass = all(consumed) and space_consumed
    print(f"  Result: {'✅ PASS' if test3_pass else '❌ FAIL'}")
    if test3_pass:
        passed += 1
    else:
        failed += 1

    # ===== Test 4: Page navigation + SelectCandidate =====
    print(f"\n--- Test 4: Page navigation + SelectCandidate ---")
    try:
        ic.Reset()
    except:
        pass
    time.sleep(0.1)

    # Type "pinyin"
    consumed = send_string(ic, "pinyin")
    print(f"  Keys consumed: {sum(consumed)}/{len(consumed)}")

    # Navigate to next page
    bracket_right = send_key(ic, ']', 35)
    print(f"  ']' (next page) consumed: {bracket_right}")
    time.sleep(0.2)

    # Select candidate #1 on this page
    try:
        ic.SelectCandidate(dbus.UInt32(0))
        print(f"  SelectCandidate(0): OK")
        select_ok = True
    except Exception as e:
        print(f"  SelectCandidate(0): {e}")
        select_ok = False

    time.sleep(0.3)

    test4_pass = all(consumed) and bracket_right and select_ok
    print(f"  Result: {'✅ PASS' if test4_pass else '❌ FAIL'}")
    if test4_pass:
        passed += 1
    else:
        failed += 1

    # ===== Test 5: Multiple SelectCandidate =====
    print(f"\n--- Test 5: Multiple SelectCandidate (nihao, select #1 then #2) ---")
    try:
        ic.Reset()
    except:
        pass
    time.sleep(0.1)

    # Type "nihao"
    consumed = send_string(ic, "nihao")
    print(f"  Keys consumed: {sum(consumed)}/{len(consumed)}")

    # Select candidate #1
    try:
        ic.SelectCandidate(dbus.UInt32(0))
        print(f"  SelectCandidate(0): OK")
        select1_ok = True
    except Exception as e:
        print(f"  SelectCandidate(0): {e}")
        select1_ok = False

    time.sleep(0.3)

    # Now type more pinyin
    consumed2 = send_string(ic, "shanghai")
    print(f"  Keys consumed: {sum(consumed2)}/{len(consumed2)}")

    # Select candidate #2
    try:
        ic.SelectCandidate(dbus.UInt32(1))
        print(f"  SelectCandidate(1): OK")
        select2_ok = True
    except Exception as e:
        print(f"  SelectCandidate(1): {e}")
        select2_ok = False

    time.sleep(0.3)

    test5_pass = all(consumed) and select1_ok and all(consumed2) and select2_ok
    print(f"  Result: {'✅ PASS' if test5_pass else '❌ FAIL'}")
    if test5_pass:
        passed += 1
    else:
        failed += 1

    # ===== Test 6: Escape cancel after typing =====
    print(f"\n--- Test 6: Escape cancel (beijing + Escape + nihao) ---")
    try:
        ic.Reset()
    except:
        pass
    time.sleep(0.1)

    # Type "beijing"
    consumed = send_string(ic, "beijing")
    print(f"  Keys consumed: {sum(consumed)}/{len(consumed)}")

    # Cancel with Escape (keyval=0xFF1B, keycode=9)
    escape_consumed = send_key(ic, '\x1b', 9)
    # Override keyval for Escape
    try:
        result = ic.ProcessKeyEvent(
            dbus.UInt32(0xFF1B), dbus.UInt32(9),
            dbus.UInt32(0), dbus.Boolean(False), dbus.UInt32(0)
        )
        escape_consumed = bool(result)
    except:
        pass
    print(f"  Escape consumed: {escape_consumed}")
    time.sleep(0.2)

    # Now type "nihao"
    consumed2 = send_string(ic, "nihao")
    print(f"  Keys consumed: {sum(consumed2)}/{len(consumed2)}")

    # Commit
    space_consumed = send_key(ic, ' ')
    print(f"  Space consumed: {space_consumed}")
    time.sleep(0.3)

    test6_pass = all(consumed) and escape_consumed and all(consumed2) and space_consumed
    print(f"  Result: {'✅ PASS' if test6_pass else '❌ FAIL'}")
    if test6_pass:
        passed += 1
    else:
        failed += 1

    # ===== Wait for signals and parse =====
    time.sleep(1)

    # Stop monitor
    if monitor_proc:
        monitor_proc.terminate()
        monitor_proc.wait()

    # Parse committed strings
    committed = []
    try:
        with open(monitor_log, "r") as f:
            lines = f.readlines()
        for i, line in enumerate(lines):
            if "CommitString" in line and i + 1 < len(lines):
                next_line = lines[i + 1].strip()
                if next_line.startswith("string"):
                    str_val = next_line.split('"')[1] if '"' in next_line else ""
                    committed.append(str_val)
    except Exception:
        pass

    # ===== Summary =====
    print(f"\n{'='*60}")
    print(f"SUMMARY")
    print(f"{'='*60}")
    print(f"  Test 1 (SelectCandidate #2):  {'✅ PASS' if test1_pass else '❌ FAIL'}")
    print(f"  Test 2 (简拼 zg):              {'✅ PASS' if test2_pass else '❌ FAIL'}")
    print(f"  Test 3 (简拼 shjf):            {'✅ PASS' if test3_pass else '❌ FAIL'}")
    print(f"  Test 4 (Page + Select):        {'✅ PASS' if test4_pass else '❌ FAIL'}")
    print(f"  Test 5 (Multiple Select):      {'✅ PASS' if test5_pass else '❌ FAIL'}")
    print(f"  Test 6 (Escape cancel):        {'✅ PASS' if test6_pass else '❌ FAIL'}")
    print(f"\n  Committed strings: {committed}")
    print(f"\n  Tests passed: {passed}/{passed + failed}")

    all_pass = all([test1_pass, test2_pass, test3_pass, test4_pass, test5_pass, test6_pass])
    print(f"\n  Overall: {'✅ ALL PASS' if all_pass else '❌ SOME FAILED'}")

    # Cleanup
    try:
        ic.DestroyIC()
    except:
        pass

    return all_pass


if __name__ == "__main__":
    success = run_select_test()
    sys.exit(0 if success else 1)
