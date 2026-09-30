#!/usr/bin/env python3
"""
pinyin-ime 用户词典导入工具
支持从多种格式导入自定义词条到用户词典

用法:
  python3 user_dict_import.py --import-fcitx5 ~/.local/share/fcitx5/pinyin/user.dict
  python3 user_dict_import.py --import-text words.txt
  python3 user_dict_import.py --export user_dict.txt
  python3 user_dict_import.py --list
"""

import argparse
import os
import sys
import struct
from pathlib import Path

USER_DATA_DIR = Path.home() / ".local" / "share" / "pinyin-ime"
USER_DICT = USER_DATA_DIR / "user.dict"
USER_HISTORY = USER_DATA_DIR / "user.history"

def ensure_dir():
    USER_DATA_DIR.mkdir(parents=True, exist_ok=True)

def import_text_file(filepath):
    """从文本文件导入词条，格式：拼音\t中文\n 或 拼音 中文\n"""
    count = 0
    entries = []
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            # 支持制表符或空格分隔
            parts = line.split('\t') if '\t' in line else line.split(None, 1)
            if len(parts) >= 2:
                pinyin = parts[0].lower()
                chinese = parts[1]
                entries.append((pinyin, chinese))
                count += 1
            elif len(parts) == 1:
                # 只有中文，尝试自动推导拼音
                chinese = parts[0]
                if len(chinese) <= 4:
                    entries.append(("", chinese))
                    count += 1
    return entries

def import_fcitx5_dict(filepath):
    """从 fcitx5-pinyin 用户词典导入"""
    entries = []
    try:
        with open(filepath, 'rb') as f:
            data = f.read()
        # fcitx5-pinyin 词典格式：二进制格式
        # 尝试解析（格式可能因版本而异）
        # 这里使用简单的方法：提取可识别的 UTF-8 中文字符串
        i = 0
        while i < len(data):
            # 查找 UTF-8 中文字符序列
            if data[i] >= 0xE4 and data[i] <= 0xE9 and i + 2 < len(data):
                # 可能是中文字符（UTF-8 三字节）
                start = i
                while i + 2 < len(data) and data[i] >= 0xE4 and data[i] <= 0xE9:
                    i += 3
                chinese = data[start:i].decode('utf-8', errors='ignore')
                if len(chinese) >= 1 and len(chinese) <= 8:
                    entries.append(("", chinese))
            else:
                i += 1
    except Exception as e:
        print(f"解析 fcitx5 词典失败: {e}", file=sys.stderr)
    return entries

def write_learn_format(entries, output_file):
    """写入 pinyin_ime_cli --learn-file 格式"""
    with open(output_file, 'w', encoding='utf-8') as f:
        for pinyin, chinese in entries:
            if pinyin:
                f.write(f"{pinyin}\t{chinese}\n")
            else:
                f.write(f"{chinese}\n")
    return len(entries)

def export_user_dict(output_file):
    """导出当前用户词典为文本格式"""
    if not USER_DICT.exists():
        print("用户词典不存在", file=sys.stderr)
        return 0
    # 用户词典是二进制格式，直接复制
    import shutil
    shutil.copy2(USER_DICT, output_file)
    return 1

def list_user_data():
    """列出用户数据信息"""
    print(f"用户数据目录: {USER_DATA_DIR}")
    print()
    for f in sorted(USER_DATA_DIR.iterdir()):
        size = f.stat().st_size
        if size < 1024:
            size_str = f"{size} B"
        elif size < 1024 * 1024:
            size_str = f"{size/1024:.1f} KB"
        else:
            size_str = f"{size/1024/1024:.1f} MB"
        print(f"  {f.name:30s} {size_str:>10s}")

    # 统计历史
    if USER_HISTORY.exists():
        size = USER_HISTORY.stat().st_size
        print(f"\n  历史记录: {size} 字节 (二进制格式)")

def main():
    parser = argparse.ArgumentParser(description='pinyin-ime 用户词典导入工具')
    parser.add_argument('--import-text', metavar='FILE',
                        help='从文本文件导入 (格式: 拼音\\t中文)')
    parser.add_argument('--import-fcitx5', metavar='FILE',
                        help='从 fcitx5-pinyin 用户词典导入')
    parser.add_argument('--export', metavar='FILE',
                        help='导出用户词典')
    parser.add_argument('--list', action='store_true',
                        help='列出用户数据')
    parser.add_argument('--output', '-o', default=str(USER_DATA_DIR / 'imported.dict'),
                        help='输出文件路径')
    args = parser.parse_args()

    ensure_dir()

    if args.list:
        list_user_data()
        return

    if args.export:
        count = export_user_dict(args.export)
        print(f"已导出 {count} 个文件到 {args.export}")
        return

    entries = []
    if args.import_text:
        entries = import_text_file(args.import_text)
        print(f"从文本文件导入: {len(entries)} 条")
    elif args.import_fcitx5:
        entries = import_fcitx5_dict(args.import_fcitx5)
        print(f"从 fcitx5 词典导入: {len(entries)} 条")

    if entries:
        count = write_learn_format(entries, args.output)
        print(f"已写入 {count} 条到 {args.output}")
        print(f"使用方法: pinyin_ime_cli --learn-file {args.output}")
        print(f"或训练: pinyin_ime_cli --train-text {args.output}")
    else:
        if not args.list and not args.export:
            parser.print_help()

if __name__ == '__main__':
    main()
