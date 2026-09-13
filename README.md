# Nexa Coffee

**트레이/메뉴바에서 남은 시간을 보여주며 PC가 잠들지 않게 하는 초경량 타이머** — Windows · macOS · Linux.

순수 C(맥은 Objective-C 한 파일) · 프레임워크 0 · 외부 라이브러리 0. Windows는 CRT조차 링크하지 않고(**약 33 KB**),
Linux는 libdbus 없이 D-Bus를 직접 말하며(musl 정적 · libc 하나), macOS는 AppKit·IOKit만 쓴다.
아이콘은 정수 연산 전용 자체 래스터라이저가 그려 **세 OS에서 같은 픽셀**이 나온다.

- **SosomLab**: <https://sosomlab.com> · 저장소: <https://github.com/SosomLab/nexa-coffee>
- 형제 프로젝트: [nexa-shortcut](https://github.com/SosomLab/nexa-shortcut)(초경량 원칙) · [nexa-clip](https://github.com/SosomLab/nexa-clip)(트레이·SNI 계약)

## 하는 일

| 상태 | 트레이 아이콘 | 동작 |
| --- | --- | --- |
| 대기 | 앱 아이콘과 같은 링(실선 1/3 · 옅은 실선 2/9 · 점 4/9)을 단색으로 | 아무것도 하지 않는다 |
| 동작 | 얇은 외곽선 + **남은 비율만큼의 굵은 호**(꼬리가 그라데이션으로 사라짐) + 가운데 **정수** | 화면보호기 · 화면 끄기 · 절전 · 최대 절전 진입을 막는다 |

가운데 숫자는 **단위별 색**으로 구분한다: 일 = 보라 · 시간 = 초록 · 분 = 주황 · 초 = 빨강 · 무제한 = 파랑 ∞.
24h 이상은 일, 1h 이상은 시간, 59분~1분은 분, 59초 이하는 초(모두 내림).

좌/우클릭 메뉴 맨 위에는 **남은 시간이 `0 days 12 hours 50 minutes 3 seconds`(1보다 크면 복수형 · 한국어 `0일 12시간 50분 3초`) 형식으로 1초마다 갱신**된다.
그 아래: **무제한 · 12시간 · 6시간 · 2시간 · 1시간 · 30분 · 사용자 지정… · 끄기 — 실행 시 자동 시작 ▸(끄기 / 사용자 지정…) · 언어 ▸(English / 한국어 / 日本語 / 中文) — 정보… · 종료**.
언어는 OS 로케일을 따르다가 메뉴에서 고르면 그 선택이 설정에 저장된다(`lang=ja`).
"사용자 지정…"은 `[ 0 ] 일 [ 12 ] 시간 [ 50 ] 분  [시작]` 한 줄짜리 입력 창을 띄운다. 자동 시작의 "사용자 지정…"도 같은 창(버튼은 저장)이며,
저장한 시간으로 다음 실행 때 바로 시작한다. "정보…"는 About 화면.

| OS | 입력 창 | About |
| --- | --- | --- |
| Windows | 리소스 대화상자(CRT 없음) | MessageBox |
| macOS | NSPanel(모달) | 표준 About 패널 |
| Linux | 외부 도구 `yad` → `zenity` → `kdialog`(설치된 것) — 없으면 알림으로 안내 | zenity/kdialog 메시지 · 없으면 알림 |

타이머가 끝나면 절전 억제를 풀고 작업에 쓴 메모리를 모두 돌려준다(아이콘 버퍼 해제 · 작업 집합/힙 반납).

### OS별 절전 억제 수단과 한계

| OS | 수단 | 덮개 닫힘 |
| --- | --- | --- |
| Windows | `SetThreadExecutionState(ES_SYSTEM_REQUIRED \| ES_DISPLAY_REQUIRED)` | ✖ 전원 정책(`powercfg /setacvalueindex SCHEME_CURRENT SUB_BUTTONS LIDACTION 0` · 관리자) |
| macOS | IOKit `PreventUserIdleSystemSleep` + `PreventUserIdleDisplaySleep` 어설션 + 30초마다 사용자 활동 선언 | ✖ OS 강제 절전(`sudo pmset -a disablesleep 1`) |
| Linux | `org.freedesktop.ScreenSaver.Inhibit` + logind `Inhibit("sleep:idle:handle-lid-switch", block)` | ✅ logind가 막는다(polkit 기본 허용) |

## 설치

[GitHub Releases](https://github.com/SosomLab/nexa-coffee/releases)에서 받는다 — `SHA256SUMS.txt`로 확인.
⚠️ v1은 **코드 서명이 없다**(macOS Gatekeeper · Windows SmartScreen 경고).

| OS | 파일 | 실행 |
| --- | --- | --- |
| Windows | `nexa-coffee-<v>-windows-x64.zip`(x86도 있음) | 압축 풀고 `nexa-coffee-x64.exe` 실행 · 시작 프로그램 등록은 `Win+R` → `shell:startup`에 바로가기 |
| macOS | `nexa-coffee-<v>-macos-universal.zip` | `xattr -dr com.apple.quarantine "Nexa Coffee.app"` 후 응용 프로그램 폴더로 · 로그인 항목에 추가 |
| Linux | `nexa-coffee-<v>-linux-x64.tar.gz` | `./install.sh`(~/.local) · 트레이 호스트 필요(KDE 기본 · GNOME은 AppIndicator 확장) |

패키지 관리자(brew · winget · choco) 매니페스트는 [`packaging/`](packaging/README.md)에 준비돼 있다(등록은 후속).

## 빌드

```bash
make            # macOS/Linux 네이티브 → dist/nexa-coffee
make app        # macOS .app 번들
make static     # Linux musl 정적(배포판 비종속)
make win        # Windows x64/x86 크로스(brew install mingw-w64 / apt install gcc-mingw-w64)
make test       # 코어 단위 테스트 + 아이콘 덤프(build/icons/*.pam)
sh scripts/linux-smoke.sh dist/nexa-coffee   # D-Bus 계약 스모크(dbus-daemon 필요 · macOS는 brew install dbus)
build.bat       # Windows MSVC(x64 Native Tools 프롬프트)
```

설계·결정·현황은 [`docs/`](docs/README.md), 새 세션 진입점은 [CLAUDE.md](CLAUDE.md).

## 라이선스

[MIT](LICENSE) — 누구나 무료로 사용·복사·수정·배포할 수 있다. ([한국어 번역](LICENSE.ko.md))
