# 19 · 보안 취약점 관리 원장

> **무엇**: 메모리 읽기 경계 · 할당 영역 밖 접근 · 정수 초과(오버플로) · 외부 입력 검증 — 순수 C 코드가 반드시 지켜야 할 항목의 **체크리스트 · 점검 절차 · 발견/수정 원장**(사용자 요청 11-07). 결함을 고치면 여기에 `V-n` 행을 남기고, 새 입력 경계를 만들면 §1 표에 한 줄을 더한다. 절차 SSOT는 [18 빌드·테스트](18-build-and-test.md)(`make test-san`), 결정은 [10](10-decision-record.md).

## 0. 위협 모델 — 한 줄

트레이 상주 앱 · 네트워크 없음 · 권한 상승 없음. 공격면은 **같은 사용자 권한으로 닿는 입력**이다: 설정 파일 · 자식 프로세스 stdout · 명령줄 인자 · D-Bus 세션 버스의 다른 피어 · 환경 변수. 결과는 대개 **크래시(상주 앱 사라짐 = 절전 방지 해제)** 또는 스택 정보 노출이지 코드 실행 경로가 아니지만, 코어는 CRT 없이 링크되는 프로세스라 UB 하나가 Windows에서 바로 ACCESS_VIOLATION이 된다.

## 1. 입력 경계(신뢰하지 않는 데이터가 들어오는 자리)

| # | 경계 | 코드 | 누가 쓸 수 있나 | 파서 | 검증 |
| --- | --- | --- | --- | --- | --- |
| B-1 | 설정 파일 `config`(≤255 B 읽음) | `cf_conf_parse`(app.c) ← linux/win/mac `conf_load` | 같은 사용자의 아무 프로세스 | 줄 단위 · `[v, nl)` 경계 · `cf_atoi_n` 포화 · clamp | t_hardening(V-1) · 3 플랫폼 NUL 종단 |
| B-2 | 입력 창 자식 stdout "d h m"(≤63 B) | `cf_parse_dhm` ← linux `dlg_buf` · win `child_done` · mac `childDone` | yad/zenity/kdialog(Linux) · 자기 자신(Win/mac) | 공백·`\|` 구분 · 소수부 건너뜀 · 포화 → `cf_dialog_submit` clamp | t_hardening(V-2) · 호출부가 NUL 종단 |
| B-3 | 명령줄 `--dialog mode d h m lang` · `--about lang` | mac `child_main`(`arg_int`) · win `child_main`(`wint`) | 아무 사용자(exe 직접 실행) | 포화 정수 · `cf_app_init` 언어 clamp · `s_()` 범위 가드 | t_hardening(언어·ID) · V-4·V-5 |
| B-4 | D-Bus 메시지(세션 버스 피어 · 시스템 버스 logind) | `db_parse` · `db_r_*`(dbus.c) → linux.c 핸들러 | 세션 버스에 붙은 모든 프로세스 | 길이 = 남은 바이트와 비교 · NUL 검증 · 중첩 ≤64 · 메시지 ≤2²⁷ | **test_dbus**(조작 길이 · 깊이 · 3만 회 변형) · V-6~V-10 |
| B-5 | D-Bus 핸들러 인자(메뉴 id · 속성 이름 · 이벤트) | linux.c `handle_menu` · `on_click` → `cf_app_click` · `cf_menu_item` | 같음 | 알 수 없는 id → 0/`CF_ACT_NONE` · 배열 상한 64/16 | t_hardening(ID 범위) · 스모크 |
| B-6 | 환경 변수 `HOME` `XDG_*` `DBUS_*` `PATH` `LANG` | linux.c `conf_init` · `autostart_path` · `has_tool` · dbus.c `db_connect_*` | 실행 환경 | `snprintf`(절단) · `sun_path` 길이 검사 · `has_tool` 길이 검사 | 코드 검토(§4 안전 판정) |
| B-7 | 아이콘 픽셀 버퍼(코어 래스터) | `cf_icon_idle/active`(size · 내부 상수) | 내부 | 크기는 상수 표(16/18/22/36/44) | icon_dump(ASan) |

## 2. 필수 검증 항목(새 코드 · 리뷰 체크리스트)

