#!/bin/sh
# linux-smoke.sh — Linux 실기 없이 D-Bus 계약을 검증한다(CI · Docker · 개발 PC 공용).
# 세션 버스와 시스템 버스를 **둘 다 따로** 띄우고(실제 세션·시스템 버스는 건드리지 않는다) nexa-coffee를 붙인 뒤
# dbus-send로 SNI 속성·dbusmenu 레이아웃·클릭 이벤트를 확인한다. 가짜 상대(test/fake_peer.c)로
#   ① GNOME식 워처(Register 응답 전에 GetAll) · 워처 재등장 재등록 · 16/22/44 픽스맵(T-10)
#   ② logind Inhibit fd 수신·해제 · polkit 거부 시 "sleep:idle" 재시도 · 전부 거부돼도 동작 유지(T-3)
# 까지 본다. 다른 앱의 자동 시험과 겹치지 않도록 HOME·XDG·버스·임시 파일을 전부 자기 폴더에 가두고 끝나면 지운다.
set -eu
cd "$(dirname "$0")/.."
BIN=${1:-dist/nexa-coffee}
make -s build/fake_peer >/dev/null
PEER=$PWD/build/fake_peer
T=$(mktemp -d "${TMPDIR:-/tmp}/nexa-coffee-smoke.XXXXXX")
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_RUNTIME_DIR="$T/run"
mkdir -p "$HOME/bin" "$XDG_RUNTIME_DIR"; chmod 700 "$XDG_RUNTIME_DIR"
CONF="$XDG_CONFIG_HOME/nexa-coffee/config"
PIDS=
# 종료 시 정리 — 이미 끝난 프로세스에 kill이 실패해도 원래 종료 상태를 유지한다(bash -e에서 exit 1로 바뀌던 문제)
cleanup() { st=$?; for p in $PIDS; do kill "$p" 2>/dev/null || true; done; rm -rf "$T"; exit $st; }
trap cleanup EXIT INT TERM
fail() { echo "FAIL: $*"; for f in "$T"/*.log; do [ -f "$f" ] && { echo "--- $(basename "$f")"; cat "$f"; }; done; exit 1; }

# 테스트 전용 버스 둘(macOS launchd·CI 컨테이너에서도 동작)
# (명령 치환은 서브셸이라 PID는 파일로 넘긴다)
bus() { dbus-daemon --config-file=scripts/dbus-test.conf --fork --print-address=1 --print-pid=3 3>"$T/$1.pid"; }
DBUS_SESSION_BUS_ADDRESS=$(bus session); PIDS="$PIDS $(cat "$T/session.pid")"
DBUS_SYSTEM_BUS_ADDRESS=$(bus system); PIDS="$PIDS $(cat "$T/system.pid")"
export DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS

# 대화창 도구를 흉내내는 가짜 yad(앱 시작 전에 PATH에 넣는다): 인자와 무관하게 "0 1 5"를 출력
printf '#!/bin/sh\necho "0.000000 1.000000 5.000000"\n' > "$HOME/bin/yad"; chmod +x "$HOME/bin/yad"
export PATH="$HOME/bin:$PATH"

# 가짜 상대: peer <이름> <인자…> → $T/<이름>.log · READY까지 대기
peer() { n=$1; shift; "$PEER" "$@" > "$T/$n.log" 2>&1 & eval "PID_$n=$!"; PIDS="$PIDS $!"; wait_log "$n" READY; }
unpeer() { eval "p=\$PID_$1"; kill "$p" 2>/dev/null || true; wait "$p" 2>/dev/null || true; }
# 로그에 패턴이 n번 이상 나올 때까지(최대 3초)
wait_log() { n=${3:-1}; i=0; until c=$(grep -c -- "$2" "$T/$1.log" 2>/dev/null) || true; [ "${c:-0}" -ge "$n" ]; do i=$((i+1)); [ $i -lt 30 ] || fail "$1.log never had '$2' x$n"; sleep 0.1; done; }

start_app() {
  LANG=ko_KR.UTF-8 "$BIN" > "$T/app.log" 2>&1 & APP_PID=$!; PIDS="$PIDS $APP_PID"
  NAME="org.kde.StatusNotifierItem-$APP_PID-1"
  # 이름이 버스에 뜰 때까지(최대 5초) 기다린다
  i=0
  until dbus-send --session --print-reply --dest=org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.NameHasOwner "string:$NAME" 2>/dev/null | grep 'boolean true' >/dev/null; do
    i=$((i+1)); [ $i -lt 50 ] || fail "app did not register $NAME"
    kill -0 $APP_PID 2>/dev/null || fail "app died"
    sleep 0.1
  done
}
quit_app() {
  call /MenuBar com.canonical.dbusmenu.Event int32:13 string:clicked variant:int32:0 uint32:0 >/dev/null
  i=0; while kill -0 $APP_PID 2>/dev/null; do i=$((i+1)); [ $i -lt 30 ] || fail "app did not quit"; sleep 0.1; done
  wait $APP_PID || true
}
call() { dbus-send --session --print-reply --dest="$NAME" "$@"; }
click() { call /MenuBar com.canonical.dbusmenu.Event "int32:$1" string:clicked variant:int32:0 uint32:0 >/dev/null; }
# grep -q는 파이프를 일찍 닫아 echo가 SIGPIPE를 받는다 — 끝까지 읽는 형태로
has() { printf '%s\n' "$1" | grep -- "$2" >/dev/null || fail "missing: $2"; }
# 툴팁/설정이 기대 값이 될 때까지(최대 3초) 기다린다 — 자식 프로세스(대화창) 체인은 시간이 걸린다
wait_tip() { i=0; until call /StatusNotifierItem org.freedesktop.DBus.Properties.Get string:org.kde.StatusNotifierItem string:ToolTip 2>/dev/null | grep -- "$1" >/dev/null; do i=$((i+1)); [ $i -lt 30 ] || fail "tooltip never matched: $1"; sleep 0.1; done; }
wait_conf() { i=0; until grep -q -- "$1" "$CONF" 2>/dev/null; do i=$((i+1)); [ $i -lt 30 ] || fail "config never matched: $1"; sleep 0.1; done; }

peer login1 login1 allow
start_app

echo "== SNI GetAll"
out=$(call /StatusNotifierItem org.freedesktop.DBus.Properties.GetAll string:org.kde.StatusNotifierItem)
has "$out" 'string "ApplicationStatus"'
has "$out" 'Nexa Coffee — 대기 중'
has "$out" 'int32 16'
has "$out" 'int32 22'
has "$out" 'int32 44'
has "$out" 'object path "/MenuBar"'
echo "== watcher appears late (GNOME order: GetAll before Register reply)"
peer watcher watcher
wait_log watcher 'REGISTER '
wait_log watcher 'GETALL ok pixmaps=16,22,44 status=Active'
echo "== watcher restarts → item re-registers"
unpeer watcher
peer watcher watcher
wait_log watcher 'GETALL ok pixmaps=16,22,44'
echo "== Introspect"
call /MenuBar org.freedesktop.DBus.Introspectable.Introspect | grep 'com.canonical.dbusmenu' >/dev/null || fail "introspect"
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
call /MenuBar org.freedesktop.DBus.Properties.Get string:com.canonical.dbusmenu string:Version | grep 'uint32 3' >/dev/null || fail "Version"
call /MenuBar com.canonical.dbusmenu.GetGroupProperties array:int32:1,8 array:string: | grep 'string "끄기"' >/dev/null || fail "GetGroupProperties"
call /MenuBar com.canonical.dbusmenu.AboutToShow int32:0 | grep 'boolean false' >/dev/null || fail "AboutToShow"
echo "== click 12h → logind inhibit (lid included)"
click 2
wait_tip '12시간 남음'
wait_log login1 'INHIBIT sleep:idle:handle-lid-switch'
call /MenuBar com.canonical.dbusmenu.GetGroupProperties array:int32:14 array:string: | grep -E '[0-9]+일 [0-9]+시간 [0-9]+분 [0-9]+초' >/dev/null || fail "status label"
call /MenuBar com.canonical.dbusmenu.GetGroupProperties array:int32:2 array:string: | grep 'int32 1' >/dev/null || fail "12h radio"
grep -q 'custom=0,1,0' "$CONF" || fail "custom=0,1,0"
echo "== active icon keeps all three sizes"
out=$(call /StatusNotifierItem org.freedesktop.DBus.Properties.Get string:org.kde.StatusNotifierItem string:IconPixmap)
has "$out" 'int32 16'
has "$out" 'int32 44'
echo "== auto-start submenu"
out=$(call /MenuBar com.canonical.dbusmenu.GetLayout int32:10 int32:-1 array:string:)
has "$out" 'string "끄기"'
has "$out" 'string "사용자 지정…"'
click 20
wait_conf 'auto=0,0,0'
echo "== language submenu (ja → ko)"
out=$(call /MenuBar com.canonical.dbusmenu.GetLayout int32:16 int32:-1 array:string:)
has "$out" 'string "English"'
has "$out" 'string "日本語"'
click 32
wait_conf 'lang=ja'
call /MenuBar com.canonical.dbusmenu.GetGroupProperties array:int32:13 array:string: | grep 'string "終了"' >/dev/null || fail "ja quit"
call /MenuBar com.canonical.dbusmenu.GetGroupProperties array:int32:14 array:string: | grep -E '[0-9]+日 [0-9]+時間 [0-9]+分 [0-9]+秒' >/dev/null || fail "ja status"
call /StatusNotifierItem org.freedesktop.DBus.Properties.Get string:org.kde.StatusNotifierItem string:ToolTip | grep '残り' >/dev/null || fail "ja tooltip"
click 31
wait_conf 'lang=ko'
echo "== custom dialog (fake yad via PATH)"
click 7
wait_tip '1시간 남음'
wait_conf 'custom=0,1,5'
echo "== auto-start dialog (fake yad)"
click 21
wait_conf 'auto=0,1,5'
call /MenuBar com.canonical.dbusmenu.GetGroupProperties array:int32:10 array:string: | grep '1시간 5분' >/dev/null || fail "auto label"
echo "== off → logind released"
click 8
wait_tip '대기 중'
wait_log login1 'RELEASE sleep:idle:handle-lid-switch'
echo "== quit"
quit_app
unpeer watcher

echo "== auto-start at launch (auto=0,1,5 saved) → inhibit without a click"
start_app
wait_tip '1시간'
wait_log login1 'INHIBIT sleep:idle:handle-lid-switch' 2
click 8
wait_log login1 'RELEASE sleep:idle:handle-lid-switch' 2
quit_app
unpeer login1
rm -f "$CONF"

echo "== polkit denies lid → retries sleep:idle"
peer login1 login1 deny-lid
start_app
click 2
wait_log login1 'DENY sleep:idle:handle-lid-switch'
wait_log login1 'INHIBIT sleep:idle$'
wait_tip '12시간 남음'
click 8
wait_log login1 'RELEASE sleep:idle$'
quit_app
unpeer login1

echo "== polkit denies everything → timer still runs"
peer login1 login1 deny-all
start_app
click 2
wait_log login1 'DENY sleep:idle$'
wait_tip '12시간 남음'
kill -0 $APP_PID 2>/dev/null || fail "app died after inhibit denial"
click 8
wait_tip '대기 중'
quit_app
unpeer login1

echo "linux-smoke: all green"
