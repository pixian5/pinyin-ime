Name:           pinyin-ime
Version:        0.9.70
Release:        1.oe2203
Summary:        Pinyin input method for Linux based on libime engine

License:        MIT
URL:            https://gitcode.com/laotongtian/pinyin-ime
Source0:        %{name}-%{version}.tar.gz

BuildArch:      aarch64
Requires:       fcitx5 >= 5.1
Requires:       libime >= 1.1

%description
pinyin-ime is a Chinese pinyin input method for Linux, built on the
libime engine with fcitx5 framework integration.

%prep
%setup -q

%install
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/lib/fcitx5
mkdir -p %{buildroot}/usr/share/libime
mkdir -p %{buildroot}/usr/share/fcitx5/addon
mkdir -p %{buildroot}/usr/share/fcitx5/conf

install -m755 bin/pinyin_ime_cli %{buildroot}/usr/bin/pinyin_ime_cli
install -m755 lib/fcitx5/libpinyinime.so %{buildroot}/usr/lib/fcitx5/libpinyinime.so
install -m644 share/libime/zh_CN.lm %{buildroot}/usr/share/libime/zh_CN.lm
install -m644 share/libime/sc.dict %{buildroot}/usr/share/libime/sc.dict
install -m644 share/fcitx5/addon/pinyinime.conf %{buildroot}/usr/share/fcitx5/addon/pinyinime.conf
install -m644 share/fcitx5/conf/pinyinime.conf %{buildroot}/usr/share/fcitx5/conf/pinyinime.conf

%files
%defattr(-,root,root,-)
/usr/bin/pinyin_ime_cli
/usr/lib/fcitx5/libpinyinime.so
/usr/share/libime/zh_CN.lm
/usr/share/libime/sc.dict
/usr/share/fcitx5/addon/pinyinime.conf
/usr/share/fcitx5/conf/pinyinime.conf

%post
ldconfig

%postun
ldconfig

%changelog
* Fri Oct 09 2026 pinyin-ime <pinyin-ime@noreply.gitcode.com> - 0.9.70-1
- v0.9.70: E2E 315/315, full build from source
- CLI + fcitx5 addon + LM + dict
