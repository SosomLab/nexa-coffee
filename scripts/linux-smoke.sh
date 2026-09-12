#!/bin/sh
# linux-smoke.sh — Linux 실기 없이 D-Bus 계약을 검증한다(CI · Docker 공용).
# 세션 버스를 띄우고 nexa-coffee를 붙인 뒤 dbus-send로 SNI 속성·dbusmenu 레이아웃·클릭 이벤트를 확인한다.
set -eu
cd "$(dirname "$0")/.."
BIN=${1:-dist/nexa-coffee}
export HOME=$(mktemp -d)
export XDG_CONFIG_HOME="$HOME/.config"
# 시스템 세션 설정 대신 테스트 전용 설정으로 띄운다(macOS launchd·CI 컨테이너 모두에서 동작)
PIDF=$(mktemp)
ADDR=$(dbus-daemon --config-file=scripts/dbus-test.conf --fork --print-address=1 --print-pid=3 3>"$PIDF")
export DBUS_SESSION_BUS_ADDRESS="$ADDR"
DBUS_PID=$(cat "$PIDF"); rm -f "$PIDF"
trap 'kill $APP_PID 2>/dev/null; kill $DBUS_PID 2>/dev/null' EXIT

LANG=ko_KR.UTF-8 "$BIN" & APP_PID=$!
NAME="org.kde.StatusNotifierItem-$APP_PID-1"
# 이름이 버스에 뜰 때까지(최대 5초) 기다린다
i=0
until dbus-send --session --print-reply --dest=org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.NameHasOwner "string:$NAME" 2>/dev/null | grep 'boolean true' >/dev/null; do
  i=$((i+1)); [ $i -lt 50 ] || { echo "app did not register $NAME"; exit 1; }
  kill -0 $APP_PID 2>/dev/null || { echo "app died"; exit 1; }
  sleep 0.1
done
call() { dbus-send --session --print-reply --dest="$NAME" "$@"; }
# grep -q는 파이프를 일찍 닫아 echo가 SIGPIPE를 받는다 — 끝까지 읽는 형태로
has() { printf '%s\n' "$1" | grep -- "$2" >/dev/null; }

echo "== SNI GetAll"
out=$(call /StatusNotifierItem org.freedesktop.DBus.Properties.GetAll string:org.kde.StatusNotifierItem)
has "$out" 'string "ApplicationStatus"'
has "$out" 'Nexa Coffee — 대기 중'
has "$out" 'int32 22'
has "$out" 'int32 44'
has "$out" 'object path "/MenuBar"'
echo "== Introspect"
call /MenuBar org.freedesktop.DBus.Introspectable.Introspect | grep 'com.canonical.dbusmenu' >/dev/null
echo "== GetLayout"
out=$(call /MenuBar com.canonical.dbusmenu.GetLayout int32:0 int32:-1 array:string:)
has "$out" 'string "무제한"'
has "$out" 'string "12시간"'
has "$out" 'string "사용자 지정 (1시간)"'
has "$out" 'string "23시간"'
has "$out" 'string "실행 시 자동 시작"'
has "$out" 'string "separator"'
echo "== dbusmenu props"
call /MenuBar org.freedesktop.DBus.Properties.Get string:com.canonical.dbusmenu string:Version | grep 'uint32 3' >/dev/null
call /MenuBar com.canonical.dbusmenu.GetGroupProperties array:int32:1,8 array:string: | grep 'string "끄기"' >/dev/null
call /MenuBar com.canonical.dbusmenu.AboutToShow int32:0 | grep 'boolean false' >/dev/null
echo "== click 12h"
call /MenuBar com.canonical.dbusmenu.Event int32:2 string:clicked variant:int32:0 uint32:0 >/dev/null
sleep 0.3
out=$(call /StatusNotifierItem org.freedesktop.DBus.Properties.Get string:org.kde.StatusNotifierItem string:ToolTip)
has "$out" '12시간 남음'
call /MenuBar com.canonical.dbusmenu.GetGroupProperties array:int32:2 array:string: | grep 'int32 1' >/dev/null
grep -q 'sel=2' "$XDG_CONFIG_HOME/nexa-coffee/config"
echo "== custom 0d 0h 5m + start"
call /MenuBar com.canonical.dbusmenu.Event int32:201 string:clicked variant:int32:0 uint32:0 >/dev/null
call /MenuBar com.canonical.dbusmenu.Event int32:301 string:clicked variant:int32:0 uint32:0 >/dev/null
call /MenuBar com.canonical.dbusmenu.Event int32:20 string:clicked variant:int32:0 uint32:0 >/dev/null
sleep 0.3
call /StatusNotifierItem org.freedesktop.DBus.Properties.Get string:org.kde.StatusNotifierItem string:ToolTip | grep '1시간 남음' >/dev/null
grep -q 'custom=0,1,5' "$XDG_CONFIG_HOME/nexa-coffee/config"
echo "== off"
call /MenuBar com.canonical.dbusmenu.Event int32:8 string:clicked variant:int32:0 uint32:0 >/dev/null
sleep 0.2
call /StatusNotifierItem org.freedesktop.DBus.Properties.Get string:org.kde.StatusNotifierItem string:ToolTip | grep '대기 중' >/dev/null
echo "== quit"
call /MenuBar com.canonical.dbusmenu.Event int32:12 string:clicked variant:int32:0 uint32:0 >/dev/null
sleep 0.3
if kill -0 $APP_PID 2>/dev/null; then echo "app did not quit"; exit 1; fi
wait $APP_PID || true
echo "linux-smoke: all green"
