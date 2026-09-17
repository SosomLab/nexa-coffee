# 18 · 빌드 · 테스트 (SSOT)

## 빌드

| 대상 | 명령 | 도구 | 산출물 |
| --- | --- | --- | --- |
| macOS(유니버설) | `make` · `make app` | Xcode CLT(clang) | `dist/nexa-coffee` · `dist/Nexa Coffee.app` |
| Linux(동적) | `make` | gcc | `dist/nexa-coffee` |
| Linux(정적 · 배포용) | `make static` | `musl-tools` | `dist/nexa-coffee`(libc 하나 · 배포판 비종속) |
| Windows(크로스) | `make win` | mingw-w64 | `dist/nexa-coffee-x64.exe` · `-x86.exe` |
| Windows(MSVC) | `build.bat` | VS x64 Native Tools | `dist\nexa-coffee-x64.exe` |

플래그 원칙(nexa-shortcut 계승): `-Os` · `-fno-ident` · `-fno-asynchronous-unwind-tables` · `-fno-stack-protector` · 섹션 GC · strip.
Windows는 `-nostdlib` + `-fno-builtin -fno-tree-loop-distribute-patterns`(코어의 바이트 루프가 memset 호출로 되접히는 것 방지) + 진입점 `start`.
x86은 64비트 나눗셈 헬퍼 때문에 `-lgcc`를 정적으로 붙인다.

## 테스트

