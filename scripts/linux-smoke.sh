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
# 종료 시 정리 — 이미 끝난 프로세스에 kill이 실패해도 원래 종료 상태를 유지한다(bash -e에서 exit 1로 바뀌던 문제)
trap 'st=$?; kill $APP_PID 2>/dev/null || true; kill $DBUS_PID 2>/dev/null || true; exit $st' EXIT

# 대화창 도구를 흉내내는 가짜 yad(앱 시작 전에 PATH에 넣는다): 인자와 무관하게 "0 1 5"를 출력
mkdir -p "$HOME/bin"; printf '#!/bin/sh\necho "0.000000 1.000000 5.000000"\n' > "$HOME/bin/yad"; chmod +x "$HOME/bin/yad"
export PATH="$HOME/bin:$PATH"
APP_PID=; DBUS_PID=${DBUS_PID:-}
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
# 툴팁/설정이 기대 값이 될 때까지(최대 3초) 기다린다 — 자식 프로세스(대화창) 체인은 시간이 걸린다
wait_tip() { i=0; until call /StatusNotifierItem org.freedesktop.DBus.Properties.Get string:org.kde.StatusNotifierItem string:ToolTip 2>/dev/null | grep -- "$1" >/dev/null; do i=$((i+1)); [ $i -lt 30 ] || { echo "tooltip never matched: $1"; exit 1; }; sleep 0.1; done; }
wait_conf() { i=0; until grep -q -- "$1" "$XDG_CONFIG_HOME/nexa-coffee/config" 2>/dev/null; do i=$((i+1)); [ $i -lt 30 ] || { echo "config never matched: $1"; exit 1; }; sleep 0.1; done; }

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
has "$out" 'string "대기 중"'
has "$out" 'string "무제한"'
has "$out" 'string "12시간"'
has "$out" 'string "사용자 지정…"'
has "$out" 'string "Nexa Coffee 정보…"'
has "$out" 'string "실행 시 자동 시작"'
has "$out" 'string "separator"'
echo "== dbusmenu props"
call /MenuBar org.freedesktop.DBus.Properties.Get string:com.canonical.dbusmenu string:Version | grep 'uint32 3' >/dev/null
call /MenuBar com.canonical.dbusmenu.GetGroupProperties array:int32:1,8 array:string: | grep 'string "끄기"' >/dev/null
call /MenuBar com.canonical.dbusmenu.AboutToShow int32:0 | grep 'boolean false' >/dev/null
echo "== click 12h"
call /MenuBar com.canonical.dbusmenu.Event int32:2 string:clicked variant:int32:0 uint32:0 >/dev/null
wait_tip '12시간 남음'
call /MenuBar com.canonical.dbusmenu.GetGroupProperties array:int32:14 array:string: | grep -E '[0-9]+일 [0-9]+시간 [0-9]+분 [0-9]+초' >/dev/null
call /MenuBar com.canonical.dbusmenu.GetGroupProperties array:int32:2 array:string: | grep 'int32 1' >/dev/null
grep -q 'custom=0,1,0' "$XDG_CONFIG_HOME/nexa-coffee/config"
echo "== auto-start submenu"
out=$(call /MenuBar com.canonical.dbusmenu.GetLayout int32:10 int32:-1 array:string:)
has "$out" 'string "끄기"'
has "$out" 'string "사용자 지정…"'
call /MenuBar com.canonical.dbusmenu.Event int32:20 string:clicked variant:int32:0 uint32:0 >/dev/null
wait_conf 'auto=0,0,0'
echo "== custom dialog (fake yad via PATH)"
call /MenuBar com.canonical.dbusmenu.Event int32:7 string:clicked variant:int32:0 uint32:0 >/dev/null
wait_tip '1시간 남음'
wait_conf 'custom=0,1,5'
echo "== auto-start dialog (fake yad)"
call /MenuBar com.canonical.dbusmenu.Event int32:21 string:clicked variant:int32:0 uint32:0 >/dev/null
wait_conf 'auto=0,1,5'
call /MenuBar com.canonical.dbusmenu.GetGroupProperties array:int32:10 array:string: | grep '1시간 5분' >/dev/null
echo "== off"
call /MenuBar com.canonical.dbusmenu.Event int32:8 string:clicked variant:int32:0 uint32:0 >/dev/null
wait_tip '대기 중'
echo "== quit"
call /MenuBar com.canonical.dbusmenu.Event int32:13 string:clicked variant:int32:0 uint32:0 >/dev/null
sleep 0.3
if kill -0 $APP_PID 2>/dev/null; then echo "app did not quit"; exit 1; fi
wait $APP_PID || true
echo "linux-smoke: all green"
