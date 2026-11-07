#!/bin/sh
# tmp-clean.sh — 프로젝트 밖 임시 산출물 보고·정리(규칙: CLAUDE.md §4 "임시 파일" · docs/18 "임시 파일 정리" · 사용자 요청 11-07).
#   보고만:  sh scripts/tmp-clean.sh
#   정리:    sh scripts/tmp-clean.sh --trim        (나이 가드를 넘은 것만 지운다)
# 대상 = 이 프로젝트 접두사 **화이트리스트**만(다른 프로젝트 임시 파일은 건드리지 않는다):
#   ① ${TMPDIR:-/tmp}/nexa-coffee-smoke.* · nexa-coffee-install.*  — 스크립트 고정물(trap으로 스스로 지우지만 kill -9·끊긴 세션이 남긴다)
#      나이 NC_TMP_AGE_H 시간(기본 24) 넘은 것만 — 돌고 있는 스모크의 폴더를 지우지 않기 위해
#   ② Claude 세션 스크래치 /private/tmp/claude-*/-Users-*-nexa-coffee/<세션>/  — NC_SCRATCH_AGE_D 일(기본 7) 넘은 세션만
#   ③ 프로젝트 안 build/ dist/ 크기 보고(정리는 `make clean` · 여기서 지우지 않음)
#   ④ Windows %TEMP%\nc-config.bak(docs/18 ③ 실기 백업)은 실기 "마무리" 단계에서 사람이 지운다(이 스크립트 범위 밖)
# 새 임시 이름을 쓰는 코드/스크립트는 접두사 nexa-coffee-* 를 쓰고 아래 PATTERNS에 추가한다(packaging/linux/build-pkgs.sh는 mktemp -d 무작위 이름 + trap — 남으면 OS가 재부팅 때 거둠).
set -eu
cd "$(dirname "$0")/.."
TRIM=0; [ "${1:-}" = "--trim" ] && TRIM=1
AGE_H=${NC_TMP_AGE_H:-24}
AGE_D=${NC_SCRATCH_AGE_D:-7}
T=${TMPDIR:-/tmp}
PATTERNS="nexa-coffee-smoke.* nexa-coffee-install.*"
total=0; removed=0

size_of() { du -sk "$1" 2>/dev/null | cut -f1; }
report() { # $1=경로 $2=오래됨(1/0)
    k=$(size_of "$1"); k=${k:-0}; total=$((total + k))
    printf '  %8s KB  %s  %s\n' "$k" "$([ "$2" = 1 ] && echo '오래됨' || echo '최근  ')" "$1"
}

echo "① 스크립트 고정물 ($T · ${AGE_H}h 넘은 것만 정리)"
for pat in $PATTERNS; do
    find "$T" -maxdepth 1 -name "$pat" -mmin +$((AGE_H * 60)) 2>/dev/null | while read -r p; do
        report "$p" 1; [ $TRIM = 1 ] && rm -rf "$p" && echo "           → 삭제"
    done
    find "$T" -maxdepth 1 -name "$pat" ! -mmin +$((AGE_H * 60)) 2>/dev/null | while read -r p; do report "$p" 0; done
done

echo "② Claude 세션 스크래치 (${AGE_D}일 넘은 세션만 정리)"
for root in /private/tmp/claude-*/-Users-*-nexa-coffee /tmp/claude-*/-Users-*-nexa-coffee; do
    [ -d "$root" ] || continue
    find "$root" -mindepth 1 -maxdepth 1 -type d -mtime +"$AGE_D" 2>/dev/null | while read -r p; do
        report "$p" 1; [ $TRIM = 1 ] && rm -rf "$p" && echo "           → 삭제"
    done
    find "$root" -mindepth 1 -maxdepth 1 -type d ! -mtime +"$AGE_D" 2>/dev/null | while read -r p; do report "$p" 0; done
done

echo "③ 프로젝트 안 산출물 (정리 = make clean)"
for d in build dist; do [ -d "$d" ] && report "$d" 0; done

echo "합계(보고 대상): $((total / 1024)) MB · 모드: $([ $TRIM = 1 ] && echo '정리(--trim)' || echo '보고만')"
