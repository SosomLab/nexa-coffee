# CLAUDE.md — Nexa Coffee 프로젝트 컨텍스트 (이식용 메모리)

> 다른 PC에서 clone 시 즉시 컨텍스트를 복원하기 위한 휴대용 메모리. **먼저 읽기:** [docs/STATUS.md](docs/STATUS.md) → [docs/10-decision-record.md](docs/10-decision-record.md).

## 1. 이 프로젝트는

**Nexa Coffee** = 트레이/메뉴바 절전 방지 타이머(Windows · macOS · Linux). **순수 C · 프레임워크 0 · 외부 라이브러리 0 · 극단적 최소화**.

- 조직: SosomLab · 개발자: Sangyong Bae · kiros33@gmail.com · 저장소: <https://github.com/SosomLab/nexa-coffee>
- 라이선스: **MIT**(누구나 무료 — nexa-shortcut과 동일. beep/clip/dir2의 PolyForm NC와 **다르다**).
- 현 단계: **v0.1.1 배포됨(09-17)** — Release · brew 탭 ✓ · winget [PR #436346](https://github.com/microsoft/winget-pkgs/pull/436346) 검증 통과(머지 대기) · choco 0.1.0 모더레이션 대기.
  - **Windows 실기 사실상 완료**(09-13·09-17): 트레이·툴팁·메뉴 라디오·열린 메뉴 1초 갱신·`powercfg`·i18n(CJK·언어 메뉴·입력 창 라벨 폭)·T-13 메모리·**DR-14 파이프 끝단**. 남은 둘(아이콘 DPI·TaskbarCreated)은 화면 배율·셸을 건드려야 해 [18 실기 점검표](docs/18-build-and-test.md#사람이-해야-하는-실기-점검windows)로 넘겼다.
  - **v1은 코드 서명이 없다** — SmartScreen·Gatekeeper 경고의 원인이자, 0.1.0 x86이 Defender 오탐(`Trojan:Win32/Tecabans.STV!cl`)으로 격리된 근본 원인. 0.1.1에서 **PE 버전 리소스**를 넣어 완화했다(09-17 실측: 0.1.0 x86 격리 / 0.1.1 x86 통과).

### 참조 원천(재발명 금지)

| 원천 | 경로 | 무엇을 가져왔나 |
| --- | --- | --- |
| **nexa-shortcut** | `../kiros33/nexa-shortcut` | ★ 초경량 원칙 — CRT 미링크 · `-nostdlib` · 진입점 `start` · Makefile/build.bat · 크기 예산 CI · winget/choco 포터블 |
| **nexa-clip** | `../kiros33/nexa-clip` | 트레이 계약 — SNI 속성 · dbusmenu 레이아웃(`nclip-plat/src/tray.rs::sni`) · macOS NSStatusItem · 패키징/릴리스 워크플로 · 문서 규약(docs/16) |
| **nexa-beep / nexa-dir2** | `../kiros33/…` | 브랜딩 계열 규칙(라운드 스퀘어 rx 232 · 세로 그라디언트) · 문서 4층 체계 |

## 2. 확정 결정(요약 — 전문 [docs/10](docs/10-decision-record.md))

| # | 결정 |
| --- | --- |
| DR-1 | **C**로 쓴다(맥은 .m 한 파일). Rust std만으로도 수백 KB라 "극단적 최소화"에 안 맞는다 |
| DR-2 | 코어(`src/core`)는 **libc·부동소수점·힙 금지** — 정수 래스터라이저 · 3-OS 동일 픽셀 · Windows CRT 미링크 가능 |
| DR-3 | Linux는 **libdbus 없이 D-Bus를 직접 구현**(`src/plat/dbus.c`) → musl 정적 · libc 하나 |
| DR-4′ | UI = 메뉴 + **입력 창 1개**(일·시·분 숫자 + 시작/저장) + About. 서브메뉴 방식은 사용자 QA로 폐기(09-12) |
| DR-5 | 대기 아이콘(`icon_idle.c`)과 동작 아이콘(`icon_active.c`)은 **별도 모듈** · 대기 = 앱 아이콘 모티프 단색 |
| DR-6 | 메뉴 트리는 코어(`app.c`)에 한 번만 두고 각 OS가 그대로 네이티브 메뉴로 옮긴다 |
| DR-7 | 작업 종료 시 메모리 회수 — 아이콘 버퍼 free + OS별 반납(`SetProcessWorkingSetSize` / `malloc_zone_pressure_relief` / `malloc_trim`) |
| DR-14 | **창은 자식 프로세스**(입력 창·About) — 자기 자신 `--dialog`/`--about` 재실행 · stdout 파이프. 프레임워크 텍스트 캐시가 프로세스 안에선 안 돌아와서(09-13 실측) |
| DR-8 | 덮개 닫힘은 Windows·macOS에선 관리자 정책 영역 — 앱이 손대지 않고 문서에 명시. Linux만 logind로 막는다 |
| DR-13 | i18n = 코어 문구 블록(언어당 NUL 구분 문자열 1개 · 포인터 테이블 없음). "언어 ▸" 메뉴에서 고른 뒤에만 `lang=` 저장, 아니면 OS 로케일 자동 |

## 3. 구조

```
src/core/   coffee.h · util.c · timer.c · font.c · draw.c · icon_idle.c · icon_active.c · app.c   ← 플랫폼 비종속
src/plat/   win.c(Win32 · CRT 없음) · mac.m(AppKit+IOKit) · linux.c(SNI·dbusmenu·억제) · dbus.c/h(미니 D-Bus)
test/       test_core.c(단위) · icon_dump.c(PAM 덤프)     scripts/  linux-smoke.sh(D-Bus 계약) · dbus-test.conf · linux-docker.sh
packaging/  branding(SVG SSOT = tools/gen-icon.py) · macos · linux · homebrew · winget · choco
```

## 4. 작업 규약

- 문서·커밋/푸시 규약 SSOT = [docs/16](docs/16-doc-git-conventions.md). 기록: journal 상세 → DEVLOG 요약 → STATUS/TODO.
- **큰 단위 = 브랜치, 세부 = 커밋. push·태그는 사용자 명시 요청 시에만.** `git add <파일>`만(`-A`·`.` 금지).
- 🔴 push 전: `make test` · `make win` · `sh scripts/linux-smoke.sh`(brew dbus) · macOS `make app` 실행 확인. Linux PC에는 mingw/musl이 없어 `make win`·`make app`은 CI(`gh run watch`)로 대신한다. 크기 예산(CI): Windows ≤ 64KB · Linux 정적 ≤ 128KB · macOS 유니버설 ≤ 256KB.
- 코어에 libc 호출·float·malloc을 넣지 않는다(Windows 링크가 깨진다). 플랫폼 파일만 OS API를 만진다.
- 아이콘을 바꾸면 `make test` 후 `build/icons/*.pam`을 눈으로 확인(magick으로 PNG 변환) — 16px 두 자리 숫자 선명도 기준.

## 5. 다음 단계

0. **채널 마무리(T-6)** — winget #436346 머지 대기(할 일 없음) · choco는 0.1.0이 승인돼야 0.1.1을 올린다. ⚠️ 검수 중인 **0.1.0은 32비트에서 깨진 상태**(x86 zip을 Defender가 격리 · 64비트는 무사)라, 모더레이터에게 reject를 요청하는 편이 빠르다(웹 로그인 필요 · 코멘트 API 없음). 검수 상태는 공개 API가 없어 choco는 패키지 페이지 HTML, winget은 PR 라벨로 본다.
1. Windows 남은 실기 둘(아이콘 DPI · TaskbarCreated) — 절차는 [18 실기 점검표](docs/18-build-and-test.md#사람이-해야-하는-실기-점검windows). 그 과정에서 **T-18**(대기 중 DPI 변경 시 아이콘 미갱신)도 재현된다.
2. macOS — 언어 메뉴 실기(T-11) · 화면보호기 실측(T-4) · 로그인 항목 등록 안내. Linux — KDE Plasma · 입력 창 클릭 · logind polkit(T-3) · 16px 픽스맵(T-10).
3. ja/zh 원어민 검토(T-12) · 시작 프로그램 등록(T-5).

> 재릴리스 판단: v0.1.1 태그 이후 바뀐 것은 워크플로·패키징 문서·docs뿐이고 `src/`·`res/`·`VERSION`은 그대로다. **코드를 고칠 때(예: T-18) v0.1.2로 묶는다.**
