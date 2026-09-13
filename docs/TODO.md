# TODO — 백로그

| ID | 우선 | 규모 | 내용 | 상태 |
| --- | --- | --- | --- | --- |
| T-1 | P0 | 소 | 첫 커밋 · main 병합 · push | ✅ 09-12 |
| T-2 | P0 | 중 | Windows 실기 — MSVC 로컬 빌드·test green · 트레이 등록·툴팁·ko 로케일 ✅ 09-13(레지스트리) · **남음**: 아이콘 DPI(100/150/200%) · 메뉴 라디오/체크 · 열린 메뉴 1초 갱신 재그리기 · `powercfg /requests` · TaskbarCreated | ◐ |
| T-3 | P0 | 중 | Linux 실기 — GNOME(AppIndicator) ✅ 09-13(등록 경쟁 수정) · **남음**: KDE Plasma · 입력 창 클릭 확인 · logind polkit 거부 시 fallback · 워처 재등장 재등록 | ◐ |
| T-10 | P1 | 소 | Linux 16px 픽스맵 프레임 추가 — GNOME 패널이 22→16 축소해 두 자리 숫자 흐릿(09-13 실기) · 스모크에 "Register 응답 전 GetAll" 순서 재현 | ☐ |
| T-11 | P0 | 소 | i18n Windows·macOS **실기** 확인 — 빌드는 09-13 7차에 Mac에서 확인(mingw x64 27,136 B · mac 154 KB). 메뉴 CJK 글꼴 · 입력 창 라벨 폭(일본어 "キャンセル"·중국어) · Windows `LANG_CHINESE` 번체/간체 구분 없음(간체 문구만) | ☐ |
| T-13 | P1 | 소 | Windows 상주 메모리 — ① 메뉴·자식 종료 뒤 `SetProcessWorkingSetSize(-1,-1)` ② 클래스 아이콘 LoadIcon 제거(창 아이콘은 `cf_icon_idle`) — 09-13 10차 구현 · **Windows 실기 확인 필요** | ◐ |
| T-12 | P1 | 소 | 일본어·중국어 문구 원어민 검토 — About 설명 · logind 억제 사유 · "대화창 도구 없음" 알림 | ☐ |
| T-4 | P1 | 소 | macOS 화면보호기 실측(30초 사용자 활동 선언으로 충분한지) — 부족하면 `IOPMAssertionCreateWithProperties` 검토 | ☐ |
| T-5 | P1 | 소 | 시작 프로그램 등록 안내/옵션(Windows shell:startup · macOS 로그인 항목 · Linux ~/.config/autostart) | ☐ |
| T-6 | P1 | 중 | 채널 등록 — render-manifests.sh + homebrew.yml + publish-windows-packages.yml · v0.1.0 태그로 첫 제출(09-13 11차) · winget/choco 검수 대기 추적 | ◐ |
| T-7 | P2 | 소 | Windows 256px PNG 아이콘 프레임 여부 — ICO를 PNG 프레임으로 바꿔 16·32·48이 3.7 KB. 256은 +10 KB라 보류 | ✅ 09-13 |
| T-8 | P2 | 소 | 툴팁에 종료 예정 시각 추가 여부 | ☐ |
| T-9 | P2 | 소 | Docker 기반 Linux 검증(`scripts/linux-docker.sh`)을 실제로 한 번 돌리기(이 세션은 Docker 데몬 미가동) | ☐ |
| T-14 | P1 | 중 | **창을 자식 프로세스로**(DR-14) — macOS 구현·실측 ✅(부모 6.0 MB 유지) · Windows 구현 ✅(크로스 빌드만 · 실기 ☐: 입력 창 결과 파이프 · About · 작업 집합) | ◐ |
