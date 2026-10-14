Name:           pinyin-ime
Version:        0.9.87
Release:        1.oe2203
Summary:        Pinyin input method for Linux based on libime engine

License:        MIT
URL:            https://gitcode.com/laotongtian/pinyin-ime
Source0:        %{name}-%{version}.tar.gz

BuildArch:      aarch64
Requires:       fcitx5 >= 5.1
Requires:       libstdc++ >= 13

%description
pinyin-ime is a Chinese pinyin input method for Linux, built on the
libime engine with fcitx5 framework integration.

Features:
- Full pinyin input with sentence-level decoding
- Abbreviation input (bjdx → 北京大学)
- Space-separated word input (nihao shijie → 你好世界)
- Chinese punctuation auto-conversion (. → 。)
- Fuzzy pinyin (z/zh, n/l, an/ang)
- Double pinyin (xiaohe, ziranma, etc.)
- Pinyin error correction
- User dictionary and learning
- Association prediction

%prep
%setup -q

%install
# Directories
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/lib64/fcitx5
mkdir -p %{buildroot}/usr/lib64
mkdir -p %{buildroot}/usr/share/libime/libime
mkdir -p %{buildroot}/usr/share/fcitx5/addon
mkdir -p %{buildroot}/usr/share/fcitx5/conf
mkdir -p %{buildroot}/usr/share/fcitx5/inputmethod

# CLI binary
install -m755 pinyin_ime_cli %{buildroot}/usr/bin/pinyin_ime_cli

# fcitx5 addon
install -m755 dist/libpinyinime.so %{buildroot}/usr/lib64/fcitx5/libpinyinime.so

# libIME libraries (bundled, built with GCC 13)
install -m755 pinyin-ime-s3/v0.9.84a/pinyin-ime-v0.9.84a/lib/libIMEPinyin.so.1.1.17 %{buildroot}/usr/lib64/
install -m755 pinyin-ime-s3/v0.9.84a/pinyin-ime-v0.9.84a/lib/libIMECore.so.1.1.17 %{buildroot}/usr/lib64/
install -m755 pinyin-ime-s3/v0.9.84a/pinyin-ime-v0.9.84a/lib/libIMETable.so.1.1.17 %{buildroot}/usr/lib64/
ln -sf libIMEPinyin.so.1.1.17 %{buildroot}/usr/lib64/libIMEPinyin.so
ln -sf libIMEPinyin.so.1.1.17 %{buildroot}/usr/lib64/libIMEPinyin.so.0
ln -sf libIMECore.so.1.1.17 %{buildroot}/usr/lib64/libIMECore.so
ln -sf libIMECore.so.1.1.17 %{buildroot}/usr/lib64/libIMECore.so.0
ln -sf libIMETable.so.1.1.17 %{buildroot}/usr/lib64/libIMETable.so
ln -sf libIMETable.so.1.1.17 %{buildroot}/usr/lib64/libIMETable.so.0

# Data files
install -m644 pinyin-ime-s3/v0.9.84a/pinyin-ime-v0.9.84a/data/zh_CN.lm %{buildroot}/usr/share/libime/libime/
install -m644 pinyin-ime-s3/v0.9.84a/pinyin-ime-v0.9.84a/data/sc.dict %{buildroot}/usr/share/libime/libime/

# Prediction model (if exists)
if [ -f /usr/share/libime/libime/zh_CN.lm.predict ]; then
    install -m644 /usr/share/libime/libime/zh_CN.lm.predict %{buildroot}/usr/share/libime/libime/
fi

# Config files
install -m644 pinyin-ime-s3/v0.9.84a/pinyin-ime-v0.9.84a/addon/pinyinime.conf %{buildroot}/usr/share/fcitx5/addon/pinyinime.conf
install -m644 fcitx5-addon/conf/pinyinime.conf %{buildroot}/usr/share/fcitx5/conf/pinyinime.conf
install -m644 dist/pinyinime.conf %{buildroot}/usr/share/fcitx5/inputmethod/pinyinime.conf

%files
%defattr(-,root,root,-)
/usr/bin/pinyin_ime_cli
/usr/lib64/fcitx5/libpinyinime.so
/usr/lib64/libIMEPinyin.so*
/usr/lib64/libIMECore.so*
/usr/lib64/libIMETable.so*
/usr/share/libime/libime/zh_CN.lm
/usr/share/libime/libime/sc.dict
%if %{exists /usr/share/libime/libime/zh_CN.lm.predict}
/usr/share/libime/libime/zh_CN.lm.predict
%endif
/usr/share/fcitx5/addon/pinyinime.conf
/usr/share/fcitx5/conf/pinyinime.conf
/usr/share/fcitx5/inputmethod/pinyinime.conf

%post
ldconfig

%postun
ldconfig

%changelog
* Tue Oct 14 2026 pinyin-ime <pinyin-ime@noreply.gitcode.com> - 0.9.87-1
- v0.9.87: 中文标点输入 + 版本号修正
- v0.9.86: 空格分隔输入支持
- v0.9.85: CLI 重新编译 + 系统级安装
- 22/22 测试全部通过
