# MILESTONES

| 목표 | 상태 | 비고 |
| --- | --- | --- |
| M0 프로젝트 골격(3-OS 빌드 · 문서 · CI) | ✅ 09-12 | 한 세션 |
| M1 코어 — 타이머 표시 규칙 · 아이콘 2종 · 메뉴 모델 · 설정 · i18n | ✅ 09-12 | `test_core` CHECK 71곳 · CI 3-OS green |
| M2 macOS — 메뉴바 · 어설션 · 실기 | ✅ 09-12 | 유니버설 154 KB · 대기 6.0 MB. 남음: 화면보호기 실측(T-4) · 언어 메뉴 실기(T-11) |
| M3 Windows — 트레이 · 실행 상태 · CRT 없음 | ✅ 실기 09-17 | mingw x64 30,208 B(버전 리소스 포함). 트레이·메뉴 라디오·1초 갱신·`powercfg`·i18n·DR-14 파이프 모두 확인. 남음: 아이콘 DPI·TaskbarCreated(사람 손 — [18 점검표](18-build-and-test.md#사람이-해야-하는-실기-점검windows)) |
| M4 Linux — 자체 D-Bus · SNI · dbusmenu · ScreenSaver/logind | 🚧 GNOME 실기 ✅ 09-13 | 정적 51 KB · 등록 경쟁 수정. 남음: KDE Plasma · 입력 창 클릭 · polkit 거부 fallback(T-3) · 16px 픽스맵(T-10) |
| M5 배포 — Releases 워크플로 · brew/winget/choco 등록 | 🚧 brew ✅ · winget 검증 통과 · choco 검수 중 | v0.1.1. brew 탭 설치 실사용 ✓ · winget [PR #436346](https://github.com/microsoft/winget-pkgs/pull/436346) `Azure-Pipeline-Passed` → 머지 대기 · choco 0.1.0 모더레이션 대기(승인돼야 0.1.1)(T-6) |

**v1 남은 축**: 코드 서명·공증(없어서 SmartScreen·Gatekeeper 경고 · 백신 오탐의 근본 원인) · 시작 프로그램 등록(T-5) · ja/zh 원어민 검토(T-12).
