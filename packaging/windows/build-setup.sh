#!/bin/sh
# Windows 설치본(setup.exe) 빌드 — release.yml · ci.yml 이 같은 스크립트를 쓴다.
#   sh packaging/windows/build-setup.sh <버전> <exe 폴더> <출력 폴더>
#   <exe 폴더>에 nexa-coffee-x64.exe · nexa-coffee-x86.exe 가 있어야 한다(make win 산출물).
#
# ★ 릴리스 설치본은 **Windows 러너 + 공식 NSIS**로 만든다(11-18 · docs/19).
#   0.1.5 는 Ubuntu apt 의 NSIS(3.10-2ubuntu2 · Debian 빌드 스텁)로 만들었고, 스크립트가 0.1.4 와 같았는데도
#   Defender 가 Trojan:Win32/Suschil!rfn 으로 격리 · choco 스캔이 VirusTotal 탐지 10건 초과로 반려했다.
#   공식 NSIS 로 만든 형제 앱(nexa-beep) 설치본은 같은 설정으로 통과한다 → 스텁 차이가 유력한 원인.
#   Linux 의 makensis 로도 돌아가지만(ci ubuntu 의 스크립트 컴파일 검증용) 그 산출물은 배포하지 않는다.
set -eu
V=${1:?버전}; EXE=${2:?exe 폴더}; OUT=${3:?출력 폴더}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)

MAKENSIS=makensis
if ! command -v makensis >/dev/null 2>&1; then
    for d in "/c/Program Files (x86)/NSIS" "/c/Program Files/NSIS"; do
        if [ -x "$d/makensis.exe" ]; then MAKENSIS="$d/makensis.exe"; break; fi
    done
fi
command -v "$MAKENSIS" >/dev/null 2>&1 || { echo "makensis 를 찾을 수 없다(NSIS 설치 필요)" >&2; exit 1; }
echo "NSIS: $("$MAKENSIS" -VERSION) ($MAKENSIS)"

W="$ROOT/build/nsis"
rm -rf "$W"; mkdir -p "$W" "$OUT"
cp "$EXE/nexa-coffee-x64.exe" "$EXE/nexa-coffee-x86.exe" "$ROOT/packaging/branding/nexa-coffee.ico" "$ROOT/LICENSE" "$W/"
# VIProductVersion 은 숫자 넷만 — 개발 빌드(0.0.0-dev.<sha>)도 VERSION 파일의 세 자리로 만든다
sed -e "s/@VERSION@/$V/g" -e "s/@VIVERSION@/$(cat "$ROOT/VERSION").0/g" "$ROOT/packaging/windows/installer.nsi" > "$W/installer.nsi"
# 스크립트 폴더에서 돌린다 — 인자로 넘기는 경로가 없어 Git Bash(MSYS)의 경로 변환을 타지 않는다
(cd "$W" && "$MAKENSIS" -V2 -INPUTCHARSET UTF8 installer.nsi)
mv "$W/nexa-coffee-$V-windows-setup.exe" "$OUT/"
ls -la "$OUT/nexa-coffee-$V-windows-setup.exe"