| 단계 | 명령 | 검사 |
| --- | --- | --- |
| 코어 단위 | `make test` | util · 표시 규칙 · 다음 변화 시각 · 메뉴 트리/클릭 · 설정 왕복 · 툴팁 · 아이콘 결정성/경계 |
| 아이콘 육안 | `make test` → `build/icons/*.pam` | `magick x.pam -background '#1e1e1e' -flatten -filter point -resize 800% x.png` |
| Linux D-Bus 계약 | `sh scripts/linux-smoke.sh dist/nexa-coffee` | 세션 버스 위에서 SNI 속성 · dbusmenu 레이아웃/속성 · Event 클릭 → 작업/설정 · 종료 |
| macOS 실기 | `make app && open "dist/Nexa Coffee.app"` — **확인 뒤 `make clean`**(dist 번들이 설치본과 함께 Launchpad에 보인다 · 09-13) | 메뉴바 아이콘 · `pmset -g assertions \| grep Coffee` · `ps -o rss` · 창 확인은 `NEXA_COFFEE_SHOW=custom\|auto\|about dist/nexa-coffee` |
| Windows 실기 | exe 실행 | 트레이 · 우클릭 메뉴 · `powercfg /requests`(SYSTEM/DISPLAY에 nexa-coffee). 사람 손이 필요한 둘은 ↓ [실기 점검표](#사람이-해야-하는-실기-점검windows) |

`linux-smoke.sh`는 시스템 세션 버스를 쓰지 않고 `scripts/dbus-test.conf`로 전용 dbus-daemon을 띄운다(macOS launchd·CI 컨테이너 공통). macOS는 `brew install dbus`.
Docker가 있으면 `scripts/linux-docker.sh`가 Linux 컨테이너에서 빌드+정적+스모크를 한 번에 돌린다.

## 사람이 해야 하는 실기 점검(Windows)

트레이 메뉴·입력 창·`powercfg`는 자동화했다(09-17 — 부모 창에 `WM_TRAYICON`을 post해 메뉴를 띄우고 팝업을 캡처).
아래 **둘은 화면 설정이나 셸을 건드려야 해서 자동화하지 않는다.** 남은 T-2 항목이다.

### 준비(공통)

1. 실행 파일: 릴리스 자산의 `nexa-coffee-x64.exe`(또는 `build.bat` 산출물). 설치본이 아니어도 된다.
2. 설정은 `%APPDATA%
exa-coffee\config`에 있다. **먼저 복사해 두고 끝나면 되돌린다.**
   ```
   copy "%APPDATA%
exa-coffee\config" "%TEMP%
c-config.bak"
   ```
3. **트레이 아이콘을 보이게 한다** — Windows 11은 기본으로 오버플로(`^`) 안에 숨긴다.
   설정 → 개인 설정 → 작업 표시줄 → **기타 시스템 트레이 아이콘** → `nexa-coffee` 토글 켜기.
   (이걸 안 하면 아래 두 점검 모두 "아이콘이 안 보인다"로 잘못 판정하게 된다.)
4. 숫자 아이콘을 보려면 **작업을 시작해야 한다** — 메뉴에서 `12시간`을 고르면 아이콘에 `12`가 그려진다(두 자리가 선명도 기준).

### ① 아이콘 DPI (100 / 150 / 200%)

**왜**: 아이콘은 코어 정수 래스터라이저가 `GetSystemMetrics(SM_CXSMICON)` 크기로 직접 그린다 —
100%=16px · 150%=24px · 200%=32px. 셸이 늘려 그리는 게 아니라 **그 크기로 새로 그려야** 두 자리 숫자가 선명하다.

| # | 절차 | 합격 기준 |
| --- | --- | --- |
| 1 | 배율 100%에서 앱 실행 → `12시간` 선택 | 아이콘의 `12`가 또렷하다(기준 이미지) |
| 2 | 앱을 **끄고** 설정 → 시스템 → 디스플레이 → **배율 150%** → 앱 다시 실행 → `12시간` | `12`가 100%와 같은 선명도. 흐릿하거나 뭉개지면 실패 |
| 3 | 배율 **200%**로 바꾸고 2를 반복 | 〃 |
| 4 | **앱을 켠 채(작업 동작 중)** 배율을 바꾼다 | **1초 안에** 새 크기로 다시 그려진다(`job_tick()`이 매 틱 `icon_size()`를 다시 읽는다) |
| 5 | **앱을 켠 채(대기 중)** 배율을 바꾼다 | ⚠️ **현재는 안 바뀐다** — 알려진 구멍([T-18](TODO.md)). 바뀌면 T-18이 이미 고쳐진 것 |
| 6 | 배율 100%로 되돌린다 | — |

**실패하면 의심할 곳**: `icon_size()`(`src/plat/win.c`)의 `SM_CXSMICON` 값 · `enable_dpi()`의 Per-Monitor V2 설정 ·
`cf_icon_active()`의 해당 크기 래스터(먼저 `make test` 후 `build/icons/*.pam`을 키워서 눈으로 본다).

### ② TaskbarCreated (탐색기 재시작 후 트레이 복구)

**왜**: 탐색기가 죽었다 살아나면 트레이가 통째로 비워진다. 앱은 `RegisterWindowMessageW("TaskbarCreated")` 브로드캐스트를
받아 아이콘을 **다시 등록**해야 한다. 이걸 안 하면 탐색기 재시작 이후 앱은 살아 있는데 아이콘만 영영 사라진다.

> 합성 테스트로는 증명되지 않는다 — 같은 메시지를 앱에 직접 보내도 `tray_set(add=TRUE)`는 `NIM_ADD`만 하므로
> 아이콘이 이미 있으면 그냥 실패한다(실제 상황에서는 아이콘이 없으니 `NIM_ADD`가 맞다). **진짜 재시작이 필요하다.**

| # | 절차 | 합격 기준 |
| --- | --- | --- |
| 1 | 앱 실행 → `12시간` 선택(동작 중 상태로 만든다) | 아이콘에 `12` · `powercfg /requests`에 SYSTEM·DISPLAY 등록 |
| 2 | 작업 관리자(Ctrl+Shift+Esc) → `Windows 탐색기` → **다시 시작** (화면이 잠깐 깜빡인다) | — |
| 3 | 10초 안에 트레이를 본다 | **아이콘이 돌아온다** · 숫자가 이어진다(0부터 다시 세지 않는다) |
| 4 | 아이콘에 마우스를 올린다 | 툴팁이 남은 시간을 정상 표시 |
| 5 | `powercfg /requests` | 억제가 **끊기지 않고** 유지된다 |
| 6 | 대기 상태에서도 1~3을 반복 | 대기 아이콘이 돌아온다 |

**실패하면 의심할 곳**: `wnd_proc`의 `if (msg == g_taskbar_created)` 분기 · `g_taskbar_created` 등록 시점 ·
`tray_set(..., TRUE)`의 `NIM_ADD` 반환값(실패 시 `NIM_MODIFY` 재시도가 필요한지).

### 마무리

- 설정 되돌리기: `copy "%TEMP%
c-config.bak" "%APPDATA%
exa-coffee\config"`
- 남은 프로세스 확인: `tasklist | findstr nexa-coffee` · `powercfg /requests`에 nexa 항목이 없어야 한다
- 결과는 [TODO](TODO.md) T-2(필요하면 T-18)와 그날 [journal](journal/)에 **실측값과 함께** 적는다 —
  "확인함"이 아니라 "150%에서 16px 아이콘이 늘어나 흐릿했다"처럼.

## 메모리 실측(macOS)
`footprint --pid <pid>`의 `phys_footprint`가 기준(RSS는 공유 캐시 포함). 실험 절차는 journal 09-13 2차. 아이콘·리소스 크기는 `ls -la res/ packaging/branding/`.

## CI(.github/workflows)

- `ci.yml`: ubuntu(테스트 · musl 정적 · D-Bus 스모크 · mingw 크로스 · **크기 예산** Windows ≤ 64 KB / Linux ≤ 128 KB) · macos(테스트 · 유니버설 · .app · ≤ 256 KB) · windows(MSVC).
- `release.yml`: `v*` 태그 → mac universal zip · Windows x64/x86 zip · Linux tar.gz + `SHA256SUMS.txt` → GitHub Release → `homebrew.yml`(탭) · `publish-windows-packages.yml`(winget PR · choco push). 태그 = `VERSION` 파일과 일치해야 한다.
- 릴리스 절차: `VERSION` 갱신 → main green → `git tag vX.Y.Z && git push origin vX.Y.Z`(공개 행위 · 사용자 승인) → `gh run watch` → 탭 커밋 · winget PR · choco 피드 확인(검수 며칠). 재제출은 각 워크플로 `workflow_dispatch`.
- GitHub 장애 시 수동 경로(09-13 실전): `gh run download <run> -n out-linux-windows -n out-macos` → `gh release create vX.Y.Z <자산> SHA256SUMS.txt` → `./packaging/render-manifests.sh X.Y.Z assets out` → 탭에 `out/homebrew/*` 커밋 · `komac submit out/winget/manifests/s/SosomLab/NexaCoffee/X.Y.Z --yes` · Windows PC에서 `choco pack`/`choco push`.
