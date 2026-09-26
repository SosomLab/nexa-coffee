#!/bin/sh
# test-install.sh — packaging/linux/install.sh · uninstall.sh 검증(CI · 개발 PC 공용).
# 릴리스 tar.gz와 같은 배치를 임시 폴더에 만들고, 그 안의 prefix·XDG_CONFIG_HOME으로만 설치/제거한다
# (실제 ~/.local·~/.config는 건드리지 않는다). 09-26 사용자 QA: 앱 목록 누락(Exec PATH · 옛 아이콘 캐시).
set -eu
cd "$(dirname "$0")/.."
BIN=${1:-dist/nexa-coffee}
T=$(mktemp -d "${TMPDIR:-/tmp}/nexa-coffee-install.XXXXXX")
trap 'rm -rf "$T"' EXIT INT TERM
fail() { echo "FAIL: $*"; exit 1; }
ok() { echo "  ✓ $*"; }

PKG="$T/pkg"; P="$T/prefix"; export XDG_CONFIG_HOME="$T/config"
mkdir -p "$PKG"
cp "$BIN" packaging/linux/nexa-coffee.desktop packaging/linux/install.sh packaging/linux/uninstall.sh packaging/branding/nexa-coffee-256.png "$PKG/"
AS="$XDG_CONFIG_HOME/autostart/nexa-coffee.desktop"
HIC="$P/share/icons/hicolor"

echo "== 옛 아이콘 캐시가 있는 prefix(다른 앱이 만든 것)"
mkdir -p "$HIC/48x48/apps"; cp packaging/branding/nexa-coffee-256.png "$HIC/48x48/apps/other-app.png"
if command -v gtk-update-icon-cache >/dev/null 2>&1; then gtk-update-icon-cache -f -t -q "$HIC"; HAVE_GTK=1; else : > "$HIC/icon-theme.cache"; HAVE_GTK=0; fi
touch -d '2000-01-01' "$HIC" "$HIC/icon-theme.cache"

echo "== install(자동 실행 없이)"
sh "$PKG/install.sh" "$P" >/dev/null
[ -x "$P/bin/nexa-coffee" ] || fail "binary"; ok "실행 파일"
grep -qx "Exec=$P/bin/nexa-coffee" "$P/share/applications/nexa-coffee.desktop" || fail "Exec not absolute"; ok "Exec 절대 경로"
[ -f "$HIC/256x256/apps/nexa-coffee.png" ] || fail "icon"; ok "아이콘"
if [ "$HAVE_GTK" = 1 ]; then
  grep -q nexa-coffee "$HIC/icon-theme.cache" || fail "icon cache not refreshed"
  grep -q other-app "$HIC/icon-theme.cache" || fail "icon cache lost other app"; ok "아이콘 캐시 갱신(다른 앱 항목 유지)"
else
  [ "$HIC" -nt "$HIC/icon-theme.cache" ] || fail "hicolor not touched"; ok "캐시 도구 없음 → hicolor 시각 갱신(GTK가 다시 훑음)"
fi
if command -v desktop-file-validate >/dev/null 2>&1; then desktop-file-validate "$P/share/applications/nexa-coffee.desktop" || fail "desktop invalid"; ok "desktop-file-validate"; fi
[ ! -e "$AS" ] || fail "autostart without flag"; ok "자동 실행 항목 없음"

echo "== install --autostart"
sh "$PKG/install.sh" --autostart "$P" >/dev/null
grep -qx "Exec=$P/bin/nexa-coffee" "$AS" || fail "autostart Exec"
grep -qx "X-GNOME-Autostart-enabled=true" "$AS" || fail "autostart flag"; ok "로그인 자동 실행 항목"
sh "$PKG/install.sh" --autostart "$P" >/dev/null
[ "$(grep -c '^X-GNOME-Autostart-enabled' "$AS")" = 1 ] || fail "autostart duplicated on reinstall"; ok "다시 설치해도 중복 없음"

echo "== 다른 곳을 가리키는 자동 실행 항목은 제거하지 않는다"
cp "$AS" "$T/as.bak"; sed -i.orig "s|^Exec=.*|Exec=/opt/other/nexa-coffee|" "$AS"; rm -f "$AS.orig"
sh "$PKG/uninstall.sh" "$P" >/dev/null
[ -f "$AS" ] || fail "removed foreign autostart"; ok "다른 설치의 항목 보존"
cp "$T/as.bak" "$AS"

echo "== uninstall"
sh "$PKG/install.sh" "$P" >/dev/null
sh "$PKG/uninstall.sh" "$P" >/dev/null
for f in "$P/bin/nexa-coffee" "$P/share/applications/nexa-coffee.desktop" "$HIC/256x256/apps/nexa-coffee.png" "$AS"; do [ ! -e "$f" ] || fail "left: $f"; done; ok "파일·자동 실행 항목 제거"
if [ "$HAVE_GTK" = 1 ]; then
  ! grep -q nexa-coffee "$HIC/icon-theme.cache" || fail "cache still lists nexa-coffee"
  grep -q other-app "$HIC/icon-theme.cache" || fail "cache lost other app"; ok "캐시에서 빠짐(다른 앱 유지)"
fi
echo "test-install: all green"
