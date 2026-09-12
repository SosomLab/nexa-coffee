#!/bin/sh
# make-app.sh <binary> <out.app> — 메뉴바 전용 .app 번들 조립(서명 없음 · ad-hoc 서명만).
set -eu
BIN=$1; APP=$2; VERSION=${VERSION:-0.0.0}
HERE=$(cd "$(dirname "$0")" && pwd)
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$BIN" "$APP/Contents/MacOS/nexa-coffee"
cp "$HERE/../branding/nexa-coffee.icns" "$APP/Contents/Resources/nexa-coffee.icns"
sed "s/__VERSION__/$VERSION/g" "$HERE/Info.plist" > "$APP/Contents/Info.plist"
printf 'APPL????' > "$APP/Contents/PkgInfo"
codesign --force --sign - "$APP" 2>/dev/null || true
echo "built: $APP ($VERSION)"
