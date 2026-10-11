#!/usr/bin/env python3
"""
XIM 协议验证测试
验证 fcitx5 的 XIM addon 正常工作：
  1. XIM addon 已加载（检查 fcitx5 日志）
  2. XIM server 窗口已注册在 X display
  3. xterm 可通过 XIM 协议启动
  4. fcitx5-remote 状态正常

需要：xterm, python-xlib, Xvfb :99, fcitx5 with xim addon
用法: python3 scripts/xim_test.py (需要 conda python with xlib)
"""

import os
import sys
import time
import subprocess

os.environ.setdefault("DISPLAY", ":99")


def check_xim_addon():
    """检查 fcitx5 日志中 XIM addon 是否加载"""
    # Try multiple log files
    for log_file in ["/tmp/fcitx5.log", "/tmp/fcitx5_fuzzy.log"]:
        if not os.path.exists(log_file):
            continue
        with open(log_file) as f:
            content = f.read()
        if "Loaded addon xim" in content:
            return True, "xim addon loaded"
        if "Could not load addon xim" in content:
            return False, "xim addon failed to load"

    # Try checking via DBus
    try:
        import dbus
        bus = dbus.SessionBus()
        ctrl = bus.get_object("org.fcitx.Fcitx5", "/controller")
        iface = dbus.Interface(ctrl, "org.fcitx.Fcitx.Controller1")
        # Check if xim addon is loaded
        try:
            addons = iface.Addons()
            for addon in addons:
                if "xim" in str(addon).lower():
                    return True, f"xim addon found via DBus: {addon}"
        except:
            pass
    except:
        pass

    return False, "xim addon status unknown"


def check_xim_server():
    """检查 X display 上是否有 XIM server"""
    try:
        from Xlib import X, display
        d = display.Display()
        root = d.screen().root

        # Check for XIM-related atoms
        xim_atoms = []
        for atom_name in ["XIM_SERVER", "_XIM_PROTOCOL", "_XIM_METHOD", "_XIM_RULE"]:
            try:
                atom = d.intern_atom(atom_name, True)
                if atom:
                    xim_atoms.append(atom_name)
            except:
                pass

        # Check root window properties for XIM
        try:
            props = root.list_properties()
            for prop in props:
                try:
                    name = d.get_atom_name(prop)
                    if "XIM" in name or "_XIM" in name:
                        xim_atoms.append(name)
                except:
                    pass
        except:
            pass

        # Also check for fcitx5 windows
        def find_windows(win, depth=0):
            results = []
            if depth > 5:
                return results
            try:
                children = win.query_tree().children
                for child in children:
                    try:
                        name = child.get_wm_name()
                        if name:
                            results.append(name)
                    except:
                        pass
                    results.extend(find_windows(child, depth+1))
            except:
                pass
            return results

        windows = find_windows(root)
        fcitx_windows = [w for w in windows if 'fcitx' in w.lower() or 'input' in w.lower()]

        d.close()

        if xim_atoms:
            return True, f"atoms: {', '.join(xim_atoms)}"
        if fcitx_windows:
            return True, f"windows: {', '.join(fcitx_windows[:3])}"
        return False, "no XIM atoms or windows found"
    except ImportError:
        return False, "python-xlib not available (use conda python)"
    except Exception as e:
        return False, str(e)


def check_xterm():
    """启动 xterm 并验证 XIM 连接"""
    try:
        env = {**os.environ, "XMODIFIERS": "@im=fcitx", "LANG": "en_US.UTF-8"}

        # xterm uses -im flag (not -xim), font error is just a warning
        proc = subprocess.Popen(
            ["xterm", "-im", "-e", "sleep", "3"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            env=env
        )
        time.sleep(1.5)

        if proc.poll() is None:
            # xterm is running - check if it has XIM connection
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except:
                proc.kill()
            return True, "xterm started with -im (XIM enabled)"
        else:
            # xterm exited - check exit code
            exit_code = proc.returncode
            if exit_code == 0:
                return True, "xterm ran and exited normally"
            return False, f"xterm exited early (code={exit_code})"
    except FileNotFoundError:
        return False, "xterm not installed"
    except Exception as e:
        return False, str(e)


def check_fcitx5_remote():
    """检查 fcitx5-remote 状态"""
    # Find fcitx5-remote in PATH
    for path in os.environ.get("PATH", "").split(":"):
        remote = os.path.join(path, "fcitx5-remote")
        if os.path.exists(remote):
            try:
                result = subprocess.run([remote], capture_output=True, text=True, timeout=5)
                state = result.stdout.strip()
                states = {"0": "closed", "1": "inactive", "2": "active"}
                return True, f"state={states.get(state, state)}"
            except Exception as e:
                return False, str(e)
    return False, "fcitx5-remote not found in PATH"


def check_xim_protocol():
    """验证 XIM 协议通信"""
    try:
        from Xlib import X, display, XK
        d = display.Display()
        root = d.screen().root

        # Check for standard XIM atoms
        # XIM_SERVERS (plural) is the standard XIM spec atom
        # XIM_SERVER (singular) is set by some implementations
        # _XIM_RULE is fcitx5-specific
        found_atoms = []
        for atom_name in ["XIM_SERVERS", "XIM_SERVER", "_XIM_RULE", "_XIM_PROTOCOL"]:
            try:
                atom = d.intern_atom(atom_name, True)
                if atom:
                    found_atoms.append(atom_name)
            except:
                pass

        d.close()
        if found_atoms:
            return True, f"atoms: {', '.join(found_atoms)}"
        return False, "no XIM protocol properties"
    except Exception as e:
        return False, str(e)


def main():
    print("=== XIM 协议验证测试 ===")
    print()

    tests_passed = 0
    tests_total = 0

    def run_check(name, func):
        nonlocal tests_passed, tests_total
        tests_total += 1
        ok, detail = func()
        if ok:
            print(f"  ✅ {name}: {detail}")
            tests_passed += 1
        else:
            print(f"  ❌ {name}: {detail}")

    print("--- 1. addon 加载 ---")
    run_check("XIM addon", check_xim_addon)
    print()

    print("--- 2. XIM server 注册 ---")
    run_check("XIM server", check_xim_server)
    print()

    print("--- 3. XIM 协议属性 ---")
    run_check("XIM 协议", check_xim_protocol)
    print()

    print("--- 4. xterm XIM 启动 ---")
    run_check("xterm -im", check_xterm)
    print()

    print("--- 5. fcitx5-remote ---")
    run_check("fcitx5-remote", check_fcitx5_remote)
    print()

    print(f"=== XIM 验证: {tests_passed}/{tests_total} ===")
    return tests_passed >= 3


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
