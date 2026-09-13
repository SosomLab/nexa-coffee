# CLAUDE.md — Nexa Coffee 프로젝트 컨텍스트 (이식용 메모리)

> 다른 PC에서 clone 시 즉시 컨텍스트를 복원하기 위한 휴대용 메모리. **먼저 읽기:** [docs/STATUS.md](docs/STATUS.md) → [docs/10-decision-record.md](docs/10-decision-record.md).

## 1. 이 프로젝트는

**Nexa Coffee** = 트레이/메뉴바 절전 방지 타이머(Windows · macOS · Linux). **순수 C · 프레임워크 0 · 외부 라이브러리 0 · 극단적 최소화**.

- 조직: SosomLab · 개발자: Sangyong Bae · kiros33@gmail.com · 저장소: <https://github.com/SosomLab/nexa-coffee>
- 라이선스: **MIT**(누구나 무료 — nexa-shortcut과 동일. beep/clip/dir2의 PolyForm NC와 **다르다**).
- 현 단계: **v0.1.0 — 입력 창/About/남은 시간 메뉴/메모리 최적화 + 언어 메뉴(en/ko/ja/zh)까지 main push됨(09-13)** — 3-OS 빌드 green · 코어 테스트 green · macOS 실기 확인(언어 메뉴 이전) · **Linux GNOME 실기 확인**(09-13 · 등록 경쟁 수정) · Windows는 빌드만(실기 미확인).

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

0. 언어 메뉴(09-13)의 Windows·macOS 빌드/실기 확인(T-11) — Linux PC에서 구현해 CI 빌드만 거쳤다. ja/zh 문구 원어민 검토(T-12).
1. Windows 실기 확인(트레이 · DPI별 아이콘 크기 · 메뉴 라디오 표시 · 절전 억제 `powercfg /requests`).
2. Linux 남은 실기(KDE Plasma · 입력 창 클릭 · logind polkit) · 16px 픽스맵 프레임(T-10 — GNOME이 22→16 축소해 숫자 흐릿).
3. macOS 화면보호기 실측(30초 사용자 활동 선언이 충분한지) · 로그인 항목 등록 안내.
4. 패키지 채널 등록(brew tap · winget · choco) — 템플릿은 packaging/에 있다.
