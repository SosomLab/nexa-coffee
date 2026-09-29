# MILESTONES

| 목표 | 상태 | 비고 |
| --- | --- | --- |
| M0 프로젝트 골격(3-OS 빌드 · 문서 · CI) | ✅ 09-12 | 한 세션 |
| M1 코어 — 타이머 표시 규칙 · 아이콘 2종 · 메뉴 모델 · 설정 · i18n | ✅ 09-12 | `test_core` CHECK 71곳 · CI 3-OS green |
| M2 macOS — 메뉴바 · 어설션 · 실기 | ✅ 09-12 | 유니버설 154 KB · 대기 6.0 MB. 남음: 화면보호기 실측(T-4) · 언어 메뉴 실기(T-11) |
| M3 Windows — 트레이 · 실행 상태 · CRT 없음 | ✅ 실기 09-17 | mingw x64 30,208 B(버전 리소스 포함). 트레이·메뉴 라디오·1초 갱신·`powercfg`·i18n·DR-14 파이프 모두 확인. 09-26 T-18(배율 변경 시 아이콘) 코드 수정 → v0.1.2. 남음: 아이콘 DPI·TaskbarCreated 실기(사람 손 — [18 점검표](18-build-and-test.md#사람이-해야-하는-실기-점검windows)) |
| M4 Linux — 자체 D-Bus · SNI · dbusmenu · ScreenSaver/logind | ✅ GNOME 실기 09-13·09-26 | 정적 79 KB(musl) · 16/22/44 픽스맵(T-10) · **늦게 뜬 워처 크래시 수정(T-19 · v0.1.2)** · polkit 거부 재시도·재등록·Inhibit 해제를 가짜 워처·logind로 CI 자동 검증 · `install.sh --autostart`. 남음: KDE Plasma · 입력 창 실제 클릭(T-3) |
| M5 배포 — Releases 워크플로 · brew/winget/choco 등록 | 🚧 brew ✅ 0.1.2 · choco ✅ 0.1.0 · winget 검증 통과 | v0.1.2(09-26 · 릴리스 본문 = `docs/releases/<태그>.md`). choco 0.1.0 승인(09-28) → 0.1.2 제출(09-29 · 모더레이션 중) · winget [PR #436346](https://github.com/microsoft/winget-pkgs/pull/436346)(0.1.1) 머지 대기(T-6) |

**v1 남은 축**: 코드 서명·공증(없어서 SmartScreen·Gatekeeper 경고 · 백신 오탐의 근본 원인) · 시작 프로그램 등록(T-5 — Linux는 `--autostart` ✅ · 앱 메뉴 토글은 결정 대기) · ja/zh 원어민 검토(T-12).
