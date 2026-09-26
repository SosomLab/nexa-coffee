#!/bin/sh
# uninstall.sh [prefix] — install.sh가 넣은 것을 지운다(로그인 자동 실행 항목 포함). 설정(~/.config/nexa-coffee)은 남긴다.
set -eu
PREFIX=${1:-$HOME/.local}
rm -f "$PREFIX/bin/nexa-coffee" "$PREFIX/share/applications/nexa-coffee.desktop" "$PREFIX/share/icons/hicolor/256x256/apps/nexa-coffee.png"
# 자동 실행 항목은 이 prefix의 실행 파일을 가리킬 때만 지운다(다른 곳에 설치한 것을 건드리지 않게)
as="${XDG_CONFIG_HOME:-$HOME/.config}/autostart/nexa-coffee.desktop"
[ -f "$as" ] && grep -q "^Exec=$PREFIX/bin/nexa-coffee" "$as" && rm -f "$as"
hic="$PREFIX/share/icons/hicolor"
[ -f "$hic/icon-theme.cache" ] && { gtk-update-icon-cache -f -t -q "$hic" 2>/dev/null || touch "$hic"; }
echo "removed from $PREFIX"