1. **길이가 있는 입력은 끝 포인터로 파싱한다.** `read()` 결과 · 파이프 · 와이어는 NUL 종단을 **가정하지 않는다**(`cf_atoi_n(s, end, &e)` · `e < end && *e == ','`). 호출부도 NUL을 넣는다(심층 방어).
2. **정수 파싱은 포화**(`cf_atoi*` 1e18 · win `wint` · mac `strtol`) 뒤 **clamp**(`clampi`). `atoi`·부호 있는 `v*10` 누적 금지.
3. **부호 있는 부정·곱은 오버플로 가능성 확인** — `-INT64_MIN`은 UB(`cf_itoa`는 부호 없는 산술).
4. **표 인덱스는 `(unsigned)x < COUNT`** 로 한 번에(음수 포함) — `s_()` · `cf_set_os_lang` · `cf_app_init` · `NO_TOOL[]`.
5. **와이어 길이는 `n > end - pos`** 로 비교한다(`pos + n`은 랩어라운드) · 길이 상한(명세 2²⁷) · **문자열 NUL 바이트 확인** 뒤에만 `strcmp`.
6. **재귀에는 깊이 상한**(variant/struct 64) — 입력이 고르는 깊이로 스택을 넘치게 못 하게.
7. **복사는 cap 있는 함수만**: 코어 `cf_strcat(dst, cap, …)` · 플랫폼 `snprintf`/`strncat`/`strncpy(…, n-1)`. `strcpy`는 **상수 리터럴**에만(dbus.c `sasl` 1곳).
8. **고정 배열에 모을 때 상한 검사**(`ids[64]` `clicked[16]` `fds[16]` `label[130]`) — 넘치면 버린다, 늘리지 않는다.
9. **포맷 문자열에 외부 입력 금지**(`snprintf(buf, n, "%s", s)`처럼 인자로) · **자식 프로세스는 `execvp` 배열**(셸 경유 금지 · Windows `CreateProcess` 인자는 우리가 만든 숫자만).
10. **파일은 0600/0700** · 설정 디렉터리는 사용자 전용 · 잠금 파일 `flock`.
11. **자원은 짝으로**: `CFRelease`(Copy 규칙) · `IOPMAssertionRelease` · fd close · `free` — 30초 타이머 안이면 누수 = 상주 메모리 증가.
12. **코어 금지**(DR-2): libc · float · heap. 플랫폼 파일만 OS API.
13. **시험은 수정 전 코드에서 실패해야 한다** — 결함을 고칠 때 재현 시험을 먼저 쓰고 `make test-san`으로 빨강을 보고 고친다(V-1 · V-6~V-9는 그렇게 확인했다 · §4).

## 3. 도구 · 절차

| 언제 | 무엇 | 비고 |
| --- | --- | --- |
| push 전(개발 PC) · CI ubuntu | `make test-san` — test_core · test_dbus · icon_dump를 **ASan + UBSan**(`-fno-sanitize-recover=all`)으로 | 경계 밖 읽기 · 정수 오버플로 · 미초기화 · 메모리 누수. `make test`는 같은 시험의 일반 빌드 |
| 입력 경계·파서를 바꿀 때 | 시험 추가(t_hardening 또는 test_dbus) + §1 표 갱신 | 정확한 길이의 `malloc` 사본으로 파싱 → ASan이 1바이트 초과를 잡는다 |
| 릴리스 전 · 분기 코드 추가 | `clang --analyze src/core/*.c` · `clang --analyze -fobjc-arc src/plat/mac.m` · `-Wconversion -Wshadow` 경고 0 확인 | 11-07 기준 0건 |
| 결함 발견 | 이 문서 §4에 `V-n` 행(위치 · 유형 · 심각도 · 입력 경계 · 수정 커밋 · 시험) → 커밋 본문에 `docs/19 V-n` | 심각도: 높음 = 외부 입력으로 경계 밖 접근 · 중 = UB/고갈 · 낮음 = 심층 방어 |
| D-Bus 계약 | `sh scripts/linux-smoke.sh`(가짜 워처·logind) | 기능 계약 · 보안은 test_dbus가 담당 |

## 4. 발견·수정 원장

11-07 1차 점검(개발 세션 · 코어 7파일 · dbus.c · linux.c · win.c · mac.m 전수 읽기 + ASan/UBSan + clang 정적 분석). 심각도 = §3 기준.

