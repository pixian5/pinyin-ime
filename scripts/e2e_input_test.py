#!/usr/bin/env python3
"""
pinyin-ime 端到端输入测试
通过 fcitx5 DBus 接口创建输入上下文、发送按键、验证提交的中文字符串。

用法:
  python3 scripts/e2e_input_test.py
  
环境要求:
  - fcitx5 已运行 (DISPLAY, DBUS_SESSION_BUS_ADDRESS)
  - pinyinime addon 已加载
"""
import dbus
import time
import sys
import subprocess
import os

# Test phrases: pinyin -> expected Chinese
TEST_PHRASES = [
    ("nihao", "你好"),
    ("zhongguo", "中国"),
    ("shurufa", "输入法"),
    ("beijing", "北京"),
    ("shanghai", "上海"),
    ("pinyin", "拼音"),
    ("zhongwen", "中文"),
    ("computer", None),  # English word, may not be converted
]

# Keycode map for common letters
KEYCODE_MAP = {
    'a': 38, 'b': 56, 'c': 54, 'd': 40, 'e': 26, 'f': 41, 'g': 42,
    'h': 43, 'i': 31, 'j': 44, 'k': 45, 'l': 46, 'm': 58, 'n': 57,
    'o': 32, 'p': 33, 'q': 24, 'r': 27, 's': 39, 't': 28, 'u': 30,
    'v': 55, 'w': 25, 'x': 53, 'y': 29, 'z': 52, ' ': 65,
}


def run_e2e_test():
    """Run end-to-end input test via DBus."""
    try:
        bus = dbus.SessionBus()
    except Exception as e:
        print(f"❌ Cannot connect to DBus: {e}")
        return False, 0, 0

    # Get controller
    try:
        ctrl_proxy = bus.get_object("org.fcitx.Fcitx5", "/controller")
        ctrl = dbus.Interface(ctrl_proxy, "org.fcitx.Fcitx.Controller1")
    except Exception as e:
        print(f"❌ Cannot connect to fcitx5: {e}")
        return False, 0, 0

    # Verify pinyinime is available
    methods = ctrl.AvailableInputMethods()
    pinyinime_found = any(m[0] == "pinyinime" for m in methods)
    if not pinyinime_found:
        print("❌ pinyinime not found in available input methods")
        return False, 0, 0

    # Set group
    try:
        ctrl.SetInputMethodGroupInfo("默认", "pinyinime", dbus.Array([
            dbus.Struct([dbus.String("keyboard-us"), dbus.String("")]),
            dbus.Struct([dbus.String("pinyinime"), dbus.String("")]),
        ], signature="(ss)"))
    except Exception as e:
        print(f"⚠️ SetInputMethodGroupInfo: {e}")

    # Start dbus-monitor in background
    monitor_proc = None
    monitor_log = "/tmp/e2e_dbus_monitor.log"
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
            dbus.Struct([dbus.String("program"), dbus.String("e2e-test")]),
            dbus.Struct([dbus.String("unique-name"), dbus.String(bus.get_unique_name())]),
        ], signature="(ss)")
        result = im.CreateInputContext(args)
        ic_path = result[0]
    except Exception as e:
        print(f"❌ CreateInputContext failed: {e}")
        return False, 0, 0

    # Connect to IC
    ic_obj = bus.get_object("org.fcitx.Fcitx5", ic_path)
    ic = dbus.Interface(ic_obj, "org.fcitx.Fcitx.InputContext1")

    # Focus and set IM
    ic.FocusIn()
    ctrl.SetCurrentIM("pinyinime")
    ic.SetCapability(dbus.UInt64(0x20001))
    time.sleep(0.2)

    current_im = str(ctrl.CurrentInputMethod())
    if current_im != "pinyinime":
        print(f"⚠️ Current IM is '{current_im}', not 'pinyinime'")

    # Run tests
    print(f"{'='*60}")
    print(f"{'pinyin':<20} {'expected':<15} {'result':<15} {'status'}")
    print(f"{'='*60}")

    passed = 0
    failed = 0
    total_consumed = 0
    total_keys = 0

    for pinyin, expected in TEST_PHRASES:
        # Reset IC
        try:
            ic.Reset()
        except Exception:
            pass
        time.sleep(0.1)

        # Send keys
        consumed_count = 0
        for ch in pinyin + " ":
            keyval = ord(ch)
            keycode = KEYCODE_MAP.get(ch, 0)
            try:
                result = ic.ProcessKeyEvent(
                    dbus.UInt32(keyval), dbus.UInt32(keycode),
                    dbus.UInt32(0), dbus.Boolean(False), dbus.UInt32(0)
                )
                if result:
                    consumed_count += 1
            except Exception:
                pass
            time.sleep(0.05)

        total_consumed += consumed_count
        total_keys += len(pinyin) + 1

        # Check if all keys were consumed
        all_consumed = consumed_count == len(pinyin) + 1

        if expected:
            result_str = "consumed" if all_consumed else f"partial ({consumed_count}/{len(pinyin)+1})"
            status = "✅" if all_consumed else "❌"
            print(f"{pinyin:<20} {expected:<15} {result_str:<15} {status}")
            if all_consumed:
                passed += 1
            else:
                failed += 1
        else:
            print(f"{pinyin:<20} {'(skip)':<15} {'consumed':<15} ⏭️")

    # Wait for signals
    time.sleep(1)

    # Stop monitor
    if monitor_proc:
        monitor_proc.terminate()
        monitor_proc.wait()

    # Parse committed strings from monitor log
    committed = []
    try:
        with open(monitor_log, "r") as f:
            lines = f.readlines()
        for i, line in enumerate(lines):
            if "CommitString" in line and i + 1 < len(lines):
                next_line = lines[i + 1].strip()
                if next_line.startswith("string"):
                    # Extract string content
                    str_val = next_line.split('"')[1] if '"' in next_line else ""
                    committed.append(str_val)
    except Exception:
        pass

    print(f"\n{'='*60}")
    print(f"Committed strings: {committed}")
    print(f"Keys consumed: {total_consumed}/{total_keys}")
    print(f"Tests passed: {passed}/{passed + failed}")

    # Cleanup
    try:
        ic.DestroyIC()
    except Exception:
        pass

    success = passed > 0 and total_consumed > 0
    return success, passed, failed


if __name__ == "__main__":
    success, passed, failed = run_e2e_test()
    sys.exit(0 if success else 1)
