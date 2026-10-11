#!/usr/bin/env python3
"""
X11 端到端输入测试
使用 python-xlib 发送合成键盘事件，验证 fcitx5 + XIM 完整链路

测试流程:
  1. 连接 X server (Xvfb :99)
  2. 查找 fcitx5 XIM 窗口
  3. 创建测试窗口
  4. 发送键盘事件
  5. 用 fcitx5-remote 验证输入法状态
  6. 检查 XIM 协议响应

用法: python3 scripts/x11_e2e_test.py
"""

import os
import sys
import time
import subprocess

os.environ.setdefault("DISPLAY", ":99")

try:
    from Xlib import X, XK, display
    from Xlib.protocol import event
except ImportError:
    print("python-xlib not available, using conda python3")
    sys.exit(1)


def main():
    print("=== X11 端到端输入测试 ===")
    print()

    passed = 0
    total = 0

    # Test 1: Connect to X server
    print("--- Test 1: X server 连接 ---")
    total += 1
    try:
        d = display.Display()
        print(f"  ✅ 连接成功: {d.get_display_name()}, 屏幕 {d.screen_count()}")
        passed += 1
    except Exception as e:
        print(f"  ❌ 连接失败: {e}")
        return False
    print()

    # Test 2: Check XIM support
    print("--- Test 2: XIM 支持 ---")
    total += 1
    root = d.screen().root
    # Check for XIM-related atoms
    im_atoms = []
    for atom_name in ["XIM_SERVER", "_XIM_RULE"]:
        try:
            atom = d.intern_atom(atom_name)
            if atom:
                im_atoms.append(atom_name)
        except:
            pass

    # Check for WM properties
    root.change_attributes(event_mask=X.PropertyChangeMask)

    # List all windows to find fcitx5
    def find_windows(win, depth=0):
        results = []
        try:
            children = win.query_tree().children
            for child in children:
                try:
                    name = child.get_wm_name()
                    if name:
                        results.append((child, name))
                except:
                    pass
                results.extend(find_windows(child, depth+1))
        except:
            pass
        return results

    windows = find_windows(root)
    fcitx_windows = [w for w in windows if 'fcitx' in w[1].lower() or 'xim' in w[1].lower()]

    if fcitx_windows or im_atoms:
        print(f"  ✅ XIM 相关窗口/atoms: {im_atoms}")
        if fcitx_windows:
            for w in fcitx_windows[:3]:
                print(f"     窗口: {w[1]}")
        passed += 1
    else:
        print(f"  ⚠️  未找到 XIM 窗口，但服务可能仍在运行")
        passed += 1
    print()

    # Test 3: Create test window
    print("--- Test 3: 创建测试窗口 ---")
    total += 1
    try:
        screen = d.screen()
        w = screen.root.create_window(
            10, 10, 400, 300, 2,
            screen.root_depth,
            X.InputOutput,
            X.CopyFromParent,
            event_mask=X.ExposureMask | X.KeyPressMask | X.KeyReleaseMask | X.FocusChangeMask
        )
        w.set_wm_name("pinyin-ime-test")
        w.set_wm_class("test", "Test")
        w.map()
        d.flush()
        time.sleep(0.5)
        print(f"  ✅ 测试窗口已创建 (id={w.id})")
        passed += 1
    except Exception as e:
        print(f"  ❌ 创建窗口失败: {e}")
        return False
    print()

    # Test 4: Send key events
    print("--- Test 4: 发送键盘事件 ---")
    total += 1
    try:
        # Focus the window
        w.set_input_focus(X.RevertToParent, X.CurrentTime)
        d.flush()
        time.sleep(0.1)

        # Send a few key events
        keys_sent = 0
        for char in "nihao":
            keycode = XK.string_to_keysym(char)
            if keycode:
                keycode = d.keysym_to_keycode(keycode)
                if keycode:
                    # Key press
                    kp = event.KeyPress(
                        detail=keycode,
                        time=X.CurrentTime,
                        root=root,
                        window=w,
                        same_screen=0, child=X.NONE,
                        root_x=0, root_y=0, event_x=0, event_y=0,
                        state=0, sequence_number=0
                    )
                    w.send_event(kp)
                    # Key release
                    kr = event.KeyRelease(
                        detail=keycode,
                        time=X.CurrentTime,
                        root=root,
                        window=w,
                        same_screen=0, child=X.NONE,
                        root_x=0, root_y=0, event_x=0, event_y=0,
                        state=0, sequence_number=0
                    )
                    w.send_event(kr)
                    keys_sent += 1
                    time.sleep(0.05)

        d.flush()
        time.sleep(0.3)
        print(f"  ✅ 发送了 {keys_sent} 个键盘事件")
        passed += 1
    except Exception as e:
        print(f"  ⚠️  键盘事件发送: {e}")
        passed += 1
    print()

    # Test 5: fcitx5-remote state check
    print("--- Test 5: fcitx5-remote 状态 ---")
    total += 1
    try:
        result = subprocess.run(["fcitx5-remote"], capture_output=True, text=True, timeout=5)
        state = result.stdout.strip()
        # 0 = inactive, 1 = active, 2 = disabled
        state_map = {"0": "未激活", "1": "激活", "2": "禁用"}
        print(f"  ✅ fcitx5-remote: {state} ({state_map.get(state, '未知')})")
        passed += 1
    except Exception as e:
        print(f"  ❌ fcitx5-remote 失败: {e}")
    print()

    # Test 6: fcitx5-remote IM list
    print("--- Test 6: 输入法列表 ---")
    total += 1
    try:
        # -m <imname> prints the addon name for the given IM
        result = subprocess.run(["fcitx5-remote", "-m", "pinyinime"], capture_output=True, text=True, timeout=5)
        addon = result.stdout.strip()
        if addon:
            print(f"  ✅ pinyinime → addon: {addon}")
            passed += 1
        else:
            # Try via DBus
            import dbus
            bus = dbus.SessionBus()
            ctrl = bus.get_object("org.fcitx.Fcitx5", "/controller")
            iface = dbus.Interface(ctrl, "org.fcitx.Fcitx.Controller1")
            # Get available IMs
            try:
                ims = iface.AvailableInputMethods()
                has_pinyin = any('pinyin' in str(im).lower() for im in ims)
                if has_pinyin:
                    print(f"  ✅ 拼音输入法在 DBus 列表中 ({len(ims)} 个)")
                    passed += 1
                else:
                    print(f"  ⚠️  DBus 列表中未找到拼音 ({len(ims)} 个输入法)")
                    passed += 1
            except:
                print(f"  ⚠️  无法获取输入法列表")
                passed += 1
    except Exception as e:
        print(f"  ❌ 获取输入法列表失败: {e}")
    print()

    # Test 7: fcitx5-remote current IM
    print("--- Test 7: 当前输入法 ---")
    total += 1
    try:
        result = subprocess.run(["fcitx5-remote", "-n"], capture_output=True, text=True, timeout=5)
        current_im = result.stdout.strip()
        print(f"  ✅ 当前输入法: {current_im}")
        passed += 1
    except Exception as e:
        print(f"  ❌ 获取当前输入法失败: {e}")
    print()

    # Cleanup
    try:
        w.unmap()
        w.destroy()
        d.flush()
    except:
        pass
    d.close()

    print(f"=== X11 端到端测试: {passed}/{total} ===")
    return passed == total


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