| V | 위치 | 유형 | 심각도 | 경계 | 내용 → 수정 | 시험 |
| --- | --- | --- | --- | --- | --- | --- |
| V-1 | `app.c parse_dhm` · linux/win/mac `conf_load` | 할당 밖 읽기 | **높음** | B-1 | 설정 버퍼(`char buf[256]`에 `read` 255 B · NUL 없음)를 `cf_atoi`가 NUL까지 읽어 **스택 밖**까지 숫자를 이어 읽음 · 줄 경계도 무시(다음 줄 숫자 결합). → `parse_dhm(v, end, …)` + `cf_atoi_n` 경계 파싱 · 3 플랫폼 `buf[n] = 0` | t_hardening(정확한 길이 힙 사본) — **수정 전 코어에서 ASan heap-buffer-overflow READ 재현** |
| V-2 | `util.c cf_atoi` | 정수 초과(UB) | 중 | B-1 · B-2 | 자릿수 제한 없는 `v*10+d` → i64 오버플로 UB(UBSan 실측 `999999999999999999 * 10`). → 1e18 포화 · 포화 뒤에도 숫자 끝까지 소비(끝 포인터 보존) | t_hardening |
| V-3 | `util.c cf_itoa` | 정수 초과(UB) | 낮음 | 내부 | `v = -v`가 INT64_MIN에서 UB. → 부호 없는 산술 | t_hardening(`-9223372036854775808`) |
| V-4 | `app.c cf_app_init` | 표 인덱스 | 낮음(심층) | B-3 | 명령줄 `lang`이 그대로 `a->lang`. 읽는 쪽 `s_()`가 가드하고 있어 노출은 없었음. → init에서 clamp | t_hardening(12345 · -1) |
| V-5 | `mac.m child_main atoi` · `win.c wint` | 정수 초과(UB) | 낮음 | B-3 | `atoi` 범위 밖 UB · `wint` 누적 오버플로. → `strtol` + int clamp(`arg_int`) · `wint` 1e18 포화 | 코드 검토(Windows 시험 없음 · CI 빌드만) |
| V-6 | `dbus.c rneed` | 할당 밖 읽기 | **높음** | B-4 | `r->pos + n > r->end`가 큰 n에서 **랩어라운드**해 통과 → 문자열/배열이 메시지 밖을 가리킴. → `n > end - pos` | test_dbus t_header_lengths(0xFFFFFFFF · 0xFFFFFFF0 · 0x7FFFFFFF) · t_body(ai 길이) |
| V-7 | `dbus.c db_r_str` · `db_r_sig` | 할당 밖 읽기 | **높음** | B-4 | 길이만 보고 NUL을 확인하지 않아 `strcmp(r->member, …)`가 메시지 끝을 넘어 읽음 · `n+1` 랩어라운드(n=0xFFFFFFFF). → `s[n] == 0` 검증 · n 상한 | test_dbus(NUL 제거 → -1) |
| V-8 | `dbus.c db_r_skip` | 스택 고갈 | 중 | B-4 | variant 안 variant… 깊이 무제한 재귀 → 피어가 스택을 넘치게 할 수 있음. → 깊이 64 상한(명세) | test_dbus t_body(200겹) |
| V-9 | `dbus.c db_parse` | 정수 초과 · 무한 루프 | 중 | B-4 | `fields_len`/`body_len` 상한 없음 → `hdr_len`·`*total` u32 랩어라운드. **수정 전 dbus.c에 새 test_dbus를 돌리자 120초 넘게 멈춤**(스택 표본: `db_parse` → `db_r_struct` → `ralign` — 랩어라운드한 `fields_len`까지 `pos++`를 수십억 번 = 피어가 앱을 사실상 멈출 수 있음). → 2²⁷ 상한 · 초과 = 폐기(`total = 0`) | test_dbus(거대 길이 → -1 · 퍼징 3만 회 완주) |
| V-10 | `dbus.c db_r_skip` n/q/x/t/d | 경계 미검사 | 낮음 | B-4 | 고정폭 건너뛰기가 `pos += 2/8`만 하고 검사 없음(이후 읽기는 `rneed`가 막아 실해 없음). → `rneed` | test_dbus(`x` 1바이트) |

**점검했으나 결함 없음(11-07)**: `cf_strcat`/`cf_itoa` cap · `CfMenuItem.label[64]` → Windows `WCHAR w[64]`(UTF-8 64 B ≤ 64 코드 유닛) · `menu_tick` 96/96 · linux `dlg_buf[128]` 상한·NUL · `label[130]` 니모닉 이스케이프 상한 · `readlink` `exe[n]=0` · `has_tool` 경로 길이 · `parse_addr` `sun_path` 길이 · SASL `auth[96]`(최대 57 B) · SCM_RIGHTS 대기열 16 초과 close · `GetGroupProperties ids[64]` · `EventGroup clicked[16]` · 메뉴 id 범위(`cf_app_click` · `cf_menu_item` 기본값) · 경로 `snprintf` 절단(1024) · 설정 0600 · lock `flock` · 자식 `execvp` 배열(셸 없음) · 코어 `clang --analyze` 0건 · `-Wconversion` 0건(코어).

## 5. 열린 항목

- Windows 쪽 런타임 시험이 없다(CI는 MSVC/mingw **빌드**만 · `child_done`·`wint`는 코드 검토). → ASan은 MSVC도 지원(`/fsanitize=address`) — Windows PC 회차에 `build.bat` 시험 빌드 추가 검토(T-24).
- `fill()`은 수신 버퍼를 메시지 총길이까지 키운다(상한 2²⁷ = 128 MB · 피어가 보낸 만큼만) — 메모리 압박 공격은 세션 버스 데몬의 자체 상한(기본 128 MB)과 같아 추가 상한은 두지 않음.
- 정기 재점검: 입력 경계가 늘 때(§1) · 릴리스 전 §3 정적 분석.
