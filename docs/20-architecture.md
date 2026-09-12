# 20 · 아키텍처

## 1. 경계

```
┌─ src/core (플랫폼 비종속 · libc/float/heap 없음) ─────────────────────────────┐
│ timer.c   잔여초·전체초 → {unit, value, frac/4096} · 다음 변화 ms               │
│ draw.c    정수 atan2(LUT 65) · sin/cos(LUT 65) · isqrt · straight-alpha over   │
│ font.c    3x5 숫자 · 7x4 ∞                                                     │
│ icon_idle.c   대기 아이콘(실선 1/3 · 옅은 실선 2/9 · 점 4/9) — 색 인자           │
│ icon_active.c 동작 아이콘(외곽선 + 잔여 호 + 꼬리 페이드 17% + 단위색 정수)      │
│ app.c     상태(CfApp) · 메뉴 트리(id 고정) · 클릭→행동 · 설정 텍스트 · 툴팁 · ko/en │
└──────────────────────────────────────────────────────────────────────────────┘
        ▲ RGBA 버퍼 · 메뉴 트리 · 행동(START/STOP/MENU/QUIT)
┌─ src/plat ───────────────────────────────────────────────────────────────────┐
│ win.c    Shell_NotifyIcon · DIB→HICON · HMENU 재귀 · SetThreadExecutionState    │
│ mac.m    NSStatusItem · CGImage(1x/2x) · NSMenu 재귀(열릴 때 재구성) · IOPM 어설션 │
│ linux.c  SNI 속성/신호 · dbusmenu(GetLayout 재귀·Event) · ScreenSaver·logind      │
│ dbus.c   유닉스 소켓 · SASL EXTERNAL · UNIX_FD · 마샬/언마샬 · 동기 호출           │
└──────────────────────────────────────────────────────────────────────────────┘
```

플랫폼은 세 가지만 한다: ① 코어 트리로 네이티브 메뉴 만들기 ② 코어 RGBA를 OS 이미지로 감싸기 ③ 타이머·절전 억제·설정 파일 IO.

## 2. 아이콘 래스터
- 슈퍼샘플 4×4. 샘플 중심을 2배 정수 좌표(`X2 = 2·sx + 1`)로 두어 반지름도 정수(px × 8).
- 각도 = `cf_angle(dx, dy)` — 12시 = 0 · 시계방향 · 1바퀴 4096. 호 가중치 = θ < frac 이면 256, 꼬리 `FADE = 17%` 구간 선형 감쇠.
- 글리프 칸 크기는 안지름 대각선 92%에 맞춤. 칸이 1~2px면 1px로 고정 + 원점 픽셀 스냅(16px 선명도).
- 출력 = straight RGBA. Windows는 premultiplied BGRA로, Linux SNI는 ARGB 네트워크 순서로, mac은 CGImage `kCGImageAlphaLast`.

## 3. 시간 규칙
- 표시: ≥24h 일 · ≥1h 시 · ≥1m 분 · 초. 내림. 2자리 클램프(99).
- 호 비율은 전체의 1/256 단위로 양자화 → 다음 갱신 시각을 계산해 **1회성 타이머**로 깨어난다(초 단위 구간만 1초).
- 종료 = 잔여 ≤ 0 → 억제 해제 · 대기 아이콘 · 버퍼 free · OS 메모리 반납. `sel`은 유지(자동 시작이 같은 시간을 다시 쓴다).

## 4. 메뉴 id(3-OS 공통)
`1 ∞ · 2 12h · 3 6h · 4 2h · 5 1h · 6 30m · 7 사용자 지정▸ · 8 끄기 · 9 ─ · 10 자동 시작 · 11 ─ · 12 종료`
`20 시작 · 21 ─ · 22 일▸(100+d) · 23 시간▸(200+h) · 24 분▸(300+m/5)`

## 5. 설정 파일
`auto=0|1` · `sel=N` · `custom=d,h,m` — Windows `%APPDATA%\nexa-coffee\config` · macOS `~/Library/Application Support/nexa-coffee/config` · Linux `$XDG_CONFIG_HOME/nexa-coffee/config`. 같은 폴더의 `lock`(flock) / 뮤텍스로 중복 실행 방지.

## 6. OS별 절전 억제
| OS | 켜기 | 끄기 |
| --- | --- | --- |
| Windows | `SetThreadExecutionState(ES_CONTINUOUS\|ES_SYSTEM_REQUIRED\|ES_DISPLAY_REQUIRED)` | `SetThreadExecutionState(ES_CONTINUOUS)` |
| macOS | `IOPMAssertionCreateWithName` ×2 + 30초 `IOPMAssertionDeclareUserActivity` | `IOPMAssertionRelease` · 타이머 무효화 |
| Linux | `ScreenSaver.Inhibit(ss)→u`(비동기 · 쿠키) + `login1.Manager.Inhibit("sleep:idle:handle-lid-switch", block)→h`(fd) | `UnInhibit(u)` · fd close |

## 7. Linux D-Bus 최소 구현 범위
연결(path/abstract) · SASL EXTERNAL(uid hex) · `NEGOTIATE_UNIX_FD` · Hello · RequestName · AddMatch(워처 재등장) · 메서드 호출/회신/오류 · 신호 · 컨테이너(a · ( ) · { } · v) · 리틀엔디언만(빅엔디언 송신자는 버림).
