# DEVLOG — 날짜별 요약(최신 위)

- **2026-09-17** — winget·choco 검수 점검: 둘 다 "대기"가 아니라 **우리 차례**였다. winget 설치 실패 원인을 Windows 실기로 규명 — Defender가 **x86 zip만** `Trojan:Win32/Tecabans.STV!cl`(클라우드 ML 오탐)로 격리(x64·매니페스트는 정상). 조치: PE 버전 리소스 추가(VERSION 단일 출처 · MSVC 30,720→31,744 B · **A/B 확인: 0.1.0 x86 격리 / 0.1.1 x86 통과**) · choco `iconUrl`→jsDelivr(모더레이터 Requirement) + **0.1.0 재제출 완료** · winget 템플릿 스키마 헤더(validate exit 40으로 죽던 워크플로) · 워크플로 `channels` 입력 · **v0.1.1 릴리스**(brew 탭 ✓). 제출 잡 버그 2건 수정 — choco 가드가 모더레이션을 못 알아봄(피드는 미승인도 `<entry>`로 준다 → `IsApproved` 판정 · 403의 원인) · `winget validate` exit 40은 경고라 통과 처리(러너 winget **v1.11.510** 확인). 릴리스 자산으로 x86 설치 성공 재확인 → #433980 닫고 **winget PR #436346**(0.1.1) 제출. `WINGET_TOKEN`에 `workflow` 스코프가 없어 포크 동기화가 막혀 API로 우회 → 사용자가 스코프 추가해 **T-17 해소**(포크 fast-forward ✓ · 스코프 진단 스텝 추가). **PR #436346 검증 통과**(`Azure-Pipeline-Passed`). → [journal](journal/2026-09-17.md)
- **2026-09-13 (14차)** — Windows 실기: DR-14 자식 창 ✅ · T-13 ✅(부모 모듈 37→25 · 코덱/TSF 제거 실측) · MSVC 30,720 B green. `lang=en`은 사용자 선택. → [journal](journal/2026-09-13.md#14차--windows에서-dr-14자식-프로세스-창t-13-실기-검증--v010-릴리스-확인-사용자-요청--windows-pc)
- **2026-09-13 (13차)** — brew 설치 실사용 ✓ · 테스트 번들 정리 · winget ManifestVersion 불일치 수정(PR 갱신 · 템플릿 1.12.0).
- **2026-09-13 (12차)** — v0.1.0 배포: Release(수동 · API 500) · brew 탭 ✓ · choco push ✓ · winget PR #433980. → [journal](journal/2026-09-13.md#12차--v010-릴리스-실행-결과-사용자-요청)
- **2026-09-13 (11차)** — v0.1.0 릴리스 + brew/winget/choco 배포 파이프라인(render-manifests · homebrew.yml · publish-windows-packages.yml). T-14 창 자식 프로세스 포함.
- **2026-09-13 (8차)** — Windows PC: MSVC 로컬 빌드 27,648 B · test_core MSVC green · 릴리스 실행·트레이 등록 확인. 메모리 QA: 11 MB = UI 사용 후 미반납 작업 집합(개인 476 K) · PNG ICO가 WindowsCodecs 로드 → T-13. → [journal](journal/2026-09-13.md#8차--windows-실기-첫-확인--상주-메모리-분석-사용자-요청--사용자-qa--windows-pc)
- **2026-09-13 (7차)** — Mac에서 i18n·GNOME 수정 pull 후 로컬 빌드 검증(Win 27,136 B · mac 154 KB · smoke green) · 릴리즈 빌드 실행. → [journal](journal/2026-09-13.md#7차--mac에서-pull-후-로컬-검증--릴리즈-빌드-실행-사용자-요청)
- **2026-09-13 (6차)** — `feat/i18n-lang-menu` → main ff → push. CI 3-OS green(run 34736693426).
- **2026-09-13 (5차)** — 언어 ▸ 메뉴(en/ko/ja/zh) · 문구 NUL 블록으로 크기 최소화 · `lang=` 설정 · ja/zh 툴팁 어순. → [journal](journal/2026-09-13.md#5차--언어-선택-메뉴english--한국어--日本語--中文--i18n-사용자-요청)
- **2026-09-13 (4차)** — Linux 실기 첫 확인(GNOME 50). 아이콘 안 보임 = Register 응답 대기 중 `GetAll` 버림 → `handle` 연결로 수정. 대기/동작 아이콘 · 억제 ✓. → [journal](journal/2026-09-13.md#4차--linux-실기gnome-50--ubuntu-appindicators-트레이-아이콘-안-보임--등록-경쟁-수정-사용자-qa)
- **2026-09-13 (3차)** — `feat/dialog-ui-memory` 6커밋 → main ff → push.
- **2026-09-13 (2차)** — 상주 메모리 7.4→5.9 MB(MallocSpaceEfficient) · ICO PNG 프레임(exe 37,888→26,624 B) · icns 62 KB · 미사용 함수 제거. → [journal](journal/2026-09-13.md)
- **2026-09-13** — 메뉴 맨 위 남은 시간 항목(일·시·분·초 · 1초 갱신 · 복수형). 3-OS. → [journal/2026-09-13](journal/2026-09-13.md)
- **2026-09-12 (3차)** — 사용자 QA: 사용자 지정 = 입력 창(일·시·분) · 자동 시작 끄기/사용자 지정 · About. 3-OS 구현 + 스모크 확장. → [journal](journal/2026-09-12.md#4차--입력-창--about--자동-시작-2택)
- **2026-09-12 (2차)** — 첫 커밋: `feat/scaffold` 9커밋 → main ff 병합 → push → CI 3-OS green(트랩 수정 1건). → [journal/2026-09-12](journal/2026-09-12.md#2차--커밋--병합--push)
- **2026-09-12** — 프로젝트 생성. 3-OS 골격 · 코어 · 플랫폼 3종 · 브랜딩 · 패키징 · CI · 문서 한 번에. Windows 33 KB · macOS 실기 확인 · Linux D-Bus 스모크 green. → [journal/2026-09-12](journal/2026-09-12.md)
