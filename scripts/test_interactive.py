#!/usr/bin/env python3
# 交互模式回归：用 pty 驱动 pinyin_ime_cli（stdin 必须是 TTY 才进交互模式），
# 逐键输入并断言中英切换、中文标点（含引号智能配对）等新功能。headless 沙箱无显示服务器，
# 本脚本是「真正能跑起来的输入法」交互行为的可回归验证手段。
#
# 用法：
#   python3 scripts/test_interactive.py <pinyin_ime_cli> <zh_CN.lm> <sc.dict>
#
# 说明：输入法会向 stdout 打印 ANSI 清屏/换行控制序列，本脚本只断言
# 关键子串是否按序出现，不校验控制序列本身。
import os
import pty
import select
import subprocess
import sys
import time


def drain(fd, buf_list, timeout=0.3):
    """非阻塞读取 master 端输出，累积到 buf_list（按字符串拼接）。"""
    end = time.time() + timeout
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if not r:
            continue
        try:
            data = os.read(fd, 4096)
        except OSError:
            break
        if not data:
            break
        buf_list.append(data)


def send(fd, text, delay=0.18):
    """向 master 端写入按键（子进程的 stdin 即 slave 端）。"""
    os.write(fd, text.encode("utf-8"))


def run_case(cli, lm, dct, script, expect, extra_args=None, ud=None):
    """script 是一段逐步执行的元组：('send', str) 或 ('sleep', sec)。
    expect 是必须按序出现（可跨步）的子串列表。
    extra_args 是传给 pinyin_ime_cli 的额外参数（如 --correct）。"""
    master, slave = pty.openpty()
    env = dict(os.environ)
    env["TERM"] = "xterm"
    # 每个用例独立用户目录，避免 learn() 交叉污染（否则候选排序漂移、烟测不稳）。
    argv = [cli, "--user-data", ud or tempfile.mkdtemp(prefix="pinyin-ime-ud."),
          lm, dct, "5"] + (extra_args or [])
    proc = subprocess.Popen(
        argv,
        stdin=slave, stdout=slave, stderr=slave,
        env=env, close_fds=True,
    )
    os.close(slave)

    output = []  # bytes 片段列表，最后 join
    for step, val in script:
        if step == "send":
            send(master, val)
        elif step == "sleep":
            time.sleep(val)
        drain(master, output)

    # 收尾：Ctrl-C 退出并落盘用户数据
    send(master, "\x03")
    drain(master, output, 1.0)
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.terminate()
    os.close(master)

    text = b"".join(output).decode("utf-8", "replace")
    pos = -1
    for e in expect:
        idx = text.find(e, pos + 1)
        if idx < 0:
            print(f"[FAIL] 未按序找到期望子串: {e!r}")
            print(f"      实际输出(截断): {text[-2000:]!r}")
            return False
        pos = idx
    print(f"[PASS] {expect!r}")
    return True


def main():
    if len(sys.argv) != 4:
        print("用法: test_interactive.py <pinyin_ime_cli> <zh_CN.lm> <sc.dict>", file=sys.stderr)
        return 2
    cli, lm, dct = sys.argv[1], sys.argv[2], sys.argv[3]
    import tempfile, shutil
    ud = tempfile.mkdtemp(prefix="pinyin-ime-interactive-ud.")
    try:
        return _run_all(cli, lm, dct, ud)
    finally:
        shutil.rmtree(ud, ignore_errors=True)


