# TODO — 백로그

| ID | 우선 | 규모 | 내용 | 상태 |
| --- | --- | --- | --- | --- |
| T-1 | P0 | 소 | 첫 커밋 · main 병합 · push | ✅ 09-12 |
| T-2 | P0 | 중 | Windows 실기 — 빌드·트레이·툴팁·ko 로케일 ✅ 09-13 · `powercfg /requests`(DISPLAY·SYSTEM + 해제) ✅ · 메뉴 라디오/체크 ✅ · 열린 메뉴 1초 갱신(59:58→59:54) ✅ 09-17 · **남음**: 아이콘 DPI(100/150/200%) · TaskbarCreated — 둘 다 화면 설정·셸을 건드려야 해 자동화 안 함. 절차·합격 기준은 [18 실기 점검표](18-build-and-test.md#사람이-해야-하는-실기-점검windows) | ◐ |
| T-3 | P0 | 중 | Linux 실기 — GNOME(AppIndicator) ✅ 09-13(등록 경쟁 수정) · **자동화 ✅ 09-26**(스모크 가짜 워처·logind): 워처 재등장 재등록(**v0.1.1 SIGSEGV 발견·수정** — T-19) · polkit 거부 시 `sleep:idle` 재시도 · 전부 거부돼도 타이머 유지 · Inhibit fd 해제 · **남음**: KDE Plasma(VM 없음) · 입력 창 실제 클릭(사람 · 다른 앱 자동 시험과 겹쳐 입력 주입 안 함) | ◐ |
| T-10 | P1 | 소 | Linux 16px 픽스맵 프레임 — `ICON_SIZES` {16,22,44}(상주 +1 KB) · 16px 덤프에서 두 자리 숫자 선명 · 스모크가 GetAll/IconPixmap에서 16/22/44 확인 · "Register 응답 전 GetAll" 순서는 가짜 워처로 재현 ✅ 09-26. **남음**: GNOME 패널 실물 확인(설치본 교체 후 · 사용자) | ✅ 09-26 |
| T-11 | P0 | 소 | i18n 실기 — **Windows ✅ 09-17**: 메뉴 CJK 글꼴(ko/ja/zh) · 언어 메뉴 라디오 · 입력 창 라벨 폭 네 언어 전부 잘림 없음(`キャンセル`·`取消` 포함). **남음**: macOS 실기(맥 필요) · `LANG_CHINESE` 번체/간체 구분 없음(간체만 — 알려진 한계) | ◐ |
| T-13 | P1 | 소 | Windows 상주 메모리 — ① 메뉴·자식 종료 뒤 `SetProcessWorkingSetSize(-1,-1)` ② 클래스 아이콘 LoadIcon 제거(창 아이콘은 `cf_icon_idle`) — 09-13 10차 구현 · 14차 Windows 실기: 부모 모듈 37→25 · 코덱/TSF 없음 · WS 1.26 MB | ✅ 09-13 |
| T-12 | P1 | 소 | 일본어·중국어 문구 원어민 검토 — About 설명 · logind 억제 사유 · "대화창 도구 없음" 알림 | ☐ |
| T-4 | P1 | 소 | macOS 화면보호기 실측(30초 사용자 활동 선언으로 충분한지) — 부족하면 `IOPMAssertionCreateWithProperties` 검토 | ☐ |
| T-5 | P1 | 소 | 시작 프로그램 등록 — **Linux ✅ 09-26**: `install.sh --autostart`(XDG autostart · uninstall이 함께 제거 · `scripts/test-install.sh` CI 검증 · T-19 수정으로 셸보다 먼저 떠도 안전) · Windows(`shell:startup`)·macOS(로그인 항목)는 README 안내. **결정 대기**: 앱 메뉴에 "로그인 시 실행" 토글을 넣을지(3-OS 코어 메뉴 변경) | ◐ |
| T-6 | P1 | 중 | 채널 등록 — brew ✅(**0.1.2**) · choco 0.1.0: 09-14 iconUrl · 09-21 `<copyright>` 반영해 09-26 재제출(Updated · 재스캔) → **승인되면 0.1.2를 `tag=v0.1.2 channels=choco force=true`로**(가드는 직전 태그 0.1.1을 보는데 0.1.1은 안 올렸다 · 패키지 페이지를 며칠마다 확인) · winget **PR [#436346](https://github.com/microsoft/winget-pkgs/pull/436346)**(0.1.1) 검증 통과 → 머지 대기 → 머지되면 0.1.2 제출. 노출되면 README 설치표 확인 | ◐ |
| T-7 | P2 | 소 | Windows 256px PNG 아이콘 프레임 여부 — ICO를 PNG 프레임으로 바꿔 16·32·48이 3.7 KB. 256은 +10 KB라 보류 | ✅ 09-13 |
| T-8 | P2 | 소 | 툴팁에 종료 예정 시각 추가 여부 | ☐ |
| T-9 | P2 | 소 | Docker 기반 Linux 검증(`scripts/linux-docker.sh`)을 실제로 한 번 돌리기(이 세션은 Docker 데몬 미가동) | ☐ |
| T-14 | P1 | 중 | **창을 자식 프로세스로**(DR-14) — macOS ✅ · Windows ✅ 09-13(자식 창·TSF·작업 집합) · **시작 버튼 → 파이프 → 작업 시작 ✅ 09-17**(메뉴→입력 창 자식 프로세스→Enter→powercfg 억제 등록→메뉴 남은 시간 일치, 끝단까지 자동 검증) | ✅ 09-17 |
| T-15 | P2 | 소 | cask `postflight` → `postflight_steps` DSL 전환 검토(Homebrew deprecated 경고 · nexa-clip 공통) | ☐ |
| T-18 | P2 | 소 | **대기 중 DPI 변경 시 트레이 아이콘이 옛 크기로 남는다** — 09-26 코드 수정: `icon_size()`가 `GetSystemMetricsForDpi(SM_CXSMICON, GetDpiForWindow)`(PMv2에서 `GetSystemMetrics`는 시작 시점 값에 머묾) + `WM_DPICHANGED`/`WM_DISPLAYCHANGE`/`WM_SETTINGCHANGE`에서 대기 중 `show_idle(FALSE)`. **남음**: Windows 실기([18 점검표](18-build-and-test.md#사람이-해야-하는-실기-점검windows) ① 5단계) | ◐ |
| T-16 | P0 | 소 | **Windows x86 백신 오탐** `Trojan:Win32/Tecabans.STV!cl`(클라우드 ML · x64는 깨끗) — PE 버전 리소스 추가 ✅ · v0.1.1 실제 자산으로 `winget install --architecture x86` **성공** ✅ 09-17. 재발하면 [WDSI 오탐 신고](https://www.microsoft.com/en-us/wdsi/filesubmission)(웹 폼 · 사용자 직접). 근본 해결은 코드 서명(v1 범위 밖) | ✅ 09-17 |
| T-17 | P1 | 소 | `WINGET_TOKEN`(PAT)에 `workflow` 스코프 — 사용자가 기존 classic PAT 스코프만 수정(값 유지 · 시크릿 재등록 없음). `repo, workflow` 확인 · 포크 fast-forward 성공(upstream과 identical). 워크플로에 스코프 진단 + 무조건 동기화 추가 | ✅ 09-17 |
| T-19 | P0 | 소 | **Linux: 앱보다 늦게 뜬 트레이 워처에서 SIGSEGV**(v0.1.0·0.1.1) — NameOwnerChanged 핸들러 안의 동기 등록이 버퍼 맨 앞의 같은 신호를 다시 꺼내 무한 재귀. 로그인 자동 시작·셸 재시작·확장 켜기에서 발생. `need_register` 플래그로 메인 루프에서 등록 · 스모크가 회귀 검출(v0.1.1 실패 / 수정본 통과). **v0.1.2로 배포** | ✅ 09-26 |
