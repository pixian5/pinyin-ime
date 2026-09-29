#!/usr/bin/env python3
"""用户词库同步工具 — 导出/导入/合并用户学习数据

用法:
  # 导出用户词库到文件
  python3 scripts/sync_user_data.py export --user-data ~/.local/share/pinyin-ime -o backup.json

  # 导入用户词库（合并到现有数据）
  python3 scripts/sync_user_data.py import -i backup.json --user-data ~/.local/share/pinyin-ime

  # 合并两个用户词库
  python3 scripts/sync_user_data.py merge --from dir1 --to dir2

  # 同步到远程（rclone/S3）
  python3 scripts/sync_user_data.py push --user-data ~/.local/share/pinyin-ime --remote s3:bucket/pinyin-ime
  python3 scripts/sync_user_data.py pull --user-data ~/.local/share/pinyin-ime --remote s3:bucket/pinyin-ime

格式：JSON，包含 user.history 和 user.dict 的 base64 编码内容，以及元数据。
"""
import argparse
import base64
import json
import os
import subprocess
import sys
import time

def read_user_data(user_dir):
    """读取用户数据目录（二进制安全）"""
    data = {"history_b64": "", "dict_b64": "", "meta": {}}
    hist_path = os.path.join(user_dir, "user.history")
    dict_path = os.path.join(user_dir, "user.dict")
    hist_size = 0
    dict_size = 0
    if os.path.exists(hist_path):
        with open(hist_path, 'rb') as f:
            raw = f.read()
            data["history_b64"] = base64.b64encode(raw).decode('ascii')
            hist_size = len(raw)
    if os.path.exists(dict_path):
        with open(dict_path, 'rb') as f:
            raw = f.read()
            data["dict_b64"] = base64.b64encode(raw).decode('ascii')
            dict_size = len(raw)
    data["meta"] = {
        "exported_at": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "history_size": hist_size,
        "dict_size": dict_size,
    }
    return data

def write_user_data(user_dir, data, merge=False):
    """写入用户数据目录（二进制安全）"""
    os.makedirs(user_dir, exist_ok=True)
    hist_path = os.path.join(user_dir, "user.history")
    dict_path = os.path.join(user_dir, "user.dict")

    if merge:
        # 合并模式：直接追加（libime 的历史文件支持追加）
        for path, key in [(hist_path, "history_b64"), (dict_path, "dict_b64")]:
            if not data[key]:
                continue
            new_data = base64.b64decode(data[key])
            existing_size = os.path.getsize(path) if os.path.exists(path) else 0
            with open(path, 'ab') as f:
                f.write(new_data)
            print(f"  {key}: 追加 {len(new_data)} 字节（原有 {existing_size} 字节）", file=sys.stderr)
    else:
        # 覆盖模式
        for path, key in [(hist_path, "history_b64"), (dict_path, "dict_b64")]:
            raw = base64.b64decode(data[key]) if data[key] else b''
            with open(path, 'wb') as f:
                f.write(raw)
        print(f"  history: {data['meta']['history_size']} 字节", file=sys.stderr)
        print(f"  dict: {data['meta']['dict_size']} 字节", file=sys.stderr)

def cmd_export(args):
    data = read_user_data(args.user_data)
    with open(args.output, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    print(f"导出完成: {args.output}", file=sys.stderr)
    print(f"  history: {data['meta']['history_size']} 字节", file=sys.stderr)
    print(f"  dict: {data['meta']['dict_size']} 字节", file=sys.stderr)

def cmd_import(args):
    with open(args.input, 'r', encoding='utf-8') as f:
        data = json.load(f)
    write_user_data(args.user_data, data, merge=args.merge)
    print(f"导入完成: {args.user_data}（{'合并' if args.merge else '覆盖'}模式）", file=sys.stderr)

def cmd_merge(args):
    data = read_user_data(args.from_dir)
    write_user_data(args.to_dir, data, merge=True)
    print(f"合并完成: {args.from_dir} → {args.to_dir}", file=sys.stderr)

def cmd_push(args):
    """推送到远程（rclone）"""
    data = read_user_data(args.user_data)
    tmp = "/tmp/pinyin-ime-sync.json"
    with open(tmp, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False)
    subprocess.run(["rclone", "copy", tmp, args.remote], check=True)
    os.unlink(tmp)
    print(f"推送完成: {args.user_data} → {args.remote}", file=sys.stderr)

def cmd_pull(args):
    """从远程拉取（rclone）"""
    tmp = "/tmp/pinyin-ime-sync.json"
    subprocess.run(["rclone", "copy", args.remote, os.path.dirname(tmp)], check=True)
    with open(tmp, 'r', encoding='utf-8') as f:
        data = json.load(f)
    write_user_data(args.user_data, data, merge=args.merge)
    os.unlink(tmp)
    print(f"拉取完成: {args.remote} → {args.user_data}", file=sys.stderr)

def main():
    parser = argparse.ArgumentParser(description="用户词库同步工具")
    sub = parser.add_subparsers(dest="command", required=True)

    p_export = sub.add_parser("export", help="导出用户词库")
    p_export.add_argument("--user-data", default=os.path.expanduser("~/.local/share/pinyin-ime"))
    p_export.add_argument("-o", "--output", required=True)
    p_export.set_defaults(func=cmd_export)

    p_import = sub.add_parser("import", help="导入用户词库")
    p_import.add_argument("-i", "--input", required=True)
    p_import.add_argument("--user-data", default=os.path.expanduser("~/.local/share/pinyin-ime"))
    p_import.add_argument("--merge", action="store_true", help="合并而非覆盖")
    p_import.set_defaults(func=cmd_import)

    p_merge = sub.add_parser("merge", help="合并两个用户词库")
    p_merge.add_argument("--from", dest="from_dir", required=True)
    p_merge.add_argument("--to", dest="to_dir", required=True)
    p_merge.set_defaults(func=cmd_merge)

    p_push = sub.add_parser("push", help="推送到远程（rclone）")
    p_push.add_argument("--user-data", default=os.path.expanduser("~/.local/share/pinyin-ime"))
    p_push.add_argument("--remote", required=True)
    p_push.set_defaults(func=cmd_push)

    p_pull = sub.add_parser("pull", help="从远程拉取（rclone）")
    p_pull.add_argument("--user-data", default=os.path.expanduser("~/.local/share/pinyin-ime"))
    p_pull.add_argument("--remote", required=True)
    p_pull.add_argument("--merge", action="store_true", help="合并而非覆盖")
    p_pull.set_defaults(func=cmd_pull)

    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()
