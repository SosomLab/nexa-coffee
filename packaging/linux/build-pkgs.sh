#!/bin/sh
# build-pkgs.sh <정적 바이너리> <버전> <출력 폴더> — 이미 빌드된 musl 정적 바이너리를 .deb · .rpm으로 포장한다(빌드 안 함).
#   산출: nexa-coffee_<v>_amd64.deb · nexa-coffee-<v>-1.x86_64.rpm (이름 규칙 = SosomLab/linux-repo apps/nexa-coffee.toml)
#   레이아웃(FHS): /usr/bin/nexa-coffee · /usr/share/applications/nexa-coffee.desktop ·
#                  /usr/share/icons/hicolor/{64x64,256x256}/apps/nexa-coffee.png · /usr/share/doc/nexa-coffee/{LICENSE,copyright}
#   이식 원천: nexa-sql packaging/linux/build-deb.sh · build-rpm.sh · nexa-sql.spec (deb와 rpm이 같은 스테이징을 담는다).
#   필요: dpkg-deb(dpkg-dev) · rpmbuild(apt-get install rpm). 정적 바이너리라 Depends 없음.
set -eu
BIN=$1 VERSION=$2 OUT=$3
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
[ -x "$BIN" ] || { echo "바이너리 없음: $BIN" >&2; exit 1; }
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
PKG=$WORK/deb-root; U=$PKG/usr

# ── 스테이징(deb · rpm 공용) ──
install -Dm755 "$BIN" "$U/bin/nexa-coffee"
install -Dm644 "$ROOT/packaging/linux/nexa-coffee.desktop" "$U/share/applications/nexa-coffee.desktop"
for n in 64 256; do install -Dm644 "$ROOT/packaging/branding/nexa-coffee-$n.png" "$U/share/icons/hicolor/${n}x$n/apps/nexa-coffee.png"; done
install -Dm644 "$ROOT/LICENSE" "$U/share/doc/nexa-coffee/LICENSE"
cat > "$U/share/doc/nexa-coffee/copyright" <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: nexa-coffee
Source: https://github.com/SosomLab/nexa-coffee

Files: *
Copyright: 2026 SosomLab
License: MIT
 See LICENSE in this directory.
EOF

# ── .deb ──
mkdir -p "$PKG/DEBIAN"
cat > "$PKG/DEBIAN/control" <<EOF
Package: nexa-coffee
Version: $VERSION
Section: utils
Priority: optional
Architecture: amd64
Maintainer: Sangyong Bae <kiros33@gmail.com>
Installed-Size: $(du -sk "$U" | cut -f1)
Recommends: zenity | yad | kdialog
Suggests: gnome-shell-extension-appindicator
Homepage: https://github.com/SosomLab/nexa-coffee
Description: Tray timer that keeps the computer awake
 Nexa Coffee is a tiny tray / menu bar timer for Windows, macOS and Linux that
 blocks sleep and the screensaver for a chosen time. Pure C, static binary, no
 framework. On Linux it uses StatusNotifierItem (GNOME needs the AppIndicator
 extension) and logind / ScreenSaver inhibitors over D-Bus.
EOF
cat > "$PKG/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor 2>/dev/null || true
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q /usr/share/applications 2>/dev/null || true
exit 0
EOF
cp "$PKG/DEBIAN/postinst" "$PKG/DEBIAN/postrm"; chmod 0755 "$PKG/DEBIAN/postinst" "$PKG/DEBIAN/postrm"
DEB=$OUT/nexa-coffee_${VERSION}_amd64.deb
dpkg-deb --build --root-owner-group "$PKG" "$DEB" >/dev/null
rm -rf "$PKG/DEBIAN"

# ── .rpm(같은 스테이징 · 소스 없이 %install에서 복사) ──
# RPM Version은 '-'를 못 쓴다 — 사전 릴리스 접미사는 '~'(nexa-sql 규약).
RPM_VER=$(echo "$VERSION" | tr '-' '~')
TOP=$WORK/rpmbuild; mkdir -p "$TOP/SPECS"
cat > "$TOP/SPECS/nexa-coffee.spec" <<'EOF'
%global debug_package %{nil}
%global __strip /bin/true
%global __os_install_post %{nil}
Name:           nexa-coffee
Version:        %{_version}
Release:        1
Summary:        Tray timer that keeps the computer awake
License:        MIT
URL:            https://github.com/SosomLab/nexa-coffee
Recommends:     zenity

%description
Nexa Coffee is a tiny tray / menu bar timer for Windows, macOS and Linux that
blocks sleep and the screensaver for a chosen time. Pure C, static binary, no
framework. On Linux it uses StatusNotifierItem and logind / ScreenSaver inhibitors over D-Bus.

%install
mkdir -p %{buildroot}%{_prefix}
cp -a %{_stagedir}/. %{buildroot}%{_prefix}/

%post
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t -f %{_datadir}/icons/hicolor 2>/dev/null || :
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q %{_datadir}/applications 2>/dev/null || :

%postun
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t -f %{_datadir}/icons/hicolor 2>/dev/null || :
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q %{_datadir}/applications 2>/dev/null || :

%files
%{_bindir}/nexa-coffee
%{_datadir}/applications/nexa-coffee.desktop
%{_datadir}/icons/hicolor/*/apps/nexa-coffee.png
%license %{_docdir}/nexa-coffee/LICENSE
%doc %{_docdir}/nexa-coffee/copyright
EOF
rpmbuild -bb "$TOP/SPECS/nexa-coffee.spec" --define "_topdir $TOP" --define "_version $RPM_VER" \
    --define "_stagedir $U" --target x86_64 >/dev/null
RPM=$OUT/nexa-coffee-$RPM_VER-1.x86_64.rpm
mv "$(find "$TOP/RPMS" -name '*.rpm' | head -1)" "$RPM"

# ── 검증(설치 없이) ──
dpkg-deb --info "$DEB" | sed -n '2,8p'
dpkg-deb --contents "$DEB" | awk '{print $NF}' | grep -E 'bin/nexa-coffee$|\.desktop$|256x256'
rpm -qpi "$RPM" 2>/dev/null | sed -n '1,4p'
rpm -qpl "$RPM" 2>/dev/null | grep -E 'bin/nexa-coffee$|\.desktop$'
ls -l "$DEB" "$RPM"
