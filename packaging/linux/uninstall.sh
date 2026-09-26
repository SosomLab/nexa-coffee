#!/bin/sh
set -eu
PREFIX=${1:-$HOME/.local}
rm -f "$PREFIX/bin/nexa-coffee" "$PREFIX/share/applications/nexa-coffee.desktop" "$PREFIX/share/icons/hicolor/256x256/apps/nexa-coffee.png"
hic="$PREFIX/share/icons/hicolor"
[ -f "$hic/icon-theme.cache" ] && { gtk-update-icon-cache -f -t -q "$hic" 2>/dev/null || touch "$hic"; }
echo "removed from $PREFIX"
