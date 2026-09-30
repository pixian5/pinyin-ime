#!/usr/bin/env python3
"""
pinyin-ime 用户词典管理工具
用法:
  python3 scripts/user_dict.py add <拼音> <中文>     添加用户词条
  python3 scripts/user_dict.py list                   列出所有用户词条
  python3 scripts/user_dict.py search <关键词>        搜索用户词条
  python3 scripts/user_dict.py import <文件>          从文件批量导入
  python3 scripts/user_dict.py export <文件>          导出到文件
  python3 scripts/user_dict.py clear                  清空用户词典
  python3 scripts/user_dict.py stats                  显示统计信息
"""
import subprocess
import sys
import os
import struct

CLI = os.path.join(os.path.dirname(__file__), '..', 'cpp', 'build', 'pinyin_ime_cli')
LD = "/root/workspace/native-build/mamba-env/lib:/root/workspace/native-build/opt/fcitx5/lib:/root/workspace/native-build/opt/libime/lib"
os.environ['LD_LIBRARY_PATH'] = LD + ':' + os.environ.get('LD_LIBRARY_PATH', '')

USER_DATA = os.path.expanduser("~/.local/share/pinyin-ime")
USER_DICT = os.path.join(USER_DATA, "user.dict")
USER_HIST = os.path.join(USER_DATA, "user.history")

def run_cli(args, input_text=None):
    cmd = [CLI] + args
    r = subprocess.run(cmd, input=input_text, capture_output=True, text=True, timeout=30)
    return r.stdout.strip(), r.stderr.strip()

def cmd_add(pinyin, chinese):
    """添加用户词条"""
    learn_input = f"{pinyin}\t{chinese}\n"
    out, err = run_cli(["--learn-file", "/dev/stdin", "--quiet"], learn_input)
    print(f"学习结果: {out}")
    if err:
        print(f"信息: {err}")

def cmd_list():
    """列出所有用户词条"""
    out, err = run_cli(["--dump-user", "--quiet"])
    if out:
        print("=== 用户学习历史 ===")
        print(out)
    else:
        print("（无用户词条）")

def cmd_search(keyword):
    """搜索用户词条"""
    out, err = run_cli(["--dump-user", "--quiet"])
    if not out:
        print("（无用户词条）")
        return
    matches = [line for line in out.split('\n') if keyword in line]
    if matches:
        print(f"=== 搜索 '{keyword}' ===")
        for m in matches:
            print(f"  {m}")
    else:
        print(f"未找到匹配 '{keyword}' 的词条")

def cmd_import(filepath):
    """从文件批量导入"""
    if not os.path.exists(filepath):
        print(f"文件不存在: {filepath}")
        return
    out, err = run_cli(["--learn-file", filepath, "--quiet"])
    print(f"导入结果: {out}")
    if err:
        print(f"信息: {err}")

def cmd_export(filepath):
    """导出到文件"""
    out, err = run_cli(["--dump-user", "--quiet"])
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(out + '\n')
    lines = out.strip().split('\n') if out.strip() else []
    print(f"已导出 {len(lines)} 条到 {filepath}")

def cmd_clear():
    """清空用户词典"""
    for f in [USER_DICT, USER_HIST]:
        if os.path.exists(f):
            os.remove(f)
            print(f"已删除: {f}")
    print("用户词典已清空")

def cmd_stats():
    """显示统计信息"""
    print("=== 用户数据统计 ===")
    for name, path in [("用户词典", USER_DICT), ("学习历史", USER_HIST)]:
        if os.path.exists(path):
            size = os.path.getsize(path)
            with open(path, 'rb') as f:
                lines = f.read().count(b'\n')
            print(f"  {name}: {path}")
            print(f"    大小: {size} 字节, {lines} 行")
        else:
            print(f"  {name}: 不存在")
    
    # 引擎统计
    out, err = run_cli(["--stats", "--quiet"])
    if out:
        print(f"\n=== 引擎状态 ===")
        print(out)

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    
    cmd = sys.argv[1]
    
    if cmd == "add" and len(sys.argv) == 4:
        cmd_add(sys.argv[2], sys.argv[3])
    elif cmd == "list":
        cmd_list()
    elif cmd == "search" and len(sys.argv) == 3:
        cmd_search(sys.argv[2])
    elif cmd == "import" and len(sys.argv) == 3:
        cmd_import(sys.argv[2])
    elif cmd == "export" and len(sys.argv) == 3:
        cmd_export(sys.argv[2])
    elif cmd == "clear":
        cmd_clear()
    elif cmd == "stats":
        cmd_stats()
    else:
        print(__doc__)

if __name__ == "__main__":
    main()
