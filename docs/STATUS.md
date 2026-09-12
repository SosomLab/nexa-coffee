# STATUS — 지금 상태

## 09-13 3차 — 커밋 · main 병합 · push (사용자 요청)

- `feat/dialog-ui-memory` 6커밋(core → win → mac → linux → 브랜딩/도구 → docs) → main ff 병합 → push. push 전 test·3-OS 빌드·smoke green 재확인.
- 포함: 09-12 3차(입력 창·About·자동 시작 2택) + 09-13 1차(메뉴 남은 시간) + 09-13 2차(메모리·자원).
- **다음**: CI 결과 확인 → Windows/Linux 실기(T-2·T-3).

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
