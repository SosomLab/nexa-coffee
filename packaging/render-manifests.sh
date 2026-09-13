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
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$f" | cut -d' ' -f1; else shasum -a 256 "$f" | cut -d' ' -f1; fi
}

V="$VERSION"
SHA_WIN_X64=$(sha "nexa-coffee-$V-windows-x64.zip")
SHA_WIN_X86=$(sha "nexa-coffee-$V-windows-x86.zip")
SHA_MAC=$(sha     "nexa-coffee-$V-macos-universal.zip")
SHA_LINUX=$(sha   "nexa-coffee-$V-linux-x64.tar.gz")
DATE=$(date -u +%Y-%m-%d)

fill() {
  sed -e "s/@VERSION@/$V/g" -e "s/@DATE@/$DATE/g" \
      -e "s/@SHA_WIN_X64@/$SHA_WIN_X64/g" -e "s/@SHA_WIN_X86@/$SHA_WIN_X86/g" \
      -e "s/@SHA_MAC@/$SHA_MAC/g" -e "s/@SHA_LINUX@/$SHA_LINUX/g" "$1" > "$2"
}

mkdir -p "$OUT"

# ── winget(포터블 zip) — microsoft/winget-pkgs 경로 규약: manifests/s/SosomLab/NexaCoffee/<버전>/ ──
id="SosomLab.NexaCoffee"
dir="$OUT/winget/manifests/s/SosomLab/NexaCoffee/$V"
mkdir -p "$dir"
fill "$here/winget/version.yaml"   "$dir/$id.yaml"
fill "$here/winget/locale.yaml"    "$dir/$id.locale.en-US.yaml"
fill "$here/winget/installer.yaml" "$dir/$id.installer.yaml"

# ── Chocolatey — 그대로 `choco pack` 가능한 형태 ──
dir="$OUT/choco/nexa-coffee"
mkdir -p "$dir/tools"
fill "$here/choco/nexa-coffee.nuspec"                "$dir/nexa-coffee.nuspec"
fill "$here/choco/tools/chocolateyinstall.ps1"       "$dir/tools/chocolateyinstall.ps1"
cp   "$here/choco/tools/chocolateybeforemodify.ps1"  "$dir/tools/chocolateybeforemodify.ps1"

# ── Homebrew(탭 저장소 배치 그대로: Casks/ · Formula/) ──
mkdir -p "$OUT/homebrew/Casks" "$OUT/homebrew/Formula"
fill "$here/homebrew/nexa-coffee.rb"          "$OUT/homebrew/Casks/nexa-coffee.rb"
fill "$here/homebrew/nexa-coffee-portable.rb" "$OUT/homebrew/Formula/nexa-coffee-portable.rb"

if grep -rn '@[A-Z_]*@' "$OUT"; then echo "::error::치환되지 않은 자리표시자가 남았다" >&2; exit 1; fi
echo "생성 완료 ($V):"; find "$OUT" -type f | sort
