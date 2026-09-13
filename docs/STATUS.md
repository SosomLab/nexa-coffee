# STATUS — 지금 상태

## 09-13 13차 — brew 설치 실사용 확인 · winget 매니페스트 버전 불일치 수정 (사용자 QA)

- brew Cask 설치 실사용 ✓(사용자). 로컬 테스트 번들(dist)이 함께 보여 제거 · 설치본만 남김 → 절차에 `make clean` 추가.
- winget PR #433980: 검증 오류 = ManifestVersion 불일치(installer 1.12.0 vs 나머지 1.6.0 · komac 제출 부작용) → PR 브랜치 수정 push · **봇 재검증 대기**. 템플릿 1.12.0 통일.
- 상세: [journal/2026-09-13](journal/2026-09-13.md#13차--brew-설치-실사용--테스트-번들-정리--winget-검증-오류-수정-사용자-qa).

## 09-13 12차 — v0.1.0 배포 완료 (사용자 요청)

| 채널 | 상태 |
| --- | --- |
| GitHub Release v0.1.0 | ✅ 자산 5개(mac universal zip · Win x64/x86 zip · Linux tar.gz · SHA256SUMS) — API 500 장애로 수동 생성 |
| Homebrew `brew install --cask kiros33/tap/nexa-coffee` | ✅ 탭 반영 · `brew fetch` sha 검증 ✓(Linux formula `nexa-coffee-portable`도 반영) |
| Chocolatey `choco install nexa-coffee` | ✅ push 완료 · **모더레이션 대기**(며칠) |
| winget `winget install SosomLab.NexaCoffee` | ✅ PR [#433980](https://github.com/microsoft/winget-pkgs/pull/433980) · **검수 대기** |

- 상세·장애 대응: [journal/2026-09-13](journal/2026-09-13.md#12차--v010-릴리스-실행-결과-사용자-요청). 추적: T-6(검수 상태) · T-15(cask postflight_steps).

## 09-13 11차 — v0.1.0 릴리스 · 패키지 관리자 배포 (사용자 요청)

- 파이프라인: 태그 `v0.1.0` → release(5 산출물 + SHA256SUMS) → homebrew(탭 커밋) · publish-windows-packages(choco push · winget PR). 결과는 아래 12차에.
- 포함: T-14(창 자식 프로세스) · Windows T-13 · 배포 파이프라인. 상세: [journal/2026-09-13](journal/2026-09-13.md#11차--v010-릴리스--brewwingetchoco-배포-파이프라인-사용자-요청).

## 09-13 10차 — T-14 구현: 창(입력 창·About)을 자식 프로세스로 · Windows T-13 (사용자 요청 · 미커밋)

- **DR-14**: 3-OS 모두 창은 자식 프로세스(자기 자신 `--dialog`/`--about` 재실행 → stdout "d h m"). macOS 실측 부모 **6.0 MB 유지**(이전 11.3 MB 잔류). Windows는 크로스 빌드만(실기 ☐). Linux는 파서 공용화.
- **사용자 확인 필요**: 입력 창에서 시작 버튼 → 작업 시작(파이프 경로) — 자동화는 키 입력 없이 취소 경로만 검증. 상세: [journal/2026-09-13](journal/2026-09-13.md#10차--t-14-구현-창을-자식-프로세스로-사용자-요청-구현해줘).

## 09-13 9차 — Mac: 릴리즈 빌드 재시작 · 실행 용량/메모리 점검 (사용자 요청 · 미커밋)

- 실행 파일: 유니버설 154 KB(x86_64 55 KB · arm64 89 KB — 16 KB 정렬) · 번들 276 KB. 더 뺄 것 없음.
- 메모리: 대기 **6.1 MB**는 최소 상태. 그러나 **입력 창 1회 사용 후 11.3 MB로 늘어 내려오지 않음**(About +2.9 · 메뉴 +2.2 MB) — Windows 8차 QA와 같은 구조. pressure relief는 효과 미미.
- 제안 **T-14**: 창을 자식 프로세스로(Linux zenity와 같은 구조 · 3-OS 통일). 상세: [journal/2026-09-13](journal/2026-09-13.md#9차--mac-pull8차--릴리즈-빌드-재시작--실행-용량메모리-최소화-점검-사용자-요청).

## 09-13 8차 — Windows 실기 첫 확인 · 상주 메모리 분석 (사용자 요청 · 사용자 QA)

- **무엇**: Windows PC(VS 2022 Build Tools)에서 4~7차를 pull해 MSVC 로컬 빌드 **27,648 B** · `test_core` MSVC 실행 green(첫 로컬) · 릴리스 exe 실행 → 트레이 등록·툴팁 "Nexa Coffee — 대기 중"·한국어 로케일을 `NotifyIconSettings` 레지스트리로 확인. 아이콘·메뉴 시각 확인은 사용자(오버플로 "^"에 숨음).
- **메모리(사용자 QA)**: 작업 관리자 11.4 MB는 메뉴·입력 창 사용 후 **반납 지점이 없어** 남은 작업 집합. 실제 개인 작업 집합 476 K · 반납 직후 1.3 MB. PNG 프레임 ICO가 시작 시 WindowsCodecs를 올리는 것도 확인(BMP ICO 실험으로 검증). 조치안 **T-13**(UI 닫힘 뒤 반납 · 클래스 아이콘 코어 생성).
- **사용자 보고**: 3-OS 빌드·테스트 완료(Mac 7차 + Windows 8차). T-2 ◐.
- **다음**: T-13 → T-2 남은 실기(DPI · 메뉴 1초 갱신 재그리기 · `powercfg /requests`) → T-11. 상세: [journal/2026-09-13](journal/2026-09-13.md#8차--windows-실기-첫-확인--상주-메모리-분석-사용자-요청--사용자-qa--windows-pc).

## 09-13 7차 — Mac에서 pull 후 로컬 검증 · 릴리즈 빌드 실행 (사용자 요청)

- **무엇**: 5·6차(Linux PC 작업 · Windows/macOS는 CI 컴파일만)를 이 Mac에서 직접 빌드·실행. `make test` green · macOS 유니버설 154 KB + .app 276 KB 실행(고유 메모리 6.0 MB) · Windows 크로스 x64 **27,136 B** / x86 29,696 B(i18n 문자열 +0.5 KB) · linux-smoke(언어 왕복 포함) green.
- **T-11 부분 해소**: 두 OS 빌드 확인 완료. 남은 것은 실기(언어 메뉴 CJK 글꼴 · 입력 창 라벨 폭 · Windows 간체/번체).
- **다음**: T-11 실기 → T-10(16px 픽스맵) → T-12. 상세: [journal/2026-09-13](journal/2026-09-13.md#7차--mac에서-pull-후-로컬-검증--릴리즈-빌드-실행-사용자-요청).

## 09-13 6차 — 커밋 · main 병합 · push · CI (사용자 요청)

- `feat/i18n-lang-menu` 4커밋(core → plat → docs → 진행사항) → main ff → push. 앞선 0717da7(GNOME 등록 경쟁 수정)도 같이 올라감.
- **CI(run 34736693426 · 0062ce3)**: linux ✓ 28s · windows-msvc ✓ 23s · macos ✓ 19s — **i18n 코드가 MSVC·clang에서도 컴파일·크기 예산 통과**(UTF-8 CJK 리터럴 · NUL 블록). main green.
- **다음**: T-11(Windows·macOS 언어 메뉴 실기) · T-12(ja/zh 원어민 검토) · T-2 · T-3 남은 항목 · T-10.

## 09-13 5차 — 언어 선택 메뉴 · i18n(en/ko/ja/zh) (사용자 요청) — `feat/i18n-lang-menu` 4커밋 → main ff → push

- **무엇**: 자동 시작 아래 "언어 ▸ English / 한국어 / 日本語 / 中文"(라디오). 고르면 `lang=` 저장, 안 고르면 OS 로케일 자동. 툴팁 어순 ja/zh는 "残り2時間 / 剩余2小时".
- **크기**: 문구를 언어당 NUL 구분 문자열 블록 하나로(포인터 테이블·재배치 없음). Linux 51,416 B 그대로.
- **검증**: test green(네 언어 전 문구 검사 포함) · smoke green(ja 전환 왕복) · GNOME 실기 D-Bus 전환 ✓. **Windows·macOS는 이 PC에서 빌드 불가**(mingw/musl 없음) → CI 결과는 아래 6차 참조. 후속: T-11(두 OS 실기) · T-12(ja/zh 원어민 검토).
- 상세: [journal/2026-09-13](journal/2026-09-13.md#5차--언어-선택-메뉴english--한국어--日本語--中文--i18n-사용자-요청).

## 09-13 4차 — Linux 실기 첫 확인 · GNOME 트레이 아이콘 안 보임 수정 (사용자 QA)

- **증상/원인**: 등록·속성·로그 모두 정상인데 아이콘만 없음. `busctl monitor`로 보니 GNOME AppIndicator가 Register **응답 전에** `GetAll`을 보내고, 우리는 등록 응답 동기 대기 중 그 요청을 버리고 있었다(`register_watcher`의 `db_call(..., NULL)`).
- **수정**: 대기 중 요청도 `handle`로 처리(1줄). 재실행 후 대기/동작 아이콘 · 메뉴 라벨 · logind 억제 · 설정 저장까지 GNOME(Wayland) 실기 ✓.
- **남음**: 16px 픽스맵 프레임 추가(패널 16px 축소로 숫자 흐릿) · KDE Plasma · 입력 창(zenity 경로)은 사람이 클릭해서 확인. 상세: [journal/2026-09-13](journal/2026-09-13.md#4차--linux-실기gnome-50--ubuntu-appindicators-트레이-아이콘-안-보임--등록-경쟁-수정-사용자-qa).

## 09-13 3차 — 커밋 · main 병합 · push (사용자 요청)

- `feat/dialog-ui-memory` 6커밋(core → win → mac → linux → 브랜딩/도구 → docs) → main ff 병합 → push. push 전 test·3-OS 빌드·smoke green 재확인.
- 포함: 09-12 3차(입력 창·About·자동 시작 2택) + 09-13 1차(메뉴 남은 시간) + 09-13 2차(메모리·자원).
- **CI(run 34706566010)**: linux ✓ · macos ✓ · windows-msvc ✓ — main green.
- **다음**: Windows/Linux 실기(T-2·T-3).

## 09-13 2차 — 상주 메모리 최소화 · 불필요 자원 검토 (사용자 요청)

- **핵심 수치**: macOS 고유 메모리 7.4 → **5.9 MB**(`MallocSpaceEfficient=1` 자체 세팅 후 재실행 + LSEnvironment). RSS 21 MB의 나머지는 공유 캐시라 프로세스가 줄일 수 없다.
- **자원**: Windows exe 37,888 → **26,624 B**(ICO PNG 프레임 3.7 KB) · icns 152 → 62 KB · 미사용 함수 4개 제거 · 정확한 크기 버퍼 · 시작 직후 힙/작업 집합 반납.
- **효과 없어 채택 안 함**: pressure relief 단독 · Prohibited 정책 · MallocNanoZone=0(실측 오차 범위).
- **검증**: test green · smoke green · 번들 실행 footprint 실측. 상세: [journal/2026-09-13](journal/2026-09-13.md#2차--상주-메모리-최소화--불필요-자원-검토-사용자-요청).

## 09-13 1차 — 메뉴 맨 위 남은 시간(1초 갱신) (사용자 요청)

- **무엇**: 좌클릭 메뉴 첫 줄에 `0 days 12 hours 50 minutes 3 seconds` / `0일 12시간 50분 3초`(비활성 항목)를 메뉴가 열린 동안 1초마다 갱신. 영어는 값이 1보다 크면 복수형(사용자 확정 — 0은 단수).
- **구현**: 코어 `cf_remaining_label` + id 14/15 · macOS common-modes 타이머 · Windows `WM_TIMER`+`SetMenuItemInfoW`+`#32768` 재그리기(실기 미확인) · Linux `ItemsPropertiesUpdated`(AboutToShow 후 30초 창).
- **검증**: test_core green · linux-smoke(라벨 정규식) green · macOS 메뉴 갱신은 눈으로 미확인(전체 화면 창이 메뉴바를 가려 `NEXA_COFFEE_SHOW=menu` 캡처 실패) — 사용자 확인 요청. Windows exe 37,888 B.
- **다음**: 커밋/푸시(요청 시) → T-2·T-3. 상세: [journal/2026-09-13](journal/2026-09-13.md).

## 09-12 3차 — 입력 창 · About · 자동 시작 2택 (사용자 QA)

- **무엇**: 사용자 지정을 서브메뉴 → **일·시·분 입력 창**으로(DR-4′). 자동 시작 ▸ 끄기/사용자 지정…(같은 창 · 저장). About 추가. 설정 형식 `auto=d,h,m` · `custom=d,h,m`.
- **구현**: Windows DIALOGEX+MessageBox(exe 35,840 B) · macOS NSPanel 모달+표준 About(유니버설 137 KB) · Linux yad/zenity/kdialog 자식 프로세스(비동기 파이프)+알림 fallback.
- **검증**: test_core green(입력 창 규칙·자동 시작·About 포함) · linux-smoke에 가짜 yad로 입력 창 경로까지 green(3회 반복) · macOS 자동 시작(auto=0,0,5) 실기 어설션 확인. 입력 창·About 화면은 macOS에서 `NEXA_COFFEE_SHOW=custom|about` 디버그 훅으로 띄워 스크린샷으로 확인.
- **다음**: 커밋/푸시(요청 시) → T-2·T-3. 상세: [journal/2026-09-12](journal/2026-09-12.md#4차--입력-창--about--자동-시작-2택).

## 09-12 2차 — 첫 커밋 · main 병합 · push (사용자 요청)

- **무엇**: `feat/scaffold` 브랜치에 수직 슬라이스 9커밋(저장소 골격 → 코어 → 테스트 → Windows → macOS → Linux → 빌드/패키징 → CI → 문서) → main 병합(fast-forward) → `origin/main` push → 브랜치 삭제.
- **검증**: push 직전 `make test` green · `make` · `make app` · `make win` · `scripts/linux-smoke.sh` green(09-12 1차 실측과 동일).
- **CI 첫 실행(run 34699256593)**: macos ✓ · windows-msvc ✓(MSVC로도 CRT 없이 링크됨) · linux ✗ — 스모크는 실제 Linux(ubuntu · musl 정적 바이너리)에서 all green이었으나 EXIT 트랩의 kill 실패가 bash -e에서 exit 1로 번짐 → 트랩 수정 커밋으로 재실행.
- **재실행(run 34699355800)**: linux ✓ · macos ✓ · windows-msvc ✓ — **main green**.
- **다음**: Windows/Linux 실기(T-2·T-3). 상세: [journal/2026-09-12](journal/2026-09-12.md).

## 직전(09-12 1차) — v0.1.0 골격 완성

- **무엇**: 사용자 요청 한 세션에 3-OS 프로젝트 구성 완료 — 코어(정수 래스터라이저 · 타이머 · 메뉴 모델 · 설정 · i18n) · Windows(CRT 없음) · macOS(AppKit/IOKit) · Linux(자체 D-Bus + SNI/dbusmenu + ScreenSaver/logind) · 브랜딩 · 패키징 템플릿 · CI/릴리스 워크플로 · 문서.
- **실측**: Windows x64 exe **33,280 B**(아이콘 15 KB 포함 · 코드 ~18 KB) · x86 35,840 B · macOS 유니버설 119 KB(단일 아키 ~40 KB) · macOS 상주 RSS **~21 MB**(AppKit 하한) · 코어 테스트 green · Linux D-Bus 스모크 green(macOS dbus-daemon 위에서 · Docker 데몬은 이 세션에서 뜨지 않았다).
- **확인된 것**: macOS 실기 — 자동 시작 시 초록 "1" 링 표시 · `pmset -g assertions`에 PreventUserIdleSystemSleep/DisplaySleep/UserIsActive 3종 · 대기 템플릿 아이콘.
- **미확인**: Windows 실기(빌드만) · Linux 실기(프로토콜 계약만 검증) · macOS 화면보호기 실측 · 메뉴 스크린샷(System Events 접근성 권한 없음).
- **다음**: 첫 커밋 → Windows/Linux 실기 → 채널 등록. 상세: [journal/2026-09-12](journal/2026-09-12.md).
