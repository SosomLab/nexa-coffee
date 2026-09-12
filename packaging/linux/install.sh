#!/bin/sh
# install.sh [prefix] — tar.gz 포터블 배포물에서 사용자 로컬 설치(기본 ~/.local). 제거: uninstall.sh
set -eu
PREFIX=${1:-$HOME/.local}
HERE=$(cd "$(dirname "$0")" && pwd)
install -Dm755 "$HERE/nexa-coffee" "$PREFIX/bin/nexa-coffee"
install -Dm644 "$HERE/nexa-coffee.desktop" "$PREFIX/share/applications/nexa-coffee.desktop"
install -Dm644 "$HERE/nexa-coffee-256.png" "$PREFIX/share/icons/hicolor/256x256/apps/nexa-coffee.png"
echo "installed to $PREFIX (autostart: copy the .desktop into ~/.config/autostart/)"
