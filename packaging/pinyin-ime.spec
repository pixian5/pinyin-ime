# pinyin-ime RPM 规格文件（由 scripts/build_packages.sh 使用）
#
# 构建方式（沙箱或普通 openEuler/Fedora 机器均可）:
#   bash scripts/build_packages.sh rpm
#
# 说明:
#   - Source0 由 build_packages.sh 现场打包（安装树 rootfs: usr/bin、usr/lib64、
#     usr/share/...），spec 本身不含构建步骤，只做装配。
#   - 数据文件（zh_CN.lm/sc.dict）不随包分发：addon 自动搜索 libime 数据包的
#     标准路径（/usr/share/libime、/usr/lib64/libime 等），或由 DataDir 指定。
#   - @VERSION@ 由 build_packages.sh 用 VERSION 文件内容替换。
Name:           pinyin-ime
Version:        @VERSION@
Release:        1
Summary:        Pinyin input method for fcitx5 based on libime
License:        LGPL-2.1-or-later
URL:            https://gitcode.com/laotongtian/pinyin-ime
Source0:        %{name}-%{version}.tar.gz

BuildArch:      aarch64

Requires:       fcitx5 >= 5.1

%description
pinyin-ime 是基于 libime 引擎的 Linux 拼音输入法，以 fcitx5 addon 形式提供：

- 全拼 / 简拼 / 整句解码（libime 语言模型）
- 拼写纠错（M3）、模糊音按对开关、双拼（8 种方案）
- 用户词典与自学习、联想预测、中文标点自动转换

本包安装：fcitx5 addon（libpinyinime.so）、addon 描述与配置模板、
CLI 测试工具 pinyin_ime_cli。

数据文件（zh_CN.lm / sc.dict）不随本包分发：addon 会自动搜索发行版
libime 数据包的标准安装路径（/usr/share/libime、/usr/lib64/libime、
/usr/lib/aarch64-linux-gnu/libime 等）；如系统未提供，可安装 libime
数据包或在 ~/.config/fcitx5/conf/pinyinime.conf 中设置 DataDir。

%prep
%setup -q

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
cp -a usr %{buildroot}/usr

%files
%defattr(-,root,root,-)
/usr/bin/pinyin_ime_cli
/usr/lib64/fcitx5/libpinyinime.so
/usr/share/fcitx5/addon/pinyinime.conf
/usr/share/fcitx5/conf/pinyinime.conf
/usr/share/doc/pinyin-ime/README.md

%changelog
* Mon Oct 19 2026 pinyin-ime team <pinyin-ime@gitcode.com> - @VERSION@-1
- 打包 fcitx5 addon + CLI + 配置模板（v0.9.103：GTK/Qt immodule e2e 通过、全新打包链路）
