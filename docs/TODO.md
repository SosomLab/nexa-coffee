# TODO — 백로그

| ID | 우선 | 규모 | 내용 | 상태 |
| --- | --- | --- | --- | --- |
| T-1 | P0 | 소 | 첫 커밋 · main 병합 · push | ✅ 09-12 |
| T-2 | P0 | 중 | Windows 실기 — 빌드·트레이·툴팁·ko 로케일 ✅ 09-13 · `powercfg /requests`(DISPLAY·SYSTEM + 해제) ✅ · 메뉴 라디오/체크 ✅ · 열린 메뉴 1초 갱신(59:58→59:54) ✅ 09-17 · **남음**: 아이콘 DPI(100/150/200%) · TaskbarCreated — 둘 다 화면 설정·셸을 건드려야 해 자동화 안 함. 절차·합격 기준은 [18 실기 점검표](18-build-and-test.md#사람이-해야-하는-실기-점검windows) | ◐ |
| T-3 | P0 | 중 | Linux 실기 — GNOME(AppIndicator) ✅ 09-13(등록 경쟁 수정) · **남음**: KDE Plasma · 입력 창 클릭 확인 · logind polkit 거부 시 fallback · 워처 재등장 재등록 | ◐ |
| T-10 | P1 | 소 | Linux 16px 픽스맵 프레임 추가 — GNOME 패널이 22→16 축소해 두 자리 숫자 흐릿(09-13 실기) · 스모크에 "Register 응답 전 GetAll" 순서 재현 | ☐ |
| T-11 | P0 | 소 | i18n 실기 — **Windows ✅ 09-17**: 메뉴 CJK 글꼴(ko/ja/zh) · 언어 메뉴 라디오 · 입력 창 라벨 폭 네 언어 전부 잘림 없음(`キャンセル`·`取消` 포함). **남음**: macOS 실기(맥 필요) · `LANG_CHINESE` 번체/간체 구분 없음(간체만 — 알려진 한계) | ◐ |
| T-13 | P1 | 소 | Windows 상주 메모리 — ① 메뉴·자식 종료 뒤 `SetProcessWorkingSetSize(-1,-1)` ② 클래스 아이콘 LoadIcon 제거(창 아이콘은 `cf_icon_idle`) — 09-13 10차 구현 · 14차 Windows 실기: 부모 모듈 37→25 · 코덱/TSF 없음 · WS 1.26 MB | ✅ 09-13 |
| T-12 | P1 | 소 | 일본어·중국어 문구 원어민 검토 — About 설명 · logind 억제 사유 · "대화창 도구 없음" 알림 | ☐ |
| T-4 | P1 | 소 | macOS 화면보호기 실측(30초 사용자 활동 선언으로 충분한지) — 부족하면 `IOPMAssertionCreateWithProperties` 검토 | ☐ |
| T-5 | P1 | 소 | 시작 프로그램 등록 안내/옵션(Windows shell:startup · macOS 로그인 항목 · Linux ~/.config/autostart) | ☐ |
| T-6 | P1 | 중 | 채널 등록 — brew ✅(0.1.1) · choco: iconUrl 수정 후 **0.1.0 재제출 ✅**(모더레이션 대기 · 승인돼야 0.1.1 push 가능) · winget **PR [#436346](https://github.com/microsoft/winget-pkgs/pull/436346)**: **검증 통과**(`Azure-Pipeline-Passed`·`Validation-Completed`) → 사람 리뷰·머지 대기. 노출되면 README 설치표 확인 | ◐ |
| T-7 | P2 | 소 | Windows 256px PNG 아이콘 프레임 여부 — ICO를 PNG 프레임으로 바꿔 16·32·48이 3.7 KB. 256은 +10 KB라 보류 | ✅ 09-13 |
| T-8 | P2 | 소 | 툴팁에 종료 예정 시각 추가 여부 | ☐ |
| T-9 | P2 | 소 | Docker 기반 Linux 검증(`scripts/linux-docker.sh`)을 실제로 한 번 돌리기(이 세션은 Docker 데몬 미가동) | ☐ |
| T-14 | P1 | 중 | **창을 자식 프로세스로**(DR-14) — macOS ✅ · Windows ✅ 09-13(자식 창·TSF·작업 집합) · **시작 버튼 → 파이프 → 작업 시작 ✅ 09-17**(메뉴→입력 창 자식 프로세스→Enter→powercfg 억제 등록→메뉴 남은 시간 일치, 끝단까지 자동 검증) | ✅ 09-17 |
| T-15 | P2 | 소 | cask `postflight` → `postflight_steps` DSL 전환 검토(Homebrew deprecated 경고 · nexa-clip 공통) | ☐ |
| T-18 | P2 | 소 | **대기 중 DPI 변경 시 트레이 아이콘이 옛 크기로 남는다** — `job_tick()`은 매 틱 `icon_size()`를 다시 읽지만(동작 중 ✅), 대기 중엔 `show_idle()`을 부르는 계기가 없다. `WM_DPICHANGED`/`WM_SETTINGCHANGE`에서 `!running`이면 `show_idle(FALSE)` (09-17 코드 관찰). 재현 절차는 [18 실기 점검표](18-build-and-test.md#사람이-해야-하는-실기-점검windows)의 ① 5단계 | ☐ |
| T-16 | P0 | 소 | **Windows x86 백신 오탐** `Trojan:Win32/Tecabans.STV!cl`(클라우드 ML · x64는 깨끗) — PE 버전 리소스 추가 ✅ · v0.1.1 실제 자산으로 `winget install --architecture x86` **성공** ✅ 09-17. 재발하면 [WDSI 오탐 신고](https://www.microsoft.com/en-us/wdsi/filesubmission)(웹 폼 · 사용자 직접). 근본 해결은 코드 서명(v1 범위 밖) | ✅ 09-17 |
| T-17 | P1 | 소 | `WINGET_TOKEN`(PAT)에 `workflow` 스코프 — 사용자가 기존 classic PAT 스코프만 수정(값 유지 · 시크릿 재등록 없음). `repo, workflow` 확인 · 포크 fast-forward 성공(upstream과 identical). 워크플로에 스코프 진단 + 무조건 동기화 추가 | ✅ 09-17 |
