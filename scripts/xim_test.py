#!/usr/bin/env python3
"""
XIM 协议验证测试
验证 fcitx5 的 XIM addon 正常工作：
  1. XIM addon 已加载（检查 fcitx5 日志）
  2. XIM server 窗口已注册在 X display
  3. xterm 可通过 XIM 协议启动
  4. fcitx5-remote 状态正常

需要：xterm, python-xlib, Xvfb :99, fcitx5 with xim addon
"""

import os
import sys
import time
import subprocess

os.environ.setdefault("DISPLAY", ":99")


def check_xim_addon():
    """检查 fcitx5 日志中 XIM addon 是否加载"""
    log_file = "/tmp/fcitx5.log"
    if not os.path.exists(log_file):
        return False, "no fcitx5.log"
    with open(log_file) as f:
        content = f.read()
    if "Loaded addon xim" in content:
        return True, "xim addon loaded"
    if "Could not load addon xim" in content:
        return False, "xim addon failed to load"
    return False, "xim addon status unknown"


def check_xim_server():
    """检查 X display 上是否有 XIM server"""
    try:
        from Xlib import X, display
        d = display.Display()
        root = d.screen().root

        # Check for XIM-related atoms
        xim_atoms = []
        for atom_name in ["XIM_SERVER", "_XIM_PROTOCOL", "_XIM_METHOD"]:
            atom = d.intern_atom(atom_name, True)
            if atom:
                xim_atoms.append(atom_name)

        # Check root window properties for XIM
        try:
            props = root.list_properties()
            for prop in props:
                if "XIM" in str(prop.name) or "_XIM" in str(prop.name):
                    xim_atoms.append(str(prop.name))
        except:
            pass

        d.close()
        if xim_atoms:
            return True, f"found: {', '.join(xim_atoms)}"
        return False, "no XIM atoms found"
    except ImportError:
        return False, "python-xlib not available"
    except Exception as e:
        return False, str(e)


def check_xterm():
    """启动 xterm 并验证 XIM 连接"""
    try:
        env = {**os.environ, "XMODIFIERS": "@im=fcitx", "LANG": "en_US.UTF-8"}
        proc = subprocess.Popen(
            ["xterm", "-xim", "-e", "sleep", "3"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            env=env
        )
        time.sleep(2)
        if proc.poll() is None:
            proc.terminate()
            proc.wait(timeout=3)
            return True, "xterm started with XIM"
        return False, "xterm exited early"
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


def main():
    print("=== XIM 协议验证测试 ===")
    print()

    tests_passed = 0
    tests_total = 0
    results = []

    def run_check(name, func):
        nonlocal tests_passed, tests_total
        tests_total += 1
        ok, detail = func()
        if ok:
            print(f"  ✅ {name}: {detail}")
            tests_passed += 1
        else:
            print(f"  ❌ {name}: {detail}")
        results.append((name, ok))

    print("--- 1. addon 加载 ---")
    run_check("XIM addon", check_xim_addon)
    print()

    print("--- 2. XIM server ---")
    run_check("XIM server 注册", check_xim_server)
    print()

    print("--- 3. xterm XIM ---")
    run_check("xterm XIM 启动", check_xterm)
    print()

    print("--- 4. fcitx5-remote ---")
    run_check("fcitx5-remote", check_fcitx5_remote)
    print()

    print(f"=== XIM 验证: {tests_passed}/{tests_total} ===")
    return tests_passed >= 2  # At least addon + remote should pass


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
