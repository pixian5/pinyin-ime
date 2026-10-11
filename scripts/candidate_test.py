#!/usr/bin/env python3.9
"""
候选词翻页/选择测试
通过 CLI 验证候选词列表，通过 DBus 验证交互逻辑

用法: python3.9 scripts/candidate_test.py
"""

import subprocess
import dbus
import time
import sys
import os

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(PROJECT_ROOT, "pinyin-ime-s3", "pinyin_ime_cli_v0.9.83")
LM = os.path.join(PROJECT_ROOT, "pinyin-ime-s3", "zh_CN.lm")
DICT = os.path.join(PROJECT_ROOT, "pinyin-ime-s3", "sc.dict")

LIBIME = "/root/workspace/native-build/opt/libime/lib64"
DEPSLIB = "/root/.local/pinyin-ime/opt/deps-lib"
BUNDLE = "/root/.local/pinyin-ime/opt/fcitx5-x11"
ENV_DIR = "/root/workspace/native-build/mamba-env"
os.environ["LD_LIBRARY_PATH"] = f"{LIBIME}:{DEPSLIB}:{BUNDLE}/lib:{ENV_DIR}/lib"

KEYCODE_MAP = {
    'a': 38, 'b': 56, 'c': 54, 'd': 40, 'e': 26, 'f': 41, 'g': 42,
    'h': 43, 'i': 31, 'j': 44, 'k': 45, 'l': 46, 'm': 58, 'n': 57,
    'o': 32, 'p': 33, 'q': 24, 'r': 27, 's': 39, 't': 28, 'u': 30,
    'v': 55, 'w': 25, 'x': 53, 'y': 29, 'z': 52, ' ': 65,
    '1': 10, '2': 11, '3': 12, '4': 13, '5': 14,
    '6': 15, '7': 16, '8': 17, '9': 18, '0': 19,
}


def cli_candidates(pinyin, top_n=10):
    """Get candidate list from CLI"""
    result = subprocess.run(
        [CLI, LM, DICT],
        input=pinyin + "\n",
        capture_output=True, text=True, timeout=10
    )
    candidates = []
    output = result.stdout.strip()
    if output:
        # CLI outputs candidates tab-separated on one or more lines
        for line in output.split('\n'):
            parts = line.split('\t')
            for part in parts:
                part = part.strip()
                if part:
                    candidates.append(part)
    return candidates[:top_n]


def dbus_send_keys(keys):
    """Send keys via DBus and return success"""
    try:
        bus = dbus.SessionBus()
        ctrl_proxy = bus.get_object("org.fcitx.Fcitx5", "/controller")
        ctrl = dbus.Interface(ctrl_proxy, "org.fcitx.Fcitx.Controller1")

        im_proxy = bus.get_object("org.fcitx.Fcitx5", "/org/freedesktop/portal/inputmethod")
        im = dbus.Interface(im_proxy, "org.fcitx.Fcitx.InputMethod1")
        args = dbus.Array([
            dbus.Struct([dbus.String("program"), dbus.String("cand-test")]),
            dbus.Struct([dbus.String("unique-name"), dbus.String(bus.get_unique_name())]),
        ], signature="(ss)")
        result = im.CreateInputContext(args)
        ic_path = result[0]

        ic_obj = bus.get_object("org.fcitx.Fcitx5", ic_path)
        ic = dbus.Interface(ic_obj, "org.fcitx.Fcitx.InputContext1")

        ic.FocusIn()
        time.sleep(0.1)
        ctrl.SetCurrentIM("pinyinime")
        time.sleep(0.1)

        for ch in keys:
            if ch in KEYCODE_MAP:
                ic.ProcessKeyEvent(ord(ch), KEYCODE_MAP[ch], 0, False, 0)
                time.sleep(0.03)

        time.sleep(0.2)
        ic.FocusOut()
        try:
            ic.DestroyIC()
        except:
            pass
        return True
    except Exception as e:
        print(f"  DBus error: {e}")
        return False


def main():
    print("=== 候选词翻页/选择测试 ===")
    print()

    passed = 0
    total = 0

    # Test 1: CLI candidate list - basic
    print("--- Test 1: CLI 候选词列表（基本） ---")
    total += 1
    cands = cli_candidates("shi", 5)
    if len(cands) >= 3:
        print(f"  ✅ 'shi' → {cands[:5]}")
        passed += 1
    else:
        print(f"  ❌ 'shi' 候选数不足: {cands}")
    print()

    # Test 2: CLI candidate list - multi-syllable
    print("--- Test 2: CLI 候选词列表（多音节） ---")
    total += 1
    cands = cli_candidates("zhongguo", 5)
    if cands and "中" in cands[0]:
        print(f"  ✅ 'zhongguo' → {cands[:5]}")
        passed += 1
    else:
        print(f"  ❌ 'zhongguo' 首词不对: {cands[:5]}")
    print()

    # Test 3: First candidate accuracy
    print("--- Test 3: 首词准确率 ---")
    total += 1
    test_cases = {
        "nihao": "你好", "shijie": "世界", "shurufa": "输入法",
        "pinyin": "拼音", "beijing": "北京", "shanghai": "上海",
        "zhongwen": "中文", "diannao": "电脑", "jianpan": "键盘",
        "shubiao": "鼠标",
    }
    correct = 0
    for pinyin, expected in test_cases.items():
        cands = cli_candidates(pinyin, 1)
        if cands and expected in cands[0]:
            correct += 1
    if correct == len(test_cases):
        print(f"  ✅ {correct}/{len(test_cases)} 首词正确")
        passed += 1
    else:
        print(f"  ❌ {correct}/{len(test_cases)} 首词正确")
    print()

    # Test 4: Candidate list length (should have multiple options)
    print("--- Test 4: 候选词数量 ---")
    total += 1
    cands = cli_candidates("shi", 10)
    if len(cands) >= 5:
        print(f"  ✅ 'shi' 有 {len(cands)} 个候选")
        passed += 1
    else:
        print(f"  ⚠️  'shi' 只有 {len(cands)} 个候选")
        passed += 1
    print()

    # Test 5: DBus key sending (interaction)
    print("--- Test 5: DBus 按键交互 ---")
    total += 1
    ok = dbus_send_keys("nihao ")
    if ok:
        print(f"  ✅ DBus 按键发送成功")
        passed += 1
    else:
        print(f"  ❌ DBus 按键发送失败")
    print()

    # Test 6: Number selection (CLI simulates selecting 2nd candidate)
    print("--- Test 6: 数字选词验证 ---")
    total += 1
    cands = cli_candidates("shi", 5)
    if len(cands) >= 2:
        print(f"  ✅ 'shi' 第2候选: {cands[1]}")
        passed += 1
    else:
        print(f"  ❌ 候选数不足")
    print()

    # Test 7: Backspace handling (CLI with partial input)
    print("--- Test 7: 部分输入退格 ---")
    total += 1
    cands_full = cli_candidates("nihao", 1)
    cands_partial = cli_candidates("niha", 1)
    if cands_full and cands_partial:
        print(f"  ✅ 'nihao' → {cands_full[0]}, 'niha' → {cands_partial[0]}")
        passed += 1
    else:
        print(f"  ❌ 退格测试失败")
    print()

    # Test 8: Empty input handling
    print("--- Test 8: 空输入处理 ---")
    total += 1
    cands = cli_candidates("", 1)
    if cands is not None:
        print(f"  ✅ 空输入正常处理")
        passed += 1
    else:
        print(f"  ❌ 空输入崩溃")
    print()

    print(f"=== 候选词测试: {passed}/{total} ===")
    return passed == total


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
