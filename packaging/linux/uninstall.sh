#!/bin/sh
set -eu
PREFIX=${1:-$HOME/.local}
rm -f "$PREFIX/bin/nexa-coffee" "$PREFIX/share/applications/nexa-coffee.desktop" "$PREFIX/share/icons/hicolor/256x256/apps/nexa-coffee.png"
echo "removed from $PREFIX"
