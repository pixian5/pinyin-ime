# pinyin-ime RPM 规格文件（由 scripts/build_packages.sh 使用）
#
# 构建方式（沙箱或普通 openEuler/Fedora 机器均可）:
#   bash scripts/build_packages.sh rpm
#
# 说明:
#   - Source0 由 build_packages.sh 现场打包（安装树 rootfs: usr/bin、usr/lib64、
#     usr/share/...），spec 本身不含构建步骤，只做装配。
#   - 数据文件（zh_CN.lm/sc.dict）随包安装到 /usr/share/libime，开箱即用；
#     也可由 ~/.config/fcitx5/conf/pinyinime.conf 的 DataDir 覆盖。
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
Requires:       libime
Requires:       libstdc++
Requires:       libgcc

%description
pinyin-ime 是基于 libime 引擎的 Linux 拼音输入法，以 fcitx5 addon 形式提供：

- 全拼 / 简拼 / 整句解码（libime 语言模型）
- 拼写纠错（M3）、模糊音按对开关、双拼（8 种方案）
- 用户词典与自学习、联想预测、中文标点自动转换

本包安装：fcitx5 addon（libpinyinime.so）、输入法描述、配置模板、
CLI 测试工具 pinyin_ime_cli，以及开箱即用的 zh_CN.lm / sc.dict。

数据文件安装到 /usr/share/libime，addon 默认从该标准路径加载；也可在
~/.config/fcitx5/conf/pinyinime.conf 中用 DataDir 覆盖。

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
/usr/share/fcitx5/inputmethod/pinyinime.inputmethod
/usr/share/libime/zh_CN.lm
/usr/share/libime/sc.dict
/usr/share/doc/pinyin-ime/README.md

%changelog
* Mon Oct 19 2026 pinyin-ime team <pinyin-ime@gitcode.com> - @VERSION@-1
- 打包 fcitx5 addon + CLI + 配置模板 + 输入法描述 + LM/dict
- 修复 addon 配置：Library=libpinyinime、依赖 core 版本与运行时一致
