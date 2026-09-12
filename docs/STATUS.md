# STATUS — 지금 상태

## 09-12 2차 — 첫 커밋 · main 병합 · push (사용자 요청)

- **무엇**: `feat/scaffold` 브랜치에 수직 슬라이스 9커밋(저장소 골격 → 코어 → 테스트 → Windows → macOS → Linux → 빌드/패키징 → CI → 문서) → main 병합(fast-forward) → `origin/main` push → 브랜치 삭제.
- **검증**: push 직전 `make test` green · `make` · `make app` · `make win` · `scripts/linux-smoke.sh` green(09-12 1차 실측과 동일).
- **다음**: GitHub Actions ci.yml 첫 실행 확인(`gh run watch`) → Windows/Linux 실기(T-2·T-3). 상세: [journal/2026-09-12](journal/2026-09-12.md).

## 직전(09-12 1차) — v0.1.0 골격 완성

- **무엇**: 사용자 요청 한 세션에 3-OS 프로젝트 구성 완료 — 코어(정수 래스터라이저 · 타이머 · 메뉴 모델 · 설정 · i18n) · Windows(CRT 없음) · macOS(AppKit/IOKit) · Linux(자체 D-Bus + SNI/dbusmenu + ScreenSaver/logind) · 브랜딩 · 패키징 템플릿 · CI/릴리스 워크플로 · 문서.
- **실측**: Windows x64 exe **33,280 B**(아이콘 15 KB 포함 · 코드 ~18 KB) · x86 35,840 B · macOS 유니버설 119 KB(단일 아키 ~40 KB) · macOS 상주 RSS **~21 MB**(AppKit 하한) · 코어 테스트 green · Linux D-Bus 스모크 green(macOS dbus-daemon 위에서 · Docker 데몬은 이 세션에서 뜨지 않았다).
- **확인된 것**: macOS 실기 — 자동 시작 시 초록 "1" 링 표시 · `pmset -g assertions`에 PreventUserIdleSystemSleep/DisplaySleep/UserIsActive 3종 · 대기 템플릿 아이콘.
- **미확인**: Windows 실기(빌드만) · Linux 실기(프로토콜 계약만 검증) · macOS 화면보호기 실측 · 메뉴 스크린샷(System Events 접근성 권한 없음).
- **다음**: 첫 커밋 → Windows/Linux 실기 → 채널 등록. 상세: [journal/2026-09-12](journal/2026-09-12.md).
