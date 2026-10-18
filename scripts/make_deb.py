#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_deb.py — 纯 Python 的 .deb 组装器（沙箱内无 dpkg-deb/dpkg 时使用）。

.deb 文件 = ar 归档，依次含三个成员：
    debian-binary     内容固定 "2.0\\n"
    control.tar.gz    ./control、./md5sums
    data.tar.gz       ./usr/...  安装树（rootfs 内容）

只依赖 Python 标准库；产物结构与 dpkg-deb 输出的常规 .deb 一致，
可被 dpkg/apt 正常安装（本机无 dpkg 时用 scripts/build_packages.sh
内置的校验逻辑检查结构与 md5sums 一致性）。

用法示例：
    python3 scripts/make_deb.py \
        --rootfs /tmp/stage-deb --output dist/pinyin-ime_0.9.103_arm64.deb \
        --name pinyin-ime --version 0.9.103 --arch arm64 \
        --depends "fcitx5" --short-desc "Linux pinyin input method based on libime"
"""
import argparse
import gzip
import hashlib
import io
import os
import stat
import sys
import tarfile

# 固定 mtime（秒），保证同一输入多次打包字节稳定；避开 0 以兼容个别挑剔的解包工具
FIXED_MTIME = 1700000000


def _tar_filter(ti: tarfile.TarInfo) -> tarfile.TarInfo:
    """统一 uid/gid/mtime，保证可复现打包。"""
    ti.uid = ti.gid = 0
    ti.uname = ti.gname = "root"
    ti.mtime = FIXED_MTIME
    return ti


def build_data_tar(rootfs: str) -> bytes:
    """把 rootfs 目录打包成 data.tar（未压缩 tar 字节）。"""
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w", format=tarfile.GNU_FORMAT) as tf:
        # 根目录条目 "./"
        root_ti = tarfile.TarInfo("./")
        root_ti.type = tarfile.DIRTYPE
        root_ti.mode = 0o755
        root_ti.uid = root_ti.gid = 0
        root_ti.uname = root_ti.gname = "root"
        root_ti.mtime = FIXED_MTIME
        tf.addfile(root_ti)

        for dirpath, dirnames, filenames in os.walk(rootfs):
            dirnames.sort()
            for name in sorted(dirnames):
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, rootfs)
                tf.add(full, arcname="./" + rel, recursive=False, filter=_tar_filter)
            for name in sorted(filenames):
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, rootfs)
                tf.add(full, arcname="./" + rel, recursive=False, filter=_tar_filter)
    return buf.getvalue()


def build_md5sums(rootfs: str) -> str:
    """生成 dpkg 风格 md5sums（路径不带 ./ 前缀，不含目录/符号链接）。"""
    lines = []
    for dirpath, dirnames, filenames in os.walk(rootfs):
        dirnames.sort()
        for name in sorted(filenames):
            full = os.path.join(dirpath, name)
            if os.path.islink(full) or not os.path.isfile(full):
                continue
            rel = os.path.relpath(full, rootfs)
            h = hashlib.md5()
            with open(full, "rb") as fp:
                for chunk in iter(lambda: fp.read(1 << 20), b""):
                    h.update(chunk)
            lines.append(f"{h.hexdigest()}  {rel}")
    return "\n".join(lines) + ("\n" if lines else "")


def build_control_tar(control: str, md5sums: str) -> bytes:
    """把 control / md5sums 打包成 control.tar（未压缩 tar 字节）。"""
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w", format=tarfile.GNU_FORMAT) as tf:
        for fname, data in (("./control", control.encode()), ("./md5sums", md5sums.encode())):
            ti = tarfile.TarInfo(fname)
            ti.size = len(data)
            ti.mode = 0o644
            ti.uid = ti.gid = 0
            ti.uname = ti.gname = "root"
            ti.mtime = FIXED_MTIME
            tf.addfile(ti, io.BytesIO(data))
    return buf.getvalue()


def gz(data: bytes) -> bytes:
    """gzip 压缩（mtime=0，可复现）。"""
    out = io.BytesIO()
    with gzip.GzipFile(fileobj=out, mode="wb", compresslevel=9, mtime=0) as gp:
        gp.write(data)
    return out.getvalue()


def ar_member(out, name: str, data: bytes) -> None:
    """写入一个 ar 成员（Debian 风格 60 字节头 + 数据，奇数长度补 \\n）。"""
    if len(name) > 16:
        raise ValueError(f"ar 成员名过长: {name}")
    header = (
        f"{name:<16}{FIXED_MTIME:<12}{'0':<6}{'0':<6}{'100644':<8}{len(data):<10}`\n"
    ).encode("ascii")
    out.write(header)
    out.write(data)
    if len(data) % 2:
        out.write(b"\n")


def make_control(args, installed_kb: int) -> str:
    """拼装 control 文件文本（字段顺序按 dpkg 习惯）。"""
    lines = [
        f"Package: {args.name}",
        f"Version: {args.version}",
        f"Architecture: {args.arch}",
        f"Maintainer: {args.maintainer}",
        f"Installed-Size: {installed_kb}",
    ]
    if args.depends:
        lines.append(f"Depends: {args.depends}")
    if args.recommends:
        lines.append(f"Recommends: {args.recommends}")
    lines += [
        f"Section: {args.section}",
        f"Priority: {args.priority}",
        f"Homepage: {args.homepage}",
        f"Description: {args.short_desc}",
    ]
    # 长描述：非空行加前导空格；空行写成 " ."
    for ln in args.long_desc.splitlines():
        lines.append(" " + ln if ln.strip() else " .")
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description="纯 Python .deb 组装器")
    ap.add_argument("--rootfs", required=True, help="安装树根目录（其中应有 usr/...）")
    ap.add_argument("--output", required=True, help="输出 .deb 文件路径")
    ap.add_argument("--name", required=True)
    ap.add_argument("--version", required=True)
    ap.add_argument("--arch", required=True, help="dpkg 架构名，如 arm64 / amd64")
    ap.add_argument("--maintainer", default="laotongtian <laotongtian@gitcode.com>")
    ap.add_argument("--depends", default="")
    ap.add_argument("--recommends", default="")
    ap.add_argument("--section", default="utils")
    ap.add_argument("--priority", default="optional")
    ap.add_argument("--homepage", default="https://gitcode.com/laotongtian/pinyin-ime")
    ap.add_argument("--short-desc", required=True)
    ap.add_argument("--long-desc", default="")
    args = ap.parse_args()

    rootfs = os.path.abspath(args.rootfs)
    if not os.path.isdir(rootfs):
        print(f"!! rootfs 不存在: {rootfs}", file=sys.stderr)
        return 1

    # Installed-Size = 文件字节数之和 / 1024（dpkg 惯例）
    total = 0
    nfiles = 0
    for dirpath, _dirnames, filenames in os.walk(rootfs):
        for name in filenames:
            full = os.path.join(dirpath, name)
            if os.path.isfile(full) and not os.path.islink(full):
                total += os.path.getsize(full)
                nfiles += 1
    installed_kb = (total + 1023) // 1024

    control = make_control(args, installed_kb)
    md5sums = build_md5sums(rootfs)
    data_tgz = gz(build_data_tar(rootfs))
    control_tgz = gz(build_control_tar(control, md5sums))

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "wb") as out:
        out.write(b"!<arch>\n")
        ar_member(out, "debian-binary", b"2.0\n")
        ar_member(out, "control.tar.gz", control_tgz)
        ar_member(out, "data.tar.gz", data_tgz)

    out_size = os.path.getsize(args.output)
    print(f"[make_deb] 已生成 {args.output}")
    print(f"[make_deb] 文件数 {nfiles}，安装后 {installed_kb} KiB，包大小 {out_size} 字节")
    return 0


if __name__ == "__main__":
    sys.exit(main())
