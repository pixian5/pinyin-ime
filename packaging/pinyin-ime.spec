Name:           pinyin-ime
Version:        0.9.4
Release:        1
Summary:        Linux pinyin input method based on libime
License:        LGPL-2.1-or-later
URL:            https://gitcode.com/laotongtian/pinyin-ime
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
Requires:       libime
Requires:       fcitx5

%description
pinyin-ime is a Linux pinyin input method engine based on libime.
It provides both a command-line interface (pinyin_ime_cli) and a
Fcitx5 addon (libpinyinime.so) for system-level input.

Features:
- Full pinyin, simplified pinyin, fuzzy pinyin
- Double pinyin (8 schemes: Xiaohe/Ziranma/MS/Ziguang/ABC/...)
- User language model with self-learning
- Chinese/English switching, Chinese punctuation
- Emoji input, cursor movement, undo
- M3 correction module (CHIME + bigram transition)

%prep
%setup -q

%build
mkdir -p cpp/build
cd cpp/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=%{_prefix}
make %{?_smp_mflags}

# Build fcitx5 addon
cd ../../fcitx5-addon
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=%{_prefix}
make %{?_smp_mflags}

%install
# CLI
install -Dm755 %{_builddir}/%{name}-%{version}/cpp/build/pinyin_ime_cli \
    %{buildroot}%{_bindir}/pinyin_ime_cli

# fcitx5 addon
install -Dm755 %{_builddir}/%{name}-%{version}/fcitx5-addon/build/libpinyinime.so \
    %{buildroot}%{_libdir}/fcitx5/libpinyinime.so
install -Dm644 %{_builddir}/%{name}-%{version}/fcitx5-addon/build/pinyinime.conf \
    %{buildroot}%{_datadir}/fcitx5/addon/pinyinime.conf

# Scripts
install -Dm755 scripts/benchmark.py \
    %{buildroot}%{_datadir}/pinyin-ime/scripts/benchmark.py
install -Dm755 scripts/user_dict.py \
    %{buildroot}%{_datadir}/pinyin-ime/scripts/user_dict.py
install -Dm755 scripts/user_dict_import.py \
    %{buildroot}%{_datadir}/pinyin-ime/scripts/user_dict_import.py
install -Dm755 scripts/ci_test.sh \
    %{buildroot}%{_datadir}/pinyin-ime/scripts/ci_test.sh

# Docs
install -Dm644 docs/QUICKSTART.md \
    %{buildroot}%{_docdir}/%{name}/QUICKSTART.md
install -Dm644 docs/FEATURES.md \
    %{buildroot}%{_docdir}/%{name}/FEATURES.md
install -Dm644 docs/EVALUATION.md \
    %{buildroot}%{_docdir}/%{name}/EVALUATION.md

# Config
install -Dm644 /dev/null %{buildroot}%{_sysconfdir}/pinyin-ime/config

%files
%{_bindir}/pinyin_ime_cli
%{_libdir}/fcitx5/libpinyinime.so
%{_datadir}/fcitx5/addon/pinyinime.conf
%{_datadir}/pinyin-ime/scripts/
%{_docdir}/%{name}/
%config(noreplace) %{_sysconfdir}/pinyin-ime/config

%changelog
* Mon Sep 30 2026 laotongtian <laotongtian@gitcode.com> - 0.9.4-1
- v0.9.4: CI测试(24/24), 安装脚本, 打包配置, 功能对比文档
- E2E fcitx5 addon测试 38/40通过
- 性能基准: 168句/秒, 97.5%日常准确率
- 新增: 词典导入工具, GitCode CI配置, 快速上手指南, 评测报告
* Mon Sep 30 2026 laotongtian <laotongtian@gitcode.com> - 0.9.3-1
- v0.9.3: 常用词频提升, 版本统一, RPM打包
- 日常40句准确率从87.5%提升至97.5%
* Sat Sep 28 2026 laotongtian <laotongtian@gitcode.com> - 0.9.0-1
- 初始版本: 基于libime的拼音输入法