def _run_all(cli, lm, dct, ud):
    cases = [
        # 每项 = (按键脚本, 期望按序出现的子串列表)
        # ① 全拼整句 + 空格上屏
        ([("send", "nihao"), ("send", " "), ("sleep", 0.2)],
         ["「你好」"]),
        # ② 中英切换：Tab 进英文，回车整行上屏，Tab 切回中文
        ([("send", "\t"), ("send", "hello"), ("send", "\r"), ("send", "\t"), ("sleep", 0.2)],
         ["hello"]),
        # ③ 中文标点（空闲状态直接上屏）
        ([("send", ","), ("send", "."), ("send", "?"), ("sleep", 0.2)],
         ["，", "。", "？"]),
        # ④ 中文标点（打拼音中：先上屏首选，再出标点）
        ([("send", "zhongguo"), ("send", "!"), ("sleep", 0.2)],
         ["「中国」", "！"]),
        # ⑤ 引号智能配对（空闲：开“→闭”交替）
        ([("send", '"'), ("send", '"'), ("send", '"'), ("sleep", 0.2)],
         ["“", "”", "“"]),
        # ⑥ 引号智能配对（打拼音中：先上屏首选再出开引号）
        ([("send", "nihao"), ("send", '"'), ("sleep", 0.2)],
         ["「你好」", "“"]),
        # ⑦ 光标左右移动（多词往返：←/→ 按词边界移动候选光标后整句仍正确上屏）
        ([("send", "woshiyige"), ("send", "\x1b[D"), ("send", "\x1b[C"),
          ("send", " "), ("sleep", 0.2)],
         ["「我是一个」"]),
        # ⑧ 逐词分段（连按 ← 走到句首再整句上屏；方向键不被误当 Esc/标点/拼音）
        ([("send", "woshiyige"), ("send", "\x1b[D"), ("send", "\x1b[D"),
          ("send", "\x1b[D"), ("send", "\x1b[D"), ("send", " "),
          ("sleep", 0.2)],
         ["「我是一个」"]),
        # ⑨ Home：← 到词中（此时空格不致整句上屏），Home 跳回句首后空格整句上屏
        ([("send", "woshiyige"), ("send", "\x1b[D"), ("send", "\x1b[H"),
          ("send", " "), ("sleep", 0.2)],
         ["「我是一个」"]),
        # ⑩ End：← 到词中，End 跳回句尾后空格整句上屏
        ([("send", "woshiyige"), ("send", "\x1b[D"), ("send", "\x1b[F"),
          ("send", " "), ("sleep", 0.2)],
         ["「我是一个」"]),
        # ⑪ 单引号智能配对（空闲：开‘→闭’交替）
        ([("send", "'"), ("send", "'"), ("send", "'"), ("sleep", 0.2)],
         ["‘", "’", "‘"]),
        # ⑫ 单引号作为隔音符（打拼音中 ' 仍是隔音符，xi'an → 西安）
        ([("send", "xi'an"), ("send", " "), ("sleep", 0.2)],
         ["「西安」"]),
        # ⑬ 表情：Ctrl-E 进入表情选择，按 1 选第一个表情 😀，选完自动退回中文
        ([("send", "\x05"), ("send", "1"), ("sleep", 0.2)],
         ["😀"]),
        # ⑭ 表情翻页：Ctrl-E 进入，. 翻到第 2 页，按 1 选 👍
        ([("send", "\x05"), ("send", "."), ("send", "1"), ("sleep", 0.2)],
         ["👍"]),
        # ⑮ 交互态纠错（--correct）：senme 打错 → 上屏「什么」（无 --correct 会是「森么」）
        ([("send", "senme"), ("send", " "), ("sleep", 0.3)],
         ["「什么」"], ["--correct"]),
        # ⑯ 交互态纠错（--correct）：zongguo 打错 → 上屏「中国」（无纠正会是「宗国」）
        ([("send", "zongguo"), ("send", " "), ("sleep", 0.3)],
         ["「中国」"], ["--correct"]),
        # ⑰ 交互态纠错不误伤简拼（--correct）：zg 首字母输入 → 仍「中国」
        ([("send", "zg"), ("send", " "), ("sleep", 0.3)],
         ["「中国」"], ["--correct"]),
    ]

    ok = 0
    for item in cases:
        script, expect = item[0], item[1]
        extra = item[2] if len(item) > 2 else None
        if run_case(cli, lm, dct, list(script), list(expect), extra, ud):
            ok += 1
    total = len(cases)
    print(f"==== 交互回归：{ok}/{total} 通过 ====")
    return 0 if ok == total else 1


if __name__ == "__main__":
    sys.exit(main())