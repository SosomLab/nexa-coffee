#!/usr/bin/env bash
# 패키지 매니저 매니페스트 생성 — winget · Chocolatey · Homebrew (이식 원본: nexa-clip packaging/render-manifests.sh)
#
#   render-manifests.sh <VERSION> <자산디렉터리> <출력디렉터리>
#
# 자산 디렉터리에는 릴리스에 올라간 파일이 그대로 있어야 한다. 체크섬은 **그 파일에서 직접 계산**한다 —
# 손으로 적은 해시는 언젠가 틀리고, 틀린 해시는 사용자 기기에서 설치 실패로 나타난다.
# ★ 이 스크립트가 유일한 치환 지점이다(릴리스·제출 워크플로가 각자 치환하면 갈라진다).
set -euo pipefail

VERSION="${1:?사용법: render-manifests.sh <VERSION> <자산디렉터리> <출력디렉터리>}"
ASSETS="${2:?자산 디렉터리}"
OUT="${3:?출력 디렉터리}"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

sha() {
  local f="$ASSETS/$1"
  [ -f "$f" ] || { echo "::error::자산 없음: $1" >&2; exit 1; }
  if command -v sha256sum >/dev/null 2>&1; then sha256sum < "$f" | cut -d" " -f1; else shasum -a 256 < "$f" | cut -d" " -f1; fi
}

V="$VERSION"
# Windows — 설치본(setup.exe · x64/x86 한 파일)과 포터블 zip은 서로 다른 패키지다(10-17 분리 · DR-16).
SHA_WIN_SETUP=$(sha "nexa-coffee-$V-windows-setup.exe")
SHA_WIN_X64=$(sha "nexa-coffee-$V-windows-x64-portable.zip")
SHA_WIN_X86=$(sha "nexa-coffee-$V-windows-x86-portable.zip")
SHA_MAC=$(sha     "nexa-coffee-$V-macos-universal.zip")
SHA_LINUX=$(sha   "nexa-coffee-$V-linux-x64.tar.gz")
DATE=$(date -u +%Y-%m-%d)

fill() {
  sed -e "s/@VERSION@/$V/g" -e "s/@DATE@/$DATE/g" \
      -e "s/@SHA_WIN_SETUP@/$SHA_WIN_SETUP/g" -e "s/@SHA_WIN_X64@/$SHA_WIN_X64/g" -e "s/@SHA_WIN_X86@/$SHA_WIN_X86/g" \
      -e "s/@SHA_MAC@/$SHA_MAC/g" -e "s/@SHA_LINUX@/$SHA_LINUX/g" "$1" > "$2"
}

mkdir -p "$OUT"

# ── winget — microsoft/winget-pkgs 경로 규약: ID의 점을 폴더로(SosomLab.NexaCoffee.Portable → s/SosomLab/NexaCoffee/Portable/<버전>/) ──
#    설치본 SosomLab.NexaCoffee ← packaging/winget/ · 포터블 SosomLab.NexaCoffee.Portable ← packaging/winget-portable/
emit_winget() {
  local id=$1 src=$2 dir="$OUT/winget/manifests/s/$(echo "$1" | tr '.' '/')/$V"
  mkdir -p "$dir"
  fill "$here/$src/version.yaml"   "$dir/$id.yaml"
  fill "$here/$src/locale.yaml"    "$dir/$id.locale.en-US.yaml"
  fill "$here/$src/installer.yaml" "$dir/$id.installer.yaml"
}
emit_winget SosomLab.NexaCoffee          winget
emit_winget SosomLab.NexaCoffee.Portable winget-portable

# ── Chocolatey — 그대로 `choco pack` 가능한 형태. 설치본 nexa-coffee ← choco/ · 포터블 nexa-coffee-portable ← choco-portable/ ──
emit_choco() {
  local id=$1 src=$2 dir="$OUT/choco/$1" f
  mkdir -p "$dir/tools"
  fill "$here/$src/$id.nuspec" "$dir/$id.nuspec"
  for f in "$here/$src/tools/"*.ps1; do fill "$f" "$dir/tools/$(basename "$f")"; done
}
emit_choco nexa-coffee          choco
emit_choco nexa-coffee-portable choco-portable

# ── Homebrew(탭 저장소 배치 그대로: Casks/ · Formula/) ──
mkdir -p "$OUT/homebrew/Casks" "$OUT/homebrew/Formula"
fill "$here/homebrew/nexa-coffee.rb"          "$OUT/homebrew/Casks/nexa-coffee.rb"
fill "$here/homebrew/nexa-coffee-portable.rb" "$OUT/homebrew/Formula/nexa-coffee-portable.rb"

if grep -rn '@[A-Z_]*@' "$OUT"; then echo "::error::치환되지 않은 자리표시자가 남았다" >&2; exit 1; fi
echo "생성 완료 ($V):"; find "$OUT" -type f | sort
