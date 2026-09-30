Name:           pinyin-ime
Version:        0.9.3
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

# Config
install -Dm644 /dev/null %{buildroot}%{_sysconfdir}/pinyin-ime/config

%files
%{_bindir}/pinyin_ime_cli
%{_libdir}/fcitx5/libpinyinime.so
%{_datadir}/fcitx5/addon/pinyinime.conf
%config(noreplace) %{_sysconfdir}/pinyin-ime/config

%changelog
* Mon Sep 30 2026 laotongtian <laotongtian@gitcode.com> - 0.9.3-1
- v0.8.7: CLI path fix, install script, domain LM training
- v0.8.6: full corpus training, multi-domain evaluation
- v0.8.5: user LM training, evaluation framework
