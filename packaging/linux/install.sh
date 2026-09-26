#!/bin/sh
# install.sh [--autostart] [prefix] — tar.gz 포터블 배포물에서 사용자 로컬 설치(기본 ~/.local). 제거: uninstall.sh
#   --autostart  로그인할 때 자동 실행(XDG autostart · ~/.config/autostart/nexa-coffee.desktop). 나중에 켜려면 다시 실행.
set -eu
AUTOSTART=0
case "${1:-}" in --autostart) AUTOSTART=1; shift ;; esac
PREFIX=${1:-$HOME/.local}
HERE=$(cd "$(dirname "$0")" && pwd)
install -Dm755 "$HERE/nexa-coffee" "$PREFIX/bin/nexa-coffee"
# Exec는 절대 경로로 바꿔 쓴다 — 세션(gnome-shell)의 PATH엔 ~/.local/bin이 없을 수 있고,
# GLib은 Exec 프로그램을 PATH에서 못 찾으면 그 항목을 앱 목록에서 아예 숨긴다(09-26 실측).
mkdir -p "$PREFIX/share/applications"
sed "s|^Exec=nexa-coffee|Exec=$PREFIX/bin/nexa-coffee|" "$HERE/nexa-coffee.desktop" > "$PREFIX/share/applications/nexa-coffee.desktop"
chmod 644 "$PREFIX/share/applications/nexa-coffee.desktop"
install -Dm644 "$HERE/nexa-coffee-256.png" "$PREFIX/share/icons/hicolor/256x256/apps/nexa-coffee.png"
# hicolor에 icon-theme.cache가 이미 있으면(다른 앱이 만든 것) GTK는 그 캐시만 믿는다 — 새 아이콘이 안 보인다.
# 캐시가 있을 때만 갱신한다(없으면 GTK가 디렉터리를 직접 훑으므로 만들 필요 없다). 도구가 없어도 설치는 성공.
hic="$PREFIX/share/icons/hicolor"
[ -f "$hic/icon-theme.cache" ] && { gtk-update-icon-cache -f -t -q "$hic" 2>/dev/null || touch "$hic"; }
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q "$PREFIX/share/applications" 2>/dev/null || true
# 로그인 자동 실행 — 셸의 트레이 호스트보다 먼저 떠도 된다(늦게 뜬 워처에 재등록 · 0.1.1까지는 여기서 죽었다 T-19).
if [ "$AUTOSTART" = 1 ]; then
  ad="${XDG_CONFIG_HOME:-$HOME/.config}/autostart"
  mkdir -p "$ad"
  cp "$PREFIX/share/applications/nexa-coffee.desktop" "$ad/nexa-coffee.desktop"
  echo "X-GNOME-Autostart-enabled=true" >> "$ad/nexa-coffee.desktop"
  echo "installed to $PREFIX · starts at login ($ad/nexa-coffee.desktop)"
else
  echo "installed to $PREFIX (start at login: rerun with --autostart)"
fi
